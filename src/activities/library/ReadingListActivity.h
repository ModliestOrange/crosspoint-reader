#pragma once

#include <string>

#include "activities/UiListActivity.h"
#include "util/ReadingList.h"

// Opens a `.readinglist` file and shows its books in the file's order,
// whatever folder or series each one is in, with saved reading progress.
// util/ReadingList.h does the loading and matching.
class ReadingListActivity final : public UiListActivity {
 public:
  ReadingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string listPath);

  void onEnter() override;
  void onExit() override;

 private:
  // Books, then the "Add books" row (unless the file could not be read).
  int listCount() const override { return static_cast<int>(list.entries().size()) + (loadFailed ? 0 : 1); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title.c_str(); }

  void reload();
  void pickBookToAdd();
  void addBook(const std::string& bookPath);

  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);
  static void showRebuildPopup(const void* ctx);

  std::string listPath;
  std::string title;
  ReadingList list;
  bool loadFailed = false;

  // Scratch for the row being laid out; the provider contract only needs it
  // valid until the next provideRow() call.
  // Folder the book picker last returned from; shared by every list this boot.
  static std::string lastPickFolder;

  char rowLabelBuf[320]{};
  char rowValueBuf[8]{};
};
