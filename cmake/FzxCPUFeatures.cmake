include(CheckCXXSourceRuns)
include(CMakePushCheckState)

cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "-msse2")
  check_cxx_source_runs("
    #include <emmintrin.h>
    int main() {
      __m128i r = _mm_add_epi8(
        _mm_setzero_si128(),
        _mm_setzero_si128());
    }
  " FZX_HAS_SSE2)
cmake_pop_check_state()

cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "-msse4.1")
  check_cxx_source_runs("
    #include <smmintrin.h>
    int main() {
      __m128 r = _mm_blendv_ps(
        _mm_setzero_ps(),
        _mm_setzero_ps(),
        _mm_setzero_ps());
    }
  " FZX_HAS_SSE41)
cmake_pop_check_state()

cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "-mavx")
  check_cxx_source_runs("
    #include <immintrin.h>
    int main() {
      __m256 r = _mm256_add_ps(
        _mm256_setzero_ps(),
        _mm256_setzero_ps());
    }
  " FZX_HAS_AVX)
cmake_pop_check_state()

cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "-mavx2")
  check_cxx_source_runs("
    #include <immintrin.h>
    int main() {
      __m256i r = _mm256_add_epi8(
        _mm256_setzero_si256(),
        _mm256_setzero_si256());
    }
  " FZX_HAS_AVX2)
cmake_pop_check_state()
