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

void setIPC_Ready(uint32_t *ptr);
void resetIPC_Ready(uint32_t *ptr);
void setIPC_Ack(uint32_t *ptr);

using namespace std;

int main(void) {
    // src_id(32) | dst_id(32) | packet_id(32) | req(32*2) | data(32*8) | read(32) | request_size(32) | READY(1) | VALID(1)
    // Creating shared memory object
    while(true){
        int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
        // Setting file size
        ftruncate(fd, SHM_SIZE);
        // Mapping shared memory object to process address space
        uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memset(ptr, 0, SHM_SIZE);
        uint32_t src_id, dst_id, packet_id, req, data, read, request_size;
        uint32_t data_test;
        uint32_t ready, valid, ack;
        cout << endl << "SHM_NAME = " << SHM_NAME << endl;
        setIPC_Ready(ptr);
        // Reading from shared memory object
        printf("Wait for NoC to set valid signal.\n");
        while(CHECKVALID(ptr) != 1) {
            // getchar();
            // ready = CHECKREADY(ptr);
            // valid = CHECKVALID(ptr);
            // ack = CHECKACK(ptr);
            // src_id = GETSRC_ID(ptr);
            // dst_id = GETDST_ID(ptr);
            // packet_id = GETPACKET_ID(ptr);
            // printf("Checking again...\n");
            // cout << "ready = " << ready << endl;
            // cout << "valid = " << valid << endl;
            // cout << "ack = " << ack << endl;
            // cout << "src_id = " << src_id << endl;
            // cout << "dst_id = " << dst_id << endl;
            // cout << "packet_id = " << packet_id << endl;
            // for(int i =0;i<16;i++){
            //     data_test = GETTEST(ptr, i);
            //     cout << "data_test" << i << " = 0x" << std::hex << data_test << std::dec << endl;
            // }
        }
        ready = CHECKREADY(ptr);
        valid = CHECKVALID(ptr);
        // cout << "ready = " << ready << endl;
        // cout << "valid = " << valid << endl;
        resetIPC_Ready(ptr);
        src_id = GETSRC_ID(ptr);
        dst_id = GETDST_ID(ptr);
        packet_id = GETPACKET_ID(ptr);
        cout << "<<Request received!>>" << endl;
        cout << "src_id = " << src_id << endl;
        cout << "dst_id = " << dst_id << endl;
        for(int i = 0; i < 2; i++) {
            req = GETREQ(ptr, i);
            cout << "req[" << i << "] = 0x" << std::hex << req << std::dec << endl;
        }
        for(int i = 0; i < 8; i++) {
            data = GETDATA(ptr, i);
            cout << "data[" << i << "] = 0x" << std::hex << data << std::dec << endl;
        }
        read = GETREAD(ptr);
        cout << "read (1 = true): " << read << endl;
        request_size = GETREQUEST_SIZE(ptr);
        cout << "request_size = " << request_size << endl;
        setIPC_Ack(ptr);
        // data_test = GETTEST(ptr, 15);
        cout << "<Transaction completed!>" << endl << endl;
        // Unmapping shared memory object from process address space
        // munmap(ptr, SHM_SIZE);
    }
    return 0;
}

void setIPC_Ready(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 31));
    return;
}

void resetIPC_Ready(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 31));
    return;
}

void setIPC_Ack(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 29));
    return;
}