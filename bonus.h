#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define SCORE_GAP_LEADING -0.005
#define SCORE_GAP_TRAILING -0.005
#define SCORE_GAP_INNER -0.01
#define SCORE_MATCH_CONSECUTIVE 1.0
#define SCORE_MATCH_SLASH 0.9
#define SCORE_MATCH_WORD 0.8
#define SCORE_MATCH_CAPITAL 0.7
#define SCORE_MATCH_DOT 0.6

#include <stddef.h>

extern const double bonus_states[3][256];
extern const size_t bonus_index[256];

#define COMPUTE_BONUS(last_ch, ch) (bonus_states[bonus_index[(unsigned char)(ch)]][(unsigned char)(last_ch)])

#ifdef __cplusplus
}
#endif

// vim: sts=2 sw=2 et
