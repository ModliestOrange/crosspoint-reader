#include "ReadingListActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <LibraryBuilder.h>
#include <LibraryIndexFile.h>
#include <LibraryText.h>
#include <Logging.h>
#include <ReadingListText.h>

#include <cstdio>

#include "CrossPointSettings.h"
#include "CrossPointState.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

namespace {
bool isBookPath(const std::string_view entry) {
  return FsHelpers::hasReflowableBookExtension(entry) || FsHelpers::hasXtcExtension(entry);
}
}  // namespace

ReadingListActivity::ReadingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string listPath)
    : UiListActivity("ReadingList", renderer, mappedInput), listPath(std::move(listPath)) {}

void ReadingListActivity::onEnter() {
  UiListActivity::onEnter();
  title = std::string(library::readingListFileStem(listPath));
  loadEntries();

  // Reopen on the book read last, so the list doubles as a "what's next".
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i].path.empty() && entries[i].path == APP_STATE.openEpubPath) {
      nav.reset(static_cast<int>(i));
      break;
    }
  }
  requestUpdate(true);
}

void ReadingListActivity::onExit() {
  UiListActivity::onExit();
  entries.clear();
  entries.shrink_to_fit();
}

void ReadingListActivity::loadEntries() {
  entries.clear();
  loadFailed = false;

  std::vector<std::string> lines;
  {
    std::string content;
    if (!Storage.readFileToString("ReadingList", listPath, library::READING_LIST_MAX_BYTES, content)) {
      LOG_ERR("ReadingList", "cannot read %s", listPath.c_str());
      loadFailed = true;
      return;
    }
    library::splitReadingListLines(content, lines);
  }  // the raw file is released before the entries are built

  const std::string folder = FsHelpers::extractFolderPath(listPath);
  entries.reserve(lines.size());
  bool hasTitles = false;
  for (auto& line : lines) {
    Entry entry;
    if (isBookPath(line)) {
      std::string path = library::resolveReadingListPath(folder, line);
      if (Storage.exists(path.c_str())) entry.path = std::move(path);
      entry.label = std::string(library::readingListFileStem(line));
    } else {
      entry.label = std::move(line);
      hasTitles = true;
    }
    entries.push_back(std::move(entry));
  }
  if (hasTitles) resolveTitles();
}

void ReadingListActivity::resolveTitles() {
  library::LibraryIndexFile index;
  const bool readMetadata = SETTINGS.libraryUseMetadata != 0;
  if (library::isLibraryIndexDirty() || !index.open(library::libraryIndexPath()) ||
      index.header().metadataEnabled != readMetadata) {
    index.close();
    GUI.drawPopup(renderer, tr(STR_LIBRARY_REBUILDING));
    library::BuildStats stats;
    if (!library::buildLibraryIndex("/", stats, readMetadata) || !index.open(library::libraryIndexPath())) {
      LOG_ERR("ReadingList", "library index unavailable");
      return;
    }
  }

  // Fold each wanted title once; the index pass below compares folded text.
  std::vector<std::string> wanted(entries.size());
  size_t unresolved = 0;
  for (size_t i = 0; i < entries.size(); ++i) {
    if (!entries[i].path.empty() || isBookPath(entries[i].label)) continue;
    wanted[i] = library::fold(entries[i].label);
    if (!wanted[i].empty()) ++unresolved;
  }

  std::string name;
  std::string bookTitle;
  const uint16_t count = index.bookCount();
  for (uint16_t ordinal = 0; ordinal < count && unresolved > 0; ++ordinal) {
    library::ClixRecord record;
    if (!index.readRecord(ordinal, record) || !index.readName(record, name) || !index.readTitle(record, bookTitle)) {
      continue;
    }
    const std::string foldedTitle = library::fold(bookTitle);
    const std::string foldedStem = library::fold(library::readingListFileStem(name));
    for (size_t i = 0; i < entries.size(); ++i) {
      if (wanted[i].empty() || !library::readingListTitleMatches(wanted[i], foldedTitle, foldedStem)) continue;
      if (!index.readPath(record, entries[i].path)) continue;
      wanted[i].clear();
      --unresolved;
    }
  }
  index.close();
}

void ReadingListActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingListActivity*>(ctx);
  if (index >= self->entries.size()) return;
  const Entry& entry = self->entries[index];
  snprintf(self->rowLabelBuf, sizeof(self->rowLabelBuf), "%u. %s", static_cast<unsigned>(index + 1),
           entry.label.c_str());
  item.label = self->rowLabelBuf;
  if (entry.path.empty()) item.value = tr(STR_READING_LIST_NOT_FOUND);
  item.actionValue = static_cast<int16_t>(index);
}

void ReadingListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  const std::string& path = entries[index].path;
  if (path.empty()) return;
  app.clearTapFlash();
  onSelectBook(path);
}

void ReadingListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (entries.empty()) {
    screen.centeredText(loadFailed ? tr(STR_READING_LIST_UNREADABLE) : tr(STR_NO_FILES_FOUND), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.rowProvider = &ReadingListActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(entries.size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  fui::TextStyle label = screen.theme().smallText;
  label.maxLines = 2;
  props.labelText = label;
  syncListViewport(screen, props);
  screen.list(props);
}
