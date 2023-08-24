/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the global traffic table
 */

#include "GlobalTrafficTable.h"

GlobalTrafficTable::GlobalTrafficTable()
{
    pthread_mutex_init(&mutex, NULL);
}

bool GlobalTrafficTable::load(const char *fname)
{
	// Open file
	ifstream fin(fname, ios::in);
	if (!fin)
		return false;

	// Initialize variables
	traffic_table.clear();

	// Cycle reading file
	while (!fin.eof())
	{
		char line[1024];
		fin.getline(line, sizeof(line) - 1);
        // printf("line :\n%s\n", line);

		if (line[0] != '\0')
		{
			if (line[0] != '%')
			{
				int src, dst; // Mandatory
				double pir, por;
				int t_on, t_off, t_period;
                //! modified
                int req_type, tensor_size;
                int tensor_id;
                int depend_tensor_num;
                int depend_tenosr_id_1, depend_tenosr_id_2, depend_tenosr_id_3, depend_tenosr_id_4;
                bool isOutput; //* If next tensor is a output tensor
                //* Indicate that this tensor is the last input tensor in this round
                uint64_t req_addr;
                //TODO: The total packet number of one request (tensor) is calculated by memory size divided by cache block size

				int params =
                    sscanf(line, "%d %d %d %d %llx %d %d %d %d %d %d %lf %lf %d %d %d",
                        &src, &dst, &req_type, &tensor_size, &req_addr,
                        &tensor_id, &depend_tensor_num,
                        &depend_tenosr_id_1, &depend_tenosr_id_2, &depend_tenosr_id_3, &depend_tenosr_id_4,
                        &pir, &por, &t_on, &t_off, &t_period
                        );
				if (params >= 2)
				{
					// Create a communication from the parameters read on the line
					Communication communication;

					// Mandatory fields
					communication.src = src;
					communication.dst = dst;

                    //! Modified
					if (params >= 3)
						communication.req_type = req_type;
					else
						communication.req_type = 0;

					if (params >= 4)
						communication.tensor_size = tensor_size;
                    else
                        communication.tensor_size = 0;

                    if (params >= 5)
                        communication.req_addr = req_addr;
                    else
                        communication.req_addr = 0;

                    if (params >= 6)
                        communication.tensor_id = tensor_id;
                    else
                        communication.tensor_id = 0;
                    
                    if (params >= 7)
                        communication.depend_tensor_num = depend_tensor_num;
                    else
                        communication.depend_tensor_num = 0;
                    
                    if (params >= 8)
                        communication.depend_tenosr_id_1 = depend_tenosr_id_1;
                    else
                        communication.depend_tenosr_id_1 = 0;
                    
                    if (params >= 9)
                        communication.depend_tenosr_id_2 = depend_tenosr_id_2;
                    else
                        communication.depend_tenosr_id_2 = 0;
                    
                    if (params >= 10)
                        communication.depend_tenosr_id_3 = depend_tenosr_id_3;
                    else
                        communication.depend_tenosr_id_3 = 0;

                    if (params >= 11)
                        communication.depend_tenosr_id_4 = depend_tenosr_id_4;
                    else
                        communication.depend_tenosr_id_4 = 0;
                    //!
					// Custom PIR
					if (params >= 6 && pir >= 0 && pir <= 1)
						communication.pir = pir;
					else
						communication.pir =
								GlobalParams::packet_injection_rate;

					// Custom POR
					if (params >= 7 && por >= 0 && por <= 1)
						communication.por = por;
					else
						communication.por = communication.pir; // GlobalParams::probability_of_retransmission;

					// Custom Ton
					if (params >= 8 && t_on >= 0)
						communication.t_on = t_on;
					else
						communication.t_on = 0;

					// Custom Toff
					if (params >= 9 && t_off >= 0)
					{
						assert(t_off > t_on);
						communication.t_off = t_off;
					}
					else
						communication.t_off =
								GlobalParams::reset_time +
								GlobalParams::simulation_time;

					// Custom Tperiod
					if (params >= 10 && t_period > 0)
					{
						assert(t_period > t_off);
						communication.t_period = t_period;
					}
					else
						communication.t_period =
								GlobalParams::reset_time +
								GlobalParams::simulation_time;
                    communication.used = false;
					traffic_table.push_back(communication);
				}
			}
		}
	}
	return true;
}

int GlobalTrafficTable::getTrafficSize(){
    return traffic_table.size();
}

//TODO: Sync with HW
void GlobalTrafficTable::addTraffic(int src_id, int dst_id, int isReqt, int req_size, uint64_t req_addr, uint32_t* req_data, bool req_type){
    Communication communication;
    communication.src = src_id;
    communication.dst = dst_id;
    // Default values
    communication.count = 1;
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

double GlobalTrafficTable::getCumulativePirPor(
    const int src_id,
    const int ccycle,
    const bool pir_not_por,
    vector<pair<int, double>> &dst_prob)
{
	double cpirnpor = 0.0;
	dst_prob.clear();
	for (unsigned int i = 0; i < traffic_table.size(); i++)
	{
		Communication comm = traffic_table[i];
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

//TODO: Move bank choose section to packet make, and change to coalescing version
// Get each communication packet counts - AddDate: 2023/04/02
Communication GlobalTrafficTable::getPacketinCommunication(const int src_id)
{
	for (unsigned int i = 0; i < traffic_table.size(); i++)
	{
		if (traffic_table[i].src == src_id && !traffic_table[i].used)
		{
            if(traffic_table[i].dst == -1){
                unsigned int index_mask = 0;
                unsigned int cache_set_index = 0;
                int cache_num_sets;           // Number of sets
                int cache_num_offset_bits;    // Number of block bits
                int cache_num_index_bits;     // Number of cache_set_index bits
                cache_num_sets = CACHE_SIZE / (CACHE_BLOCK_SIZE * CACHE_WAYS);
                cache_num_offset_bits = log2((CACHE_BLOCK_SIZE/4));
                cache_num_index_bits = log2(cache_num_sets);
                for(int i=0;i<cache_num_index_bits;i++){
                    cache_set_index = (cache_set_index << 1) + 1;
                    index_mask = (index_mask << 1) + 1;
                }
                cache_set_index = ((traffic_table[i].req_addr >> (cache_num_offset_bits+2)) & index_mask);
                if(ADDR_MAPPING_MODE == 0){
                    unsigned int choose_bank = cache_set_index % BANK_NUM;
                    traffic_table[i].dst = ((choose_bank/(BANK_NUM/4)) + 1)*5 - 1;
                }
                else if(ADDR_MAPPING_MODE == 1){
                    unsigned int address_partition = ((unsigned int)-1) / BANK_NUM;
                    unsigned int choose_bank = (traffic_table[i].req_addr / address_partition);
                    traffic_table[i].dst = ((choose_bank/(BANK_NUM/4)) + 1)*5 - 1;
                }
            }
            traffic_table[i].used = true;
		    return traffic_table[i];
        }
    }
    Communication comm;
    comm.src = -1;
	return comm;
}

int GlobalTrafficTable::occurrencesAsSource(const int src_id)
{
	int count = 0;

	for (unsigned int i = 0; i < traffic_table.size(); i++)
		if (traffic_table[i].src == src_id)
			count++;
	return count;
}
