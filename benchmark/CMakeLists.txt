add_executable(bm_fzy fzy.cpp)
target_link_libraries(bm_fzy PRIVATE fzxopts libfzx benchmark::benchmark)
add_executable(bm_fzx fzx.cpp)
target_link_libraries(bm_fzx PRIVATE fzxopts libfzx)
