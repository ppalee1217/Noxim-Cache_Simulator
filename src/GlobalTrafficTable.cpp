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
                int isOutput; //* If this tensor is a output tensor
                uint64_t req_addr;
                //TODO: The total packet number of one request (tensor) is calculated by memory size divided by cache block size
                //! In this version, the maximum allowed number of dependent tensors is limited to 4.
				int params =
                    sscanf(line, "%d %d %d %d %llx %d %d %d %d %d %d %d %lf %lf %d %d %d",
                        &src, &dst, &req_type, &tensor_size, &req_addr,
                        &tensor_id, &depend_tensor_num, &isOutput,
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
                    communication.depend_tensor_id.clear();
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
                        communication.isOutput = isOutput;
                    else
                        communication.isOutput = false;

                    if (params >= 9)
                        communication.depend_tensor_id.push_back(depend_tenosr_id_1);
                    
                    if (params >= 10)
                        communication.depend_tensor_id.push_back(depend_tenosr_id_2);
                    
                    if (params >= 11)
                        communication.depend_tensor_id.push_back(depend_tenosr_id_3);

                    if (params >= 12)
                        communication.depend_tensor_id.push_back(depend_tenosr_id_4);
                    //!
					// Custom PIR
					if (params >= 13 && pir >= 0 && pir <= 1)
						communication.pir = pir;
					else
						communication.pir =
								GlobalParams::packet_injection_rate;

					// Custom POR
					if (params >= 14 && por >= 0 && por <= 1)
						communication.por = por;
					else
						communication.por = communication.pir; // GlobalParams::probability_of_retransmission;

					// Custom Ton
					if (params >= 15 && t_on >= 0)
						communication.t_on = t_on;
					else
						communication.t_on = 0;

					// Custom Toff
					if (params >= 16 && t_off >= 0)
					{
						assert(t_off > t_on);
						communication.t_off = t_off;
					}
					else
						communication.t_off =
								GlobalParams::reset_time +
								GlobalParams::simulation_time;

					// Custom Tperiod
					if (params >= 17 && t_period > 0)
					{
						assert(t_period > t_off);
						communication.t_period = t_period;
					}
					else
						communication.t_period =
								GlobalParams::reset_time +
								GlobalParams::simulation_time;
                    communication.used = false;
                    // printf("src: %d, dst: %d, req_type: %d, tensor_size: %d, req_addr: %llx, tensor_id: %d, depend_tensor_num: %d, isOutput: %d\n",
                    //     communication.src, communication.dst, communication.req_type, communication.tensor_size, communication.req_addr,
                    //     communication.tensor_id, communication.depend_tensor_num, communication.isOutput);
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
Communication GlobalTrafficTable::getPacketinCommunication(const int src_id, bool enable_output)
{
	pthread_mutex_lock(&mutex);
    for (unsigned int i = 0; i < traffic_table.size(); i++)
	{
		if (traffic_table[i].src == src_id && !traffic_table[i].used)
		{
            if(traffic_table[i].isOutput){
                if(!enable_output){
                    pthread_mutex_unlock(&mutex);
                    return traffic_table[i];
                }
            }
            traffic_table[i].used = true;
            pthread_mutex_unlock(&mutex);
		    return traffic_table[i];
        }
    }
    pthread_mutex_unlock(&mutex);
    Communication comm;
    comm.src = -1;
    comm.isOutput = false;
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
