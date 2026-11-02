#include <gtest/gtest.h>

#include "LibraryText.h"
#include "ReadingListText.h"

using library::fold;
using library::readingListFileStem;
using library::readingListTitleMatches;
using library::resolveReadingListPath;
using library::splitReadingListLines;

TEST(ReadingListText, SplitsTrimmedLinesAndSkipsCommentsAndBlanks) {
  std::vector<std::string> lines;
  splitReadingListLines("\xEF\xBB\xBF# Warcraft\r\n\r\n  Rise of the Horde  \r\n\tThe Last Guardian\n#x\nLast", lines);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(lines[0], "Rise of the Horde");
  EXPECT_EQ(lines[1], "The Last Guardian");
  EXPECT_EQ(lines[2], "Last");
}

TEST(ReadingListText, StopsAtEntryCap) {
  std::vector<std::string> lines;
  splitReadingListLines("a\nb\nc\nd\n", lines, 2);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[1], "b");
}

TEST(ReadingListText, EmptyContentGivesNoLines) {
  std::vector<std::string> lines{"stale"};
  splitReadingListLines("", lines);
  EXPECT_TRUE(lines.empty());
}

TEST(ReadingListText, ResolvesRelativeAbsoluteAndWindowsPaths) {
  EXPECT_EQ(resolveReadingListPath("/", "a.epub"), "/a.epub");
  EXPECT_EQ(resolveReadingListPath("/Books", "Warcraft\\b.epub"), "/Books/Warcraft/b.epub");
  EXPECT_EQ(resolveReadingListPath("/Books/", "./c.epub"), "/Books/c.epub");
  EXPECT_EQ(resolveReadingListPath("/Books", "/Other/d.epub"), "/Other/d.epub");
  EXPECT_EQ(resolveReadingListPath("", "e.epub"), "/e.epub");
}

TEST(ReadingListText, MatchesTitleExactlyAfterFolding) {
  EXPECT_TRUE(readingListTitleMatches(fold("Vol'jin: Shadows of the Horde"), fold("Vol'jin - Shadows of the Horde"),
                                      fold("whatever")));
  EXPECT_TRUE(readingListTitleMatches(fold("arthas rise of the lich king"), fold("Arthas: Rise of the Lich King"), ""));
}

TEST(ReadingListText, MatchesFileStemSuffixOnWordBreak) {
  const std::string stem = fold("Warcraft Second War 01 - Rise of the Horde");
  EXPECT_TRUE(readingListTitleMatches(fold("Rise of the Horde"), fold("Warcraft Second War 01"), stem));
  EXPECT_FALSE(readingListTitleMatches(fold("of the Hord"), "", stem));
  EXPECT_FALSE(readingListTitleMatches(fold("ise of the Horde"), "", stem));
  EXPECT_FALSE(
      readingListTitleMatches(fold("Night of the Dragon"), "", fold("Warcraft Archive 01 - Day of the Dragon")));
}

TEST(ReadingListText, EmptyEntryNeverMatches) { EXPECT_FALSE(readingListTitleMatches("", "", "")); }

TEST(ReadingListText, FileStemDropsFolderAndExtension) {
  EXPECT_EQ(readingListFileStem("/Books/War Crimes.epub"), "War Crimes");
  EXPECT_EQ(readingListFileStem("Plain"), "Plain");
  EXPECT_EQ(readingListFileStem("/a.b/.hidden"), ".hidden");
}

TEST(ReadingListText, SanitizesListNamesForFat) {
  EXPECT_EQ(library::sanitizeReadingListName("  Warcraft: Order  "), "Warcraft Order");
  EXPECT_EQ(library::sanitizeReadingListName("a/b\\c*d?e\"f<g>h|i"), "a b c d e f g h i");
  EXPECT_EQ(library::sanitizeReadingListName("..hidden.."), "hidden");
  EXPECT_EQ(library::sanitizeReadingListName(" ?* "), "");
  EXPECT_EQ(library::sanitizeReadingListName("Mistborn Era 2"), "Mistborn Era 2");
}

TEST(ReadingListText, FindsExistingTitleIgnoringCaseAndPunctuation) {
  const std::string content = "# list\nRise of the Horde\nVol'jin: Shadows of the Horde\n";
  EXPECT_TRUE(library::readingListHasTitle(content, "rise of the horde"));
  EXPECT_TRUE(library::readingListHasTitle(content, "Vol'jin - Shadows of the Horde"));
  EXPECT_FALSE(library::readingListHasTitle(content, "Tides of Darkness"));
  EXPECT_FALSE(library::readingListHasTitle(content, "# list"));
  EXPECT_FALSE(library::readingListHasTitle(content, ""));
}

TEST(ReadingListText, AppendsOnItsOwnLine) {
  EXPECT_EQ(library::appendReadingListLine("", "A"), "A\n");
  EXPECT_EQ(library::appendReadingListLine("A\n", "B"), "A\nB\n");
  EXPECT_EQ(library::appendReadingListLine("A", "B"), "A\nB\n");
}
