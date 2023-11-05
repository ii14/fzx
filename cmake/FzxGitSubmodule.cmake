option(FZX_UPDATE_SUBMODULES "Update required git submodules" ON)

find_package(Git)

function(fzx_git_submodule path)
  if(NOT FZX_UPDATE_SUBMODULES)
    return()
  endif()

  message(STATUS "fzx: Update git submodule: ${path}")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" submodule update --init --recursive "${PROJECT_SOURCE_DIR}/${path}"
    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
    RESULT_VARIABLE GIT_SUBMODULE_RESULT)

  if(NOT GIT_SUBMODULE_RESULT EQUAL "0")
    message(FATAL_ERROR "fzx: git submodule update --init --recursive ${PROJECT_SOURCE_DIR}/${path} failed")
  endif()
endfunction()
