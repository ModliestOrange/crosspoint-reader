#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// Every reading list in library::READING_LISTS_FOLDER, plus a "New list" row.
// Browse mode (from Home) opens a list and shows how much of each is read.
// Pick mode returns the chosen or newly created list as a FilePathResult; it
// backs "Add to reading list" and skips the per-list progress counts.
class ReadingListsActivity final : public UiListActivity {
 public:
  enum class Mode : uint8_t { Browse, Pick };

  ReadingListsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, Mode mode = Mode::Browse);

  void onEnter() override;
  void onExit() override;

 private:
  struct ListRow {
    std::string path;
    std::string name;
    uint16_t total = 0;
    uint16_t finished = 0;
  };

  int listCount() const override { return static_cast<int>(rows.size()) + 1; }  // + "New list"
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  void onBackButton() override;
  const char* headerTitle() const override;

  void loadRows();
  void openList(const std::string& path);
  void startNewList();
  void selectPath(const std::string& path);

  static void provideRow(void* ctx, uint16_t index, freeink::ui::ListItem& item);
  static void showRebuildPopup(const void* ctx);

  const Mode mode;
  std::vector<ListRow> rows;
  char rowValueBuf[24]{};
};
