/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the global traffic table
 */

#include "GlobalTrafficTableNIC.h"

GlobalTrafficTableNIC::GlobalTrafficTableNIC()
{
    pthread_mutex_init(&mutex, NULL);
    traffic_table_NIC.clear();
}

int GlobalTrafficTableNIC::getTrafficSize(){
    return traffic_table_NIC.size();
}

void GlobalTrafficTableNIC::addTraffic(int src_id, int dst_id, int isReqt, int req_size, uint64_t req_addr, uint32_t* req_data, bool req_type){
    Communication communication;
    communication.src = src_id;
    communication.dst = dst_id;
    // Default values
    communication.count = 1;
    communication.finish = 0;
    communication.isReqt = isReqt;
    communication.req_addr = req_addr;
    printf(":req_addr : %llx\n", req_addr);
    communication.req_type = req_type;
    communication.req_data = (uint32_t*) malloc(sizeof(uint32_t)*8);
    if(!isReqt)
        for(int i=0;i<8;i++){
            communication.req_data[i] = req_data[i];
            printf("req_data[%d] : %x\n", i, req_data[i]);
        }
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
    traffic_table_NIC.push_back(communication);
    pthread_mutex_unlock(&mutex);
    return;
}

double GlobalTrafficTableNIC::getCumulativePirPor(
    const int src_id,
    const int ccycle,
    const bool pir_not_por,
    vector<pair<int, double>> &dst_prob)
{
	double cpirnpor = 0.0;
	dst_prob.clear();
	for (unsigned int i = 0; i < traffic_table_NIC.size(); i++)
	{
		Communication comm = traffic_table_NIC[i];
		if (comm.src == src_id)
		{
			int r_ccycle = ccycle % comm.t_period;
			if (r_ccycle > comm.t_on && r_ccycle < comm.t_off)
			{
				cpirnpor += pir_not_por ? comm.pir : comm.por;
				pair<int, double> dp(comm.dst, cpirnpor);
				dst_prob.push_back(dp);
			}
		}
	}

	return cpirnpor;
}

// Get each communication packet counts - AddDate: 2023/04/02
Communication GlobalTrafficTableNIC::getPacketinCommunication(const int src_id, int isReqt)
{
	for (unsigned int i = 0; i < traffic_table_NIC.size(); i++)
	{
        if(traffic_table_NIC[i].count > 0){
            printf("Traffic table NIC size : %d\n", traffic_table_NIC.size());
            printf("traffic_table_NIC[%d].src : %d\n", i, traffic_table_NIC[i].src);
            printf("traffic_table_NIC[%d].count : %d\n", i, traffic_table_NIC[i].count);
            printf("traffic_table_NIC[%d].isReqt : %d\n", i, traffic_table_NIC[i].isReqt);
            printf("---------\n");
        }
		if (traffic_table_NIC[i].src == src_id && traffic_table_NIC[i].count > 0 && traffic_table_NIC[i].isReqt == isReqt)
		{
            pthread_mutex_lock(&mutex);
            traffic_table_NIC[i].count = traffic_table_NIC[i].count - 1;
            req_count++;
            traffic_table_NIC[i].finish = req_count;
            pthread_mutex_unlock(&mutex);
		    return traffic_table_NIC[i];
        }
    }
    Communication comm;
    comm.src = -1;
	return comm;
}

int GlobalTrafficTableNIC::occurrencesAsSource(const int src_id)
{
	int count = 0;

	for (unsigned int i = 0; i < traffic_table_NIC.size(); i++)
		if (traffic_table_NIC[i].src == src_id)
			count++;
	return count;
}
