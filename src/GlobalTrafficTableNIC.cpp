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

void GlobalTrafficTableNIC::addTraffic(uint32_t src_id, uint32_t dst_id, uint32_t packet_id, uint32_t tensor_id,uint64_t* addr, uint32_t* req_type, uint32_t* flit_word_num, int** data, int packet_size){
    CommunicationNIC comm;
    comm.src = src_id;
    comm.dst = dst_id;
    // Default values
    comm.packet_id = packet_id;
    comm.tensor_id = tensor_id;
    for(int i=0;i<packet_size;i++){
        comm.addr.push_back(addr[i]);
        comm.req_type.push_back(req_type[i]);
        comm.flit_word_num.push_back(flit_word_num[i]);
        vector <int> tmp_data;
        for(int j=0;j<flit_word_num[i];j++){
            tmp_data.push_back(data[i][j]);    
        }
        comm.data.push_back(tmp_data);
    }
    comm.pir = GlobalParams::packet_injection_rate;
    comm.por = comm.pir;
    comm.t_on = 0;
    comm.t_off = GlobalParams::reset_time + GlobalParams::simulation_time;
    comm.t_period = GlobalParams::reset_time + GlobalParams::simulation_time;
    comm.used = false;
    pthread_mutex_lock(&mutex);
    traffic_table_NIC.push_back(comm);
    add_traffic_count++;
    pthread_mutex_unlock(&mutex);
    // printf("add_traffic_count : %d\n", add_traffic_count);
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
		CommunicationNIC comm = traffic_table_NIC[i];
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
void GlobalTrafficTableNIC::getPacketinCommunication(const int src_id, CommunicationNIC* comm)
{
    pthread_mutex_lock(&mutex);
	for (unsigned int i = 0; i < traffic_table_NIC.size(); i++)
	{
		if (traffic_table_NIC[i].src == src_id && !traffic_table_NIC[i].used)
		{
            get_req_count++;
            traffic_table_NIC[i].used = true;
            comm->src = traffic_table_NIC[i].src;
            comm->dst = traffic_table_NIC[i].dst;
            comm->packet_id = traffic_table_NIC[i].packet_id;
            comm->tensor_id = traffic_table_NIC[i].tensor_id;
            for(int j=0;j<traffic_table_NIC[i].addr.size();j++){
                comm->addr.push_back(traffic_table_NIC[i].addr[j]);
                comm->req_type.push_back(traffic_table_NIC[i].req_type[j]);
                comm->flit_word_num.push_back(traffic_table_NIC[i].flit_word_num[j]);
                vector <int> tmp_Data;
                for(int k=0;k<traffic_table_NIC[i].flit_word_num[j];k++){
                    tmp_Data.push_back(traffic_table_NIC[i].data[j][k]);
                }
                comm->data.push_back(tmp_Data);
            }
            comm->pir = traffic_table_NIC[i].pir;
            comm->por = traffic_table_NIC[i].por;
            comm->t_on = traffic_table_NIC[i].t_on;
            comm->t_off = traffic_table_NIC[i].t_off;
            comm->t_period = traffic_table_NIC[i].t_period;
            pthread_mutex_unlock(&mutex);
            // printf("get_req_count : %d\n", get_req_count);
            // printf("Traffic table NIC size : %d\n", traffic_table_NIC.size());
            // printf("traffic_table_NIC[%d].src : %d\n", i, traffic_table_NIC[i].src);
            // printf("req_count : %d\n", req_count);
            // printf("---------\n");
		    return;
        }
    }
    comm->src = -1;
    pthread_mutex_unlock(&mutex);
	return;
}

int GlobalTrafficTableNIC::occurrencesAsSource(const int src_id)
{
	int count = 0;

	for (unsigned int i = 0; i < traffic_table_NIC.size(); i++)
		if (traffic_table_NIC[i].src == src_id)
			count++;
	return count;
}
