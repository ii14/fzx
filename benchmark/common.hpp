#pragma once

#include <cstdio>

#define DATASET_URL "https://gist.github.com/ii14/637689ef8d071824e881a78044670310/raw/dc1dbc859daa38b62f4b9a69dec1fc599e4735e7/data.txt"

inline void noDataError()
{
  fprintf(stderr, "No data, aborting.\n");
  fprintf(stderr, "Provide the data set for the benchmark over stdin.\n");
  fprintf(stderr, "\n");
  fprintf(stderr, "Download the benchmark dataset:\n");
  fprintf(stderr, "wget " DATASET_URL "\n");
}
