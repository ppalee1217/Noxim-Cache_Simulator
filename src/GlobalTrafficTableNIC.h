/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the definition of the global traffic table
 */

#ifndef __NOXIMGLOBALTRAFFIC_TABLE_NIC_H__
#define __NOXIMGLOBALTRAFFIC_TABLE_NIC_H__

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

class GlobalTrafficTableNIC {

public:
    GlobalTrafficTableNIC(); 
    int get_req_count = 0;
    int add_traffic_count = 0;
    pthread_mutex_t mutex;
    // Returns the cumulative pir por along with a vector of pairs. The
    // first component of the pair is the destination. The second
    // component is the cumulative shotting probability.
    double getCumulativePirPor(const int src_id,
    const int ccycle,
    const bool pir_not_por,
    vector < pair < int, double > > &dst_prob);
    // Returns the number of occurrences of soruce src_id in the traffic
    // table
    int occurrencesAsSource(const int src_id);
    int getTrafficSize();
    void getPacketinCommunication(const int src_id, CommunicationNIC* comm);
    void addTraffic(uint32_t src_id, uint32_t dst_id, uint32_t packet_id, uint32_t tensor_id,uint64_t* addr,uint32_t* req_type,uint32_t* flit_word_num,int** data, int packet_size);

private:
    vector < CommunicationNIC > traffic_table_NIC;
};

#endif
