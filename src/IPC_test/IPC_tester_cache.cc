#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>  // for O_* constants
#include <string.h>
#include <cstring>
#include <string>
#include <sys/mman.h>  // for POSIX shared memory API
#include <sys/stat.h>  // for mode constants
#include <unistd.h>    // ftruncate
#include <stdint.h>
#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <queue>

#define READ_DATA 0x1234fedc
#define SHM_NAME_NOC "cache_nic4_NOC"
#define SHM_NAME_CACHE "cache_nic4_CACHE"
#define SHM_SIZE 512
#define REQ_PACKET_SIZE 64
#define DATA_PACKET_SIZE 256
#define CHECKREADY(p) (*(p + 15) >> 31)
#define CHECKVALID(p) ((*(p + 15) >> 30) & 0b1)
#define CHECKACK(p) ((*(p + 15) >> 29) & 0b1)
#define CHECKFINISH(p) ((*(p + 15) >> 28) & 0b1)
#define GETSRC_ID(p) (*p)
#define GETDST_ID(p) (*(p + 1))
#define GETPACKET_ID(p) (*(p + 2))
#define GETREQ(p, pos) (*(p + 3 + pos))
#define GETDATA(p, pos) (*(p + 5 + pos))
#define GETREAD(p) (*(p + 13))
#define GETREQUEST_SIZE(p) (*(p + 14))
#define GETTEST(p,pos) (*(p + pos))

typedef struct packet_data{
    uint32_t src_id;
    uint32_t dst_id;
    uint32_t packet_id;
    uint32_t req[2];
    uint32_t data[8];
    uint32_t read;
    uint32_t request_size;
} packet_data;

void setIPC_Valid(uint32_t *ptr);
void resetIPC_Valid(uint32_t *ptr);
void setIPC_Ready(uint32_t *ptr);
void resetIPC_Ready(uint32_t *ptr);
void setIPC_Ack(uint32_t *ptr);
void resetIPC_Ack(uint32_t *ptr);
void setIPC_Data(uint32_t *ptr, uint32_t data, int const_pos, int varied_pos);
void setIPC_Finish(uint32_t *ptr, bool finish);
void *reader(void* arg);
void *writer(void* arg);

using namespace std;


int main(void) {
    // src_id(32) | dst_id(32) | packet_id(32) | req(32*2) | data(32*8) | read(32) | request_size(32) | READY(1) | VALID(1)
    std::queue <packet_data> packet_queue;
    pthread_t t1, t2;
    pthread_create(&t2, NULL, writer, (void *)&packet_queue);  // create writer thread
    pthread_create(&t1, NULL, reader, (void *)&packet_queue);  // create reader thread
    while(true){
    }
}

void* reader(void* arg){
    while(true){
        std::queue<packet_data> *packet_queue = (std::queue<packet_data> *)arg;
        // Creating shared memory object
        int fd = shm_open(SHM_NAME_CACHE, O_CREAT | O_RDWR, 0666);
        // Setting file size
        ftruncate(fd, SHM_SIZE);
        // Mapping shared memory object to process address space
        uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memset(ptr, 0, SHM_SIZE);
        uint32_t src_id, dst_id, packet_id, req, data, read, request_size;
        uint32_t data_test;
        bool ready, valid, ack;
        packet_data packet;
        cout << endl <<  "<<Reader>>" << endl;
        cout << "(R) SHM_NAME_CACHE = " << SHM_NAME_CACHE << endl;
        // Reading from shared memory object
        printf("(R) Wait for Cache Simulator to set valid signal.\n");
        while(CHECKVALID(ptr) != 1) {
            setIPC_Ready(ptr);
            usleep(10);
        }
        ready = CHECKREADY(ptr);
        valid = CHECKVALID(ptr);
        resetIPC_Ready(ptr);
        src_id = GETSRC_ID(ptr);
        dst_id = GETDST_ID(ptr);
        packet_id = GETPACKET_ID(ptr);
        packet.src_id = src_id;
        packet.dst_id = dst_id;
        packet.packet_id = packet_id;
        cout << endl << "(R) <<Request received!>>" << endl;
        // for(int i=0;i<15;i++){
        //     cout << "(R) test[" << i << "] = 0x" << std::hex << std::setw(8) << std::setfill('0') << GETTEST(ptr,i) << std::dec << endl;
        // }
        // printf("----------------------------------------\n");
        cout << "(R) src_id = " << src_id << endl;
        cout << "(R) dst_id = " << dst_id << endl;
        for(int i = 0; i < 2; i++) {
            req = GETREQ(ptr, i);
            packet.req[i] = req;
            cout << "(R) req [" << i << "] = 0x" << std::hex << std::setw(8) << std::setfill('0') << req << std::dec << endl;
        }
        for(int i = 0; i < 8; i++) {
            data = GETDATA(ptr, i);
            packet.data[i] = data;
            cout << "(R) data[" << i << "] = 0x" << std::hex << std::setw(8) << std::setfill('0') << data << std::dec << endl;
        }
        read = GETREAD(ptr);
        packet.read = read;
        cout << "(R) read (1 = true): " << read << endl;
        request_size = GETREQUEST_SIZE(ptr);
        packet.request_size = request_size;
        cout << "(R) request_size = " << request_size << endl;
        setIPC_Ack(ptr);
        // data_test = GETTEST(ptr, 15);
        cout << "<<Reader Transaction completed!>>" << endl;
        packet_queue->push(packet);
        while(CHECKACK(ptr)!=0){
        }
    }
}

void* writer(void* arg){
    uint read_data = 0xffffffff;
    while(true){
        std::queue<packet_data> *packet_queue = (std::queue<packet_data> *)arg;
        uint32_t src_id, dst_id, packet_id, req, read, data_uint, request_size;
        uint32_t data_test;
        bool ready, valid, ack, finish;
        std::string data = "";
        cout << endl << "<<Writer>>" << endl;
        // Creating shared memory object
        int fd = shm_open(SHM_NAME_NOC, O_CREAT | O_RDWR, 0666);
        // Setting file size
        ftruncate(fd, SHM_SIZE);
        // Mapping shared memory object to process address space
        uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memset(ptr, 0, SHM_SIZE);
        cout << "(W) SHM_NAME_NOC = " << SHM_NAME_NOC << endl;
        cout << "(W) Waiting for Cache Simulator to set ready signal." << endl;
        while(CHECKREADY(ptr) != 1){
        }
        cout << "(W) Cache Simulator is ready to read." << endl;
        //* Setting the valid bit and size
        cout << "(W) Input src ID: ";
        cin >> src_id;
        cout << "(W) Input destination ID: ";
        cin >> dst_id;
        cout << "(W) Input packet ID: ";
        cin >> packet_id;
        cout << "(W) Input request type (0: write / 1: read): ";
        cin >> read;
        // set src_id
        setIPC_Data(ptr, dst_id, 0, 0);
        // set dst_id
        setIPC_Data(ptr, src_id, 1, 0);
        // set packet_id
        setIPC_Data(ptr, packet_id, 2, 0);
        // set request addr
        for(int i=0;i<(REQ_PACKET_SIZE/32);i++){
            cout << "(W) Input request address (32 bits, in hex): ";
            cin >> data;
            req = std::stoul(data, nullptr, 16);
            setIPC_Data(ptr, req, 3, i);
        }
        cout << "(W) Input request size (in flits/words): ";
        cin >> request_size;
        // set request data (write only)
        if(read != 1){
            for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
                cout << "(W) Input write data (32 bits, in hex): ";
                cin >> data;
                uint32_t int_data = std::stoul(data, nullptr, 16);
                setIPC_Data(ptr, int_data, 5, i);
            }
        }
        else{
            for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
                setIPC_Data(ptr, read_data, 5, i);
                read_data--;
            }
        }
        // set request size (read only)
        setIPC_Data(ptr, request_size, 14, 0);
        // set read
        setIPC_Data(ptr, read, 13, 0);
        cout << "(W) Is processing finished? (0: no / 1: yes): ";
        cin >> finish;
        // set finish
        setIPC_Finish(ptr, finish);
        setIPC_Valid(ptr);
        cout << "(W) Wait for Cache Simulator to set ack signal." << endl;
        while(CHECKACK(ptr) != 1){
        }
        resetIPC_Ack(ptr);
        cout << "(W) Cache Simulator ack signal is sent back." << endl;
        cout << "<<Writer Transaction completed!>>" << endl;
        resetIPC_Valid(ptr);
    }
}

void setIPC_Data(uint32_t *ptr, uint32_t data, int const_pos, int varied_pos){
    *(ptr + const_pos + varied_pos) = data;
    return;
}

void setIPC_Valid(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 30));
    return;
}

void resetIPC_Valid(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 30));
    return;
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

void resetIPC_Ack(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 29));
    return;
}

void setIPC_Finish(uint32_t *ptr, bool finish){
    if(finish){
        *(ptr + 15) = (*(ptr + 15) | (0b1 << 28));
    }
    else{
        *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 28));
    }
    return;
}