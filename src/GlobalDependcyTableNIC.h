#ifndef __NOXIMGLOBALTRACE_TABLE_H__
#define __NOXIMGLOBALTRACE_TABLE_H__

#include <stdio.h>
#include <cmath>
#include <stdlib.h>
#include <vector>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>  // for O_* constants
#include <string.h>
#include <sys/mman.h>  // for POSIX shared memory API
#include <sys/stat.h>  // for mode constants
#include <unistd.h>    // ftruncate
#include <stdint.h>
#include <iomanip>
#include <unistd.h>

#include "DataStructs.h"

using namespace std;

class GlobalDependcyTableNIC {

public:
    GlobalDependcyTableNIC(); 
    void addDependcy(tensorDependcyNIC tensorDependcyNIC);
    void reducePacketNum(int local_id, int tensor_id, int packet_num);
    bool checkDependcyReturn(int tensor_id);
    pthread_mutex_t mutex;
private:
    vector <tensorDependcyNIC> NIC_dependcy_table;
};

#endif
