/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the definition of the global traffic table
 */

#ifndef __NOXIMGLOBALTRAFFIC_TABLE_H__
#define __NOXIMGLOBALTRAFFIC_TABLE_H__

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
#include <algorithm>

#include "DataStructs.h"

using namespace std;

class GlobalTrafficTable {

public:
    GlobalTrafficTable(); 
    // Load traffic table from file. Returns true if ok, false otherwise
    bool load(const char *fname);
    // Returns the cumulative pir por along with a vector of pairs. The
    // first component of the pair is the destination. The second
    // component is the cumulative shotting probability.
    double getCumulativePirPor(const int src_id,
    const int ccycle,
    const bool pir_not_por,
    vector < pair < int, double > > &dst_prob);
    int getTrafficSize();
    // Returns the number of occurrences of soruce src_id in the traffic
    // table
    int occurrencesAsSource(const int src_id);
    pthread_mutex_t mutex;
    int req_count = 0;
    Communication getPacketinCommunication(const int src_id, int isReqt);
    void addTraffic(int src_id, int dst_id, int isReqt, int req_size, uint64_t req_addr, uint32_t* req_data, bool req_type);

private:
    vector < Communication > traffic_table;
};

#endif
