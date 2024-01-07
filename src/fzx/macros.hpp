// Licensed under LGPLv3 - see LICENSE file for details.

#pragma once

#define UNUSED(x) (::fzx::unused(x))

#if defined(__GNUC__) || defined(__clang__)
# define EXPORT __attribute__((visibility("default")))
#elif defined(_MSC_VER)
# define EXPORT __declspec(dllexport)
#else
# define EXPORT
#endif

#if defined(__GNUC__) || defined(__clang__)
# define INLINE __attribute__((always_inline)) inline
# define NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
# define INLINE __forceinline
# define NOINLINE __declspec(noinline)
#else
# define INLINE inline
# define NOINLINE
#endif

#if defined(_MSC_VER)
# define RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
# define RESTRICT __restrict__
#else
# define RESTRICT
#endif

#if defined(__GNUC__) || defined(__clang__)
# define LIKELY(x) __builtin_expect(static_cast<bool>(x), 1)
# define UNLIKELY(x) __builtin_expect(static_cast<bool>(x), 0)
#else
# define LIKELY(x) static_cast<bool>(x)
# define UNLIKELY(x) static_cast<bool>(x)
#endif

#define ASSERT(x) (LIKELY(x) ? void(0) : ::fzx::detail::assertFail(#x, __FILE__, __LINE__))
#if !defined(FZX_OPTIMIZE)
# define DEBUG_ASSERT(x) ASSERT(x)
#else
# define DEBUG_ASSERT(x)
#endif

#if defined(FZX_OPTIMIZE)
# if defined(__clang__)
#  define ASSUME(x) __builtin_assume(x)
# elif defined(__GNUC__)
#  define ASSUME(x) (static_cast<bool>(x) ? void(0) : __builtin_unreachable())
# elif defined(_MSC_VER)
#  define ASSUME(x) __assume(x)
# else
#  define ASSUME(x) UNUSED(x)
# endif
#else
# define ASSUME(x) (LIKELY(x) ? void(0) : ::fzx::detail::assumeFail(#x, __FILE__, __LINE__))
#endif

#if defined(FZX_OPTIMIZE)
# if defined(__GNUC__) || defined(__clang__)
#  define UNREACHABLE() __builtin_unreachable()
# elif defined(_MSC_VER)
#  define UNREACHABLE() __assume(false)
# else
#  define UNREACHABLE()
# endif
#else
# define UNREACHABLE() ::fzx::detail::unreachableFail(__FILE__, __LINE__)
#endif

#if defined(__GNUC__) || defined(__clang__)
# define ASSUME_ALIGNED(ptr, align) __builtin_assume_aligned(ptr, align)
#else
# define ASSUME_ALIGNED(ptr, align) ptr
#endif

#if defined(__GNUC__) || defined(__clang__)
# define PREFETCH(...) __builtin_prefetch(__VA_ARGS__)
#else
# define PREFETCH(...)
#endif

#if defined(__clang__)
# define FZX_PRAGMA_CLANG(x) _Pragma(x)
# define FZX_PRAGMA_GCC(x)
# define FZX_PRAGMA_MSVC(x)
#elif defined(__GNUC__)
# define FZX_PRAGMA_CLANG(x)
# define FZX_PRAGMA_GCC(x) _Pragma(x)
# define FZX_PRAGMA_MSVC(x)
#elif defined(_MSC_VER)
# define FZX_PRAGMA_CLANG(x)
# define FZX_PRAGMA_GCC(x)
# define FZX_PRAGMA_MSVC(x) _Pragma(x)
#else
# define FZX_PRAGMA_CLANG(x)
# define FZX_PRAGMA_GCC(x)
# define FZX_PRAGMA_MSVC(x)
#endif

namespace fzx {

template <typename T>
constexpr void unused([[maybe_unused]] const T& x) noexcept
{
}

namespace detail {

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] NOINLINE void assertFail(const char* expr, const char* file, unsigned long line);
// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] NOINLINE void assumeFail(const char* expr, const char* file, unsigned long line);
// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] NOINLINE void unreachableFail(const char* file, unsigned long line);

} // namespace detail

} // namespace fzx
