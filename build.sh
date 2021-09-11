#!/bin/bash

# Stop on error
set -e

# Print commands
set -x

# Run first to smoke test
g++ -g -Wall -Wextra -Werror -march=native -DPRINT_DIGEST_DEBUG create_labels.cpp memory_handling.cpp main.cpp -o test_debug -I./blst/src ./blst/libblst.a -pthread

# Run in parallel
#g++ -g -Wall -Wextra -Werror -march=native -O3 create_labels.cpp memory_handling.cpp main.cpp -o test -I./blst/src ./blst/libblst.a -lprofiler -pthread &

#g++ -g -Wall -Wextra -Werror -march=native -O3 -DNO_EXP_LAYER create_labels.cpp memory_handling.cpp main.cpp -o test_layer_1 -I./blst/src ./blst/libblst.a -lprofiler -ltcmalloc -pthread &

g++ -Wall -Wextra -Werror -march=native -O3 -fomit-frame-pointer create_labels.cpp memory_handling.cpp gbench_create_labels.cpp -o bench -I./blst/src ./blst/libblst.a -lbenchmark -pthread &

#clang++-10 -Wall -Wextra -Werror -march=native -O3 -fomit-frame-pointer create_labels.cpp memory_handling.cpp gbench_create_labels.cpp -o bench_clang -I./blst/src ./blst/libblst.a -lbenchmark -pthread &

wait



