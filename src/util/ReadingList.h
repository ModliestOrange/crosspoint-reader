#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace library {
class LibraryIndexFile;
}

// One `.readinglist` file loaded and matched against the card, shared by the
// list screen, the Reading Lists screen and "Add to reading list".
// lib/LibraryIndex/ReadingListText.h documents the file format.
class ReadingList {
 public:
  struct Entry {
    std::string label;     // the line as written, or a path line's file stem
    std::string path;      // resolved book path; empty when not on the card
    bool byTitle = false;  // matched through the Library index, not a path line
    int8_t progress = -1;  // saved reading percentage; -1 when never opened
  };

  // Called once before a blocking Library index rebuild, so the caller can
  // show a popup.
  using RebuildNotice = void (*)(const void* ctx);

  // Read the file and resolve every entry. False when the file cannot be read.
  bool load(const std::string& listPath, RebuildNotice notice = nullptr, const void* noticeCtx = nullptr);
  // Fill `progress` for books that have saved progress. Unread books cost one
  // file-exists check each.
  void loadProgress();

  const std::vector<Entry>& entries() const { return items; }
  size_t foundCount() const;
  size_t finishedCount() const;
  void clear();

  // Every `.readinglist` file in library::READING_LISTS_FOLDER, sorted by name.
  static void listFiles(std::vector<std::string>& outPaths);
  // Create an empty list named `name` (sanitized). Fails if one exists.
  static bool create(const std::string& name, std::string& outPath);
  // Append `title` to the list unless it is already there.
  enum class AddResult : uint8_t { Added, AlreadyThere, Failed };
  static AddResult addTitle(const std::string& listPath, const std::string& title);
  // Best title for a book file: Recent books, then the Library index, then the
  // file name.
  static std::string titleForBook(const std::string& bookPath);

 private:
  void resolveTitles(RebuildNotice notice, const void* noticeCtx);
  // One pass over the index. Returns true when a matched book is no longer on
  // the card, which means the index is out of date.
  bool matchTitles(library::LibraryIndexFile& index);

  std::vector<Entry> items;
};
