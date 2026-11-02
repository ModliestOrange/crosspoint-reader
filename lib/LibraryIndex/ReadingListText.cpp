#include "ReadingListText.h"

namespace library {

namespace {

bool isBlank(const char c) { return c == ' ' || c == '\t' || c == '\r'; }

std::string_view trim(std::string_view text) {
  while (!text.empty() && isBlank(text.front())) text.remove_prefix(1);
  while (!text.empty() && isBlank(text.back())) text.remove_suffix(1);
  return text;
}

}  // namespace

void splitReadingListLines(std::string_view content, std::vector<std::string>& out, const size_t maxEntries) {
  out.clear();
  if (content.substr(0, 3) == "\xEF\xBB\xBF") content.remove_prefix(3);

  while (!content.empty() && out.size() < maxEntries) {
    const size_t newline = content.find('\n');
    const std::string_view line = trim(content.substr(0, newline));
    content.remove_prefix(newline == std::string_view::npos ? content.size() : newline + 1);
    if (line.empty() || line.front() == '#') continue;
    out.emplace_back(line);
  }
}

std::string resolveReadingListPath(const std::string_view listFolder, const std::string_view entry) {
  std::string relative(entry);
  for (char& c : relative) {
    if (c == '\\') c = '/';
  }
  if (!relative.empty() && relative.front() == '/') return relative;
  while (relative.compare(0, 2, "./") == 0) relative.erase(0, 2);

  std::string path(listFolder.empty() ? std::string_view("/") : listFolder);
  if (path.back() != '/') path.push_back('/');
  path += relative;
  return path;
}

bool readingListTitleMatches(const std::string_view foldedEntry, const std::string_view foldedTitle,
                             const std::string_view foldedStem) {
  if (foldedEntry.empty()) return false;
  if (foldedTitle == foldedEntry || foldedStem == foldedEntry) return true;
  if (foldedStem.size() <= foldedEntry.size()) return false;
  // fold() leaves single spaces between words, so a word break is one space.
  const size_t start = foldedStem.size() - foldedEntry.size();
  return foldedStem[start - 1] == ' ' && foldedStem.substr(start) == foldedEntry;
}

std::string_view readingListFileStem(std::string_view path) {
  const size_t slash = path.find_last_of('/');
  if (slash != std::string_view::npos) path.remove_prefix(slash + 1);
  const size_t dot = path.find_last_of('.');
  if (dot != std::string_view::npos && dot > 0) path = path.substr(0, dot);
  return path;
}

}  // namespace library
