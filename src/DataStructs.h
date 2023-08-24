/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the top-level of Noxim
 */

#ifndef _DATASTRUCS_H__
#define _DATASTRUCS_H__

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <systemc.h>
#include "GlobalParams.h"

#define FLIT_DATA_SIZE 256
// Coord -- XY coordinates type of the Tile inside the Mesh
class Coord
{
public:
    int x; // X coordinate
    int y; // Y coordinate

    inline bool operator==(const Coord &coord) const
    {
        return (coord.x == x && coord.y == y);
    }
};

// FlitType -- Flit type enumeration
enum FlitType
{
    FLIT_TYPE_HEAD,
    FLIT_TYPE_BODY,
    FLIT_TYPE_TAIL
};

// Payload -- Payload definition
struct Payload
{
    uint64_t addr; // Bus for the data to be exchanged
    int read; // Read or Write packet (Only work when transmitting data to/from memory)
    uint32_t request_size; // Request size (When read = true, request size should be specified)
    inline bool operator==(const Payload &payload) const
    {
        return (payload.addr == addr);
    }
};

// DataPayload -- DataPayload definition
struct DataPayload
{
    uint32_t data[8]; // Bus for the data to be exchanged
    inline bool operator==(const DataPayload &data_payload) const
    {
        return (
            data_payload.data[0] == data[0] &&
            data_payload.data[1] == data[1] &&
            data_payload.data[2] == data[2] &&
            data_payload.data[3] == data[3] &&
            data_payload.data[4] == data[4] &&
            data_payload.data[5] == data[5] &&
            data_payload.data[6] == data[6] &&
            data_payload.data[7] == data[7]
            );
    }
};

// Structure used to store information into the table
struct Communication {
    int src;			  // ID of the source node (PE)
    //! dst should be -1 if the packet is transmitted to the cache
    int dst;			  // ID of the destination node (PE)
    double pir;			// Packet Injection Rate for the link
    double por;			// Probability Of Retransmission for the link
    int t_on;			  // Time (in cycles) at which activity begins
    int t_off;			// Time (in cycles) at which activity ends
    int t_period;   // Period after which activity starts again
    //! Noxim <-> Cache modified
    int tensor_size;     // Read (request data size) / Write (write data size)
    int req_type;
    uint64_t req_addr;
    bool used;
    //! For AIonChip HW sync
    int tensor_id;
    int depend_tensor_num;
    int depend_tenosr_id_1;
    int depend_tenosr_id_2;
    int depend_tenosr_id_3;
    int depend_tenosr_id_4;
};

struct tensorDependcyTable {
  string tensor_name;
  bool return_flag;
};

// Packet -- Packet definition
struct Packet
{
    int src_id;
    int dst_id;
    int vc_id;
    //! Modified
    //* Packet size = flit number(1~16) => The packet size will be specified in packet generation
    //* Flit size = 1~8 words? or 8 words?
    //* Flit should have req_addr(uint64_t), req_type(bool), req_size(int)
    //* DataFlit should have req_data(uint32_t*)
    //! Should flit size be specified? (Is its size fixed?)
    //! If it is fixed, what happend when R/W data in flit will exceed a cache line?
    //! e.g., if we writes 8 word at middle of a cache line(suppose 8 words per cache line)
    //! Different address mapping method may cause different extent of Non-contiguous memory addresses
    //! Coalesing unit should be implemented to help to merge the flits into one packet to same Cache NIC
    uint32_t packet_id;      // The packet ID
    vector <Payload> payload; // To store payload(req) for each flit
    vector <DataPayload> data_payload; // To store payload(data) for each flit
    //!
    double timestamp; // SC timestamp at packet generation
    int size;   // flit size
    int flit_left; // Number of remaining flits inside the packet
    bool use_low_voltage_path;
    // Constructors
    Packet() {}

    Packet(const int s, const int d, const int vc, const double ts, const int sz, const int r, const uint64_t* addr, const uint32_t** data, const int req_sz, const int isReqt, const uint32_t pkt_id)
    {
        make(s, d, vc, ts, sz, r, addr, data, fin, req_sz, isReqt, pkt_id);
    }

    void make(const int s, const int d, const int vc, const double ts, const int sz, const int r, const uint64_t* addr, const uint32_t** data, const int req_sz, const int isReqt, const uint32_t pkt_id)
    {
        //TODO: Add coalescing unit in this function
        src_id = s;
        dst_id = d;
        vc_id = vc;
        size = sz;  // Packet size = flit number(1~16)
        flit_left = sz;
        timestamp = ts;
        use_low_voltage_path = false;
        //! Modified
        packet_id = pkt_id;
        payload.clear();
        data_payload.clear();
        for(int i=0;i<sz;i++){
            if(isReqt){
                Payload p;
                p.addr = addr[i];
                if(r){
                    p.request_size = req_sz;
                    p.read = 1;
                }
                else{
                    p.request_size = (FLIT_DATA_SIZE/32);
                    p.read = 0;
                }
                payload.push_back(p);
            }
            else{
                DataPayload p;
                for(int j=0; j< (FLIT_DATA_SIZE/32); j++)
                    p.data[j] = data[i][j];
                data_payload.push_back(p);
            }
        }
    }
};

// RouteData -- data required to perform routing
struct RouteData
{
    int current_id;
    int src_id;
    int dst_id;
    int dir_in; // direction from which the packet comes from
    int vc_id;
};

struct ChannelStatus
{
    int free_slots; // occupied buffer slots
    bool available; //
    inline bool operator==(const ChannelStatus &bs) const
    {
        return (free_slots == bs.free_slots && available == bs.available);
    };
};

// NoP_data -- NoP Data definition
struct NoP_data
{
    int sender_id;
    ChannelStatus channel_status_neighbor[DIRECTIONS];

    inline bool operator==(const NoP_data &nop_data) const
    {
        return (sender_id == nop_data.sender_id &&
                nop_data.channel_status_neighbor[0] ==
                    channel_status_neighbor[0] &&
                nop_data.channel_status_neighbor[1] ==
                    channel_status_neighbor[1] &&
                nop_data.channel_status_neighbor[2] ==
                    channel_status_neighbor[2] &&
                nop_data.channel_status_neighbor[3] ==
                    channel_status_neighbor[3]);
    };
};

struct TBufferFullStatus
{
    TBufferFullStatus()
    {
        for (int i = 0; i < MAX_VIRTUAL_CHANNELS; i++)
            mask[i] = false;
    };
    inline bool operator==(const TBufferFullStatus &bfs) const
    {
        for (int i = 0; i < MAX_VIRTUAL_CHANNELS; i++)
            if (mask[i] != bfs.mask[i])
                return false;
        return true;
    };

    bool mask[MAX_VIRTUAL_CHANNELS];
};

// Flit -- Flit definition
struct Flit
{
    int src_id;
    int dst_id;
    int vc_id;          // Virtual Channel
    //! Modified
    uint32_t packet_id;      // The packet ID
    //!
    FlitType flit_type; // The flit type (FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL)
    int sequence_no;    // The sequence number of the flit inside the packet
    int sequence_length;
    Payload payload;  // Optional payload
    double timestamp; // Unix timestamp at packet generation
    int hop_no;       // Current number of hops from source to destination
    bool use_low_voltage_path;

    int hub_relay_node;

    inline bool operator==(const Flit &flit) const
    {
        return (
            flit.src_id == src_id &&
            flit.dst_id == dst_id &&
            flit.flit_type == flit_type &&
            flit.vc_id == vc_id &&
            flit.sequence_no == sequence_no &&
            flit.sequence_length == sequence_length &&
            flit.payload == payload &&
            flit.timestamp == timestamp &&
            flit.hop_no == hop_no &&
            flit.use_low_voltage_path == use_low_voltage_path
        );
    }
};

// Struct for Data NoC - AddDate: 2023/04/29
struct DataFlit
{
    int src_id;
    int dst_id;
    int vc_id;
    //! Modified
    uint32_t packet_id;      // The packet ID
    DataPayload data_payload;
    //!
    FlitType flit_type;
    int sequence_no;
    int sequence_length;
    double timestamp;

    inline bool operator==(const DataFlit &flit) const
    {
        return (
            flit.src_id == src_id &&
            flit.dst_id == dst_id &&
            flit.vc_id == vc_id &&
            flit.sequence_no == sequence_no &&
            flit.sequence_length == sequence_length &&
            flit.data_payload == data_payload &&
            flit.timestamp == timestamp);
    }
};

typedef struct
{
    string label;
    double value;
} PowerBreakdownEntry;

enum
{
    BUFFER_PUSH_PWR_D,
    BUFFER_POP_PWR_D,
    BUFFER_FRONT_PWR_D,
    BUFFER_TO_TILE_PUSH_PWR_D,
    BUFFER_TO_TILE_POP_PWR_D,
    BUFFER_TO_TILE_FRONT_PWR_D,
    BUFFER_FROM_TILE_PUSH_PWR_D,
    BUFFER_FROM_TILE_POP_PWR_D,
    BUFFER_FROM_TILE_FRONT_PWR_D,
    ANTENNA_BUFFER_PUSH_PWR_D,
    ANTENNA_BUFFER_POP_PWR_D,
    ANTENNA_BUFFER_FRONT_PWR_D,
    ROUTING_PWR_D,
    SELECTION_PWR_D,
    CROSSBAR_PWR_D,
    LINK_R2R_PWR_D,
    LINK_R2H_PWR_D,
    NI_PWR_D,
    WIRELESS_TX,
    WIRELESS_DYNAMIC_RX_PWR,
    WIRELESS_SNOOPING,
    NO_BREAKDOWN_ENTRIES_D
};

enum
{
    TRANSCEIVER_RX_PWR_BIASING,
    TRANSCEIVER_TX_PWR_BIASING,
    BUFFER_ROUTER_PWR_S,
    BUFFER_TO_TILE_PWR_S,
    BUFFER_FROM_TILE_PWR_S,
    ANTENNA_BUFFER_PWR_S,
    LINK_R2H_PWR_S,
    ROUTING_PWR_S,
    SELECTION_PWR_S,
    CROSSBAR_PWR_S,
    NI_PWR_S,
    TRANSCEIVER_RX_PWR_S,
    TRANSCEIVER_TX_PWR_S,
    NO_BREAKDOWN_ENTRIES_S
};

typedef struct
{
    int size;
    PowerBreakdownEntry breakdown[NO_BREAKDOWN_ENTRIES_D + NO_BREAKDOWN_ENTRIES_S];
} PowerBreakdown;

#endif
