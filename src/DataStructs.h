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
    uint64_t addr;      // Address of the data to be exchanged
    int read;           // Read or Write packet (Only work when transmitting data to/from memory)
    int flit_word_num;  // Store flit size for each flit (i.e., the word number this flit requests/contains)
    inline bool operator==(const Payload &payload) const
    {
        return (payload.addr == addr);
    }
};

// DataPayload -- DataPayload definition
struct DataPayload
{
    int data[8]; // Data to be exchanged
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
struct CommunicationNIC {
    int src;			  // ID of the source node (PE)
    int dst;			  // ID of the destination node (PE)
    int packet_id;
    int tensor_id;
    vector <uint64_t> addr;
    vector <int> req_type;
    vector <int> flit_word_num;
    vector <vector<int>> data;
    double pir;			// Packet Injection Rate for the link
    double por;			// Probability Of Retransmission for the link
    int t_on;			  // Time (in cycles) at which activity begins
    int t_off;			// Time (in cycles) at which activity ends
    int t_period;   // Period after which activity starts again
    bool used;
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
    bool isOutput;
    int tensor_id;
    int depend_tensor_num;
    vector <int> depend_tensor_id;
};

struct tensorDependcy {
  int tensor_id;
  int packet_count;
  bool return_flag;
};

struct tensorDependcyNIC {
    int tensor_id;
    int packet_count;
    int nic_id;
    bool return_flag;
};

// Packet -- Packet definition
struct Packet
{
    int src_id;
    int dst_id;
    int vc_id;
    //! Modified
    //* flit size: 1~8 words
    //* packet number: 1~16
    int packet_num; // Info Cache/PE that how many packets will be issued for this request(tensor)
    uint32_t packet_id;      // The packet ID
    int tensor_id;
    vector <Payload> payload; // To store payload(req) for each flit
    vector <DataPayload> data_payload; // To store payload(data) for each flit
    vector <int> depend_tensor_id;
    //!
    double timestamp;   // SC timestamp at packet generation
    int size;           // flit size(Should contain header flit)
    int flit_left;      // Number of remaining flits inside the packet
    bool use_low_voltage_path;
    // Constructors
    Packet() {}

    Packet(const int s, const int d, const int vc, const double ts, const int sz, const int r, const int _tensor_id, const uint64_t* addr, int** data, const int* req_wd_num, const int isReqt, const uint32_t pkt_id, const vector <int> _depend_tensor_id, int pkt_num)
    {
        make(s, d, vc, ts, sz, r, _tensor_id, addr, data, req_wd_num, isReqt, pkt_id, _depend_tensor_id, pkt_num);
    }

    void make(const int s, const int d, const int vc, const double ts, const int sz, const int r, const int _tensor_id, const uint64_t* addr, int** data, const int* req_wd_num, const int isReqt, const uint32_t pkt_id, const vector <int> _depend_tensor_id, int pkt_num)
    {
        //TODO: Add coalescing unit in this function
        src_id = s;
        dst_id = d;
        vc_id = vc;
        size = (sz+2);  //* Packet size = flit number(1~16), plus header flit
        flit_left = (sz+2);
        timestamp = ts;
        use_low_voltage_path = false;
        //! Modified
        // if(_tensor_id>0){
        //     printf("PE %d --> PE%d tensor_id: %d\n",s , d ,_tensor_id);
        // }
        tensor_id = _tensor_id;
        packet_num = pkt_num;
        packet_id = pkt_id;
        payload.clear();
        data_payload.clear();
        depend_tensor_id.clear();
        depend_tensor_id = _depend_tensor_id;
        for(int i=0;i<sz;i++){
            if(isReqt){
                Payload p;
                p.addr = addr[i];
                p.flit_word_num = req_wd_num[i];
                (r) ? p.read = 1 : p.read = 0;
                payload.push_back(p);
            }
            else{
                DataPayload p;
                for(int j=0; j< req_wd_num[i]; j++){
                    if(data != NULL){
                        p.data[j] = data[i][j];
                    }
                    else
                        p.data[j] = 0;
                }
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
    int packet_num;     // Info Cache/PE that how many packets will be issued for this request(tensor)
    vector <int> depend_tensor_id;
    int tensor_id;
    //!
    Payload payload;  // Optional payload
    FlitType flit_type; // The flit type (FLIT_TYPE_HEAD, FLIT_TYPE_BODY, FLIT_TYPE_TAIL)
    int sequence_no;    // The sequence number of the flit inside the packet
    int sequence_length;
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
            flit.timestamp == timestamp
            );
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
