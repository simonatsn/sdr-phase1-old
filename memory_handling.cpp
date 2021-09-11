// Copyright Supranational LLC
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0

#include <iostream>         // printing
#include <fstream>          // file read
#include "create_labels.h"
#include <fcntl.h>          // mmap
#include <sys/mman.h>       // mmap
#include <errno.h>
#include <string.h>
#include <unistd.h>

static void* parents_cache_alloc = nullptr;
static int   parants_cache_fd    = 0;
static void* layer_labels_alloc  = nullptr;
static void* exp_labels_alloc    = nullptr;

uint32_t *allocate_layer() {
  uint32_t *layer = (uint32_t *)mmap(NULL, SECTOR_SIZE, PROT_READ | PROT_WRITE,
                                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED, -1, 0);
  if (layer == (void *)-1) {
    printf("mmap layer failed err %s\n", strerror(errno));
    exit(1);
  }
  if (((uintptr_t)layer & 0x3F) != 0) {
    printf("ERROR - layer not aligned\n");
    exit(1);
  }
  if (mlock(layer, PARENTS_SIZE) != 0) {
    printf("mlock layer failed err %s\n", strerror(errno));
    exit(1);
  }
  return layer;
}

uint32_t *map_parent_cache() {
  const char *parents_cache_filename;
  if (SECTOR_SIZE == 2048) {             // 2KB parents cache file
    parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-3894e5db3e22e371be947bd27e5c6e8e7da3ca81c6bd8005e42504ff101796f1.cache";
  } else if (SECTOR_SIZE == 536870912) { // 512M parents cache file
    parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-b9440d6f444972abcd5ebc48231d93b92e7d1c132968170ae29c44d68fa04d04.cache";
  } else if (SECTOR_SIZE == 34359738368) { // 32G parents cache file
    parents_cache_filename = "/var/tmp/filecoin-parents/v28-sdr-parent-d5500bc0dddadb609f867d94da1471ecbaac3fe6f8ac68a4705cebde04a765b8.cache";
  } else {
    std::cout << "ERROR - non supported parent cache sector size" << std::endl;
    return NULL;
  }
  
  // Open the parent cache file
  int fd = open(parents_cache_filename, O_RDONLY);
  parants_cache_fd = fd;
  
  uint32_t *parents = (uint32_t *)mmap(NULL, PARENTS_SIZE, PROT_READ,
                                       MAP_PRIVATE | MAP_LOCKED, fd, 0);
  if (parents == (void *)-1) {
    printf("mmap parents failed err %s\n", strerror(errno));
    exit(1);
  }
  if (((uintptr_t)parents & 0x3F) != 0) {
    printf("ERROR - parents not aligned\n");
    exit(1);
  }
  if (mlock(parents, PARENTS_SIZE) != 0) {
    printf("mlock parents failed err %s\n", strerror(errno));
    exit(1);
  }

  return parents;
}

int setup_create_label_memory(uint32_t** parents_cache,
                              uint32_t** layer_labels,
                              uint32_t** exp_labels) {
  *parents_cache = map_parent_cache();
  *layer_labels = allocate_layer();
  *exp_labels = allocate_layer();

  parents_cache_alloc = *parents_cache;
  layer_labels_alloc  = *layer_labels;
  exp_labels_alloc    = *exp_labels;

  return 0;
}

void cleanup_create_label_memory() {
  if (munmap(parents_cache_alloc, PARENTS_SIZE) != 0) {
    printf("munmap parents failed err %s\n", strerror(errno));
    exit(1);
  }
  if (munmap(layer_labels_alloc, SECTOR_SIZE) != 0) {
    printf("munmap parents failed err %s\n", strerror(errno));
    exit(1);
  }
  if (munmap(exp_labels_alloc, SECTOR_SIZE) != 0) {
    printf("munmap parents failed err %s\n", strerror(errno));
    exit(1);
  }
  close(parants_cache_fd);
}
