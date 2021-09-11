// Copyright Supranational LLC
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

// g++ -Wall -Wextra -Werror -march=native -O3 create_labels.cpp memory_handling.cpp main.cpp -I../../blst/src ../../blst/libblst.a

#include <cstdint>          // uint*
#include <iostream>         // printing
#include "create_labels.h"
#include <gperftools/profiler.h>

int main() {
  // From captured vector
  // /tmp/.tmpNuyVpu
  // 0x1f98037e0e7f19137fdeb0bf98a3c2779da16730f1bb7c5443f69373d6b3aef3
  uint8_t replica_id[] = {
    243, 174, 179, 214, 115, 147, 246,  67,
     84, 124, 187, 241,  48, 103, 161, 157,
    119, 194, 163, 152, 191, 176, 222, 127,
     19,  25, 127,  14, 126,   3, 152,  31
  };

  uint32_t* parents_cache = nullptr;
  uint32_t* layer_labels  = nullptr;
  uint32_t* exp_labels    = nullptr;

  if (setup_create_label_memory(&parents_cache, &layer_labels, &exp_labels)) {
    return 1;
  }

  #ifndef NO_LAYER_1
  //ProfilerStart("layer1.profile");
    #ifdef PRINT_DIGEST_DEBUG
      std::cout << std::endl << "Layer 1" << std::endl;
    #endif
    // Do layer 1.  Needs to be separate due to base only
    create_label(parents_cache, replica_id, layer_labels, NULL,
                 NODE_COUNT, 1);
    //ProfilerStop();
  #endif

  #ifndef NO_EXP_LAYER
    // Do the rest of the layers 2-n
    // NOTE - only works for odd number of layers due to exp and layer swap
    for (uint32_t layer = 2; layer <= LAYER_COUNT; layer += 2) {
      printf("starting layer %d\n", layer);
      #ifdef PRINT_DIGEST_DEBUG
      std::cout << std::endl << "Layer " << layer << std::endl;
      #endif
      //ProfilerStart("exp.profile");
      create_label(parents_cache, replica_id, exp_labels, layer_labels,
                   NODE_COUNT, layer);
      //ProfilerStop();
  
      #ifdef PRINT_DIGEST_DEBUG
        std::cout << std::endl << "Layer " << layer + 1 << std::endl;
      #endif
      create_label(parents_cache, replica_id, layer_labels, exp_labels,
                   NODE_COUNT, layer + 1);
    }
  #endif

  cleanup_create_label_memory();

  return 0;
}

