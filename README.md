
# Setup

```
git clone https://github.com/supranational/blst
cd blst
./build.sh
cd ..
```

# Build

```
./build.sh
```

# Run simple test

```
./test_debug
```

You can check for the expected result by comparing the bytes produced.

First row of layer 1:
```
2c715ed2 96858021 ad6d7f5c 22b16ce1 e9b2357d bc0a2c33 9454aec1 22982162 
```

Last row of layer 2:
```
e532d96e a035e581 fcc40169 2ef9079c 7196bb22 f06fc8af 6f50982c 3d2ad045 
```

We initially checked these against a tmp file produced by lotus bench that you unfortunately won't have but you could compare with something you have locally. You would need to update the replica_id in main if you do make changes. For reference:
```
hexdump /tmp/.tmpNuyVpu/sc-02-data-layer-1.dat | head
hexdump /tmp/.tmpNuyVpu/sc-02-data-layer-2.dat | tail
```

# Run a benchmark

Change create_labels.h to use the 512M or 32GB sector size.

```
./build.sh
./run_bench.sh
```

512MB should take about 10s

# Run a full benchmark

Change LAYER_COUNT to 10 in create_labels.h

