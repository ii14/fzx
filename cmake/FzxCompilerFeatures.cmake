include(CheckCXXSourceCompiles)

# GCC and Clang
check_cxx_source_compiles("
  int main() {
    if (false)
      __builtin_unreachable();
    return 0;
  }
" FZX_HAS_BUILTIN_UNREACHABLE)

# GCC and Clang
check_cxx_source_compiles("
  int main() {
    return __builtin_expect(0, 0);
  }
" FZX_HAS_BUILTIN_EXPECT)

# GCC and Clang
check_cxx_source_compiles("
  int main() {
    __builtin_assume(0 == 0);
    return 0;
  }
" FZX_HAS_BUILTIN_ASSUME)

# MSVC
check_cxx_source_compiles("
  int main() {
    __assume(0 == 0);
    return 0;
  }
" FZX_HAS_ASSUME)

check_cxx_source_compiles("
  int main(int, char** argv) {
    const char* __restrict p = argv[0];
    return p != nullptr;
  }
" FZX_HAS_RESTRICT1)

check_cxx_source_compiles("
  int main(int, char** argv) {
    const char* __restrict__ p = argv[0];
    return p != nullptr;
  }
" FZX_HAS_RESTRICT2)

# GCC and Clang
check_cxx_source_compiles("
  int main(int, char** argv) {
    __builtin_prefetch(argv[0], 0, 0);
    return 0;
  }
" FZX_HAS_BUILTIN_PREFETCH)

# GCC and Clang
check_cxx_source_compiles("
  int main() {
    static_assert(sizeof(int) == 4);
    return __builtin_ffs(0);
  }
" FZX_HAS_BUILTIN_FFS)

# GCC and Clang
check_cxx_source_compiles("
  int main() {
    static_assert(sizeof(long) == 8);
    return __builtin_ffsl(0);
  }
" FZX_HAS_BUILTIN_FFSL)

# MSVC
check_cxx_source_compiles("
  int main() {
    unsigned long index;
    return static_cast<int>(_BitScanForward(&index, 0) ? index + 1 : 0);
  }
" FZX_HAS_BIT_SCAN_FORWARD)

# MSVC
check_cxx_source_compiles("
  int main() {
    unsigned long index;
    return static_cast<int>(_BitScanForward64(&index, 0) ? index + 1 : 0);
  }
" FZX_HAS_BIT_SCAN_FORWARD_64)
