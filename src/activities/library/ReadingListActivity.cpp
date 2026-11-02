#include "ReadingListActivity.h"

#include <FsHelpers.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>
#include <ReadingListText.h>

#include <algorithm>
#include <cstdio>

#include "CrossPointState.h"
#include "activities/home/FileBrowserActivity.h"
#include "components/UITheme.h"
#include "components/UiAppHelpers.h"

namespace fui = freeink::ui;

std::string ReadingListActivity::lastPickFolder;

ReadingListActivity::ReadingListActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string listPath)
    : UiListActivity("ReadingList", renderer, mappedInput), listPath(std::move(listPath)) {}

void ReadingListActivity::showRebuildPopup(const void* ctx) {
  const auto* self = static_cast<const ReadingListActivity*>(ctx);
  GUI.drawPopup(self->renderer, tr(STR_LIBRARY_REBUILDING));
}

void ReadingListActivity::onEnter() {
  UiListActivity::onEnter();
  title = std::string(library::readingListFileStem(listPath));
  reload();

  // Reopen on the book read last, so the list doubles as a "what's next".
  const auto& entries = list.entries();
  const auto lastRead = std::find_if(entries.begin(), entries.end(), [](const ReadingList::Entry& entry) {
    return !entry.path.empty() && entry.path == APP_STATE.openEpubPath;
  });
  if (lastRead != entries.end()) nav.reset(static_cast<int>(lastRead - entries.begin()));
  requestUpdate(true);
}

void ReadingListActivity::reload() {
  ReadingList fresh;
  const bool ok = fresh.load(listPath, &ReadingListActivity::showRebuildPopup, this);
  fresh.loadProgress();
  RenderLock lock(*this);
  list = std::move(fresh);
  loadFailed = !ok;
  if (nav.selected >= listCount()) nav.selected = std::max(0, listCount() - 1);
}

void ReadingListActivity::onExit() {
  UiListActivity::onExit();
  list.clear();
}

void ReadingListActivity::provideRow(void* ctx, const uint16_t index, fui::ListItem& item) {
  auto* self = static_cast<ReadingListActivity*>(ctx);
  const auto& entries = self->list.entries();
  item.actionValue = static_cast<int16_t>(index);
  if (index >= entries.size()) {
    item.label = tr(STR_READING_LIST_ADD_BOOKS);
    item.icon = listIconFor(UIIcon::Book);
    return;
  }
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
}

void ReadingListActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  const auto& entries = list.entries();
  if (index >= static_cast<int>(entries.size())) {
    pickBookToAdd();
    return;
  }
  const std::string& path = entries[index].path;
  if (path.empty()) return;
  onSelectBook(path);
}

void ReadingListActivity::pickBookToAdd() {
  // Start where the last pick happened, so adding a series is a few presses.
  if (lastPickFolder.empty() || !Storage.exists(lastPickFolder.c_str())) {
    lastPickFolder = Storage.exists("/Books") ? "/Books" : "/";
  }
  auto picker = makeUniqueNoThrow<FileBrowserActivity>(renderer, mappedInput, lastPickFolder,
                                                       FileBrowserActivity::Mode::PickBook);
  if (!picker) {
    LOG_ERR("ReadingList", "OOM: book picker");
    return;
  }
  startActivityForResult(std::move(picker), [this](const ActivityResult& result) {
    if (result.isCancelled) return;
    if (const auto* picked = std::get_if<FilePathResult>(&result.data)) addBook(picked->path);
  });
}

void ReadingListActivity::addBook(const std::string& bookPath) {
  lastPickFolder = FsHelpers::extractFolderPath(bookPath);
  const auto added = ReadingList::addTitle(listPath, ReadingList::titleForBook(bookPath));
  if (added != ReadingList::AddResult::Added) {
    GUI.drawPopup(renderer, added == ReadingList::AddResult::AlreadyThere ? tr(STR_READING_LIST_ALREADY_ADDED)
                                                                          : tr(STR_READING_LIST_ADD_FAILED));
    delay(1000);
    return;
  }
  reload();
  moveSelectionTo(listCount() - 1);  // stay on "Add books" to add the next one
}

void ReadingListActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  screen.setContentMarginFromScreen(fui::Insets{static_cast<int16_t>(metrics.topPadding + metrics.headerHeight), 0,
                                                static_cast<int16_t>(metrics.buttonHintsHeight), 0});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));

  if (loadFailed) {
    screen.centeredText(tr(STR_READING_LIST_UNREADABLE), screen.theme().bodyText);
    return;
  }

  fui::ListProps props;
  props.rowProvider = &ReadingListActivity::provideRow;
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
