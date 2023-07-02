#pragma once

#include <utility>

#define UNUSED(x) (::fzx::unused(x))

#if defined(__has_attribute)
# define FZX_HAS_ATTRIBUTE(x) __has_attribute(x)
#else
# define FZX_HAS_ATTRIBUTE(x) 0
#endif

#if (defined(__GNUC__) || defined(__clang__)) && FZX_HAS_ATTRIBUTE(always_inline)
# define ALWAYS_INLINE [[gnu::always_inline]]
#else
# define ALWAYS_INLINE
#endif

#if (defined(__GNUC__) || defined(__clang__)) && FZX_HAS_ATTRIBUTE(noinline)
# define NOINLINE [[gnu::noinline]]
#else
# define NOINLINE
#endif

#if defined(__has_builtin)
# define FZX_HAS_BUILTIN(x) __has_builtin(x)
#else
# define FZX_HAS_BUILTIN(x) 0
#endif

#if FZX_HAS_BUILTIN(__builtin_expect)
# define LIKELY(x) __builtin_expect(static_cast<bool>(x), 1)
# define UNLIKELY(x) __builtin_expect(static_cast<bool>(x), 0)
#else
# define LIKELY(x) static_cast<bool>(x)
# define UNLIKELY(x) static_cast<bool>(x)
#endif

#if FZX_HAS_BUILTIN(__builtin_prefetch)
# define PREFETCH(...) __builtin_prefetch(__VA_ARGS__)
#else
# define PREFETCH(...)
#endif

#define ASSERT(x) (LIKELY(x) ? void(0) : ::fzx::detail::assertFail(#x, __FILE__, __LINE__))
#if !defined(FZX_OPTIMIZE)
# define DEBUG_ASSERT(x) ASSERT(x)
#else
# define DEBUG_ASSERT(x) UNUSED(x)
#endif

#if defined(FZX_OPTIMIZE)
# if FZX_HAS_BUILTIN(__builtin_assume)
#  define ASSUME(x) __builtin_assume(x)
# elif FZX_HAS_BUILTIN(__builtin_unreachable)
#  define ASSUME(x) (static_cast<bool>(x) ? void(0) : __builtin_unreachable())
# else
#  define ASSUME(x) UNUSED(x)
# endif
#else
# define ASSUME(x) (LIKELY(x) ? void(0) : ::fzx::detail::assumeFail(#x, __FILE__, __LINE__))
#endif

namespace fzx {

template <typename T>
constexpr void unused([[maybe_unused]] const T& x) noexcept { }

namespace detail {

// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] void assertFail(const char* expr, const char* file, unsigned long line);
// NOLINTNEXTLINE(google-runtime-int)
[[noreturn]] void assumeFail(const char* expr, const char* file, unsigned long line);

} // namespace detail

template <typename T>
static constexpr auto roundPow2(T x) noexcept
{
  --x;
  for (unsigned i = 1; i < sizeof(x); i *= 2)
    x |= x >> i;
  return ++x;
}

// TODO: generate it with cmake, on linux it's `getconf LEVEL1_DCACHE_LINESIZE`
static constexpr auto kCacheLine = 64;

using std::swap;

} // namespace fzx
