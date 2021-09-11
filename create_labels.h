// Copyright Supranational LLC
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#ifndef __CREATE_LABELS_H__
#define __CREATE_LABELS_H__

#if (defined(__x86_64__) || defined(__x86_64) || defined(_M_X64)) && \
     defined(__SHA__)   /* -msha */
# define blst_sha256_block blst_sha256_block_data_order_shaext
#elif defined(__aarch64__) && defined(__ARM_FEATURE_CRYPTO)
# define blst_sha256_block blst_sha256_block_armv8
#else
# define blst_sha256_block blst_sha256_block_data_order
#endif

extern "C" {
  void blst_sha256_block(uint32_t* h, const void* in, size_t blocks);
  void blst_sha256_emit(uint8_t* md, const uint32_t* h);
}

//const size_t SECTOR_SIZE       = (1UL << 11); // 2K
const size_t SECTOR_SIZE       = (1UL << 20) * 512; // 512M
//const size_t SECTOR_SIZE       = (32UL << 30); // 32GB - 8:34

const size_t NODE_SIZE         = 32;
const size_t NODE_WORDS        = NODE_SIZE / sizeof(uint32_t);
const size_t NODE_COUNT        = SECTOR_SIZE / NODE_SIZE;
const size_t PARENT_COUNT_BASE = 6;
const size_t PARENT_COUNT_EXP  = 8;
const size_t PARENT_COUNT      = PARENT_COUNT_BASE + PARENT_COUNT_EXP;
const size_t PARENT_SIZE       = sizeof(uint32_t);
const size_t PARENTS_SIZE      = NODE_COUNT * PARENT_COUNT * PARENT_SIZE;
const size_t LAYER_COUNT       = 2;

int create_label(uint32_t* parents_cache, uint8_t* replica_id,
                 uint32_t* layer_labels,  uint32_t* exp_labels,
                 uint64_t  num_nodes,     uint32_t  cur_layer);

int setup_create_label_memory(uint32_t** parents_cache, uint32_t** layer_labels,
                              uint32_t** exp_labels);

void cleanup_create_label_memory();

#endif // __CREATE_LABELS_H__
