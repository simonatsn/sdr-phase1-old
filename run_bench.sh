#!/bin/bash

taskset -c 4,5,6 sudo ./bench --benchmark_filter=CreateLabelsExp --benchmark_repetitions=1
