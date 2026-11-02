add_executable(ReadingListTextTest
  ReadingListTextTest.cpp
  ${REPO_ROOT}/lib/LibraryIndex/ReadingListText.cpp
  ${REPO_ROOT}/lib/LibraryIndex/LibraryText.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(ReadingListTextTest PRIVATE
  ${REPO_ROOT}/lib/LibraryIndex
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(ReadingListTextTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ReadingListTextTest)
