file(GLOB_RECURSE TEST_FILES CONFIGURE_DEPENDS RELATIVE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/*.hpp")
add_executable(test ${TEST_FILES})
target_link_libraries(test PRIVATE libfzx Catch2::Catch2)
