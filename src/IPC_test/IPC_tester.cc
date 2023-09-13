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
#include <vector>

#define READ_DATA 0x1234fedc
#define SHM_NAME_NOC "cache_nic0_NOC"
#define SHM_NAME_CACHE "cache_nic0_CACHE"
#define SHM_SIZE 8192
#define CHECKREADY(p)               ((*p >> 31) & 0b1)
#define CHECKVALID(p)               ((*p >> 30) & 0b1)
#define CHECKACK(p)                 ((*p >> 29) & 0b1)
#define GETSRC_ID(p)                (*(p + 1))
#define GETDST_ID(p)                (*(p + 2))
#define GETPACKET_ID(p)             (*(p + 3))
#define GETPACKET_NUM(p)            (*(p + 4))
#define GETTENSOR_ID(p)             (*(p + 5))
#define GETPACKET_SIZE(p)           (*(p + 6))
#define GETADDR(p, index)           (static_cast<uint64_t>(*(p + 7 + 12*index)))
#define GETREQ_TYPE(p, index)       (*(p + 8 + 12*index))
#define GETFLIT_WORD_NUM(p, index)  (*(p + 9 + 12*index))
#define GETDATA(p, index, pos)      (static_cast<int>(*(p + 10 + pos + 12*index)))
void* new2d(int h, int w, int size);
#define NEW2D(H, W, TYPE) (TYPE **)new2d(H, W, sizeof(TYPE))

pthread_mutex_t mutex;

using namespace std;

typedef struct Payload
{
    uint64_t addr;      // Address of the data to be exchanged
    int read;           // Read or Write packet (Only work when transmitting data to/from memory)
    int flit_word_num;  // Store flit size for each flit (i.e., the word number this flit requests/contains)
    inline bool operator==(const Payload &payload) const
    {
        return (payload.addr == addr);
    }
} Payload;

// DataPayload -- DataPayload definition
typedef struct DataPayload
{
    int data[8]; // Data to be exchanged
    inline bool operator==(const DataPayload &data_payload) const
    {
        return (
            data_payload.data[0] == data[0] &&
            data_payload.data[1] == data[1] &&
            data_payload.data[2] == data[2] &&
            data_payload.data[3] == data[3] &&
            data_payload.data[4] == data[4] &&
            data_payload.data[5] == data[5] &&
            data_payload.data[6] == data[6] &&
            data_payload.data[7] == data[7]
            );
    }
} DataPayload;

typedef struct Packet{
    int src_id;
    int dst_id;
    uint32_t packet_id;
    int packet_num;
    int tensor_id;
    vector <Payload> payload;
    vector <DataPayload> data_payload;
} Packet;

void setIPC_Ready(uint32_t *ptr);
void setIPC_Valid(uint32_t *ptr);
void setIPC_Ack(uint32_t *ptr);
void resetIPC_Ready(uint32_t *ptr);
void resetIPC_Valid(uint32_t *ptr);
void resetIPC_Ack(uint32_t *ptr);
void setIPC_Data(uint32_t *ptr, int data, int const_pos, int index);
void setIPC_Addr(uint32_t *ptr, uint64_t data, int index);
void *reader(void* arg);
void *writer(void* arg);

using namespace std;

void* new2d(int h, int w, int size)
{
    int i;
    void **p;

    p = (void**)new char[h*sizeof(void*) + h*w*size];
    for(i = 0; i < h; i++)
        p[i] = ((char *)(p + h)) + i*w*size;

    return p;
}


int main(void) {
    // src_id(32) | dst_id(32) | packet_id(32) | req(32*2) | data(32*8) | read(32) | request_size(32) | READY(1) | VALID(1)
    std::queue <Packet> packet_queue;
    pthread_t t1, t2;
    pthread_mutex_init(&mutex, NULL);
    pthread_create(&t1, NULL, reader, (void *)&packet_queue);  // create reader thread
    pthread_create(&t2, NULL, writer, (void *)&packet_queue);  // create writer thread
    while(true){
    }
    return 0;
}

void* reader(void* arg){
    while(true){
        std::queue<Packet> *packet_queue = (std::queue<Packet> *)arg;
        //* Creating shared memory object
        int fd = shm_open(SHM_NAME_NOC, O_CREAT | O_RDWR, 0666);
        //* Setting file size
        ftruncate(fd, SHM_SIZE);
        //* Mapping shared memory object to process address space
        uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        memset(ptr, 0, SHM_SIZE);
        uint32_t src_id, dst_id, packet_id, tensor_id, packet_num, packet_size;
        uint64_t* addr = (uint64_t*)malloc(16*sizeof(uint64_t));
        uint32_t* req_type = (uint32_t*)malloc(16*sizeof(uint32_t));
        uint32_t* flit_word_num = (uint32_t*)malloc(16*sizeof(uint32_t));
        int** data = NEW2D(16, 8, int);
        bool ready, valid, ack;
        Packet packet;
        packet.payload.clear();
        packet.data_payload.clear();
        cout << endl <<  "<<READER>>" << endl;
        cout << "SHM_NAME_NOC = " << SHM_NAME_NOC << endl;
        //* Reading from shared memory object
        printf("Wait for NoC to set valid signal.\n");
        while(CHECKVALID(ptr) != 1) {
            setIPC_Ready(ptr);
            sleep(1);
        }
        ready = CHECKREADY(ptr);
        valid = CHECKVALID(ptr);
        // cout << "ready = " << ready << endl;
        // cout << "valid = " << valid << endl;
        resetIPC_Ready(ptr);
        src_id = GETSRC_ID(ptr);
        dst_id = GETDST_ID(ptr);
        packet_id = GETPACKET_ID(ptr);
        packet_num = GETPACKET_NUM(ptr);
        tensor_id = GETTENSOR_ID(ptr);
        packet_size = GETPACKET_SIZE(ptr);
        for(int i=0;i<16;i++){
            addr[i] = GETADDR(ptr, i);
            req_type[i] = GETREQ_TYPE(ptr, i);
            flit_word_num[i] = GETFLIT_WORD_NUM(ptr, i);
            for(int j=0;j<8;j++){
                data[i][j] = GETDATA(ptr, i, j);
            }
        }
        setIPC_Ack(ptr);
        cout << "<<Reader Transaction completed!>>" << endl;
        //* Unmapping shared memory object from process address space
        packet.src_id = src_id;
        packet.dst_id = dst_id;
        packet.packet_id = packet_id;
        packet.tensor_id = tensor_id;
        packet.packet_num = packet_num;
        for(int i=0;i<16;i++){
            if(addr[i] != 0){
                Payload payload;
                payload.addr = addr[i];
                payload.read = req_type[i];
                payload.flit_word_num = flit_word_num[i];
                packet.payload.push_back(payload);
                if(req_type[i] == 1){
                    DataPayload data_payload;
                    for(int j=0;j<8;j++){
                        data_payload.data[j] = data[i][j];
                    }
                    packet.data_payload.push_back(data_payload);
                }
            }
        }
        pthread_mutex_lock(&mutex);
        packet_queue->push(packet);
        pthread_mutex_unlock(&mutex);
        while(CHECKACK(ptr)!=0){
        }
    }
}

void* writer(void* arg){
    while(true){
        std::queue<Packet> *packet_queue = (std::queue<Packet> *)arg;
        uint32_t data_test;
        if(!packet_queue->empty()){
            cout << endl << "<<Writer>>" << endl;
            // Creating shared memory object
            int fd = shm_open(SHM_NAME_CACHE, O_CREAT | O_RDWR, 0666);
            // Setting file size
            ftruncate(fd, SHM_SIZE);
            // Mapping shared memory object to process address space
            uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            memset(ptr, 0, SHM_SIZE);
            uint32_t ready, valid, ack, data_test;
            Packet packet;
            packet = packet_queue->front();
            cout << "SHM_NAME_CACHE = " << SHM_NAME_CACHE << endl;
            cout << "Waiting for NoC to set ready signal." << endl;
            while(CHECKREADY(ptr) != 1){
                sleep(1);
            }
            cout << "NoC is ready to read." << endl;
            setIPC_Data(ptr, packet.dst_id, 1, 0);
            //* set dst_id
            setIPC_Data(ptr, packet.src_id, 2, 0);
            //* set packet_id
            setIPC_Data(ptr, packet.packet_id, 3, 0);
            //* set packet_num
            setIPC_Data(ptr, packet.packet_num, 4, 0);
            //* set tensor_id
            setIPC_Data(ptr, packet.tensor_id, 5, 0);
            //* set packet_size
            setIPC_Data(ptr, packet.payload.size(), 6, 0);
            for(int i=0;i<packet.payload.size();i++){
                //* set request addr
                setIPC_Addr(ptr, packet.payload[i].addr, i);
                //* set request type
                setIPC_Data(ptr, packet.payload[i].read, 8, i);
                //* set flit_word_num
                setIPC_Data(ptr, packet.payload[i].flit_word_num, 9, i);
                for(int j=0;j<packet.payload[i].flit_word_num;j++){
                    //* set request data
                    setIPC_Data(ptr, packet.data_payload[i].data[j], 10+j, i);
                }
            }
            setIPC_Valid(ptr);
            cout << "Wait for NoC to set ack signal." << endl;
            while(CHECKACK(ptr) != 1){
                sleep(1);
            }
            resetIPC_Ack(ptr);
            cout << "NoC ack signal is sent back." << endl;
            cout << "<<Writer Transaction completed!>>" << endl;
            resetIPC_Valid(ptr);
            pthread_mutex_lock(&mutex);
            packet_queue->pop();
            pthread_mutex_unlock(&mutex);
        }
    }
}

void setIPC_Data(uint32_t *ptr, int data, int const_pos, int index){
    *(ptr + const_pos + 12*index) = data;
    return;
}

void setIPC_Addr(uint32_t *ptr, uint64_t data, int index){
    uint64_t *dataPtr = reinterpret_cast<uint64_t*>(ptr + 7 + 12 * index);
    *dataPtr = data;
    return;
}

void setIPC_Ready(uint32_t *ptr){
    *ptr = (*ptr | (0b1 << 31));
    return;
}

void resetIPC_Ready(uint32_t *ptr){
    *ptr = (*ptr & ~(0b1 << 31));
    return;
}

void setIPC_Valid(uint32_t *ptr){
    *ptr = (*ptr | (0b1 << 30));
    return;
}

void resetIPC_Valid(uint32_t *ptr){
    *ptr = (*ptr & ~(0b1 << 30));
    return;
}

void setIPC_Ack(uint32_t *ptr){
    *ptr = (*ptr | (0b1 << 29));
    return;
}

void resetIPC_Ack(uint32_t *ptr){
    *ptr = (*ptr & ~(0b1 << 29));
    return;
}