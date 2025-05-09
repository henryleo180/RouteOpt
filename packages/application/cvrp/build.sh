#!/bin/bash

# rm -rf build bin

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

make -j$(nproc)

cd ..

./bin/cvrp /home/haoran/data_eda/instance2/120_200/CVRP_120_101.vrp -u 20009 2>&1 | tee tmp.txt

