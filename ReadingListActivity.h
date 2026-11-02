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
  int listCount() const override { return static_cast<int>(list.entries().size()); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  const char* headerTitle() const override { return title.c_str(); }

  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);
  static void showRebuildPopup(const void* ctx);

  std::string listPath;
  std::string title;
  ReadingList list;
  bool loadFailed = false;

  // Scratch for the row being laid out; the provider contract only needs it
  // valid until the next provideRow() call.
  char rowLabelBuf[320]{};
  char rowValueBuf[8]{};
};
