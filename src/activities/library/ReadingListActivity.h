#pragma once

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

namespace library {
class LibraryIndexFile;
}

// Opens a `.readinglist` file from the file browser and shows its books in the
// file's order, whatever folder or series each one is in. Path entries open
// that file; title entries are matched against the Library index once, on
// entry (lib/LibraryIndex/ReadingListText.h has the format).
class ReadingListActivity final : public UiListActivity {
 public:
  ReadingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string listPath);

  void onEnter() override;
  void onExit() override;

 private:
  struct Entry {
    std::string label;     // the line as written in the list
    std::string path;      // resolved book path; empty when not on the card
    bool byTitle = false;  // matched through the Library index, not a path line
  };

  int listCount() const override { return static_cast<int>(entries.size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title.c_str(); }

  void loadEntries();
  // Fill in paths for title entries with one pass over the Library index.
  void resolveTitles();
  // One pass over the index. Returns true when a matched book is no longer on
  // the card, which means the index is out of date.
  bool matchTitles(library::LibraryIndexFile& index);

  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);

  std::string listPath;
  std::string title;
  std::vector<Entry> entries;
  bool loadFailed = false;

  // Scratch for the row being laid out; the provider contract only needs it
  // valid until the next provideRow() call.
  char rowLabelBuf[320]{};
};
