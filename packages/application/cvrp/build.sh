#!/bin/bash

rm -rf build bin

# rm -rf build

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

make -j$(nproc)

cd ..

# ./bin/cvrp instance/P-n20-k2.vrp -u 217

# ./bin/cvrp instance/P-n60-k15.vrp -u 979

./bin/cvrp /home/haoran/data_eda/instance2/120_200/CVRP_120_100.vrp -u 17909