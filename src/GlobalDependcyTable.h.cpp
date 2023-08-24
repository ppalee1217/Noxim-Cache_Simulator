/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the global traffic table
 */

#include "GlobalTraceTable.h"

GlobalTraceTable::GlobalTraceTable()
{
    pthread_mutex_init(&mutex, NULL);
}
//TODO: Add dependency table
void GlobalTraceTable::addTrace(int src_id, int dst_id, int isReqt, int req_size, uint64_t req_addr, uint32_t* req_data, bool req_type){
    Communication communication;
    communication.src = src_id;
    communication.dst = dst_id;
    // Default values
    communication.count = 1;
    communication.finish = 0;
    communication.isReqt = isReqt;
    communication.req_addr = req_addr;
    communication.req_type = req_type;
    communication.req_data = (uint32_t*) malloc(sizeof(uint32_t)*8);
    if(req_data != NULL)
        for(int i=0;i<8;i++)
            communication.req_data[i] = req_data[i];
    else
        for(int i=0;i<8;i++)
            communication.req_data[i] = 0;
    communication.req_size = req_size;
    communication.pir = GlobalParams::packet_injection_rate;
    communication.por = communication.pir;
    communication.t_on = 0;
    communication.t_off = GlobalParams::reset_time + GlobalParams::simulation_time;
    communication.t_period = GlobalParams::reset_time + GlobalParams::simulation_time;
    pthread_mutex_lock(&mutex);
    traffic_table.insert(traffic_table.begin(), communication);
    pthread_mutex_unlock(&mutex);
    return;
}

int GlobalTraceTable::checkTrace(string traceName){

}
