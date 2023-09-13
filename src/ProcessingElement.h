/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the processing element
 */

#ifndef __NOXIMPROCESSINGELEMENT_H__
#define __NOXIMPROCESSINGELEMENT_H__

#include <queue>
#include <systemc.h>
#include <iostream>
#include <string.h>
#include <cstring>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
#include <ctime>
#include <math.h>

#include "DataStructs.h"
#include "GlobalTrafficTable.h"
#include "Utils.h"

#define NEW2D(H, W, TYPE) (TYPE **)new2d(H, W, sizeof(TYPE))

using namespace std;

SC_MODULE(ProcessingElement)
{

    // I/O Ports
    sc_in_clk clock;		// The input clock for the PE
    sc_in < bool > reset;	// The reset signal for the PE

    sc_in < Flit > flit_rx;	// The input channel
    sc_in < bool > req_rx;	// The request associated with the input channel
    sc_out < bool > ack_rx;	// The outgoing ack signal associated with the input channel
    sc_out < TBufferFullStatus > buffer_full_status_rx;

    sc_out < Flit > flit_tx;	// The output channel
    sc_out < bool > req_tx;	// The request associated with the output channel
    sc_in < bool > ack_tx;	// The outgoing ack signal associated with the output channel
    sc_in < TBufferFullStatus > buffer_full_status_tx;

    sc_in < int > free_slots_neighbor;
    
    // For data NoC - AddDate: 2023/04/29
    sc_in < DataFlit > dataflit_rx;
    sc_in < bool > datareq_rx;
    sc_out < bool > dataack_rx;
    sc_out < TBufferFullStatus > databuffer_full_status_rx;

    sc_out < DataFlit > dataflit_tx;
    sc_out < bool > datareq_tx;
    sc_in < bool > dataack_tx;
    sc_in < TBufferFullStatus > databuffer_full_status_tx;

    sc_in < int > datafree_slots_neighbor;

    bool datacurrent_level_rx, datacurrent_level_tx;
    queue < Packet > datapacket_queue;
    bool datatransmittedAtPreviousCycle;
    void datarxProcess();
    void datatxProcess();
    DataFlit nextDataFlit();

    //! Modified
    int calculatePacketNum(Communication comm, int used_cache_line_num, vector <Payload>* tmpPayloadForEachNIC, vector <DataPayload>* tmpDataPayloadForEachNIC, bool req_type, int flit_num, uint64_t* addr);
    void transCommToPacket(Communication comm);
    void checkSentBackPacket();
    void costFunction();	// The cost function
    void addWaitTime();
    void reducePacketNum(int tensor_id);
    vector <tensorDependcy> tensorDependcyTable;
    vector < Packet > received_packets;	// Received packets
    vector < Packet > received_datapackets;  // Received datapackets
    vector < Packet > packetBuffers; // Received packets without TAIL in buffer
    vector < Packet > dataPacketBuffers; // Received datapackets without TAIL in buffer
    sc_mutex req_mutex;
    sc_mutex data_mutex;
    sc_mutex dataBuffer_mutex;
    sc_mutex reqBuffer_mutex;
    bool wait_read_flag;	// Wait for all required tensor data to be read and sent back
    bool enable_output;	// Enable output
    time_t t;
    bool start_counting_cycle;
    bool output_compute_fin;
    int cycle_count;
    //!
    // Registers
    int local_id;		// Unique identification number
    bool current_level_rx;	// Current level for Alternating Bit Protocol (ABP)
    bool current_level_tx;	// Current level for Alternating Bit Protocol (ABP)
    queue < Packet > packet_queue;	// Local queue of packets
    bool transmittedAtPreviousCycle;	// Used for distributions with memory

    // Functions
    void rxProcess();		// The receiving process
    void txProcess();		// The transmitting process
    bool canShot(Packet *packet, int isReqt);	// True when the packet must be shot
    Flit nextFlit();	// Take the next flit of the current packet
    Packet trafficTest(int isReqt);	// used for testing traffic
    Packet trafficRandom();	// Random destination distribution
    Packet trafficTranspose1();	// Transpose 1 destination distribution
    Packet trafficTranspose2();	// Transpose 2 destination distribution
    Packet trafficBitReversal();	// Bit-reversal destination distribution
    Packet trafficShuffle();	// Shuffle destination distribution
    Packet trafficButterfly();	// Butterfly destination distribution
    Packet trafficLocal();	// Random with locality
    Packet trafficULocal();	// Random with locality

    GlobalTrafficTable *traffic_table;	// Reference to the Global traffic Table
    bool never_transmit;	// true if the PE does not transmit any packet 
    //  (valid only for the table based traffic)

    void fixRanges(const Coord, Coord &);	// Fix the ranges of the destination
    int randInt(int min, int max);	// Extracts a random integer number between min and max
    int getRandomSize();	// Returns a random size in flits for the packet
    void setBit(int &x, int w, int v);
    int getBit(int x, int w);
    double log2ceil(double x);
    void* new2d(int h, int w, int size);
    int roulett();
    int findRandomDestination(int local_id,int hops);
    unsigned int getQueueSize() const;

    unsigned int getDataQueueSize() const;

    // Constructor
    SC_CTOR(ProcessingElement) {
        srand((unsigned int) time(&t));

        SC_THREAD(addWaitTime);
        sensitive << reset;
        sensitive << clock.pos();

        SC_THREAD(checkSentBackPacket);
        sensitive << reset;
        sensitive << clock.pos();

        SC_THREAD(costFunction);
        sensitive << reset;
        sensitive << clock.pos();

        SC_METHOD(rxProcess);
        sensitive << reset;
        sensitive << clock.pos();

        SC_METHOD(txProcess);
        sensitive << reset;
        sensitive << clock.pos();

        SC_METHOD(datarxProcess);
        sensitive << reset;
        sensitive << clock.pos();

        SC_METHOD(datatxProcess);
        sensitive << reset;
        sensitive << clock.pos();
    }
};

#endif
