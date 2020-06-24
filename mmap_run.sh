#!/bin/sh

file_mags="B"
file_sizes="64"
file_parts="1"

mkdir ./log
mkdir ./log/mmap
rm ./log/mmap/*
for file_mag in $file_mags
do
	for file_part in $file_parts
	do
		for file_size in $file_sizes
		do
			cd making_data
			make
			./make_data $file_mag $file_size $file_part

			files=$(ls ./output_data | sed -e 's/^/.\/making_data\/output_data\//')
			cd ..
			log_name=${file_size}${file_mag}_${file_part}
			./eval.sh mmap ./log/mmap/mmap_${log_name}_m.log ./log/mmap/mmap_${log_name}_s.log $files

			cd making_data
			make clean
			cd ..

			echo "$file_size$file_mag, ${file_part} parts complete"
			sleep 1
		done
	done
done
