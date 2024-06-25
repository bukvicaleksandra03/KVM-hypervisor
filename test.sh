#!/bin/bash

# cd Guest/A
# make
# cd ../../Version_A
# make
# ./mini_hypervisor --memory 4 --page 4KB --guest ../Guest/A/guest.img
# cd ..

# cd Guest/B
# make
# cd ../../Version_B
# make
# ./mini_hypervisor --memory 4 --page 2MB --guest ../Guest/B/guest1.img ../Guest/B/guest2.img
# cd ..

cd Guest/C
make
cd ../../Version_C
make
./mini_hypervisor --memory 4 --page 4KB --guest ../Guest/C/guest1.img ../Guest/C/guest2.img -f ./flowers.txt
cd ..