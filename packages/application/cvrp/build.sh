#!/bin/bash

rm -rf build bin

mkdir build

cd build

cmake -DCMAKE_BUILD_TYPE=Release ..

make -j$(nproc)

cd ..

# ./bin/cvrp instance/P-n20-k2.vrp -u 217

# ./bin/cvrp /home/haoran/data_eda/instance2/120_200/CVRP_120_101.vrp -u 20009 2>&1 | tee tmp.txt

./bin/cvrp /home/haoran/data_eda/instance2/120_200/CVRP_120_105.vrp -u 19150 2>&1 | tee tmp.txt

# ./bin/cvrp /home/haoran/data_eda/instance2/120_200/CVRP_120_154.vrp -u 18992 2>&1 | tee 120_154.txt

