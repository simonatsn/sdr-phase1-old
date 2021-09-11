// Copyright Supranational LLC
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <cstdint>       // uint*
#include <cstring>       // memcpy
#include <fstream>       // file read
#include <iostream>      // printing
#include <iomanip>       // printing
#include <byteswap.h>    // bswap_64 TODO - make more portable?
#include <thread>
#include <atomic>
#include <unistd.h>      // usleep
#include <sys/mman.h>    // mlock
#include "create_labels.h"

//#define PRINT_DIGEST_DEBUG

const uint32_t SHA256_INITIAL_DIGEST[8] = {
  0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
  0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
};

void print_digest(uint32_t* digest) {
  for (int i = 0; i < 8; ++i) {
    std::cout << std::hex << std::setfill('0') << std::setw(8)
              << digest[i] << " ";
  }
  std::cout << std::endl;
}

void print_bytes(const char *name, uint8_t* buf, size_t size) {
  std::cout << name << " (" << std::dec << size << "): " << std::endl;
  for (size_t i = 0; i < size; ++i) {
    if (i > 0 && i % 64 == 0) {
      std::cout << std::endl;
    }
    std::cout << std::hex << std::setfill('0') << std::setw(2)
              << (uint32_t)buf[i];
    
  }
  std::cout << std::endl;
}

// Fill the buffer for cur_node
inline
void fill_buffer(uint64_t  cur_node,
                 std::atomic<uint64_t> &cur_consumer,
                 uint32_t* cur_parent, // parents for this node
                 uint32_t* layer_labels,
                 uint32_t* exp_labels,
                 uint8_t*  buf,
                 uint32_t* base_parent_missing,
                 bool      is_layer0) {
  const size_t min_base_parent_node = 2000;

  uint64_t cur_node_swap = bswap_64(cur_node); // Note switch to big endian
  std::memcpy(buf + 36, &cur_node_swap, 8); // update buf with current node

  // Perform the first hash
  uint32_t* cur_node_ptr = layer_labels + (cur_node * NODE_WORDS);
  std::memcpy(cur_node_ptr, SHA256_INITIAL_DIGEST, 32);
  blst_sha256_block(cur_node_ptr, buf, 1);
  
  // Fill in the base parents
  // Node 5 (prev node) will always be missing, and there tend to be
  // frequent close references.
  if (cur_node > min_base_parent_node) {
    *base_parent_missing = 0x20; // Mark base parent 5 as missing
    // Skip the last base parent - it always points to the preceding node,
    // which we know is not ready and will be filled in the main loop
    for (size_t k = 0; k < PARENT_COUNT_BASE - 1; ++k) {
      if (*cur_parent >= cur_consumer.load()) {
        // Node is not ready
        *base_parent_missing |= (1 << k);
      } else {
        uint32_t *parent_data = layer_labels + (*cur_parent * NODE_WORDS);
        std::memcpy(buf + 64 + (NODE_SIZE * k), parent_data, NODE_SIZE);
      }
      cur_parent++;
    }
    // Advance pointer for the last base parent
    cur_parent++;
  } else {
    *base_parent_missing = (1 << PARENT_COUNT_BASE) - 1;
    cur_parent += PARENT_COUNT_BASE;
  }

  if (!is_layer0) {
    // Read from each of the expander parent nodes
    for (size_t k = PARENT_COUNT_BASE; k < PARENT_COUNT; ++k) {
      uint32_t *parent_data = exp_labels + (*cur_parent * NODE_WORDS);
      std::memcpy(buf + 64 + NODE_SIZE * k, parent_data, NODE_SIZE);
      cur_parent++;
    }
  }
}

// This implements a producer, i.e. a thread that pre-fills the buffer
// with parent node data.
// - cur_consumer - The node currently being processed (consumed) by the
//                  hashing thread
// - cur_producer - The next node to be filled in by producer threads. The
//                  hashing thread can not yet work on this node.
// - cur_awaiting - The first not not currently being filled by any producer
//                  thread.
// - stride       - Each producer fills in this many nodes at a time. Setting
//                  this too small with cause a lot of time to be spent in
//                  thread synchronization
// - lookahead    - ring_buf size, in nodes
// - base_parent_missing - Bit mask of any base parent nodes that could not
//                         be filled in. This is an array of size lookahead.
// - is_layer0    - Indicates first (no expander parents) or subsequent layer
int create_label_runner(uint32_t* parents_cache,
                        uint32_t* layer_labels,
                        uint32_t* exp_labels, // NULL for layer 0
                        uint64_t num_nodes,
                        std::atomic<uint64_t> &cur_consumer,
                        std::atomic<uint64_t> &cur_producer,
                        std::atomic<uint64_t> &cur_awaiting,
                        size_t stride,
                        uint64_t lookahead,
                        uint8_t *ring_buf,
                        uint32_t *base_parent_missing,
                        bool is_layer0) {
  // Label data bytes per node
  const size_t bytes_per_node = (NODE_SIZE * PARENT_COUNT) + 64;

  while(true) {
    // Get next work items
    uint64_t work = cur_awaiting.fetch_add(stride);
    if (work >= num_nodes) {
      break;
    }
    uint64_t count = stride;
    if (work + stride > num_nodes) {
      count = num_nodes - work;
    }
    
    // Do the work of filling the buffers
    for (size_t s = 0; s < count; s++) {
      uint64_t i = work + s;

      // Determine which node slot in the ring_buffer to use
      // Note that node 0 does not use a buffer slot
      uint32_t cur_slot = (i - 1) % lookahead;
      
      // Don't overrun the buffer
      while(i > (cur_consumer.load() + lookahead - 1)) {
        usleep(10);
      }
      uint8_t *buf = ring_buf + cur_slot * bytes_per_node;
      
      fill_buffer(i, cur_consumer,
                  parents_cache + i * PARENT_COUNT,
                  layer_labels, exp_labels, 
                  buf, &base_parent_missing[cur_slot],
                  is_layer0);
    }

    // Wait for the previous node to finish
    while(work > (cur_producer.load() + 1)) {
      usleep(10);
    }
    
    // Mark our work as done
    cur_producer.fetch_add(count);
  }
  return 0;
}

int create_label(uint32_t* parents_cache, uint8_t*  replica_id,
                 uint32_t* layer_labels,
                 uint32_t* exp_labels, // NULL for layer0
                 uint64_t  num_nodes,     uint32_t  cur_layer) {
  size_t lookahead;
  size_t num_producers;
  size_t producer_stride;
  if (cur_layer == 1) {
    lookahead       = 400;
    num_producers   = 1;    // Number of producer threads
    producer_stride = 16;   // Producers work on groups of nodes
  } else {
    lookahead       = 800;
    num_producers   = 2;    // Number of producer threads
    producer_stride = 128;  // Producers work on groups of nodes
  }
  const size_t bytes_per_node = (NODE_SIZE * PARENT_COUNT) + 64;

  uint8_t *ring_buf = new uint8_t[lookahead * bytes_per_node];
  uint32_t *base_parent_missing = new uint32_t[lookahead];
  
  // Fill in the fixed portion of all buffers
  for (size_t i = 0; i < lookahead; i++) {
    uint8_t *buf = ring_buf + i * bytes_per_node;
    std::memcpy(buf, replica_id, 32);
    buf[35]  = (uint8_t)(cur_layer & 0xFF);
    buf[64]  = 0x80; // Padding
    buf[126] = 0x02; // Length (512 bits == 64B)
  }

  // Node the consumer is currently working on
  std::atomic<uint64_t> cur_consumer(0);
  // Highest node that is ready from the producer
  std::atomic<uint64_t> cur_producer(0);
  // Next node to be filled
  std::atomic<uint64_t> cur_awaiting(1);

  std::thread runners[num_producers];
  for (size_t i = 0; i < num_producers; i++) {
    runners[i] = std::thread([&]() {
      create_label_runner(parents_cache, layer_labels, exp_labels,
                          num_nodes,
                          cur_consumer, cur_producer, cur_awaiting,
                          producer_stride, lookahead,
                          ring_buf, base_parent_missing,
                          cur_layer == 1);
    });
  }

  uint32_t* cur_node_ptr   = layer_labels;
  uint32_t* cur_parent_ptr = parents_cache + PARENT_COUNT;

  // Calculate node 0 (special case with no parents)
  // Which is replica_id || cur_layer || 0
  // TODO - Hash and save intermediate result: replica_id || cur_layer
  uint8_t buf[(NODE_SIZE * PARENT_COUNT) + 64] = {0};
  std::memcpy(buf, replica_id, 32);
  buf[35]  = (uint8_t)(cur_layer & 0xFF);
  buf[64]  = 0x80; // Padding
  buf[126] = 0x02; // Length (512 bits == 64B)

  std::memcpy(cur_node_ptr, SHA256_INITIAL_DIGEST, 32);
  blst_sha256_block(cur_node_ptr, buf, 2);

  // Fix endianess
  blst_sha256_emit((uint8_t*)cur_node_ptr, cur_node_ptr);
  cur_node_ptr[7] &= 0x3FFFFFFF; // Strip last two bits to ensure in Fr
  #ifdef PRINT_DIGEST_DEBUG
  if (cur_layer <= 2) {
    print_digest(cur_node_ptr);
  }
  #endif

  // Keep track of which node slot in the ring_buffer to use
  uint32_t cur_slot = 0;
  size_t count_not_ready = 0;
  
  // Calculate nodes 1 to n
  cur_consumer = 1;
  uint64_t i = 1;
  while(i < num_nodes) {
    // Ensure next buffer is ready
    bool printed = false;
    uint64_t producer_val;
    while ((producer_val = cur_producer.load()) < i) {
      if (!printed) {
        printf("PRODUCER NOT READY! %ld\n", i);
        printed = true;
        count_not_ready++;
      }
      usleep(10);
    }

    // Process as many nodes as are ready
    size_t ready_count = producer_val - i + 1;
    for(size_t count = 0; count < ready_count; count++) {
      cur_node_ptr += 8;
      uint8_t *buf = ring_buf + cur_slot * bytes_per_node;
    
      // Fill in the base parents
      for (size_t k = 0; k < PARENT_COUNT_BASE; ++k) {
        if ((base_parent_missing[cur_slot] & (1 << k)) != 0) {
          std::memcpy(buf + 64 + (NODE_SIZE * k),
                      layer_labels + ((*cur_parent_ptr) * 8),
                      NODE_SIZE);
        }
        cur_parent_ptr++;
      }
      
      // Expanders are already all filled in (layer 1 doesn't use expanders)
      cur_parent_ptr += PARENT_COUNT_EXP;
      
      if (cur_layer == 1) {
        // Six rounds of all base parents
        for (size_t j = 0; j < 6; ++j) {
          blst_sha256_block(cur_node_ptr, buf + 64, 3);
        }
        
        // round 7 is only first parent
        std::memset(buf + 96, 0, 32); // Zero out upper half of last block
        buf[96]  = 0x80;            // Padding
        buf[126] = 0x27;            // Length (0x2700 = 9984 bits -> 1248 bytes)
        blst_sha256_block(cur_node_ptr, buf + 64, 1);
      } else {
        // Two rounds of all parents
        blst_sha256_block(cur_node_ptr, buf + 64, 7);
        blst_sha256_block(cur_node_ptr, buf + 64, 7);
        
        // Final round is only nine parents
        std::memset(buf + 352, 0, 32); // Zero out upper half of last block
        buf[352] = 0x80;             // Padding
        buf[382] = 0x27;             // Length (0x2700 = 9984 bits -> 1248 bytes)
        blst_sha256_block(cur_node_ptr, buf + 64, 5);
      }
      
      // Fix endianess
      blst_sha256_emit((uint8_t*)cur_node_ptr, cur_node_ptr);
      cur_node_ptr[7] &= 0x3FFFFFFF; // Strip last two bits to fit in Fr
#ifdef PRINT_DIGEST_DEBUG
      if (cur_layer <= 2) {
        print_digest(cur_node_ptr);
      }
#endif

      cur_consumer++;
      i++;
      cur_slot = (cur_slot + 1) % lookahead;
    }
  }

  printf("Count of producer not ready %ld\n", count_not_ready);
  
  for (size_t i = 0; i < num_producers; i++) {
    runners[i].join();
  }
  delete [] ring_buf;
  delete [] base_parent_missing;

  return 0;
}
