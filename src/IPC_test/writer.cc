#include <fcntl.h>  // for O_* constants
#include <string.h>
#include <sys/mman.h>  // for POSIX shared memory API
#include <sys/stat.h>  // for mode constants
#include <unistd.h>    // ftruncate
#include <stdint.h>
#include <iostream>
#include <iomanip>

#define SHM_NAME "cache_nic4"
#define SHM_SIZE 512
#define CHECKREADY(p) (*(p + 15) >> 31)
#define CHECKVALID(p) ((*(p + 15) >> 30) & 0b1)
#define CHECKACK(p) ((*(p + 15) >> 29) & 0b1)
#define GETSRC_ID(p) (*p)
#define GETDST_ID(p) (*(p + 1))
#define GETPACKET_ID(p) (*(p + 2))
#define GETREQ(p, pos) (*(p + 3 + pos))
#define GETDATA(p, pos) (*(p + 5 + pos))
#define GETREAD(p) (*(p + 13))
#define GETREQUEST_SIZE(p) (*(p + 14))
#define GETTEST(p,pos) (*(p + pos))

int main(void) {
    // Creating shared memory object
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    // Setting file size
    ftruncate(fd, SHM_SIZE);
    // Mapping shared memory object to process address space
    char* ptr = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    // Resetting shared memory data
    memset(ptr, 0, SHM_SIZE);

    while (strcmp(ptr, "stop") != 0) {
        scanf("%s", ptr);
    }
    // Unmapping shared memory object from process address space
    munmap(ptr, SHM_SIZE);
    return 0;
}
