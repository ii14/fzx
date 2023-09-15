#include <catch2/catch_session.hpp>

int main(int argc, char** argv)
{
  return Catch::Session().run(argc, argv);
}

// NOLINTNEXTLINE(readability-identifier-naming, bugprone-reserved-identifier)
extern "C" const char* __ubsan_default_options()
{
  return "print_stacktrace=1";
}

// NOLINTNEXTLINE(readability-identifier-naming, bugprone-reserved-identifier)
extern "C" const char* __asan_default_options()
{
  return "detect_leaks=1,handle_abort=1,handle_sigill=1";
}
