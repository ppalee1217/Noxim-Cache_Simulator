/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the buffer
 */

#include "DataBuffer.h"
#include "Utils.h"

DataBuffer::DataBuffer()
{
  SetMaxBufferSize(GlobalParams::buffer_depth);
  max_occupancy = 0;
  hold_time = 0.0;
  last_event = 0.0;
  hold_time_sum = 0.0;
  previous_occupancy = 0;
  mean_occupancy = 0.0;
  true_buffer = true;
  full_cycles_counter = 0;
  last_front_flit_seq = NOT_VALID;
  deadlock_detected = false;
}


void DataBuffer::setLabel(string l)
{
    //cout << "\n BUFFER LABEL: " << l << endl;
    label = l;
}

string DataBuffer::getLabel() const
{
    return label;
}

void DataBuffer::Print()
{
    queue< DataFlit > m = buffer;

    string bstr = "";
   

    char  t[] = "HBT";

    cout << sc_time_stamp().to_double() / GlobalParams::clock_period_ps << "\t";
    cout << label << " QUEUE *[";
    while (!(m.empty()))
    {
	DataFlit f = m.front();
	m.pop();
	cout << bstr << t[f.flit_type] << f.sequence_no <<  "(" << f.dst_id << ") | ";
    }
    cout << "]*" << endl;
    cout << endl;
}


void DataBuffer::deadlockCheck()
{
    // TOOD: add as parameter
    int check_threshold = 50000;

    if (IsEmpty()) return;

    DataFlit f = buffer.front();
    int seq = f.sequence_no;

    if (last_front_flit_seq==seq)
    {
	full_cycles_counter++;
    }
    else
    {
	if (deadlock_detected) 
	{
	    cout << " WRONG DEADLOCK detection, please increase the check_threshold " << endl;
	    assert(false);
	}
	last_front_flit_seq = seq;
	full_cycles_counter=0;
    }

    if (full_cycles_counter>check_threshold && !deadlock_detected) 
    {
	double current_time = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
	cout << "WARNING: DEADLOCK DETECTED at cycle " << current_time << " in buffer:  " << getLabel() << endl;
	deadlock_detected = true;
    }
}


bool DataBuffer::deadlockFree()
{
    if (IsEmpty()) return true;

    DataFlit f = buffer.front();
    
    int seq = f.sequence_no;


    if (last_front_flit_seq==seq)
    {
	full_cycles_counter++;
    }
    else
    {
	last_front_flit_seq = seq;
	full_cycles_counter=0;
    }

    if (full_cycles_counter>50000) 
    {
	return false;
    }

    return true;

}

void DataBuffer::Disable()
{
  true_buffer = false;
}

void DataBuffer::SetMaxBufferSize(const unsigned int bms)
{
  assert(bms > 0);

  max_buffer_size = bms;
}

unsigned int DataBuffer::GetMaxBufferSize() const
{
  return max_buffer_size;
}

bool DataBuffer::IsFull() const
{
  return buffer.size() == max_buffer_size;
}

bool DataBuffer::IsEmpty() const
{
  return buffer.size() == 0;
}

void DataBuffer::Drop(const DataFlit & flit) const
{
  assert(false);
}

void DataBuffer::Empty() const
{
  assert(false);
}

void DataBuffer::Push(const DataFlit & flit)
{
  SaveOccupancyAndTime();

  if (IsFull())
    Drop(flit);
  else
    buffer.push(flit);
  
  UpdateMeanOccupancy();

  if (max_occupancy < buffer.size())
    max_occupancy = buffer.size();
}

DataFlit DataBuffer::Pop()
{
  DataFlit f;

  SaveOccupancyAndTime();

  if (IsEmpty())
    Empty();
  else {
    f = buffer.front();
    buffer.pop();
  }

  UpdateMeanOccupancy();

  return f;
}

DataFlit DataBuffer::Front() const
{
  DataFlit f;

  if (IsEmpty())
    Empty();
  else
    f = buffer.front();

  return f;
}

unsigned int DataBuffer::Size() const
{
  return buffer.size();
}

unsigned int DataBuffer::getCurrentFreeSlots() const
{
  return (GetMaxBufferSize() - Size());
}

void DataBuffer::SaveOccupancyAndTime()
{
  previous_occupancy = buffer.size();
  hold_time = (sc_time_stamp().to_double() / GlobalParams::clock_period_ps) - last_event;
  last_event = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
}

void DataBuffer::UpdateMeanOccupancy()
{
  double current_time = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
  if (current_time - GlobalParams::reset_time < GlobalParams::stats_warm_up_time)
    return;

  mean_occupancy = mean_occupancy * (hold_time_sum/(hold_time_sum+hold_time)) +
    (1.0/(hold_time_sum+hold_time)) * hold_time * buffer.size();

  hold_time_sum += hold_time;
}

void DataBuffer::ShowStats(std::ostream & out)
{
  if (true_buffer)
    out << "\t" << mean_occupancy << "\t" << max_occupancy;
  else
    out << "\t\t";
}
