// Copyright Supranational LLC
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// g++ -g -Wall -Wextra -Werror -march=native -O3 create_labels.cpp memory_handling.cpp gbench_create_labels.cpp -I../../blst/src ../../blst/libblst.a -lbenchmark -lpthread

#include <cstdint>                // uint*
#include <benchmark/benchmark.h>
#include "create_labels.h"

static void BM_CreateLabels(benchmark::State& state) {
  uint8_t replica_id[] = {
    243, 174, 179, 214, 115, 147, 246,  67,
     84, 124, 187, 241,  48, 103, 161, 157,
    119, 194, 163, 152, 191, 176, 222, 127,
     19,  25, 127,  14, 126,   3, 152,  31
  };

  uint32_t* parents_cache = nullptr;
  uint32_t* layer_labels  = nullptr;
  uint32_t* exp_labels    = nullptr;

  setup_create_label_memory(&parents_cache, &layer_labels, &exp_labels);

  for (auto _ : state) {
    create_label(parents_cache, replica_id, layer_labels, NULL, NODE_COUNT, 1);
  }

  cleanup_create_label_memory();
}

static void BM_CreateLabelsExp(benchmark::State& state) {
  uint8_t replica_id[] = {
    243, 174, 179, 214, 115, 147, 246,  67,
     84, 124, 187, 241,  48, 103, 161, 157,
    119, 194, 163, 152, 191, 176, 222, 127,
     19,  25, 127,  14, 126,   3, 152,  31
  };

  uint32_t* parents_cache = nullptr;
  uint32_t* layer_labels  = nullptr;
  uint32_t* exp_labels    = nullptr;

  setup_create_label_memory(&parents_cache, &layer_labels, &exp_labels);
  create_label(parents_cache, replica_id, layer_labels, NULL, NODE_COUNT, 1);

  for (auto _ : state) {
    create_label(parents_cache, replica_id, exp_labels, layer_labels,
                 NODE_COUNT, 2);
  }

  cleanup_create_label_memory();
}

BENCHMARK(BM_CreateLabels);
BENCHMARK(BM_CreateLabelsExp);

BENCHMARK_MAIN();
