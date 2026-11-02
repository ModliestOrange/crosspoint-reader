#include "ReadingListActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <ReadingListText.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointState.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

ReadingListActivity::ReadingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string listPath)
    : UiListActivity("ReadingList", renderer, mappedInput), listPath(std::move(listPath)) {}

void ReadingListActivity::showRebuildPopup(const void* ctx) {
  const auto* self = static_cast<const ReadingListActivity*>(ctx);
  GUI.drawPopup(self->renderer, tr(STR_LIBRARY_REBUILDING));
}

void ReadingListActivity::onEnter() {
  UiListActivity::onEnter();
  title = std::string(library::readingListFileStem(listPath));
  loadFailed = !list.load(listPath, &ReadingListActivity::showRebuildPopup, this);
  list.loadProgress();

  // Reopen on the book read last, so the list doubles as a "what's next".
  const auto& entries = list.entries();
  const auto lastRead = std::find_if(entries.begin(), entries.end(), [](const ReadingList::Entry& entry) {
    return !entry.path.empty() && entry.path == APP_STATE.openEpubPath;
  });
  if (lastRead != entries.end()) nav.reset(static_cast<int>(lastRead - entries.begin()));
  requestUpdate(true);
}

void ReadingListActivity::onExit() {
  UiListActivity::onExit();
  list.clear();
}

void ReadingListActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingListActivity*>(ctx);
  const auto& entries = self->list.entries();
  if (index >= entries.size()) return;
  const ReadingList::Entry& entry = entries[index];
  snprintf(self->rowLabelBuf, sizeof(self->rowLabelBuf), "%u. %s", static_cast<unsigned>(index + 1),
           entry.label.c_str());
  item.label = self->rowLabelBuf;
  if (entry.path.empty()) {
    item.value = tr(STR_READING_LIST_NOT_FOUND);
  } else if (entry.progress >= 100) {
    item.value = tr(STR_READING_LIST_FINISHED);
  } else if (entry.progress > 0) {
    snprintf(self->rowValueBuf, sizeof(self->rowValueBuf), "%d%%", entry.progress);
    item.value = self->rowValueBuf;
  }
  item.actionValue = static_cast<int16_t>(index);
}

void ReadingListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  const std::string& path = list.entries()[index].path;
  if (path.empty()) return;
  app.clearTapFlash();
  onSelectBook(path);
}

void ReadingListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (list.entries().empty()) {
    screen.centeredText(loadFailed ? tr(STR_READING_LIST_UNREADABLE) : tr(STR_NO_FILES_FOUND), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.rowProvider = &ReadingListActivity::provideRow;
  props.rowProviderCtx = this;
  props.count = static_cast<uint16_t>(list.entries().size());
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;  // physical buttons stay in loop()
  props.valueInset = 8;
  fui::TextStyle label = screen.theme().smallText;
  label.maxLines = 2;
  props.labelText = label;
  syncListViewport(screen, props);
  screen.list(props);
}
