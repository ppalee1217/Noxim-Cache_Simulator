#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>  // for O_* constants
#include <string.h>
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

void *reader(void* arg);
void *writer(void* arg);

using namespace std;


int main(void) {
    // src_id(32) | dst_id(32) | packet_id(32) | req(32*2) | data(32*8) | read(32) | request_size(32) | READY(1) | VALID(1)
    std::queue <packet_data> packet_queue;
    pthread_t t1, t2;
    while(true){
        pthread_create(&t1, NULL, reader, (void *)&packet_queue);  // create reader thread
        pthread_create(&t2, NULL, writer, (void *)&packet_queue);  // create writer thread
        pthread_join(t1, NULL);  // wait for reader thread to finish
        pthread_join(t2, NULL);  // wait for writer thread to finish
    }
    return 0;
}

void* reader(void* arg){
    std::queue<packet_data> *packet_queue = (std::queue<packet_data> *)arg;
    // Creating shared memory object
    int fd = shm_open(SHM_NAME_NOC, O_CREAT | O_RDWR, 0666);
    // Setting file size
    ftruncate(fd, SHM_SIZE);
    // Mapping shared memory object to process address space
    uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    memset(ptr, 0, SHM_SIZE);
    uint32_t src_id, dst_id, packet_id, req, data, read, request_size;
    uint32_t data_test;
    uint32_t ready, valid, ack;
    packet_data packet;
    cout << endl <<  "<<READER>>" << endl;
    cout << "SHM_NAME_NOC = " << SHM_NAME_NOC << endl;
    // Reading from shared memory object
    printf("Wait for NoC to set valid signal.\n");
    while(CHECKVALID(ptr) != 1) {
        setIPC_Ready(ptr);
        // cout << "(Read) Address = " << std::hex << ptr << std::dec << endl;
        // for(int i=0;i<16;i++){
        //     data_test = GETTEST(ptr, i);
        //     cout << "data_test" << i << " = 0x" << std::hex << data_test << std::dec << endl;
        // }
        // cout << endl;
        sleep(1);
        // sleep(1);
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
    packet.src_id = src_id;
    packet.dst_id = dst_id;
    packet.packet_id = packet_id;
    cout << endl << "<<Request received!>>" << endl;
    cout << "src_id = " << src_id << endl;
    cout << "dst_id = " << dst_id << endl;
    for(int i = 0; i < 2; i++) {
        req = GETREQ(ptr, i);
        packet.req[i] = req;
        cout << "req[" << i << "] = 0x" << std::hex << req << std::dec << endl;
    }
    for(int i = 0; i < 8; i++) {
        data = GETDATA(ptr, i);
        packet.data[i] = data;
        cout << "data[" << i << "] = 0x" << std::hex << data << std::dec << endl;
    }
    read = GETREAD(ptr);
    packet.read = read;
    cout << "read (1 = true): " << read << endl;
    request_size = GETREQUEST_SIZE(ptr);
    packet.request_size = request_size;
    cout << "request_size = " << request_size << endl;
    setIPC_Ack(ptr);
    // data_test = GETTEST(ptr, 15);
    cout << "<<Reader Transaction completed!>>" << endl;
    // Unmapping shared memory object from process address space
    // munmap(ptr, SHM_SIZE);
    packet_queue->push(packet);
    while(CHECKACK(ptr)!=0){
    }
    pthread_exit(NULL);
}

void* writer(void* arg){
    std::queue<packet_data> *packet_queue = (std::queue<packet_data> *)arg;
    uint32_t data_test;
    cout << endl << "<<Writer>>" << endl;
    if(packet_queue->empty()){
        cout << "Nothing to write!" << endl;
        pthread_exit(NULL);
    }
    else{
        // Creating shared memory object
        int fd = shm_open(SHM_NAME_CACHE, O_CREAT | O_RDWR, 0666);
        // Setting file size
        ftruncate(fd, SHM_SIZE);
        // Mapping shared memory object to process address space
        uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memset(ptr, 0, SHM_SIZE);
        uint32_t ready, valid, ack;
        packet_data packet;
        packet = packet_queue->front();
        cout << "SHM_NAME_CACHE = " << SHM_NAME_CACHE << endl;
        cout << "Waiting for NoC to set ready signal." << endl;
        while(CHECKREADY(ptr) != 1){
            // cout << "(Write) Address = " << std::hex << ptr << std::dec << endl;
            // for(int i=0;i<16;i++){
            //     data_test = GETTEST(ptr, i);
            //     cout << "ata_test" << i << " = 0x" << std::hex << data_test << std::dec << endl;
            // }
            // cout << endl;
            sleep(1);
            // ready = CHECKREADY(ptr);
            // data_test = GETTEST(ptr, 15);
            // cout << "Checking again..." << endl;
            // cout << "ready = " << ready << endl;
            // cout << "data_test = 0x" << std::hex << data_test << std::dec << endl;
        }
        cout << "NoC is ready to read." << endl;
        //* Setting the valid bit and size
        // set src_id
        setIPC_Data(ptr, packet.dst_id, 0, 0);
        // set dst_id
        setIPC_Data(ptr, packet.src_id, 1, 0);
        // set packet_id
        setIPC_Data(ptr, packet.packet_id, 2, 0);
        // set request addr
        for(int i=0;i<(REQ_PACKET_SIZE/32);i++){
            setIPC_Data(ptr, packet.req[i], 3, i);
        }
        // set request data (write only)
        if(packet.read != 1){
            for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
                setIPC_Data(ptr, 0x0 , 5, i);
            }
        }
        else{
            //* Pseudo test, real data number should be request_size
            for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
                setIPC_Data(ptr, READ_DATA, 5, i);
            }
        }
        // set request size (read only)
        setIPC_Data(ptr, packet.request_size, 14, 0);
        // set read
        setIPC_Data(ptr, packet.read, 13, 0);
        setIPC_Valid(ptr);
        cout << "Wait for NoC to set ack signal." << endl;
        while(CHECKACK(ptr) != 1){
            // cout << "(Write) Address = " << std::hex << ptr << std::dec << endl;
            // for(int i=0;i<16;i++){
            //     data_test = GETTEST(ptr, i);
            //     cout << "data_test" << i << " = 0x" << std::hex << data_test << std::dec << endl;
            // }
            // cout << endl;
            sleep(1);
        }
        resetIPC_Ack(ptr);
        cout << "NoC ack signal is sent back." << endl;
        cout << "<<Writer Transaction completed!>>" << endl;
        resetIPC_Valid(ptr);
        // munmap(ptr, SHM_SIZE);
        // shm_unlink(shm_name);
        packet_queue->pop();
        pthread_exit(NULL);
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