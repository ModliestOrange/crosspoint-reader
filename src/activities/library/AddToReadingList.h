#pragma once

#include <string>

class Activity;
class GfxRenderer;
class MappedInputManager;

// Opens the reading-list picker on top of `host`, then appends `title` to the
// chosen (or newly created) list and briefly confirms the outcome.
void startAddToReadingList(Activity& host, GfxRenderer& renderer, MappedInputManager& mappedInput, std::string title);
