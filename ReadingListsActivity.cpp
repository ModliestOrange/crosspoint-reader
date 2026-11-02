#include "ReadingListsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <ReadingListText.h>

#include <algorithm>
#include <cstdio>

#include "ReadingListActivity.h"
#include "activities/util/KeyboardEntryActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"
#include "util/ReadingList.h"

namespace fui = freeink::ui;

namespace {
constexpr size_t MAX_LIST_NAME = 60;
}

ReadingListsActivity::ReadingListsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const Mode mode)
    : UiListActivity("ReadingLists", renderer, mappedInput), mode(mode) {}

const char* ReadingListsActivity::headerTitle() const {
  return mode == Mode::Pick ? tr(STR_ADD_TO_READING_LIST) : tr(STR_READING_LISTS);
}

void ReadingListsActivity::showRebuildPopup(const void* ctx) {
  const auto* self = static_cast<const ReadingListsActivity*>(ctx);
  GUI.drawPopup(self->renderer, tr(STR_LIBRARY_REBUILDING));
}

void ReadingListsActivity::onEnter() {
  UiListActivity::onEnter();
  loadRows();
  requestUpdate(true);
}

void ReadingListsActivity::onExit() {
  UiListActivity::onExit();
  rows.clear();
  rows.shrink_to_fit();
}

void ReadingListsActivity::loadRows() {
  std::vector<std::string> paths;
  ReadingList::listFiles(paths);

  std::vector<ListRow> loaded;
  loaded.reserve(paths.size());
  // Counting read books matches every list against the card, which takes a
  // moment; the picker only needs names.
  if (mode == Mode::Browse && !paths.empty()) GUI.drawPopup(renderer, tr(STR_LOADING_POPUP));
  for (auto& path : paths) {
    ListRow row;
    row.name = std::string(library::readingListFileStem(path));
    if (mode == Mode::Browse) {
      ReadingList list;
      if (list.load(path, &ReadingListsActivity::showRebuildPopup, this)) {
        list.loadProgress();
        row.total = static_cast<uint16_t>(list.entries().size());
        row.finished = static_cast<uint16_t>(list.finishedCount());
      }
    }
    row.path = std::move(path);
    loaded.push_back(std::move(row));
  }

  RenderLock lock(*this);
  rows = std::move(loaded);
  if (nav.selected >= listCount()) nav.selected = listCount() - 1;
}

void ReadingListsActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingListsActivity*>(ctx);
  item.actionValue = static_cast<int16_t>(index);
  if (index >= self->rows.size()) {
    item.label = tr(STR_READING_LIST_NEW);
    item.icon = listIconFor(UIIcon::Bookmark);
    return;
  }
  const ListRow& row = self->rows[index];
  item.label = row.name.c_str();
  item.icon = listIconFor(UIIcon::Bookmark);
  if (self->mode == Mode::Browse) {
    snprintf(self->rowValueBuf, sizeof(self->rowValueBuf), "%u/%u", static_cast<unsigned>(row.finished),
             static_cast<unsigned>(row.total));
    item.value = self->rowValueBuf;
  }
}

void ReadingListsActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  if (index >= static_cast<int>(rows.size())) {
    startNewList();
    return;
  }
  const std::string path = rows[index].path;
  if (mode == Mode::Pick) {
    selectPath(path);
  } else {
    openList(path);
  }
}

void ReadingListsActivity::openList(const std::string& path) {
  auto list = makeUniqueNoThrow<ReadingListActivity>(renderer, mappedInput, path);
  if (!list) {
    LOG_ERR("ReadingLists", "OOM: reading list");
    return;
  }
  // Coming back refreshes the read counts.
  startActivityForResult(std::move(list), [this](const ActivityResult&) { loadRows(); });
}

void ReadingListsActivity::startNewList() {
  auto keyboard = makeUniqueNoThrow<KeyboardEntryActivity>(renderer, mappedInput, tr(STR_READING_LIST_NAME), "",
                                                           MAX_LIST_NAME, InputType::Text);
  if (!keyboard) {
    LOG_ERR("ReadingLists", "OOM: list name keyboard");
    return;
  }
  startActivityForResult(std::move(keyboard), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    std::string path;
    if (!ReadingList::create(std::get<KeyboardResult>(result.data).text, path)) {
      LOG_ERR("ReadingLists", "cannot create list");
      GUI.drawPopup(renderer, tr(STR_READING_LIST_CREATE_FAILED));
      delay(1000);
      return;
    }
    if (mode == Mode::Pick) {
      selectPath(path);
      return;
    }
    loadRows();
    const auto created =
        std::find_if(rows.begin(), rows.end(), [&path](const ListRow& row) { return row.path == path; });
    if (created != rows.end()) moveSelectionTo(static_cast<int>(created - rows.begin()));
  });
}

void ReadingListsActivity::selectPath(const std::string& path) {
  ActivityResult res{FilePathResult{path}};
  res.isCancelled = false;
  setResult(std::move(res));
  finish();
}

void ReadingListsActivity::onBackButton() {
  if (mode == Mode::Pick) {
    ActivityResult res;
    res.isCancelled = true;
    setResult(std::move(res));
    finish();
    return;
  }
  onGoHome(HomeMenuItem::READING_LISTS);
}

void ReadingListsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  fui::ListProps props;
  props.rowProvider = &ReadingListsActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(listCount());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  fui::TextStyle label = screen.theme().smallText;
  label.maxLines = 2;
  props.labelText = label;
  syncListViewport(screen, props);
  screen.list(props);
}
