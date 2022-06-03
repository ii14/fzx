#pragma once

#include <cmath>

using score_t = double;
#define SCORE_MAX INFINITY
#define SCORE_MIN -INFINITY

static constexpr auto MATCH_MAX_LEN = 1024;

int has_match(const char *needle, const char *haystack);
score_t match_positions(const char *needle, const char *haystack, size_t *positions);
score_t match(const char *needle, const char *haystack);

// vim: sts=2 sw=2 et
