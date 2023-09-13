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
#include "GlobalDependcyTableNIC.h"
#include "Utils.h"

//! Calculate the size of the packet in runtime
//! packet flit 1~16
//! flit size 1~8 words
//* ready, valid, ack (3-bit) and reserved (29-bit)
//* src id(uint32_t), dst id(uint32_t), packet id(uint32_t), packet_num(uint32_t), tensor_id(uint32_t), packet_size(uint32_t)
//! 7*int = 7 word
//* Payload: addr(uint64_t), req_type(uint32_t), flit_word_num(uint32_t) *16
//* DataPayload: data(int*8) *16
//! 16*(uint64_t + 10*int) = 192 word
//! Total: 199 word = 6368 bit
#define SHM_SIZE 8192
#define CHECKREADY(p)               ((*p >> 31) & 0b1)
#define CHECKVALID(p)               ((*p >> 30) & 0b1)
#define CHECKACK(p)                 ((*p >> 29) & 0b1)
#define GETSRC_ID(p)                (*(p + 1))
#define GETDST_ID(p)                (*(p + 2))
#define GETPACKET_ID(p)             (*(p + 3))
#define GETPACKET_NUM(p)            (*(p + 4))
#define GETTENSOR_ID(p)             (*(p + 5))
#define GETPACKET_SIZE(p)           (*(p + 6))
#define GETADDR(p, index)           (static_cast<uint64_t>(*(p + 7 + 12*index)))
#define GETREQ_TYPE(p, index)       (*(p + 8 + 12*index))
#define GETFLIT_WORD_NUM(p, index)  (*(p + 9 + 12*index))
#define GETDATA(p, index, pos)      (static_cast<int>(*(p + 10 + pos + 12*index)))

#define NEW2D(H, W, TYPE) (TYPE **)new2d(H, W, sizeof(TYPE))

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
    int get_req_count;
    int add_traffic_count;
    vector < CommunicationNIC > traffic_table_NIC;
    int getTrafficSize();
    void getPacketinCommunication(const int src_id, CommunicationNIC* comm);
    void addTraffic(uint32_t src_id, uint32_t dst_id, uint32_t packet_id, uint32_t tensor_id,uint64_t* addr,uint32_t* req_type,uint32_t* flit_word_num,int** data, int packet_size);
    GlobalDependcyTableNIC* dependcy_table_NIC;
    sc_mutex req_mutex;
    sc_mutex data_mutex;
    sc_mutex dataBuffer_mutex;
    sc_mutex reqBuffer_mutex;
    sc_mutex packetQueue_mutex;
    sc_mutex dataPacketQueue_mutex;
    FILE * _log_w;
    FILE * _log_r;
    FILE * _log_receive;
    int _packet_count;
    int transcation_count;
    int sendback_count;
    time_t t;
    //* Note that packet will be push into vector only when TAIL flit is received
    vector < Packet > received_packets;	// Received packets
    vector < Packet > received_datapackets;  // Received datapackets
    vector < Packet > packetBuffers; // Received packets without TAIL in buffer
    vector < Packet > dataPacketBuffers; // Received datapackets without TAIL in buffer
    void checkNoCPackets(); // Iterate over the data packet to find the matching packet of the front of request packet
    void runCoalescingUnit(Packet *packet, Packet *data_packet); // Iterate through packet to sorting which flit should be sent (If read req)
    void checkCachePackets();
    void transaction(Packet req_packet, Packet data_packet); // Send packets to the IPC channel
    void* new2d(int h, int w, int size);
    //* In order to sent request to IPC channel
    void setIPC_Ready(uint32_t *ptr);
    void setIPC_Valid(uint32_t *ptr);
    void setIPC_Ack(uint32_t *ptr);
    void resetIPC_Ready(uint32_t *ptr);
    void resetIPC_Valid(uint32_t *ptr);
    void resetIPC_Ack(uint32_t *ptr);
    void setIPC_Data(uint32_t *ptr, int data, int const_pos, int index);
    void setIPC_Addr(uint32_t *ptr, uint64_t data, int index);
    //* Handle the sent back packet
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
        _packet_count = 0;
        sendback_count = 0;
        
        SC_THREAD(checkCachePackets);
        sensitive << reset;
        sensitive << clock.pos();

        SC_THREAD(checkNoCPackets);
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
