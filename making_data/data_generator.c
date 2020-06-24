#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned long long int size_arrangement(char setdatasize[], unsigned long long int size);
void generate_big_data(unsigned long long int size, FILE* f);
void generate_small_data(unsigned long long int size, int division, char datasize[], char s[]);

int main(int argc, char* argv[]) {
	char name[100];

	sprintf(name, "./%s%s.txt", argv[2], argv[1]);
	FILE* f = fopen(name, "w");
	unsigned long long int size = (unsigned long long int)atoi(argv[2]);
	size = size_arrangement(argv[1], size);
	printf("size = %lld\n", size);
	int division = atoi(argv[3]);
	generate_big_data(size, f);
	generate_small_data(size, division, argv[1], argv[2]);
	
	return 0;
}

unsigned long long int size_arrangement(char setdatasize[], unsigned long long int size) {
	char* data_size[4] = {"B", "KB", "MB", "GB"};
	if(strcmp(setdatasize, data_size[0]) == 0)
		return size*1;
	if(strcmp(setdatasize, data_size[1]) == 0)
		return size*1000;
	if(strcmp(setdatasize, data_size[2]) == 0)
		return size*1000*1000;
	if(strcmp(setdatasize, data_size[3]) == 0)
		return size*1000*1000*1000;
	return 0;
}

void generate_big_data(unsigned long long int size, FILE* f) {
	for (unsigned long long int i = 0; i < size; i++) {
        	char c = rand() % 95 + 32;
	    	fprintf(f, "%c", c);
    	}
	fclose(f);
}

void generate_small_data(unsigned long long int size, int division, char datasize[], char s[]){
	for (int i = 0; i < division; i++) {
		char subname[100];
		sprintf(subname, "./output_data/%d_%s%s.txt", i, s, datasize);
		FILE* f = fopen(subname, "w");
		for (unsigned long long int j = 0; j < size / (unsigned long long int)division; j++) {
            		char c = rand() % 95 + 32;
			fprintf(f, "%c", c);
        	}
		fclose(f);
	}
}
