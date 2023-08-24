/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the definition of the global traffic table
 */

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

class GlobalTraceTable {

public:
    GlobalTraceTable(); 
    pthread_mutex_t mutex;
    void addTrace(int src_id, int dst_id, int isReqt, int req_size, uint64_t req_addr, uint32_t* req_data, bool req_type);
    int GlobalTraceTable::checkTrace(string traceName);
private:
    vector < Communication > trace_table;
};

#endif
