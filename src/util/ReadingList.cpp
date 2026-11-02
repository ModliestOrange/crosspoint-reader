#include "ReadingList.h"

#include <FsHelpers.h>
#include <HalStorage.h>
#include <LibraryBuilder.h>
#include <LibraryIndexFile.h>
#include <LibraryText.h>
#include <Logging.h>
#include <ReadingListText.h>

#include <algorithm>
#include <functional>

#include "CrossPointSettings.h"
#include "RecentBooksStore.h"
#include "util/BookProgress.h"

namespace {
bool isBookPath(const std::string_view entry) {
  return FsHelpers::hasReflowableBookExtension(entry) || FsHelpers::hasXtcExtension(entry);
}

// Same cache folder the readers write progress.bin into (Epub.cpp, Xtc.cpp).
bool hasSavedProgress(const std::string& path) {
  const char* prefix = FsHelpers::hasXtcExtension(path) ? "/.crosspoint/xtc_" : "/.crosspoint/epub_";
  const std::string progress = prefix + std::to_string(std::hash<std::string>{}(path)) + "/progress.bin";
  return Storage.exists(progress.c_str());
}

// Opens the Library index, rebuilding it first when forced, dirty, missing, or
// built with the other metadata setting. `rebuilt` reports whether it did.
bool openFreshIndex(library::LibraryIndexFile& index, const bool forceRebuild, ReadingList::RebuildNotice notice,
                    const void* noticeCtx, bool& rebuilt) {
  rebuilt = false;
  const bool readMetadata = SETTINGS.libraryUseMetadata != 0;
  bool rebuild = forceRebuild || library::isLibraryIndexDirty();
  if (!rebuild && (!index.open(library::libraryIndexPath()) || index.header().metadataEnabled != readMetadata)) {
    rebuild = true;
  }
  if (!rebuild) return true;
  index.close();
  rebuilt = true;
  if (notice) notice(noticeCtx);
  library::BuildStats stats;
  if (!library::buildLibraryIndex("/", stats, readMetadata) || !index.open(library::libraryIndexPath())) {
    LOG_ERR("ReadingList", "library index unavailable");
    return false;
  }
  return true;
}
}  // namespace

bool ReadingList::load(const std::string& listPath, const RebuildNotice notice, const void* noticeCtx) {
  items.clear();

  std::vector<std::string> lines;
  {
    std::string content;
    if (!Storage.readFileToString("ReadingList", listPath, library::READING_LIST_MAX_BYTES, content)) {
      // An empty file is a valid, empty list (readFileToString refuses size 0).
      HalFile file;
      if (Storage.openFileForRead("ReadingList", listPath, file) && file.fileSize() == 0) return true;
      LOG_ERR("ReadingList", "cannot read %s", listPath.c_str());
      return false;
    }
    library::splitReadingListLines(content, lines);
  }  // the raw file is released before the entries are built

  const std::string folder = FsHelpers::extractFolderPath(listPath);
  items.reserve(lines.size());
  bool hasTitles = false;
  for (auto& line : lines) {
    Entry entry;
    if (isBookPath(line)) {
      std::string path = library::resolveReadingListPath(folder, line);
      if (Storage.exists(path.c_str())) entry.path = std::move(path);
      entry.label = std::string(library::readingListFileStem(line));
    } else {
      entry.label = std::move(line);
      entry.byTitle = true;
      hasTitles = true;
    }
    items.push_back(std::move(entry));
  }
  if (hasTitles) resolveTitles(notice, noticeCtx);
  return true;
}

void ReadingList::resolveTitles(const RebuildNotice notice, const void* noticeCtx) {
  // Two passes at most: an index that points at moved or deleted files is
  // rebuilt once and the titles are matched again.
  for (int attempt = 0; attempt < 2; ++attempt) {
    library::LibraryIndexFile index;
    bool rebuilt = false;
    if (!openFreshIndex(index, attempt > 0, notice, noticeCtx, rebuilt)) return;
    const bool stale = matchTitles(index);
    index.close();
    if (!stale || rebuilt) return;
    LOG_INF("ReadingList", "library index is stale; rebuilding");
    for (auto& entry : items) {
      if (entry.byTitle) entry.path.clear();
    }
  }
}

bool ReadingList::matchTitles(library::LibraryIndexFile& index) {
  // Fold each wanted title once; the index pass below compares folded text.
  std::vector<std::string> wanted(items.size());
  size_t unresolved = 0;
  for (size_t i = 0; i < items.size(); ++i) {
    if (!items[i].byTitle || !items[i].path.empty()) continue;
    wanted[i] = library::fold(items[i].label);
    if (!wanted[i].empty()) ++unresolved;
  }

  bool stale = false;
  std::string name;
  std::string bookTitle;
  std::string path;
  const uint16_t count = index.bookCount();
  for (uint16_t ordinal = 0; ordinal < count && unresolved > 0; ++ordinal) {
    library::ClixRecord record;
    if (!index.readRecord(ordinal, record) || !index.readName(record, name) || !index.readTitle(record, bookTitle)) {
      continue;
    }
    const std::string foldedTitle = library::fold(bookTitle);
    const std::string foldedStem = library::fold(library::readingListFileStem(name));
    bool pathChecked = false;
    bool pathExists = false;
    for (size_t i = 0; i < items.size(); ++i) {
      if (wanted[i].empty() || !library::readingListTitleMatches(wanted[i], foldedTitle, foldedStem)) continue;
      if (!pathChecked) {
        pathChecked = true;
        pathExists = index.readPath(record, path) && Storage.exists(path.c_str());
        if (!pathExists) stale = true;
      }
      if (!pathExists) continue;
      items[i].path = path;
      wanted[i].clear();
      --unresolved;
    }
  }
  return stale;
}

void ReadingList::loadProgress() {
  for (auto& entry : items) {
    entry.progress = -1;
    if (entry.path.empty() || !hasSavedProgress(entry.path)) continue;
    const int percent = loadBookProgress(entry.path);
    if (percent >= 0) entry.progress = static_cast<int8_t>(std::min(percent, 100));
  }
}

size_t ReadingList::foundCount() const {
  return static_cast<size_t>(
      std::count_if(items.begin(), items.end(), [](const Entry& entry) { return !entry.path.empty(); }));
}

size_t ReadingList::finishedCount() const {
  return static_cast<size_t>(
      std::count_if(items.begin(), items.end(), [](const Entry& entry) { return entry.progress >= 100; }));
}

void ReadingList::clear() {
  items.clear();
  items.shrink_to_fit();
}

void ReadingList::listFiles(std::vector<std::string>& outPaths) {
  outPaths.clear();
  auto dir = Storage.open(library::READING_LISTS_FOLDER);
  if (!dir || !dir.isDirectory()) return;
  char name[256];
  for (auto file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (file.isDirectory()) continue;
    file.getName(name, sizeof(name));
    if (name[0] == '.' || !FsHelpers::checkFileExtension(std::string_view(name), library::READING_LIST_EXTENSION)) {
      continue;
    }
    outPaths.emplace_back(name);
  }
  dir.close();
  FsHelpers::sortFileList(outPaths);
  const std::string folder = std::string(library::READING_LISTS_FOLDER) + "/";
  std::transform(outPaths.begin(), outPaths.end(), outPaths.begin(),
                 [&folder](const std::string& name) { return folder + name; });
}

bool ReadingList::create(const std::string& name, std::string& outPath) {
  const std::string stem = library::sanitizeReadingListName(name);
  if (stem.empty()) return false;
  if (!Storage.ensureDirectoryExists(library::READING_LISTS_FOLDER)) return false;
  outPath = std::string(library::READING_LISTS_FOLDER) + "/" + stem + library::READING_LIST_EXTENSION;
  if (Storage.exists(outPath.c_str())) return false;
  HalFile file;
  if (!Storage.openFileForWrite("ReadingList", outPath, file)) return false;
  return true;  // an empty file is an empty list
}

ReadingList::AddResult ReadingList::addTitle(const std::string& listPath, const std::string& title) {
  if (title.empty()) return AddResult::Failed;
  std::string content;
  if (!Storage.readFileToString("ReadingList", listPath, library::READING_LIST_MAX_BYTES, content)) {
    HalFile file;
    const bool empty = Storage.openFileForRead("ReadingList", listPath, file) && file.fileSize() == 0;
    if (!empty) return AddResult::Failed;
    content.clear();
  }
  if (library::readingListHasTitle(content, title)) return AddResult::AlreadyThere;
  const std::string updated = library::appendReadingListLine(content, title);
  if (updated.size() > library::READING_LIST_MAX_BYTES) return AddResult::Failed;

  // Write beside the list, then swap it in, so a power cut cannot truncate it.
  const std::string tmpPath = listPath + ".tmp";
  {
    HalFile file;
    if (!Storage.openFileForWrite("ReadingList", tmpPath, file)) return AddResult::Failed;
    if (file.write(reinterpret_cast<const uint8_t*>(updated.data()), updated.size()) != updated.size()) {
      return AddResult::Failed;
    }
    file.flush();
  }
  return Storage.replaceFile(tmpPath.c_str(), listPath.c_str()) ? AddResult::Added : AddResult::Failed;
}

std::string ReadingList::titleForBook(const std::string& bookPath) {
  const auto& recents = RECENT_BOOKS.getBooks();
  const auto recent = std::find_if(recents.begin(), recents.end(), [&bookPath](const RecentBook& book) {
    return book.path == bookPath && !book.title.empty();
  });
  if (recent != recents.end()) return recent->title;

  library::LibraryIndexFile index;
  if (!library::isLibraryIndexDirty() && index.open(library::libraryIndexPath())) {
    std::string path;
    std::string title;
    library::ClixRecord record;
    for (uint16_t ordinal = 0; ordinal < index.bookCount(); ++ordinal) {
      if (!index.readRecord(ordinal, record) || !index.readPath(record, path) || path != bookPath) continue;
      if (index.readTitle(record, title) && !title.empty()) return title;
      break;
    }
  }
  return std::string(library::readingListFileStem(bookPath));
}
