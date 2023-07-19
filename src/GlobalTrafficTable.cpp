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
		char line[512];
		fin.getline(line, sizeof(line) - 1);

		if (line[0] != '\0')
		{
			if (line[0] != '%')
			{
				int src, dst; // Mandatory
				double pir, por;
				int t_on, t_off, t_period;
				int count, isReqt;

				int params =
						sscanf(line, "%d %d %d %d %lf %lf %d %d %d", &src, &dst, &count, &isReqt,
									 &pir, &por, &t_on, &t_off, &t_period);
				if (params >= 2)
				{
					// Create a communication from the parameters read on the line
					Communication communication;

					// Mandatory fields
					communication.src = src;
					communication.dst = dst;

					if (params >= 3)
						communication.count = count;
					else
						communication.count = 0;

					if (params >= 4)
						communication.isReqt = isReqt;

					// Custom PIR
					if (params >= 5 && pir >= 0 && pir <= 1)
						communication.pir = pir;
					else
						communication.pir =
								GlobalParams::packet_injection_rate;

					// Custom POR
					if (params >= 6 && por >= 0 && por <= 1)
						communication.por = por;
					else
						communication.por = communication.pir; // GlobalParams::probability_of_retransmission;

					// Custom Ton
					if (params >= 7 && t_on >= 0)
						communication.t_on = t_on;
					else
						communication.t_on = 0;

					// Custom Toff
					if (params >= 8 && t_off >= 0)
					{
						assert(t_off > t_on);
						communication.t_off = t_off;
					}
					else
						communication.t_off =
								GlobalParams::reset_time +
								GlobalParams::simulation_time;

					// Custom Tperiod
					if (params >= 9 && t_period > 0)
					{
						assert(t_period > t_off);
						communication.t_period = t_period;
					}
					else
						communication.t_period =
								GlobalParams::reset_time +
								GlobalParams::simulation_time;

					// Add this communication to the vector of communications
					traffic_table.push_back(communication);
				}
			}
		}
	}

	return true;
}

double GlobalTrafficTable::getCumulativePirPor(const int src_id,
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

// Get each communication packet counts - AddDate: 2023/04/02
int GlobalTrafficTable::getPacketinCommunication(const int src_id, int &dst_id, int isReqt)
{
	for (unsigned int i = 0; i < traffic_table.size(); i++)
	{
		if (traffic_table[i].src == src_id && traffic_table[i].count > 0 && traffic_table[i].isReqt == isReqt)
		{
			traffic_table[i].count = traffic_table[i].count - 1;
			dst_id = traffic_table[i].dst;
			cout << "\n!!! NoC is " << isReqt << " , src is " << src_id << endl;
			// getchar();
			return traffic_table[i].count + 1;
		}
		else
		{
			// printf("traffic_table[%d].src = %d\n", i, traffic_table[i].src);
			// printf("src_id = %d\n", src_id);
			// printf("traffic_table[%d].count = %d\n", i, traffic_table[i].count);
			// printf("traffic_table[%d].isReqt = %d\n", i, traffic_table[i].isReqt);
			// printf("isReqt = %d\n", isReqt);
		}
	}
	return -1;
}

int GlobalTrafficTable::occurrencesAsSource(const int src_id)
{
	int count = 0;

	for (unsigned int i = 0; i < traffic_table.size(); i++)
		if (traffic_table[i].src == src_id)
			count++;
	return count;
}
