#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>
#include <assert.h>

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

#define PAGE_SIZE 4096
#define BUF_SIZE 512
#define MAX_FILE_NUM 50
#define MAX_FILE_NAME 50

// size_t = unsigned int
// off_t = int

void print_hex(const char *s, int len) {
    for (int i = 0; i < len; i++)
        printf("%02x", (unsigned int) s[i]);
    printf("\n");
}

int main (int argc, char* argv[]) {
    char buf[BUF_SIZE];
    int dev_fd, file_fd; // the fd for the device and the fd for the input file
    size_t ret;
    char file_name[MAX_FILE_NUM][MAX_FILE_NAME];
    off_t file_size, true_file_size;
    off_t total_file_size = 0;
    struct timeval start;
    struct timeval end;
    double trans_time; // calulate the time between the device is opened and it is closed
    double total_trans_time = 0;
    char *file_address, *kernel_address;

    // read system page size
    const long MMAP_SIZE = sysconf(_SC_PAGE_SIZE);

    // read file num & name
    int file_num = atoi(argv[1]);
    for(int i = 2; i < argc - 2; i++) {
        strcpy(file_name[i - 2], argv[i]);
        fprintf(stderr, "file name : %s\n", file_name[i - 2]);
    }

    // read method, ip
    char method[20];
    char ip[20];
    strcpy(method, argv[argc - 2]);
    strcpy(ip, argv[argc - 1]);


    // should be O_RDWR for PROT_WRITE when mmap()
    if ((dev_fd = open("/dev/slave_device", O_RDWR)) < 0) {
        perror("failed to open /dev/slave_device\n");
        return 1;
    }
    
    //0x12345677 : connect to master in the device
    if (ioctl(dev_fd, 0x12345677, ip) == -1) {
        perror("ioclt create slave socket error\n");
        return 1;
    }

    fprintf(stderr, "ioctl success \n");

    fprintf(stderr, "method : %s\n", method);
    fprintf(stderr, "file num : %d\n", file_num);

    for (int file_index = 0; file_index < file_num; file_index++) {
        if ((file_fd = open (file_name[file_index], O_RDWR | O_CREAT | O_TRUNC)) < 0) {
            perror("failed to open input file\n");
            return 1;
        }

        // get start time
        gettimeofday(&start, NULL);
         
        // start transfering
        switch (method[0]) {
            case 'f': { // fcntl : read()/write()
                // read file size
                ret = read(dev_fd, &file_size, sizeof(size_t));
                fprintf(stderr, "new file %s size : %lu bytes\n", file_name[file_index], file_size);
                true_file_size = file_size;
                // open file
                while (file_size > BUF_SIZE) {
                    ret = read(dev_fd, buf, sizeof(buf)); // read from the the device
                    write(file_fd, buf, ret); // write to the input file
                    file_size -= ret;
                    fprintf(stderr, "read %lu bytes, remain file size : %lu bytes\n", ret, file_size);
                }
                // there're only 'file_size' characters left to be read
                ret = read(dev_fd, buf, file_size * sizeof(char));
                write(file_fd, buf, ret);
                fprintf(stderr, "read remaining %lu bytes\n", ret);

                break;
            }
            case 'm': { // mmap
                off_t offset = 0;
                
                // get file_size
                while ((ioctl(dev_fd, 0x12345678, sizeof(off_t))) < 0 && errno == EAGAIN);
                kernel_address = mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, dev_fd, offset);
                memcpy(&file_size, kernel_address, sizeof(off_t));
                fprintf(stderr, "new file %s size : %lu bytes\n", file_name[file_index], file_size);
                true_file_size = file_size;
                
                while (offset < true_file_size) {
                    off_t len = MIN(MMAP_SIZE, file_size);
                    posix_fallocate(file_fd, offset, len);
                    fprintf(stderr, "len = %ld\n", len);
                    while((ioctl(dev_fd, 0x12345678, len)) < 0 && errno == EAGAIN);
                    kernel_address = (char *)mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, dev_fd, offset);
                    file_address = (char *)mmap(NULL, MMAP_SIZE, PROT_WRITE | PROT_READ, MAP_SHARED, file_fd, offset);
                    memcpy(file_address, kernel_address, len);
                    munmap(file_address, len);
                    offset += len;
                    file_size -= len;
                    fprintf(stderr, "offset: %ld, remaining file_size: %ld\n", offset, file_size);
                }
                ftruncate(file_fd, true_file_size);
                munmap(kernel_address, PAGE_SIZE);
                break;
            }
            close(file_fd);
        }

        // get end time
        gettimeofday(&end, NULL);
        trans_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) * 0.001;
        total_trans_time += trans_time;
        total_file_size += true_file_size;
    }   
    fprintf(stderr, "File transfer done\n");
    
    printf("Transmission time: %lf ms, File size: %ld bytes\n", total_trans_time, total_file_size);
    
    // end receiving data, close the connection
    if (ioctl(dev_fd, 0x12345679) == -1) {
        perror("ioclt client exits error\n");
        return 1;
    }

    close(dev_fd);
    return 0;
}
