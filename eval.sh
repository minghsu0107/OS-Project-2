#!/bin/sh

if [ "$1" = "fcntl" ]
then
	log_m=$2
	log_s=$3
	shift 3
	echo "using fcntl" 1>2
	echo "transfered files: $@" 1>2
	echo "logs saved in $log_m / $log_s" 1>2

	./user_program/master $# $@ fcntl 1>$log_m 2>/dev/null &

	out_file=$(echo $@ | sed -e 's/$/_out/g')
	echo "out file: ${out_file}" 1>2
	./user_program/slave  $# $out_file fcntl 127.0.0.1 1>$log_s 2>/dev/null 

else
	log_m=$2
	log_s=$3	
	shift 3
	echo "using mmap" 1>2
	echo "transfered fiels: $@" 1>2
	echo "logs saved in $log_m / $log_s" 1>2
	
	echo '' > mmap_m.log
	./user_program/master $# $@ mmap 1>$log_m 2>/dev/null &
	
	echo '' > mmap_s.log

	out_file=$(echo $@ | sed -e 's/$/_out/g')
	echo "out file: ${out_file}" 1>2
	./user_program/slave $# $out_file mmap 127.0.0.1  | grep "Transmission"  1>$log_s 2>/dev/null & 
fi
