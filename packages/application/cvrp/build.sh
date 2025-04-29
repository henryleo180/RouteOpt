#!/bin/bash

# rm -rf build bin

# rm -rf build

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

make -j$(nproc)

cd ..

# ./bin/cvrp instance/P-n20-k2.vrp -u 217

./bin/cvrp instance2/180_200/CVRP_180_115.vrp -u 28802 2>&1 | tee tmp.txt
