/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the router
 */

#ifndef __NOXIMDATAROUTER_H__
#define __NOXIMDATAROUTER_H__

#include <systemc.h>
#include "DataStructs.h"
#include "DataBuffer.h"
#include "Stats.h"
#include "GlobalRoutingTable.h"
#include "LocalRoutingTable.h"
#include "ReservationTable.h"
#include "Utils.h"
#include "routingAlgorithms/RoutingAlgorithm.h"
#include "routingAlgorithms/RoutingAlgorithms.h"
#include "selectionStrategies/SelectionStrategy.h"
#include "selectionStrategies/SelectionStrategy.h"
#include "selectionStrategies/Selection_NOP.h"
#include "selectionStrategies/Selection_BUFFER_LEVEL.h"

using namespace std;

extern unsigned int drained_volume;

SC_MODULE(DataRouter)
{
    friend class Selection_NOP;
    friend class Selection_BUFFER_LEVEL;

    sc_in_clk clock;
    sc_in <bool> reset;

    sc_in < DataFlit > flit_rx[ DIRECTIONS + 1 ];
    sc_in < bool > req_rx[ DIRECTIONS + 1 ];
    sc_out < bool > ack_rx[ DIRECTIONS + 1 ];
    sc_out < TBufferFullStatus > buffer_full_status_rx[ DIRECTIONS + 1 ];

    sc_out < DataFlit > flit_tx[ DIRECTIONS + 1 ];
    sc_out < bool > req_tx[ DIRECTIONS + 1 ];
    sc_in < bool> ack_tx[ DIRECTIONS + 1 ];
    sc_in < TBufferFullStatus > buffer_full_status_tx[ DIRECTIONS + 1 ];

    sc_out < int > free_slots[ DIRECTIONS + 1 ];
    sc_in < int > free_slots_neighbor[ DIRECTIONS + 1 ];

    sc_out < NoP_data > NoP_data_out[ DIRECTIONS ];
    sc_in < NoP_data > NoP_data_in[ DIRECTIONS ];

    int local_id;
    int routing_type;
    int selection_type;
    DataBufferBank buffer[ DIRECTIONS + 1 ];
    bool current_level_rx[ DIRECTIONS + 1 ];
    bool current_level_tx[ DIRECTIONS + 1 ];
    Stats stats;
    Power power;
    LocalRoutingTable routing_table;
    ReservationTable reservation_table;
    unsigned long routed_flits;
    RoutingAlgorithm * routingAlgorithm; 
    SelectionStrategy * selectionStrategy; 

    void process();
    void rxProcess();
    void txProcess();
    void perCycleUpdate();
    void configure(const int _id, const double _warm_up_time,
		   const unsigned int _max_buffer_size,
		   GlobalRoutingTable & grt);
    unsigned long getRoutedFlits();


    SC_CTOR(DataRouter) {
        SC_METHOD(process);
        sensitive << reset;
        sensitive << clock.pos();

        SC_METHOD(perCycleUpdate);
        sensitive << reset;
        sensitive << clock.pos();

        routingAlgorithm = RoutingAlgorithms::get(GlobalParams::routing_algorithm);

        if (routingAlgorithm == 0)
        {
            cerr << " FATAL: invalid routing -routing " << GlobalParams::routing_algorithm << ", check with noxim -help" << endl;
            exit(-1);
        }

        selectionStrategy = SelectionStrategies::get(GlobalParams::selection_strategy);

        if (selectionStrategy == 0)
        {
            cerr << " FATAL: invalid selection strategy -sel " << GlobalParams::selection_strategy << ", check with noxim -help" << endl;
            exit(-1);
        }
    }

  private:

    int route(const RouteData & route_data);
    int selectionFunction(const vector <int> &directions,
			  const RouteData & route_data);
    vector < int >routingFunction(const RouteData & route_data);
    NoP_data getCurrentNoPData();
    void NoP_report() const;
    int NoPScore(const NoP_data & nop_data, const vector <int> & nop_channels) const;
    int reflexDirection(int direction) const;
    int getNeighborId(int _id, int direction) const;
   
    int start_from_port;
    int start_from_vc[ DIRECTIONS+1 ];

  public:

    unsigned int local_drained;
    bool inCongestion();
    void ShowBuffersStats(std::ostream & out);
};

#endif
