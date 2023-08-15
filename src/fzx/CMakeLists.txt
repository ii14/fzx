file(GLOB_RECURSE LIB_FILES CONFIGURE_DEPENDS RELATIVE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/*.hpp")
file(GLOB_RECURSE TERM_FILES CONFIGURE_DEPENDS RELATIVE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/term/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/term/*.hpp")
file(GLOB_RECURSE LUA_FILES CONFIGURE_DEPENDS RELATIVE
  "${CMAKE_CURRENT_LIST_DIR}"
  "${CMAKE_CURRENT_LIST_DIR}/lua/*.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/lua/*.hpp")
list(REMOVE_ITEM LIB_FILES ${TERM_FILES})
list(REMOVE_ITEM LIB_FILES ${LUA_FILES})

add_library(libfzx STATIC ${LIB_FILES})
set_property(TARGET libfzx PROPERTY OUTPUT_NAME fzx)
set_property(TARGET libfzx PROPERTY POSITION_INDEPENDENT_CODE ON)
target_include_directories(libfzx PUBLIC ..)
target_link_libraries(libfzx PRIVATE fzxopts PUBLIC Threads::Threads)

if(FZX_BUILD_EXECUTABLE)
  add_executable(fzx ${TERM_FILES})
  target_link_libraries(fzx PRIVATE fzxopts libfzx fmt::fmt)
endif()

if(FZX_BUILD_LUA_MODULE)
  add_library(fzxlua MODULE ${LUA_FILES})
  set_property(TARGET fzxlua PROPERTY OUTPUT_NAME fzxlib)
  set_property(TARGET fzxlua PROPERTY PREFIX "")
  target_link_libraries(fzxlua PRIVATE fzxopts libfzx)
  target_include_directories(fzxlua PRIVATE src "${LUA_INCLUDE_DIR}")
  if(APPLE)
    target_link_options(fzxlua PRIVATE -undefined dynamic_lookup)
  endif()
endif()
