cd ./master_device
rmmod master_device
make clean

cd ../slave_device
rmmod slave_device
make clean

cd ../ksocket
rmmod ksocket
make clean

cd ../user_program
make clean

