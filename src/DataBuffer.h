/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the declaration of the buffer
 */

#ifndef __NOXIMDATABUFFER_H__
#define __NOXIMDATABUFFER_H__

#include <cassert>
#include <queue>
#include "DataStructs.h"
using namespace std;

class DataBuffer {

  public:

    DataBuffer();

    virtual ~ DataBuffer() {
    } void SetMaxBufferSize(const unsigned int bms);	// Set buffer max size (in flits)

    unsigned int GetMaxBufferSize() const;	// Get max buffer size

    unsigned int getCurrentFreeSlots() const;	// free buffer slots

    bool IsFull() const;	// Returns true if buffer is full

    bool IsEmpty() const;	// Returns true if buffer is empty

    virtual void Drop(const DataFlit & flit) const;	// Called by Push() when buffer is full

    virtual void Empty() const;	// Called by Pop() when buffer is empty

    void Push(const DataFlit & flit);	// Push a flit. Calls Drop method if buffer is full

    DataFlit Pop();		// Pop a flit

    DataFlit Front() const;	// Return a copy of the first flit in the buffer

    unsigned int Size() const;

    void ShowStats(std::ostream & out);

    void Disable();


    void Print();
    
    bool deadlockFree();
    void deadlockCheck();


    void setLabel(string);
    string getLabel() const;

  private:

    bool true_buffer;
    bool deadlock_detected;

    int full_cycles_counter;
    int last_front_flit_seq;

    string label;

    unsigned int max_buffer_size;

    queue < DataFlit > buffer;

    unsigned int max_occupancy;
    double hold_time, last_event, hold_time_sum;
    double mean_occupancy;
    int    previous_occupancy;
    
    void SaveOccupancyAndTime();
    void UpdateMeanOccupancy();
};

typedef DataBuffer DataBufferBank[MAX_VIRTUAL_CHANNELS];


#endif