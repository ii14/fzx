add_executable(bm_fzy fzy.cpp)
target_link_libraries(bm_fzy PRIVATE fzxopts libfzx benchmark::benchmark)
