/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the processing element
 */

#ifndef __NOXIMCacheNIC_H__
#define __NOXIMCacheNIC_H__

#include <fcntl.h>  // for O_* constants
#include <string.h>
#include <sys/mman.h>  // for POSIX shared memory API
#include <sys/stat.h>  // for mode constants
#include <unistd.h>    // ftruncate
#include <queue>
#include <systemc.h>
#include <iostream>
#include <string.h>
#include <cstring>
#include <string>
#include <stdint.h>
#include <limits.h>
#include <ctime>
#include <math.h>
#include <iomanip>
#include <stdio.h>
#include <cmath>
#include <stdlib.h>
#include <vector>
#include <pthread.h>
#include <semaphore.h>

#include "DataStructs.h"
#include "GlobalTrafficTableNIC.h"
#include "Utils.h"

//! Calculate the size of the packet in runtime
#define SHM_SIZE 512
//! packet flit 1~16
//! flit size 1~8 words
// src_id(32) | dst_id(32) | packet_id(32) | req(32*2) | data(32*8) | read(32) | request_size(32) | READY(1) | VALID(1) | ACK(1) | FINISH(1)
//* Total 482 bits
#define CHECKREADY(p) (*(p + 15) >> 31)
#define CHECKVALID(p) ((*(p + 15) >> 30) & 0b1)
#define CHECKACK(p) ((*(p + 15) >> 29) & 0b1)
#define CHECKREADY(p) (*(p + 15) >> 31)
#define CHECKVALID(p) ((*(p + 15) >> 30) & 0b1)
#define CHECKACK(p) ((*(p + 15) >> 29) & 0b1)
#define CHECKFINISH(p) ((*(p + 15) >> 28) & 0b1)
#define GETSRC_ID(p) (*p)
#define GETDST_ID(p) (*(p + 1))
#define GETPACKET_ID(p) (*(p + 2))
#define GETREQ(p, pos) (*(p + 3 + pos))
#define GETDATA(p, pos) (*(p + 5 + pos))
#define GETREAD(p) (*(p + 13))
#define GETREQUEST_SIZE(p) (*(p + 14))
#define GETTEST(p,pos) (*(p + pos))

using namespace std;

SC_MODULE(CacheNIC)
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
    // Registers
    //! Modified
    sc_mutex req_mutex;
    sc_mutex data_mutex;
    sc_mutex dataBuffer_mutex;
    sc_mutex reqBuffer_mutex;
    FILE * _log_w;
    FILE * _log_r;
    FILE * _log_receive;
    int transcation_count;
    time_t t;
    //* Note that packet will be push into vector only when TAIL flit is received
    vector < Packet > received_packets;	// Received packets
    vector < Packet > received_datapackets;  // Received datapackets
    vector < Packet > packetBuffers; // Received packets without TAIL in buffer
    vector < Packet > dataPacketBuffers; // Received datapackets without TAIL in buffer
    void checkNoCPackets(); // Iterate over the data packet to find the matching packet of the front of request packet
    void checkCachePackets();
    void transaction(Packet req_packet, Packet data_packet); // Send packets to the IPC channel
    //* In order to sent request to IPC channel
    void setIPC_Data(uint32_t *ptr, uint32_t data, int const_pos, int varied_pos);
    void setIPC_Valid(uint32_t *ptr);
    void resetIPC_Valid(uint32_t *ptr);
    void setIPC_Ready(uint32_t *ptr);
    void resetIPC_Ready(uint32_t *ptr);
    void setIPC_Ack(uint32_t *ptr);
    void resetIPC_Ack(uint32_t *ptr);
    void setIPC_Finish(uint32_t *ptr);
    void addPacketToTraffic();
    //! 
    int local_id;		// Unique identification number
    bool current_level_rx;	// Current level for Alternating Bit Protocol (ABP)
    bool current_level_tx;	// Current level for Alternating Bit Protocol (ABP)
    queue < Packet > packet_queue;	// Local queue of packets
    bool transmittedAtPreviousCycle;	// Used for distributions with memory

    // Functions
    void rxProcess();		// The receiving process
    void txProcess();		// The transmitting process
    bool canShot(Packet & packet, int isReqt);	// True when the packet must be shot
    Flit nextFlit();	// Take the next flit of the current packet
    Packet trafficTest(int isReqt);	// used for testing traffic
    Packet trafficRandom();	// Random destination distribution
    Packet trafficTranspose1();	// Transpose 1 destination distribution
    Packet trafficTranspose2();	// Transpose 2 destination distribution
    Packet trafficBitReversal();	// Bit-reversal destination distribution
    Packet trafficShuffle();	// Shuffle destination distribution
    Packet trafficButterfly();	// Butterfly destination distribution
    Packet trafficLocal();	// Random with locality

    GlobalTrafficTableNIC *traffic_table_NIC;	// Reference to the Global traffic Table
    bool never_transmit;	// true if the PE does not transmit any packet 
    //  (valid only for the table based traffic)

    void fixRanges(const Coord, Coord &);	// Fix the ranges of the destination
    int randInt(int min, int max);	// Extracts a random integer number between min and max
    int getRandomSize();	// Returns a random size in flits for the packet
    void setBit(int &x, int w, int v);
    int getBit(int x, int w);
    double log2ceil(double x);

    int findRandomDestination(int local_id,int hops);
    unsigned int getQueueSize() const;

    unsigned int getDataQueueSize() const;
    // Constructor
    SC_CTOR(CacheNIC) {
        srand((unsigned int) time(&t));
        std::string str_w = "log_w.txt";
        std::string str_r = "log_r.txt";
        std::string str_re = "log_receive.txt";
        const char * name_w = str_w.c_str();
        const char * name_r = str_r.c_str();
        const char * name_re = str_re.c_str();
        _log_r = fopen(name_w, "a");
        _log_w = fopen(name_r, "a");
        _log_receive = fopen(name_re, "a");
        transcation_count = 0;
        
        SC_CTHREAD(checkCachePackets, clock.pos());
        SC_CTHREAD(checkNoCPackets, clock.pos());

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
