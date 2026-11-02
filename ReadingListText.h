#pragma once

// Parsing and matching for `.readinglist` files: a plain-text, ordered list of
// books that may sit in different folders and series.
//
// One entry per line. Blank lines and lines starting with '#' are skipped. A
// line naming a book file (an extension the reader opens) is a path, absolute
// or relative to the list's folder; any other line is a title, matched against
// the Library index. Pure functions, host-testable (test/reading_list_text).

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace library {

// File extension the file browser opens as a reading list.
inline constexpr const char* READING_LIST_EXTENSION = ".readinglist";

// Longest list file read, and most entries kept from it.
inline constexpr size_t READING_LIST_MAX_BYTES = 16 * 1024;
inline constexpr size_t READING_LIST_MAX_ENTRIES = 200;

// Trimmed entry lines in file order: UTF-8 BOM, CR, surrounding spaces/tabs,
// blank lines and '#' comments are dropped. Stops after `maxEntries`.
void splitReadingListLines(std::string_view content, std::vector<std::string>& out,
                           size_t maxEntries = READING_LIST_MAX_ENTRIES);

// Absolute card path for a path entry. `listFolder` is the folder holding the
// list ("/" for the root). Backslashes become '/', a leading "./" is dropped,
// and a leading '/' makes the entry absolute.
std::string resolveReadingListPath(std::string_view listFolder, std::string_view entry);

// Whether a book matches a title entry. All three inputs are already folded
// (library::fold). A book matches when its title equals the entry, or its file
// stem equals the entry or ends with it after a word break, so
// "Warcraft Second War 01 - Rise of the Horde.epub" matches "Rise of the Horde".
bool readingListTitleMatches(std::string_view foldedEntry, std::string_view foldedTitle, std::string_view foldedStem);

// File name without folder and extension.
std::string_view readingListFileStem(std::string_view path);

// Folder the Reading Lists screen shows and new lists are created in.
inline constexpr const char* READING_LISTS_FOLDER = "/Reading Lists";

// A list name made safe as a FAT file stem: characters FAT rejects become
// spaces, runs of spaces collapse, and leading/trailing spaces and dots go.
// Empty when nothing usable is left.
std::string sanitizeReadingListName(std::string_view name);

// Whether `content` already holds an entry for this title (compared folded, so
// case and punctuation do not matter).
bool readingListHasTitle(std::string_view content, std::string_view title);

// `content` with `line` appended as its own line, adding the missing newline
// before it when the file does not end in one.
std::string appendReadingListLine(std::string_view content, std::string_view line);

}  // namespace library
