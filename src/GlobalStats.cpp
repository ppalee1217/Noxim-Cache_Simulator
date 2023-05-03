/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementaton of the global statistics
 */

#include "GlobalStats.h"
using namespace std;

GlobalStats::GlobalStats(const NoC * _noc)
{
    noc = _noc;

	#ifdef TESTING
    drained_total = 0;
	#endif
}

double GlobalStats::getAverageDelay(int isReqt)
{
    unsigned int total_packets = 0;
    double avg_delay = 0.0;

	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
		for (int x = 0; x < GlobalParams::mesh_dim_x; x++) 
		{
			unsigned int received_packets;
			if (isReqt) received_packets = noc->t[x][y]->r->stats.getReceivedPackets();
			else received_packets = noc->t[x][y]->dr->stats.getReceivedPackets();

			if (received_packets) 
			{
				if (isReqt) avg_delay +=
						received_packets *
						noc->t[x][y]->r->stats.getAverageDelay();
				else avg_delay +=
					received_packets *
					noc->t[x][y]->dr->stats.getAverageDelay();

				total_packets += received_packets;
			}
		}

    avg_delay /= (double) total_packets;

    return avg_delay;
}



double GlobalStats::getAverageDelay(const int src_id,
					 const int dst_id)
{
    Tile *tile = noc->searchNode(dst_id);

    assert(tile != NULL);

    return tile->r->stats.getAverageDelay(src_id);
}

double GlobalStats::getMaxDelay(int isReqt)
{
    double maxd = -1.0;

	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
		for (int x = 0; x < GlobalParams::mesh_dim_x; x++) 
		{
			Coord coord;
			coord.x = x;
			coord.y = y;
			int node_id = coord2Id(coord);
			double d = getMaxDelay(node_id, isReqt);
			if (d > maxd)
				maxd = d;
		}

    return maxd;
}

double GlobalStats::getMaxDelay(const int node_id, int isReqt)
{
	Coord coord = id2Coord(node_id);

	unsigned int received_packets;
	if (isReqt) received_packets = 
		noc->t[coord.x][coord.y]->r->stats.getReceivedPackets();
	else received_packets = 
		noc->t[coord.x][coord.y]->dr->stats.getReceivedPackets();

	if (received_packets)
	{
		if (isReqt) return noc->t[coord.x][coord.y]->r->stats.getMaxDelay();
		else return noc->t[coord.x][coord.y]->dr->stats.getMaxDelay();
	}
		
	else
		return -1.0;
}

// double GlobalStats::getMaxDelay(const int src_id, const int dst_id)
// {
//     Tile *tile = noc->searchNode(dst_id);

//     assert(tile != NULL);

//     return tile->r->stats.getMaxDelay(src_id);
// }

vector < vector < double > > GlobalStats::getMaxDelayMtx(int isReqt)
{
    vector < vector < double > > mtx;

    assert(GlobalParams::topology == TOPOLOGY_MESH); 

    mtx.resize(GlobalParams::mesh_dim_y);
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	mtx[y].resize(GlobalParams::mesh_dim_x);

    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	for (int x = 0; x < GlobalParams::mesh_dim_x; x++) 
	{
	    Coord coord;
	    coord.x = x;
	    coord.y = y;
	    int id = coord2Id(coord);
	    mtx[y][x] = getMaxDelay(id, isReqt);
	}

    return mtx;
}

double GlobalStats::getAverageThroughput(const int src_id, const int dst_id)
{
    Tile *tile = noc->searchNode(dst_id);

    assert(tile != NULL);

    return tile->r->stats.getAverageThroughput(src_id);
}

/*
double GlobalStats::getAverageThroughput()
{
    unsigned int total_comms = 0;
    double avg_throughput = 0.0;

    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	for (int x = 0; x < GlobalParams::mesh_dim_x; x++) {
	    unsigned int ncomms =
		noc->t[x][y]->r->stats.getTotalCommunications();

	    if (ncomms) {
		avg_throughput +=
		    ncomms * noc->t[x][y]->r->stats.getAverageThroughput();
		total_comms += ncomms;
	    }
	}

    avg_throughput /= (double) total_comms;

    return avg_throughput;
}
*/

double GlobalStats::getAggregatedThroughput(int isReqt)
{
    int total_cycles = GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;

    return (double)getReceivedFlits(isReqt)/(double)(total_cycles);
}

unsigned int GlobalStats::getReceivedPackets(int isReqt)
{
    unsigned int n = 0;

    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
    	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
			for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
			{
				if (isReqt) n += noc->t[x][y]->r->stats.getReceivedPackets();
				else n += noc->t[x][y]->dr->stats.getReceivedPackets();
			}
    }
    else // other delta topologies
    {
    	for (int y = 0; y < GlobalParams::n_delta_tiles; y++)
	    n += noc->core[y]->r->stats.getReceivedPackets();
    }

	// cout << "\n********Global Received Packets: " << n << endl;
    return n;
}

unsigned int GlobalStats::getReceivedFlits(int isReqt)
{
    unsigned int n = 0;
    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	    for (int x = 0; x < GlobalParams::mesh_dim_x; x++) {
			if (isReqt) n += noc->t[x][y]->r->stats.getReceivedFlits();
			else n += noc->t[x][y]->dr->stats.getReceivedFlits();
#ifdef TESTING
		drained_total += noc->t[x][y]->r->local_drained;
#endif
	    }
    }
    else // other delta topologies
    {
	for (int y = 0; y < GlobalParams::n_delta_tiles; y++)
	{
	    n += noc->core[y]->r->stats.getReceivedFlits();
#ifdef TESTING
	    drained_total += noc->core[y]->r->local_drained;
#endif
	}
    }

	// cout << "\n********Global Received Flits: " << n << endl;
    return n;
}

double GlobalStats::getThroughput(int isReqt)
{
    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
	int number_of_ip = GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y;
	return (double)getAggregatedThroughput(isReqt)/(double)(number_of_ip);
    }
    else // other delta topologies
    {
	int number_of_ip = GlobalParams::n_delta_tiles;
	return (double)getAggregatedThroughput(isReqt)/(double)(number_of_ip);
    }
}

// Only accounting IP that received at least one flit
double GlobalStats::getActiveThroughput()
{
    int total_cycles =
	GlobalParams::simulation_time -
	GlobalParams::stats_warm_up_time;
    unsigned int n = 0;
    unsigned int trf = 0;
    unsigned int rf ;
    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	    for (int x = 0; x < GlobalParams::mesh_dim_x; x++) 
	    {
		rf = noc->t[x][y]->r->stats.getReceivedFlits();

		if (rf != 0)
		    n++;

		trf += rf;
	    }
    }
    else // other delta topologies
    {
	for (int y = 0; y < GlobalParams::n_delta_tiles; y++)
	{
	    rf = noc->core[y]->r->stats.getReceivedFlits();

	    if (rf != 0)
		n++;

	    trf += rf;
	}
    }

    return (double) trf / (double) (total_cycles * n);

}

vector < vector < unsigned long > > GlobalStats::getRoutedFlitsMtx()
{

    vector < vector < unsigned long > > mtx;
    assert (GlobalParams::topology == TOPOLOGY_MESH); 

    mtx.resize(GlobalParams::mesh_dim_y);
    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	mtx[y].resize(GlobalParams::mesh_dim_x);

    for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
	    mtx[y][x] = noc->t[x][y]->r->getRoutedFlits();


    return mtx;
}

unsigned int GlobalStats::getWirelessPackets()
{
    unsigned int packets = 0;

    // Wireless noc
    for (map<int, HubConfig>::iterator it = GlobalParams::hub_configuration.begin();
            it != GlobalParams::hub_configuration.end();
            ++it)
    {
	int hub_id = it->first;

	map<int,Hub*>::const_iterator i = noc->hub.find(hub_id);
	Hub * h = i->second;

	packets+= h->wireless_communications_counter;
    }
    return packets;
}

double GlobalStats::getDynamicPower(int isReqt)
{
    double power = 0.0;

    // Electric noc
	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	    for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
		{
			if (isReqt) power += noc->t[x][y]->r->power.getDynamicPower();
			else power += noc->t[x][y]->dr->power.getDynamicPower();
		}
		

    return power;
}

double GlobalStats::getStaticPower(int isReqt)
{
    double power = 0.0;

	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
		for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
		{
			if (isReqt) power += noc->t[x][y]->r->power.getStaticPower();
			else power += noc->t[x][y]->dr->power.getStaticPower();
		}
			
    return power;
}

void GlobalStats::showStats(std::ostream & out, bool detailed)
{
    if (detailed) 
    {
		assert (GlobalParams::topology == TOPOLOGY_MESH); 
		out << endl << "detailed = [" << endl;

		for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
			for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
			noc->t[x][y]->r->stats.showStats(y * GlobalParams:: mesh_dim_x + x, out, true);
		out << "];" << endl;

		// show MaxDelay matrix
		vector < vector < double > > md_mtx = getMaxDelayMtx(0);

		out << endl << "max_delay = [" << endl;
		for (unsigned int y = 0; y < md_mtx.size(); y++) 
		{
			out << "   ";
			for (unsigned int x = 0; x < md_mtx[y].size(); x++)
			out << setw(6) << md_mtx[y][x];
			out << endl;
		}
		out << "];" << endl;

		// show RoutedFlits matrix
		vector < vector < unsigned long > > rf_mtx = getRoutedFlitsMtx();

		out << endl << "routed_flits = [" << endl;
		for (unsigned int y = 0; y < rf_mtx.size(); y++) 
		{
			out << "   ";
			for (unsigned int x = 0; x < rf_mtx[y].size(); x++)
			out << setw(10) << rf_mtx[y][x];
			out << endl;
		}
		out << "];" << endl;

		showPowerBreakDown(out, 0);
		showPowerManagerStats(out);
    }

#ifdef DEBUG

    if (GlobalParams::topology == TOPOLOGY_MESH)
    {
		out << "Queue sizes: " << endl;
		for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
			for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
				out << "PE[ "<<x << ", " << y<< " ] " << noc->t[x][y]->pe->getQueueSize()<< ", ";
		cout << endl;
		for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
			for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
				out << "PE[ "<<x << ", " << y<< " ] " << noc->t[x][y]->pe->getDataQueueSize()<< ", ";
	}
    else // other delta topologies
    {
	out << "Queue sizes: " << endl;
	for (int i=0;i<GlobalParams::n_delta_tiles;i++)
		out << "PE"<<i << ": " << noc->core[i]->pe->getQueueSize()<< ",";
	out << endl;
    }
	
    out << endl;
#endif

	cout << endl;
    //int total_cycles = GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;
    out << "% Reqt NoC ==================================================" << endl;
	out << "% Total received packets: " << getReceivedPackets(1) << endl;
    out << "% Total received flits: " << getReceivedFlits(1) << endl;
    out << "% Received/Ideal flits Ratio: " << getReceivedIdealFlitRatio(1) << endl;
    // out << "% Average wireless utilization: " << getWirelessPackets()/(double)getReceivedPackets() << endl;
    out << "% Global average delay (cycles): " << getAverageDelay(1) << endl;
    out << "% Max delay (cycles): " << getMaxDelay(1) << endl;
    out << "% Network throughput (flits/cycle): " << getAggregatedThroughput(1) << endl;
    out << "% Average IP throughput (flits/cycle/IP): " << getThroughput(1) << endl;
    out << "% Total energy (J): " << getTotalPower(1) << endl;
    out << "% \tDynamic energy (J): " << getDynamicPower(1) << endl;
    out << "% \tStatic energy (J): " << getStaticPower(1) << endl << endl;

	out << "% Data NoC ==================================================" << endl;
    out << "% Total received packets: " << getReceivedPackets(0) << endl;
    out << "% Total received flits: " << getReceivedFlits(0) << endl;
    out << "% Received/Ideal flits Ratio: " << getReceivedIdealFlitRatio(0) << endl;
    // out << "% Average wireless utilization: " << getWirelessPackets()/(double)getReceivedPackets() << endl;
    out << "% Global average delay (cycles): " << getAverageDelay(0) << endl;
    out << "% Max delay (cycles): " << getMaxDelay(0) << endl;
    out << "% Network throughput (flits/cycle): " << getAggregatedThroughput(0) << endl;
    out << "% Average IP throughput (flits/cycle/IP): " << getThroughput(0) << endl;
    out << "% Total energy (J): " << getTotalPower(0) << endl;
    out << "% \tDynamic energy (J): " << getDynamicPower(0) << endl;
    out << "% \tStatic energy (J): " << getStaticPower(0) << endl;

    if (GlobalParams::show_buffer_stats)
	{
		showBufferStats(out, 0);
		showBufferStats(out, 1);
	}
      

}

void GlobalStats::updatePowerBreakDown(map<string,double> &dst,PowerBreakdown* src)
{
    for (int i=0;i!=src->size;i++)
    {
		dst[src->breakdown[i].label]+=src->breakdown[i].value;
    }
}

void GlobalStats::showPowerManagerStats(std::ostream & out)
{
    std::streamsize p = out.precision();
    int total_cycles = sc_time_stamp().to_double() / GlobalParams::clock_period_ps - GlobalParams::reset_time;

    out.precision(4);

    out << "powermanager_stats_tx = [" << endl;
    out << "%\tFraction of: TX Transceiver off (TTXoff), AntennaBufferTX off (ABTXoff) " << endl;
    out << "%\tHUB\tTTXoff\tABTXoff\t" << endl;

    for (map<int, HubConfig>::iterator it = GlobalParams::hub_configuration.begin();
            it != GlobalParams::hub_configuration.end();
            ++it)
    {
	int hub_id = it->first;

	map<int,Hub*>::const_iterator i = noc->hub.find(hub_id);
	Hub * h = i->second;

	out << "\t" << hub_id << "\t" << std::fixed << (double)h->total_ttxoff_cycles/total_cycles << "\t";

	int s = 0;
	for (map<int,int>::iterator i = h->abtxoff_cycles.begin(); i!=h->abtxoff_cycles.end();i++) s+=i->second;

	out << (double)s/h->abtxoff_cycles.size()/total_cycles << endl;
    }

    out << "];" << endl;



    out << "powermanager_stats_rx = [" << endl;
    out << "%\tFraction of: RX Transceiver off (TRXoff), AntennaBufferRX off (ABRXoff), BufferToTile off (BTToff) " << endl;
    out << "%\tHUB\tTRXoff\tABRXoff\tBTToff\t" << endl;



    for (map<int, HubConfig>::iterator it = GlobalParams::hub_configuration.begin();
            it != GlobalParams::hub_configuration.end();
            ++it)
    {
	string bttoff_str;

	out.precision(4);

	int hub_id = it->first;

	map<int,Hub*>::const_iterator i = noc->hub.find(hub_id);
	Hub * h = i->second;

	out << "\t" << hub_id << "\t" << std::fixed << (double)h->total_sleep_cycles/total_cycles << "\t";

	int s = 0;
	for (map<int,int>::iterator i = h->buffer_rx_sleep_cycles.begin();
		i!=h->buffer_rx_sleep_cycles.end();i++)
	    s+=i->second;

	out << (double)s/h->buffer_rx_sleep_cycles.size()/total_cycles << "\t";

	s = 0;
	for (map<int,int>::iterator i = h->buffer_to_tile_poweroff_cycles.begin();
		i!=h->buffer_to_tile_poweroff_cycles.end();i++)
	{
	    double bttoff_fraction = i->second/(double)total_cycles;
	    s+=i->second;
	    if (bttoff_fraction<0.25)
		bttoff_str+=" ";
	    else if (bttoff_fraction<0.5)
		    bttoff_str+=".";
	    else if (bttoff_fraction<0.75)
		    bttoff_str+="o";
	    else if (bttoff_fraction<0.90)
		    bttoff_str+="O";
	    else 
		bttoff_str+="0";
	    

	}
	out << (double)s/h->buffer_to_tile_poweroff_cycles.size()/total_cycles << "\t" << bttoff_str << endl;
    }

    out << "];" << endl;

    out.unsetf(std::ios::fixed);

    out.precision(p);

}

void GlobalStats::showPowerBreakDown(std::ostream & out, int isReqt)
{
    map<string,double> power_dynamic;
    map<string,double> power_static;

    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
	    for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
	    {
			if (isReqt)
			{
				updatePowerBreakDown(power_dynamic, noc->t[x][y]->r->power.getDynamicPowerBreakDown());
				updatePowerBreakDown(power_static, noc->t[x][y]->r->power.getStaticPowerBreakDown());
			}
			else
			{
				updatePowerBreakDown(power_dynamic, noc->t[x][y]->dr->power.getDynamicPowerBreakDown());
				updatePowerBreakDown(power_static, noc->t[x][y]->dr->power.getStaticPowerBreakDown());
			}
	    }
    }
    else // other delta topologies
    {
	for (int y = 0; y < GlobalParams::n_delta_tiles; y++)
	{
	    updatePowerBreakDown(power_dynamic, noc->core[y]->r->power.getDynamicPowerBreakDown());
	    updatePowerBreakDown(power_static, noc->core[y]->r->power.getStaticPowerBreakDown());
	}
    }

    for (map<int, HubConfig>::iterator it = GlobalParams::hub_configuration.begin();
	    it != GlobalParams::hub_configuration.end();
	    ++it)
    {
	int hub_id = it->first;

	map<int,Hub*>::const_iterator i = noc->hub.find(hub_id);
	Hub * h = i->second;

	updatePowerBreakDown(power_dynamic, 
		h->power.getDynamicPowerBreakDown());

	updatePowerBreakDown(power_static, 
		h->power.getStaticPowerBreakDown());
    }

    printMap("power_dynamic",power_dynamic,out);
    printMap("power_static",power_static,out);

}



void GlobalStats::showBufferStats(std::ostream & out, int isReqt)
{
  out << "Router id\tBuffer N\t\tBuffer E\t\tBuffer S\t\tBuffer W\t\tBuffer L" << endl;
  out << "         \tMean\tMax\tMean\tMax\tMean\tMax\tMean\tMax\tMean\tMax" << endl;
  
  if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
    	for (int y = 0; y < GlobalParams::mesh_dim_y; y++)
    	for (int x = 0; x < GlobalParams::mesh_dim_x; x++)
      	{
			if (isReqt)
			{
				out << "Reqt NoC: " << noc->t[x][y]->r->local_id;
				noc->t[x][y]->r->ShowBuffersStats(out);
				out << endl;
			}
			else
			{
				out << "Data NoC: " << noc->t[x][y]->dr->local_id;
				noc->t[x][y]->dr->ShowBuffersStats(out);
				out << endl;
			}
     	}
    }
    else // other delta topologies
    {
    	for (int y = 0; y < GlobalParams::n_delta_tiles; y++)
    	{
			out << noc->core[y]->r->local_id;
			noc->core[y]->r->ShowBuffersStats(out);
			out << endl;
     	}
    }

}

double GlobalStats::getReceivedIdealFlitRatio(int isReqt)
{
    int total_cycles;
    total_cycles= GlobalParams::simulation_time - GlobalParams::stats_warm_up_time;
    double ratio;
    if (GlobalParams::topology == TOPOLOGY_MESH) 
    {
	ratio = getReceivedFlits(isReqt) /(GlobalParams::packet_injection_rate * (GlobalParams::min_packet_size +
		    GlobalParams::max_packet_size)/2 * total_cycles * GlobalParams::mesh_dim_y * GlobalParams::mesh_dim_x);
    }
    else // other delta topologies
    {
	ratio = getReceivedFlits(isReqt) /(GlobalParams::packet_injection_rate * (GlobalParams::min_packet_size +
		    GlobalParams::max_packet_size)/2 * total_cycles * GlobalParams::n_delta_tiles);
    }
    return ratio;
}
