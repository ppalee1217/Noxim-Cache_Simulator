/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the router
 */

#include "DataRouter.h"

inline int toggleKthBit(int n, int k)
{
	return (n ^ (1 << (k-1)));
}

void DataRouter::process()
{
    txProcess();
    rxProcess();
}

void DataRouter::rxProcess()
{
    if (reset.read()) 
    {
        TBufferFullStatus bfs;

        for (int i = 0; i < DIRECTIONS + 1; i++) 
        {
            ack_rx[i].write(0);
            current_level_rx[i] = 0;
            buffer_full_status_rx[i].write(bfs);
        }
        routed_flits = 0;
        local_drained = 0;
    } 
    else 
    { 
        for (int i = 0; i < DIRECTIONS + 1; i++) 
        {
            if (req_rx[i].read() == 1 - current_level_rx[i])
            { 
                DataFlit received_flit = flit_rx[i].read();
                int vc = received_flit.vc_id;

                if (!buffer[i][vc].IsFull()) 
                {
                    buffer[i][vc].Push(received_flit);
                    // LOG << " DataFlit " << received_flit << " collected from Input[" << i << "][" << vc <<"]" << endl << endl;

                    power.bufferRouterPush();
                    current_level_rx[i] = 1 - current_level_rx[i];

                    if (received_flit.src_id == local_id)
                        power.networkInterface();
                }
                else
                {
                    // LOG << " DataFlit " << received_flit << " buffer full Input[" << i << "][" << vc <<"]" << endl << endl;
                    assert(i== DIRECTION_LOCAL);
                }
            }
            ack_rx[i].write(current_level_rx[i]);
            TBufferFullStatus bfs;
            for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
                bfs.mask[vc] = buffer[i][vc].IsFull();
            buffer_full_status_rx[i].write(bfs);
        }
    }
}

void DataRouter::txProcess()
{
    if (reset.read()) 
    {
        for (int i = 0; i < DIRECTIONS + 1; i++) 
	    {
            req_tx[i].write(0);
            current_level_tx[i] = 0;
	    }
    } 
    else 
    {
		for (int j = 0; j < DIRECTIONS + 1; j++) 
		{
			int i = (start_from_port + j) % (DIRECTIONS + 1);
			for (int k = 0;k < GlobalParams::n_virtual_channels; k++)
			{
				int vc = (start_from_vc[i]+k)%(GlobalParams::n_virtual_channels);
				if (!buffer[i][vc].IsEmpty()) 
				{
					DataFlit flit = buffer[i][vc].Front();
					power.bufferRouterFront();

					if (flit.flit_type == FLIT_TYPE_HEAD) 
					{
						RouteData route_data;
						route_data.current_id = local_id;
						route_data.src_id = flit.src_id;
						route_data.dst_id = flit.dst_id;
						route_data.dir_in = i;
						route_data.vc_id = flit.vc_id;

						int o = route(route_data);

						if (o>=DIRECTION_HUB_RELAY)
						{
							DataFlit f = buffer[i][vc].Pop();
							// f.hub_relay_node = o-DIRECTION_HUB_RELAY;
							buffer[i][vc].Push(f);
							o = DIRECTION_HUB;
						}

						TReservation r;
						r.input = i;
						r.vc = vc;

						// LOG << " checking availability of Output[" << o << "] for Input[" << i << "][" << vc << "] flit " << flit << endl;

						int rt_status = reservation_table.checkReservation(r,o);

						if (rt_status == RT_AVAILABLE) 
						{
                            // LOG << " reserving direction " << o << " for flit " << flit << endl;
                            reservation_table.reserve(r, o);
						}
						else if (rt_status == RT_ALREADY_SAME)
						{
						    // LOG << " RT_ALREADY_SAME reserved direction " << o << " for flit " << flit << endl;
						}
						else if (rt_status == RT_OUTVC_BUSY)
						{
						    // LOG << " RT_OUTVC_BUSY reservation direction " << o << " for flit " << flit << endl;
						}
						else if (rt_status == RT_ALREADY_OTHER_OUT)
						{
						    // LOG  << "RT_ALREADY_OTHER_OUT: another output previously reserved for the same flit " << endl;
						}
						else assert(false); // no meaningful status here
					}
				}
			}
			start_from_vc[i] = (start_from_vc[i]+1)%GlobalParams::n_virtual_channels;
		}

		start_from_port = (start_from_port + 1) % (DIRECTIONS + 1);

		for (int i = 0; i < DIRECTIONS + 1; i++) 
		{ 
			vector<pair<int,int> > reservations = reservation_table.getReservations(i);
			if (reservations.size()!=0)
			{
				int rnd_idx = rand()%reservations.size();

				int o = reservations[rnd_idx].first;
				int vc = reservations[rnd_idx].second;
				if (!buffer[i][vc].IsEmpty())  
				{
				    DataFlit flit = buffer[i][vc].Front();
                    if ( (current_level_tx[o] == ack_tx[o].read()) &&
                        (buffer_full_status_tx[o].read().mask[vc] == false) ) 
                    {
                        // LOG << "Input[" << i << "][" << vc << "] forwarded to Output[" << o << "], flit: " << flit << endl;

                        flit_tx[o].write(flit);
                        current_level_tx[o] = 1 - current_level_tx[o];
                        req_tx[o].write(current_level_tx[o]);
                        buffer[i][vc].Pop();

                        if (flit.flit_type == FLIT_TYPE_TAIL)
                        {
                            TReservation r;
                            r.input = i;
                            r.vc = vc;
                            reservation_table.release(r,o);
                        }

                        /* Power & Stats ------------------------------------------------- */
                        if (o == DIRECTION_HUB) power.r2hLink();
                        else power.r2rLink();

                        power.bufferRouterPop();
                        power.crossBar();

                        if (o == DIRECTION_LOCAL) 
                        {
                            power.networkInterface();
                            // LOG << "Consumed flit " << flit << endl;
                            stats.receivedFlit(sc_time_stamp().to_double() / GlobalParams::clock_period_ps, flit);
                            if (GlobalParams:: max_volume_to_be_drained) 
                            {
                                if (drained_volume >= GlobalParams:: max_volume_to_be_drained)
                                sc_stop();
                                else 
                                {
                                    drained_volume++;
                                    local_drained++;
                                }
                            }
                        } 	
					    else if (i != DIRECTION_LOCAL) // not generated locally
						    routed_flits++;
					    /* End Power & Stats ------------------------------------------------- */
		  		    }
                    else
                    {
                        // LOG << " Cannot forward Input[" << i << "][" << vc << "] to Output[" << o << "], flit: " << flit << endl;
                        //LOG << " **DEBUG APB: current_level_tx: " << current_level_tx[o] << " ack_tx: " << ack_tx[o].read() << endl;
                        // LOG << " **DEBUG buffer_full_status_tx " << buffer_full_status_tx[o].read().mask[vc] << endl;

                        //LOG<<"END_NO_cl_tx="<<current_level_tx[o]<<"_req_tx="<<req_tx[o].read()<<" _ack= "<<ack_tx[o].read()<< endl;
                        /*
                        if (flit.flit_type == FLIT_TYPE_HEAD)
                        reservation_table.release(i,flit.vc_id,o);
                        */
                    }
	            }
	        }
        }
        if ((int)(sc_time_stamp().to_double() / GlobalParams::clock_period_ps)%2==0)
	        reservation_table.updateIndex();
    }   
}

NoP_data DataRouter::getCurrentNoPData()
{
    NoP_data NoP_data;

    for (int j = 0; j < DIRECTIONS; j++) 
    {
        try 
        {
            NoP_data.channel_status_neighbor[j].free_slots = free_slots_neighbor[j].read();
            NoP_data.channel_status_neighbor[j].available = (reservation_table.isNotReserved(j));
        }
        catch (int e)
        {
            if (e!=NOT_VALID) assert(false);
        };
    }

    NoP_data.sender_id = local_id;
    return NoP_data;
}

void DataRouter::perCycleUpdate()
{
    if (reset.read()) 
    {
        for (int i = 0; i < DIRECTIONS + 1; i++)
            free_slots[i].write(buffer[i][DEFAULT_VC].GetMaxBufferSize());
    } 
    else 
    {
        selectionStrategy->perCycleUpdate(this);
	    power.leakageRouter();
        for (int i = 0; i < DIRECTIONS + 1; i++)
        {
            for (int vc=0;vc<GlobalParams::n_virtual_channels;vc++)
            {
                power.leakageBufferRouter();
                power.leakageLinkRouter2Router();
            }
        }

	    power.leakageLinkRouter2Hub();
    }
}

vector < int > DataRouter::routingFunction(const RouteData & route_data)
{
	if (GlobalParams::verbose_mode > VERBOSE_OFF)
		LOG << "Wired routing for dst = " << route_data.dst_id << endl;

	return routingAlgorithm->route(this, route_data);
}

int DataRouter::route(const RouteData & route_data)
{

    if (route_data.dst_id == local_id)
	    return DIRECTION_LOCAL;

    power.routing();
    vector < int >candidate_channels = routingFunction(route_data);

    power.selection();
    return selectionFunction(candidate_channels, route_data);
}

void DataRouter::NoP_report() const
{
    NoP_data NoP_tmp;
	// LOG << "NoP report: " << endl;

    for (int i = 0; i < DIRECTIONS; i++)
    {
        NoP_tmp = NoP_data_in[i].read();
        if (NoP_tmp.sender_id != NOT_VALID)
            cout << NoP_tmp;
    }
}

//---------------------------------------------------------------------------

int DataRouter::NoPScore(const NoP_data & nop_data,
			  const vector < int >&nop_channels) const
{
    int score = 0;

    for (unsigned int i = 0; i < nop_channels.size(); i++) 
    {
        int available;

        if (nop_data.channel_status_neighbor[nop_channels[i]].available)
            available = 1;
        else
            available = 0;

        int free_slots =
            nop_data.channel_status_neighbor[nop_channels[i]].free_slots;

        score += available * free_slots;
    }

    return score;
}

int DataRouter::selectionFunction(const vector < int >&directions,
				   const RouteData & route_data)
{
    if (directions.size() == 1)
	return directions[0];

    return selectionStrategy->apply(this, directions, route_data);
}

void DataRouter::configure(const int _id,
			    const double _warm_up_time,
			    const unsigned int _max_buffer_size,
			    GlobalRoutingTable & grt)
{
    local_id = _id;
    stats.configure(_id, _warm_up_time);

    start_from_port = DIRECTION_LOCAL;
  
    if (grt.isValid())
	routing_table.configure(grt, _id);

    reservation_table.setSize(DIRECTIONS+1);

    for (int i = 0; i < DIRECTIONS + 1; i++)
    {
        for (int vc = 0; vc < GlobalParams::n_virtual_channels; vc++)
        {
            buffer[i][vc].SetMaxBufferSize(_max_buffer_size);
            buffer[i][vc].setLabel(string(name())+"->buffer["+i_to_string(i)+"]");
        }
        start_from_vc[i] = 0;
    }


    if (GlobalParams::topology == TOPOLOGY_MESH)
    {
        int row = _id / GlobalParams::mesh_dim_x;
        int col = _id % GlobalParams::mesh_dim_x;

        for (int vc = 0; vc<GlobalParams::n_virtual_channels; vc++)
        {
            if (row == 0)
                buffer[DIRECTION_NORTH][vc].Disable();
            if (row == GlobalParams::mesh_dim_y-1)
                buffer[DIRECTION_SOUTH][vc].Disable();
            if (col == 0)
                buffer[DIRECTION_WEST][vc].Disable();
            if (col == GlobalParams::mesh_dim_x-1)
                buffer[DIRECTION_EAST][vc].Disable();
        }
    }

}

unsigned long DataRouter::getRoutedFlits()
{
    return routed_flits;
}


int DataRouter::reflexDirection(int direction) const
{
    if (direction == DIRECTION_NORTH)
	    return DIRECTION_SOUTH;
    if (direction == DIRECTION_EAST)
	    return DIRECTION_WEST;
    if (direction == DIRECTION_WEST)
	    return DIRECTION_EAST;
    if (direction == DIRECTION_SOUTH)
	    return DIRECTION_NORTH;

    // you shouldn't be here
    assert(false);
    return NOT_VALID;
}

int DataRouter::getNeighborId(int _id, int direction) const
{
    assert(GlobalParams::topology == TOPOLOGY_MESH);

    Coord my_coord = id2Coord(_id); 

    switch (direction) {
    case DIRECTION_NORTH:
	if (my_coord.y == 0)
	    return NOT_VALID;
	my_coord.y--;
	break;
    case DIRECTION_SOUTH:
	if (my_coord.y == GlobalParams::mesh_dim_y - 1)
	    return NOT_VALID;
	my_coord.y++;
	break;
    case DIRECTION_EAST:
	if (my_coord.x == GlobalParams::mesh_dim_x - 1)
	    return NOT_VALID;
	my_coord.x++;
	break;
    case DIRECTION_WEST:
	if (my_coord.x == 0)
	    return NOT_VALID;
	my_coord.x--;
	break;
    default:
	// LOG << "Direction not valid : " << direction;
	assert(false);
    }

    int neighbor_id = coord2Id(my_coord);

    return neighbor_id;
}

bool DataRouter::inCongestion()
{
    for (int i = 0; i < DIRECTIONS; i++) {

	if (free_slots_neighbor[i]==NOT_VALID) continue;

	int flits = GlobalParams::buffer_depth - free_slots_neighbor[i];
	if (flits > (int) (GlobalParams::buffer_depth * GlobalParams::dyad_threshold))
	    return true;
    }

    return false;
}

void DataRouter::ShowBuffersStats(std::ostream & out)
{
  for (int i=0; i<DIRECTIONS+1; i++)
      for (int vc=0; vc<GlobalParams::n_virtual_channels;vc++)
	    buffer[i][vc].ShowStats(out);
}