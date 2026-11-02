#include "AddToReadingList.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <Memory.h>

#include "ReadingListsActivity.h"
#include "activities/Activity.h"
#include "components/UITheme.h"
#include "util/ReadingList.h"

void startAddToReadingList(Activity& host, GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title) {
  auto picker = makeUniqueNoThrow<ReadingListsActivity>(renderer, mappedInput, ReadingListsActivity::Mode::Pick);
  if (!picker) {
    LOG_ERR("ReadingList", "OOM: list picker");
    return;
  }
  host.startActivityForResult(std::move(picker), [&renderer, title = std::move(title)](const ActivityResult& result) {
    if (result.isCancelled) return;
    const auto* picked = std::get_if<FilePathResult>(&result.data);
    if (!picked) return;
    const auto added = ReadingList::addTitle(picked->path, title);
    const char* message = added == ReadingList::AddResult::Added          ? tr(STR_READING_LIST_ADDED)
                          : added == ReadingList::AddResult::AlreadyThere ? tr(STR_READING_LIST_ALREADY_ADDED)
                                                                          : tr(STR_READING_LIST_ADD_FAILED);
    LOG_INF("ReadingList", "add '%s' to %s: %s", title.c_str(), picked->path.c_str(), message);
    GUI.drawPopup(renderer, message);
    delay(1000);  // the caller repaints right after this handler returns
  });
}
