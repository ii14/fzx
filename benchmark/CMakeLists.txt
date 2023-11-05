add_executable(bm_score score.cpp)
target_link_libraries(bm_score PRIVATE fzxopts libfzx benchmark::benchmark)
add_executable(bm_fzx fzx.cpp)
target_link_libraries(bm_fzx PRIVATE fzxopts libfzx)
