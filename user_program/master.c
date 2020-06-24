#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#define MAX(a, b) a > b ? a : b
#define MIN(a, b) a < b ? a : b

#define PAGE_SIZE 4096
#define BUF_SIZE 512
#define MAX_FILE 50
#define MAX_NAME 50

// size_t = unsigned int
// off_t = int

off_t get_filesize(const char* file_name); // get the size of the input file

int main (int argc, char* argv[]) {
    char buf[BUF_SIZE];
    int dev_fd, file_fd; // the fd for the device and the fd for the input file
    size_t ret;
    off_t file_size, true_file_size;
    off_t total_file_size = 0;
    int file_num = 0;
    char file_name[MAX_FILE][MAX_NAME], method[20];
    struct timeval start;
    struct timeval end;
    double trans_time; // calulate the time between the device is opened and it is closed
    double total_trans_time = 0;
    char *kernel_address, *file_address;
   
    // read system page size
    const long MMAP_SIZE = sysconf(_SC_PAGE_SIZE);

    // read file num & name
    file_num = atoi(argv[1]);
    for (int i = 2; i < argc - 1; i++) {
        strcpy(file_name[i - 2], argv[i]);
        fprintf(stderr, "file name : %s\n", file_name[i - 2]);
    }
    
    // read method ip
    strcpy(method, argv[argc - 1]);
    
    // should be O_RDWR for PROT_WRITE when mmap()
    if((dev_fd = open("/dev/master_device", O_RDWR)) < 0) {
        perror("failed to open /dev/master_device\n");
        return 1;
    }
   
    // 0x12345677: create socket and accept the connection from the slave
    if (ioctl(dev_fd, 0x12345677) == -1) {
        perror("ioclt server create socket error\n");
        return 1;
    }

    fprintf(stderr, "ioctl success\n");

    fprintf(stderr, "method : %s\n", method);
    fprintf(stderr, "file num : %d\n", file_num);

    for (int i = 0; i < file_num; i++) {
        if ((file_fd = open (file_name[i], O_RDWR)) < 0) {
            perror("failed to open input file\n");
            return 1;
        }

        if((file_size = get_filesize(file_name[i])) < 0) {
            perror("failed to get filesize\n");
            return 1;
        }
        fprintf(stderr, "new file %s size %lu bytes\n", file_name[i], file_size);

        true_file_size = file_size;

        // get start time
        gettimeofday(&start, NULL);

        switch(method[0]) {
            case 'f': { // fcntl : read()/write()
                // write file size
                write (dev_fd, &file_size, sizeof(size_t));
                do {
                    ret = read(file_fd, buf, sizeof(buf)); // read from the input file
                    write(dev_fd, buf, ret); // write to the the device
                    fprintf(stderr, "write %lu bytes\n", ret);
                } while(ret > 0);

                break;
            }
            case 'm': { // mmap
                // transfer file_size
                off_t offset = 0;
                kernel_address = mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, offset); 
                memcpy(kernel_address, &file_size, sizeof(off_t));
                while(ioctl(dev_fd, 0x12345678, sizeof(off_t)) < 0 && errno == EAGAIN);

                while (offset < true_file_size) {
                    off_t len = MIN(MMAP_SIZE, file_size);
                    fprintf(stderr, "write %lu bytes\n", len);
                    file_address = (char *)mmap(NULL, MMAP_SIZE, PROT_READ, MAP_SHARED, file_fd, offset);
                    kernel_address = (char *)mmap(NULL, MMAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, dev_fd, offset); 
                    memcpy(kernel_address, file_address, len);
                    while(ioctl(dev_fd, 0x12345678, len) < 0 && errno == EAGAIN);
                    offset += len;
                    file_size -= len;
                }
                if (ioctl(dev_fd, 0x12345676, kernel_address) == -1)
                    perror("ioctl server");
                // munmap
                munmap(kernel_address, PAGE_SIZE);
                break;
            }
        }
        close(file_fd);

        // get end time
        gettimeofday(&end, NULL);
        trans_time = (end.tv_sec - start.tv_sec) * 1000 + (end.tv_usec - start.tv_usec) * 0.001;
        total_trans_time += trans_time;
        total_file_size += true_file_size;
    }
    
    printf("Transmission time: %lf ms, File size: %ld bytes\n", total_trans_time, total_file_size);
    
    // end sending data, close the connection
    if (ioctl(dev_fd, 0x12345679) == -1) {
        perror("ioctl server exits error\n");
        return 1;
    }
    close(dev_fd);

    return 0;
}

off_t get_filesize(const char* file_name) {
    struct stat st;
    stat(file_name, &st);
    return st.st_size;
}
