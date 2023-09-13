/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the processing element
 */

#include "ProcessingElement.h"

void* ProcessingElement::new2d(int h, int w, int size)
{
    int i;
    void **p;

    p = (void**)new char[h*sizeof(void*) + h*w*size];
    for(i = 0; i < h; i++)
        p[i] = ((char *)(p + h)) + i*w*size;

    return p;
}

int ProcessingElement::randInt(int min, int max)
{
    return min +
           (int)((double)(max - min + 1) * rand() / (RAND_MAX + 1.0));
}

void ProcessingElement::rxProcess()
{
    if (reset.read())
    {
        ack_rx.write(0);
        current_level_rx = 0;
        packetBuffers.clear();
    }
    else
    {
        if (req_rx.read() == 1 - current_level_rx)
        {
            Flit flit_tmp = flit_rx.read();
            current_level_rx = 1 - current_level_rx; // Negate the old value for Alternating Bit Protocol (ABP)
            if (flit_tmp.flit_type == FLIT_TYPE_HEAD){
                Packet packet_tmp;
                packet_tmp.src_id = flit_tmp.src_id;
                packet_tmp.dst_id = flit_tmp.dst_id;
                packet_tmp.vc_id = flit_tmp.vc_id;
                packet_tmp.packet_num = flit_tmp.packet_num;
                packet_tmp.packet_id = flit_tmp.packet_id;
                packet_tmp.tensor_id = flit_tmp.tensor_id;
                packet_tmp.depend_tensor_id = flit_tmp.depend_tensor_id;
                packet_tmp.timestamp = flit_tmp.timestamp;
                packet_tmp.payload.clear();
                reqBuffer_mutex.lock();
                packetBuffers.push_back(packet_tmp);
                reqBuffer_mutex.unlock();
                // printf("PE %d received a sendback req packet from NIC %d\n", local_id, (flit_tmp.src_id%(int)log2(PE_NUM)));
            }
            if (flit_tmp.flit_type == FLIT_TYPE_BODY){
                reqBuffer_mutex.lock();
                for(int i=0;i<packetBuffers.size();i++){
                    if(packetBuffers[i].packet_id == flit_tmp.packet_id){
                        packetBuffers[i].payload.push_back(flit_tmp.payload);
                        break;
                    }
                }
                reqBuffer_mutex.unlock();
            }
            if (flit_tmp.flit_type == FLIT_TYPE_TAIL){
                reqBuffer_mutex.lock();
                for(int i=0;i<packetBuffers.size();i++){
                    if(packetBuffers[i].packet_id == flit_tmp.packet_id){
                        req_mutex.lock();
                        received_packets.push_back(packetBuffers[i]);
                        req_mutex.unlock();
                        packetBuffers.erase(packetBuffers.begin()+i);
                    }
                }
                reqBuffer_mutex.unlock();
                int tensor_id = flit_tmp.tensor_id;
            }
        }
        ack_rx.write(current_level_rx);
    }
}

void ProcessingElement::txProcess()
{
    if (reset.read())
    {
        req_tx.write(0);
        current_level_tx = 0;
        transmittedAtPreviousCycle = false;
    }
    else
    {
        if(GlobalParams::traffic_distribution == TRAFFIC_TABLE_BASED){
            if (canShot(NULL, 0)){
                transmittedAtPreviousCycle = true;
                datatransmittedAtPreviousCycle = true;
            }
            else{
                transmittedAtPreviousCycle = false;
                datatransmittedAtPreviousCycle = false;
            }
        }
        else{
            Packet packet;
            if (canShot(&packet, 1))
            {
                packet_queue.push(packet);
                transmittedAtPreviousCycle = true;
                    
            }
            else{
                transmittedAtPreviousCycle = false;
            }
        }

        if (ack_tx.read() == current_level_tx)
        {
            if (!packet_queue.empty())
            {
                // printf("Sent packet out!\n");
                Flit flit = nextFlit();                  // Generate a new flit
                flit_tx->write(flit);                    // Send the generated flit
                current_level_tx = 1 - current_level_tx; // Negate the old value for Alternating Bit Protocol (ABP)
                req_tx.write(current_level_tx);
            }
        }
    }
}

Flit ProcessingElement::nextFlit()
{
    Flit flit;
    Packet packet = packet_queue.front();

    flit.src_id = packet.src_id;
    flit.dst_id = packet.dst_id;
    flit.vc_id = packet.vc_id;
    //! Modified
    flit.packet_num = packet.packet_num;
    flit.depend_tensor_id = packet.depend_tensor_id;
    flit.packet_id = packet.packet_id;
    flit.tensor_id = packet.tensor_id;
    //!
    flit.timestamp = packet.timestamp;
    flit.sequence_no = packet.size - packet.flit_left;
    flit.sequence_length = packet.size;
    flit.hop_no = 0;
    if (packet.size == packet.flit_left)
        flit.flit_type = FLIT_TYPE_HEAD;
    else if (packet.flit_left == 1)
        flit.flit_type = FLIT_TYPE_TAIL;
    else
        flit.flit_type = FLIT_TYPE_BODY;
    if(packet.size == packet.flit_left || packet.flit_left == 1){
        flit.payload = packet.payload[0];
    }
    else{
        flit.payload = packet.payload[flit.sequence_no-1];
    }
    // cout << "==========" << endl;
    // cout << "PE " << std::dec << local_id << " sent a request flit to PE " << flit.dst_id << endl;
    // cout << "Flit type: " << flit.flit_type << endl;
    // cout << "Sequence number: " << flit.sequence_no << endl;
    // cout << "Addr: 0x" << std::hex << flit.payload.addr << std::dec << endl;
    // cout << "==========" << endl;

    flit.hub_relay_node = NOT_VALID;


    packet_queue.front().flit_left--;
    if (packet_queue.front().flit_left == 0)
        packet_queue.pop();

    return flit;
}

void ProcessingElement::datarxProcess()
{
    if (reset.read())
    {
        dataack_rx.write(0);
        datacurrent_level_rx = 0;
        dataPacketBuffers.clear();
    }
    else
    {
        if (datareq_rx.read() == 1 - datacurrent_level_rx)
        {
            DataFlit flit_tmp = dataflit_rx.read();
            datacurrent_level_rx = 1 - datacurrent_level_rx;
            if (flit_tmp.flit_type == FLIT_TYPE_HEAD){
                Packet packet_tmp;
                packet_tmp.src_id = flit_tmp.src_id;
                packet_tmp.dst_id = flit_tmp.dst_id;
                packet_tmp.vc_id = flit_tmp.vc_id;
                packet_tmp.packet_id = flit_tmp.packet_id;
                packet_tmp.timestamp = flit_tmp.timestamp;
                packet_tmp.data_payload.clear();
                dataBuffer_mutex.lock();
                dataPacketBuffers.push_back(packet_tmp);
                dataBuffer_mutex.unlock();
            }
            if (flit_tmp.flit_type == FLIT_TYPE_BODY){
                dataBuffer_mutex.lock();
                for(int i=0;i<dataPacketBuffers.size();i++){
                    if(dataPacketBuffers[i].packet_id == flit_tmp.packet_id){
                        dataPacketBuffers[i].data_payload.push_back(flit_tmp.data_payload);
                        break;
                    }
                }
                dataBuffer_mutex.unlock();
            }
            if (flit_tmp.flit_type == FLIT_TYPE_TAIL){
                dataBuffer_mutex.lock();
                for(int i=0;i<dataPacketBuffers.size();i++){
                    if(dataPacketBuffers[i].packet_id == flit_tmp.packet_id){
                        received_datapackets.push_back(dataPacketBuffers[i]);
                        data_mutex.lock();
                        dataPacketBuffers.erase(dataPacketBuffers.begin()+i);
                        data_mutex.unlock();
                    }
                }
                dataBuffer_mutex.unlock();
            }
            dataack_rx.write(datacurrent_level_rx);
        }
    }
}

void ProcessingElement::datatxProcess()
{
    if (reset.read())
    {
        datareq_tx.write(0);
        datacurrent_level_tx = 0;
        datatransmittedAtPreviousCycle = false;
    }
    else
    {
        Packet packet;

        if(GlobalParams::traffic_distribution != TRAFFIC_TABLE_BASED){
            if (canShot(&packet, 0))
            {
                datapacket_queue.push(packet);
                datatransmittedAtPreviousCycle = true;
            }
            else
                datatransmittedAtPreviousCycle = false;
        }

        if (dataack_tx.read() == datacurrent_level_tx)
        {
            if (!datapacket_queue.empty())
            {
                DataFlit flit = nextDataFlit();
                dataflit_tx->write(flit);
                datacurrent_level_tx = 1 - datacurrent_level_tx;
                datareq_tx.write(datacurrent_level_tx);
            }
        }
    }
}

DataFlit ProcessingElement::nextDataFlit()
{
    DataFlit flit;
    Packet packet = datapacket_queue.front();

    flit.src_id = packet.src_id;
    flit.dst_id = packet.dst_id;
    flit.vc_id = packet.vc_id;
    //! Modified
    flit.packet_id = packet.packet_id;
    //!
    flit.timestamp = packet.timestamp;
    flit.sequence_no = packet.size - packet.flit_left;
    flit.sequence_length = packet.size;
    if (packet.size == packet.flit_left)
        flit.flit_type = FLIT_TYPE_HEAD;
    else if (packet.flit_left == 1)
        flit.flit_type = FLIT_TYPE_TAIL;
    else
        flit.flit_type = FLIT_TYPE_BODY;
    if(packet.size == packet.flit_left || packet.flit_left == 1){
        flit.data_payload = packet.data_payload[0];
    }
    else{
        flit.data_payload = packet.data_payload[flit.sequence_no-1];
    }
    // cout << "==========" << endl;
    // cout << "PE " << std::dec << local_id << " sent a data flit to PE " << flit.dst_id << endl;
    // cout << "Flit type: " << flit.flit_type << endl;
    // cout << "Sequence number: " << flit.sequence_no << endl;
    // cout << "Sent data: 0x" << std::hex << flit.payload.data << std::dec << endl;
    // cout << "==========" << endl;

    datapacket_queue.front().flit_left--;

    if (datapacket_queue.front().flit_left == 0)
        datapacket_queue.pop();

    return flit;
}

bool ProcessingElement::canShot(Packet *packet, int isReqt)
{
    if (never_transmit)
        return false;

#ifdef DEADLOCK_AVOIDANCE
    if (local_id % 2 == 0)
        return false;
#endif
    bool shot;
    double threshold;

    double now = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;

    if (GlobalParams::traffic_distribution != TRAFFIC_TABLE_BASED)
    {
        if (!transmittedAtPreviousCycle)
            threshold = GlobalParams::packet_injection_rate;
        else
            threshold = GlobalParams::probability_of_retransmission;
        shot = (((double)rand()) / RAND_MAX < threshold) && (local_id == 0);
        if (shot)
        {
            if (GlobalParams::traffic_distribution == TRAFFIC_RANDOM)
                *packet = trafficRandom();
            else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE1)
                *packet = trafficTranspose1();
            else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE2)
                *packet = trafficTranspose2();
            else if (GlobalParams::traffic_distribution == TRAFFIC_BIT_REVERSAL)
                *packet = trafficBitReversal();
            else if (GlobalParams::traffic_distribution == TRAFFIC_SHUFFLE)
                *packet = trafficShuffle();
            else if (GlobalParams::traffic_distribution == TRAFFIC_BUTTERFLY)
                *packet = trafficButterfly();
            else if (GlobalParams::traffic_distribution == TRAFFIC_LOCAL)
                *packet = trafficLocal();
            else if (GlobalParams::traffic_distribution == TRAFFIC_ULOCAL)
                *packet = trafficULocal();
            else if (GlobalParams::traffic_distribution == TRAFFIC_TEST)
                *packet = trafficTest(isReqt);
            else
            {
                cout << "Invalid traffic distribution: " << GlobalParams::traffic_distribution << endl;
                exit(-1);
            }
        }
    }
    else
    {
        // Table based communication traffic
        if (never_transmit)
            return false;
        else{
            if(wait_read_flag)
                return false;
            Communication comm = traffic_table->getPacketinCommunication(local_id, enable_output);
            if(comm.isOutput && !enable_output){
            //* The first time reading output tensor, stall until all required tensor data is read and computed
                printf("PE%d read output tensor %d, stall until all required tensor data is read and computed.\n", local_id, comm.tensor_id);
                wait_read_flag = true;
                return false;
            }
            else if(comm.isOutput && enable_output)
            //* The second time reading output tensor, start counting cycle
                enable_output = false;

            if(comm.src != -1)
            {
                transCommToPacket(comm);
                shot = true;
            }
            else
                shot = false;
        }
    }
    return shot;
}

void ProcessingElement::checkSentBackPacket(){
    while(true){
        if (reset.read())
        {
            received_packets.clear();
            received_datapackets.clear();
        }
        else
        {
            // cout << "PE " << local_id << ":" << endl;
            // cout << "received packets size: " << received_packets.size() << endl;
            // cout << "received datapackets size: " << received_datapackets.size() << endl;
            // cout << "packetBuffers size: " << packetBuffers.size() << endl;
            // cout << "dataPacketBuffers size: " << dataPacketBuffers.size() << endl;
            // cout << "----------" << endl;
            // cout << endl;
            if(received_packets.size()!=0 && received_datapackets.size()!=0){
                bool break_flag = false;
                for(int j=0; j<received_packets.size();j++){
                    for(int i=0;i<received_datapackets.size();i++){
                        if(received_packets[j].packet_id == received_datapackets[i].packet_id){
                            //! Get packet from received_packets and received_datapackets
                            req_mutex.lock();
                            data_mutex.lock();
                            Packet req_packet = received_packets[j];
                            Packet data_packet = received_datapackets[i];
                            // cout << "PE " << local_id << ":" << endl;
                            // cout << "received packets size: " << received_packets.size() << endl;
                            // cout << "received datapackets size: " << received_datapackets.size() << endl;
                            // cout << "packetBuffers size: " << packetBuffers.size() << endl;
                            // cout << "dataPacketBuffers size: " << dataPacketBuffers.size() << endl;
                            // printf("Packet id %d (tensor id %d) is sent back from Tile %d (NIC%d)\n", req_packet.packet_id, req_packet.tensor_id, req_packet.src_id, (local_id)%(int)log2(PE_NUM));
                            // cout << "----------" << endl;
                            //! Erase packet from received_packets and received_datapackets
                            received_packets.erase(received_packets.begin()+j);
                            received_datapackets.erase(received_datapackets.begin()+i);
                            data_mutex.unlock();
                            req_mutex.unlock();
                            //* break flag is used to check only one packet can be sent at a time
                            break_flag = true;
                            //! Reduce packet number in dependency table
                            reducePacketNum(req_packet.tensor_id);
                            break;
                        }
                    }
                    if(break_flag)
                        break;
                }
            }
        }
        wait();
    }
}

void ProcessingElement::reducePacketNum(int tensor_id){
    for(int i = 0; i < tensorDependcyTable.size(); i++){
        if(tensorDependcyTable[i].tensor_id == tensor_id){
            if(tensorDependcyTable[i].return_flag){
                std::cerr << "The dependcy tensor " << tensor_id << " is already returned while there is still packet sent back." << std::endl;
                std::cerr << "There must be a miscalculation of this tensor. Exiting the program. " << std::endl;
                exit(EXIT_FAILURE); // Terminate the program with a failure exit code
            }
            // if(tensorDependcyTable[i].packet_count<=10)
            //     printf("(PE %d) Packet count of tensor id %d = %d\n", local_id, tensor_id, tensorDependcyTable[i].packet_count);
            tensorDependcyTable[i].packet_count--;
            if(tensorDependcyTable[i].packet_count == 0){
                tensorDependcyTable[i].return_flag = true;
                printf("Tensor id %d is returned to PE %d\n", tensor_id, local_id);
            }
            // printf("--------------\n");
        }
    }
}

void ProcessingElement::addWaitTime(){
    //! Only support 3-level layer output tensor for now
    // layer 1: 53295115776.0
    // layer 2: 2576086016.0
    // layer 3: 3058578432.0
    //* Wait time was simplified to 1/100000 of the original cycle required
    int w_tensor_count = 0;
    int wait_time[3] = {5329511,257608,305857};
    int temp = 0;
    bool _enable_output = true;
    bool notify_flag = false;
    while(true){
        if(start_counting_cycle && _enable_output){
            cycle_count++;
            int percentage = (((float)cycle_count/(float)wait_time[w_tensor_count]))*100;
            if(percentage >= (temp+25)){
                printf("(PE %d) Write tensor %d computing progress: %d%%\n", local_id, w_tensor_count, percentage);
                temp = percentage;
            }
            if(cycle_count >= wait_time[w_tensor_count]){
                notify_flag = true;
                start_counting_cycle = false;
                cycle_count = 0;
                temp = 0;
                w_tensor_count++;
                if(w_tensor_count == 3){
                    _enable_output = false;
                }
            }
        }
        if(notify_flag){
            notify_flag = false;
            output_compute_fin = true;
        }
        wait();
    }
}

void ProcessingElement::costFunction() {
    while (true) {
        if (reset.read()) {
            start_counting_cycle = false;
            wait_read_flag = false;
            enable_output = false;
            tensorDependcyTable.clear();
        } else {
            if (wait_read_flag) {
                bool all_packet_return = true;
                //* Check every tensor id in dependency table is sent back
                for (int i = 0; i < tensorDependcyTable.size(); i++) {
                    if (!tensorDependcyTable[i].return_flag) {
                        all_packet_return = false;
                        break;
                    }
                }
                if (!all_packet_return) {
                    wait();
                    continue;
                }
                // TODO: Run cost function
                // printf("PE %d starts to running cost function\n", local_id);

                // Introduce a time-specified wait of 5 seconds
                start_counting_cycle = true;
                while(!output_compute_fin){
                    wait();
                }
                output_compute_fin = false;
                if(local_id==1)
                    printf("PE %d can send output tensor now!\n", local_id);
                wait_read_flag = false;
                enable_output = true;
                wait(SC_ZERO_TIME);
                continue;
            }
            // printf("PE %d wait for data to be sent back\n", local_id);
        }
        wait();  // Wait at the end of the loop
    }
}

//TODO: Push multiple packet into queue if coalescing happens
void ProcessingElement::transCommToPacket(Communication comm){
    int used_cache_line_num = ceil((float)comm.tensor_size / (float)32);  // Total cache line that is being used in this request
    double now = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    int tensor_id = comm.tensor_id;
    if(comm.dst == -1){
        //* R/W from/to Cache
        if(comm.req_type){
            //* If read => send request to all cache NIC
            int packet_num = ceil((float)used_cache_line_num/(float)16);
            int remain_flit_num = used_cache_line_num%16;
            int NIC_num = BANK_NUM/4;
            int packet_count = 0;   // Count how many packet will be sent back
            for(int i =0;i<packet_num;i++){
                uint64_t* addr = (uint64_t*) malloc(sizeof(uint64_t)*16);
                int* request_word_num = (int*) malloc(sizeof(int)*16);
                int flit_num = 16;
                if(i == (packet_num-1) && remain_flit_num!= 0)
                    flit_num = remain_flit_num;
                for(int j=0;j<flit_num;j++){
                    //* For the last flit inside last packet, and the word number is not 8
                    if(i == (packet_num-1) && (comm.tensor_size % 32!=0) && j == (flit_num-1))
                        request_word_num[j] = ceil((float)(comm.tensor_size % 32)/(float)4);
                    else
                        request_word_num[j] = 8;
                    addr[j] = FLIT_BIT_MASK(comm.req_addr + 32*16*i + 32*j);
                }
                int tmp_packet_count = calculatePacketNum(comm, used_cache_line_num, NULL, NULL, 1, flit_num, addr);
                packet_count += tmp_packet_count;
                for(int j=0;j<NIC_num;j++){
                    //* Calculate all Cache tile id for current PE number (2x2, 4x4, ...etc)
                    int dst_id = log2(PE_NUM)*(j+1) + j;
                    uint32_t packet_id = rand();
                    int vc = randInt(0, GlobalParams::n_virtual_channels - 1);
                    Packet packet;
                    packet.make(comm.src, dst_id, vc, now, flit_num, comm.req_type, comm.tensor_id, addr, NULL, request_word_num, 1, packet_id, comm.depend_tensor_id, packet_num);
                    Packet data_packet;
                    data_packet.make(comm.src, dst_id, vc, now, flit_num, comm.req_type, comm.tensor_id, addr, NULL, request_word_num, 0, packet_id, comm.depend_tensor_id, packet_num);
                    // printf("PE %d send read packet %d (tensor id %d, packet num %d) to Cache %d\n", local_id, packet_id, comm.tensor_id, packet_num, dst_id);
                    packet_queue.push(packet);
                    datapacket_queue.push(data_packet);
                }
                free(addr);
                free(request_word_num);
            }
            printf("PE%d: tensor id %d packet count = %d\n", local_id, tensor_id, packet_count);
            tensorDependcy tmpTable;
            tmpTable.packet_count = packet_count;
            tmpTable.tensor_id = comm.tensor_id;
            tmpTable.return_flag = false;
            tensorDependcyTable.push_back(tmpTable);
            // printf("Tensor id %d is logged in dependency table (packet count %d) at PE %d\n", comm.tensor_id, packet_count, local_id);
        }
        else{
            //* If write => coalesing unit work in here to distinguish packet sent to same cache tile
            vector <Payload>* tmpPayloadForEachNIC = new vector<Payload>[BANK_NUM/4];
            vector <DataPayload>* tmpDataPayloadForEachNIC = new vector<DataPayload>[BANK_NUM/4];
            calculatePacketNum(comm, used_cache_line_num, tmpPayloadForEachNIC, tmpDataPayloadForEachNIC, 0, 0, NULL);
            for(int i=0;i<BANK_NUM/4;i++){
                int packet_num = ceil((float)tmpPayloadForEachNIC[i].size()/(float)16);
                int remain_flit_num = tmpPayloadForEachNIC[i].size()%16;
                // printf("Remain flit num for NIC %d is %d\n", i, remain_flit_num);
                for(int j=0;j<packet_num;j++){
                    uint64_t* addr = (uint64_t*) malloc(sizeof(uint64_t)*16);
                    int* request_word_num = (int*) malloc(sizeof(int)*16);
                    int** data = NEW2D(16, 8, int);
                    int flit_num = 16;
                    if(j == (packet_num-1) && remain_flit_num != 0)
                        flit_num = remain_flit_num;
                    for(int k=0;k<flit_num;k++){
                        addr[k] = tmpPayloadForEachNIC[i][16*j+k].addr;
                        request_word_num[k] = tmpPayloadForEachNIC[i][16*j+k].flit_word_num;
                        for(int l=0;l<request_word_num[k];l++)
                            data[k][l] = tmpDataPayloadForEachNIC[i][16*j+k].data[l];
                    }
                    //* Calculate all Cache tile id for current PE number (2x2, 4x4, ...etc)
                    int dst_id = log2(PE_NUM)*(i+1) + i;
                    uint32_t packet_id = rand();
                    int vc = randInt(0, GlobalParams::n_virtual_channels - 1);
                    // printf("PE %d send write packet %010d (tensor id %d, packet num %d, flit num %d) to Cache %d\n", local_id, packet_id, comm.tensor_id, packet_num, flit_num, dst_id);
                    Packet packet;
                    packet.make(comm.src, dst_id, vc, now, flit_num, comm.req_type, comm.tensor_id, addr, data, request_word_num, 1, packet_id, comm.depend_tensor_id, packet_num);
                    Packet data_packet;
                    data_packet.make(comm.src, dst_id, vc, now, flit_num, comm.req_type, comm.tensor_id, addr, data, request_word_num, 0, packet_id, comm.depend_tensor_id, packet_num);
                    packet_queue.push(packet);
                    datapacket_queue.push(data_packet);
                    free(addr);
                    free(request_word_num);
                }
                // printf("-------------\n");
            }
            delete[] tmpPayloadForEachNIC;
            delete[] tmpDataPayloadForEachNIC;
        }
    }
    else{
        //TODO: This section does not implement completely yet
        //! Only consider the scenario that PE send request to other PE without any dependency table (i.e. no dependency recorded)
        //* R/W data from/to PE
        int packet_num = ceil((float)used_cache_line_num/(float)16);
        int remain_flit_num = used_cache_line_num%16;
        for(int i =0;i<packet_num;i++){
            uint64_t* addr = (uint64_t*) malloc(sizeof(uint64_t)*16);
            int* request_word_num = (int*) malloc(sizeof(int)*16);
            int flit_num = 16;
            if(i == (packet_num-1) && remain_flit_num!= 0)
                flit_num = remain_flit_num;
            for(int j=0;j<flit_num;j++){
                //* For the last flit inside last packet, and the word number is not 8
                if(i == (packet_num-1) && (comm.tensor_size % 32!=0) && j == (flit_num-1))
                    request_word_num[j] = ceil((float)(comm.tensor_size % 32)/(float)4);
                else
                    request_word_num[j] = 8;
                addr[j] = FLIT_BIT_MASK(comm.req_addr + 32*16*i + 32*j);
            }
            //* Calculate all Cache tile id for current PE number (2x2, 4x4, ...etc)
            uint32_t packet_id = rand();
            int vc = randInt(0, GlobalParams::n_virtual_channels - 1);
            Packet packet;
            packet.make(comm.src, comm.dst, vc, now, flit_num, comm.req_type, comm.tensor_id, addr, NULL, request_word_num, 1, packet_id, comm.depend_tensor_id, packet_num);
            Packet data_packet;
            data_packet.make(comm.src, comm.dst, vc, now, flit_num, comm.req_type, comm.tensor_id, addr, NULL, request_word_num, 0, packet_id, comm.depend_tensor_id, packet_num);
            // printf("PE %d send %s packet %d to PE %d\n", local_id, (comm.req_type) ? "read" : "write", packet_id, comm.dst);
            packet_queue.push(packet);
            datapacket_queue.push(data_packet);
            free(addr);
            free(request_word_num);
        }
    }
}

int ProcessingElement::calculatePacketNum(
    Communication comm, int used_cache_line_num, vector <Payload>* tmpPayloadForEachNIC,
    vector <DataPayload>* tmpDataPayloadForEachNIC, bool req_type, int flit_num, uint64_t* _addr)
{
    unsigned int index_mask = 0;
    unsigned int cache_set_index = 0;
    int cache_num_sets;           // Number of sets
    int cache_num_offset_bits;    // Number of block bits
    int cache_num_index_bits;     // Number of cache_set_index bits
    cache_num_sets = CACHE_SIZE / (CACHE_BLOCK_SIZE * CACHE_WAYS);
    cache_num_offset_bits = log2((CACHE_BLOCK_SIZE/4));
    cache_num_index_bits = log2(cache_num_sets);
    for(int i=0;i<cache_num_index_bits;i++){
        cache_set_index = (cache_set_index << 1) + 1;
        index_mask = (index_mask << 1) + 1;
    }
    if(req_type){
        int tmp_packet_count = 0;
        int NIC_num = BANK_NUM/4;
        bool* NIC_issue_table = (bool*) malloc(sizeof(bool)*NIC_num);
        for(int i=0;i<NIC_num;i++)
            NIC_issue_table[i] = false;
        for(int i=0;i<flit_num;i++){
            cache_set_index = ((_addr[i] >> (cache_num_offset_bits+2)) & index_mask);
            if(ADDR_MAPPING_MODE == 0){
                unsigned int choose_bank = cache_set_index % BANK_NUM;
                NIC_issue_table[choose_bank/4] = true;
            }
            else if(ADDR_MAPPING_MODE == 1){
                unsigned int address_partition = ((unsigned int)-1) / BANK_NUM;
                unsigned int choose_bank = (_addr[i] / address_partition);
                NIC_issue_table[choose_bank/4] = true;
            }
        }
        for(int i=0;i<NIC_num;i++){
            if(NIC_issue_table[i])
                tmp_packet_count++;
        }
        return tmp_packet_count;
    }
    else{
        for(int i=0;i<used_cache_line_num;i++){
            Payload pld_tmp;
            DataPayload data_pld_tmp;
            int flit_word_num = 8;
            if(i==(used_cache_line_num-1) && (comm.tensor_size % 32!=0)){
                flit_word_num = ceil((float)(comm.tensor_size % 32)/(float)4);
            }
            uint64_t addr = FLIT_BIT_MASK(comm.req_addr + 32*i);
            pld_tmp.addr = addr;
            pld_tmp.read = 0;
            pld_tmp.flit_word_num = flit_word_num;
            for(int j=0;j<flit_word_num;j++){
                data_pld_tmp.data[j] = (j + addr);
            }
            cache_set_index = ((addr >> (cache_num_offset_bits+2)) & index_mask);
            if(ADDR_MAPPING_MODE == 0){
                unsigned int choose_bank = cache_set_index % BANK_NUM;
                tmpPayloadForEachNIC[choose_bank/4].push_back(pld_tmp);
                tmpDataPayloadForEachNIC[choose_bank/4].push_back(data_pld_tmp);
            }
            else if(ADDR_MAPPING_MODE == 1){
                unsigned int address_partition = ((unsigned int)-1) / BANK_NUM;
                unsigned int choose_bank = (addr / address_partition);
                tmpPayloadForEachNIC[choose_bank/4].push_back(pld_tmp);
                tmpDataPayloadForEachNIC[choose_bank/4].push_back(data_pld_tmp);
            }
        }
        return 0;
    }
}

Packet ProcessingElement::trafficLocal()
{
    Packet p;
    p.src_id = local_id;
    double rnd = rand() / (double)RAND_MAX;

    vector<int> dst_set;

    int max_id = (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y);

    for (int i = 0; i < max_id; i++)
    {
        if (rnd <= GlobalParams::locality)
        {
            if (local_id != i && sameRadioHub(local_id, i))
                dst_set.push_back(i);
        }
        else if (!sameRadioHub(local_id, i))
            dst_set.push_back(i);
    }

    int i_rnd = rand() % dst_set.size();

    p.dst_id = dst_set[i_rnd];
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();
    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);

    return p;
}

int ProcessingElement::findRandomDestination(int id, int hops)
{
    assert(GlobalParams::topology == TOPOLOGY_MESH);

    int inc_y = rand() % 2 ? -1 : 1;
    int inc_x = rand() % 2 ? -1 : 1;
    printf("Inside ProcessingElement::findRandomDestination: src id = %d\n", id);
    Coord current = id2Coord(id);

    for (int h = 0; h < hops; h++)
    {

        if (current.x == 0)
            if (inc_x < 0)
                inc_x = 0;

        if (current.x == GlobalParams::mesh_dim_x - 1)
            if (inc_x > 0)
                inc_x = 0;

        if (current.y == 0)
            if (inc_y < 0)
                inc_y = 0;

        if (current.y == GlobalParams::mesh_dim_y - 1)
            if (inc_y > 0)
                inc_y = 0;

        if (rand() % 2)
            current.x += inc_x;
        else
            current.y += inc_y;
    }
    return coord2Id(current);
}

int roulette()
{
    int slices = GlobalParams::mesh_dim_x + GlobalParams::mesh_dim_y - 2;

    double r = rand() / (double)RAND_MAX;

    for (int i = 1; i <= slices; i++)
    {
        if (r < (1 - 1 / double(2 << i)))
        {
            return i;
        }
    }
    assert(false);
    return 1;
}

Packet ProcessingElement::trafficULocal()
{
    Packet p;
    p.src_id = local_id;

    int target_hops = roulette();

    p.dst_id = findRandomDestination(local_id, target_hops);

    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();
    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);

    return p;
}

Packet ProcessingElement::trafficRandom()
{
    Packet p;
    p.src_id = local_id;
    double rnd = rand() / (double)RAND_MAX;
    double range_start = 0.0;
    int max_id;

    if (GlobalParams::topology == TOPOLOGY_MESH)
        max_id = (GlobalParams::mesh_dim_x * GlobalParams::mesh_dim_y) - 1; // Mesh
    else                                                                    // other delta topologies
        max_id = GlobalParams::n_delta_tiles - 1;

    // Random destination distribution
    do
    {
        p.dst_id = randInt(0, max_id);

        // check for hotspot destination
        for (size_t i = 0; i < GlobalParams::hotspots.size(); i++)
        {

            if (rnd >= range_start && rnd < range_start + GlobalParams::hotspots[i].second)
            {
                if (local_id != GlobalParams::hotspots[i].first)
                {
                    p.dst_id = GlobalParams::hotspots[i].first;
                }
                break;
            }
            else
                range_start += GlobalParams::hotspots[i].second; // try next
        }
#ifdef DEADLOCK_AVOIDANCE
        assert((GlobalParams::topology == TOPOLOGY_MESH));
        if (p.dst_id % 2 != 0)
        {
            p.dst_id = (p.dst_id + 1) % 256;
        }
#endif

    } while (p.dst_id == p.src_id);

    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();
    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);

    return p;
}
// TODO: for testing only
Packet ProcessingElement::trafficTest(int isReqt)
{
    Packet p;
//     p.src_id = local_id;
//     int dst = 0;
//     int packet_size = 0;
//     bool read, finish = false;
//     (isReqt==1) ? cout << "<<Request channel>>" << endl : cout << "<<Data channel>>" << endl;
//     std::string data = "";
//     cout << "Current PE id: " << local_id << endl;
//     cout << "Enter destination PE id: ";
//     cin >> dst;
//     cout << "Is request Read(1) or Write(0): ";
//     cin >> read;
//     int request_size = 0;
//     if(isReqt){
//         //* Request channel
//         cout << "Enter request address (in HEX): ";
//         cin >> data;
//         if(read){
//             cout << "Enter request data size (in flits): ";
//             cin >> request_size;
//         }
//         cout << "Is request all finished(1) or not(0): ";
//         cin >> finish;
//         int input_data_size = data.size();
//         input_data_size = REQ_PACKET_SIZE/4 - input_data_size;
//         std::string paddingZero = "";
//         for(int i = 0; i < input_data_size; i++){
//             paddingZero += "0";
//         }
//         data = data + paddingZero;
//         packet_size = REQ_PACKET_SIZE/32;
//         cout << "Packet size is : " << packet_size << ", each flit is 32-bits" << endl;
//         for(int i = 0; i < packet_size; i++){
//             string data_payload = data.substr(i*8, 8);
//             cout << "Flit body " << i << ": " << data_payload << endl;
//             Payload pld_tmp;
//             pld_tmp.data = std::stoul(data_payload, nullptr, 16);
//             pld_tmp.read = read;
//             pld_tmp.request_size = request_size;
//             p.payload.push_back(pld_tmp);
//         }
//         cout << endl;
//     }
//     else{
//         //* Data channel
//         if(!read){
//             cout << "Enter write data (in HEX): ";
//             cin >> data;
//             int input_data_size = data.size();
//             input_data_size = (DATA_PACKET_SIZE/4) - input_data_size;
//             std::string paddingZero = "";
//             for(int i = 0; i < input_data_size; i++){
//                 paddingZero += "0";
//             }
//             data = data + paddingZero;
//             packet_size = (DATA_PACKET_SIZE/32);
//             cout << "Packet size is : " << packet_size << ", each flit is 32-bits" << endl;
//             for(int i = 0; i < packet_size; i++){
//                 string data_payload = data.substr(i*8, 8);
//                 cout << "Flit body " << i << ": " << data_payload << endl;
//                 Payload pld_tmp;
//                 pld_tmp.data = std::stoul(data_payload, nullptr, 16);
//                 pld_tmp.read = read;
//                 pld_tmp.request_size = request_size;
//                 p.payload.push_back(pld_tmp);
//             }
//             cout << endl;
//         }
//         else{
//             Payload pld_tmp;
//             pld_tmp.data = 0;
//             pld_tmp.read = read;
//             pld_tmp.request_size = request_size;
//             p.payload.push_back(pld_tmp);
//             cout << "Read data from cache" << endl << endl;
//         }
//     }
//     p.finish = finish;
//     p.dst_id = dst;
//     p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
//     //! Modified
//     p.size = p.flit_left = (packet_size + 2); //* 2 flits for header and tail
//     srand(time(NULL));
//     p.packet_id = rand();
//     //!
//     p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);

    return p;
}

Packet ProcessingElement::trafficTranspose1()
{
    assert(GlobalParams::topology == TOPOLOGY_MESH);
    Packet p;
    p.src_id = local_id;
    Coord src, dst;

    // Transpose 1 destination distribution
    src.x = id2Coord(p.src_id).x;
    src.y = id2Coord(p.src_id).y;
    dst.x = GlobalParams::mesh_dim_x - 1 - src.y;
    dst.y = GlobalParams::mesh_dim_y - 1 - src.x;
    fixRanges(src, dst);
    p.dst_id = coord2Id(dst);

    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();

    return p;
}

Packet ProcessingElement::trafficTranspose2()
{
    assert(GlobalParams::topology == TOPOLOGY_MESH);
    Packet p;
    p.src_id = local_id;
    Coord src, dst;

    // Transpose 2 destination distribution
    src.x = id2Coord(p.src_id).x;
    src.y = id2Coord(p.src_id).y;
    dst.x = src.y;
    dst.y = src.x;
    fixRanges(src, dst);
    p.dst_id = coord2Id(dst);

    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();

    return p;
}

void ProcessingElement::setBit(int &x, int w, int v)
{
    int mask = 1 << w;

    if (v == 1)
        x = x | mask;
    else if (v == 0)
        x = x & ~mask;
    else
        assert(false);
}

int ProcessingElement::getBit(int x, int w)
{
    return (x >> w) & 1;
}

inline double ProcessingElement::log2ceil(double x)
{
    return ceil(log(x) / log(2.0));
}

Packet ProcessingElement::trafficBitReversal()
{

    int nbits =
        (int)
            log2ceil((double)(GlobalParams::mesh_dim_x *
                              GlobalParams::mesh_dim_y));
    int dnode = 0;
    for (int i = 0; i < nbits; i++)
        setBit(dnode, i, getBit(local_id, nbits - i - 1));

    Packet p;
    p.src_id = local_id;
    p.dst_id = dnode;

    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();

    return p;
}

Packet ProcessingElement::trafficShuffle()
{

    int nbits =
        (int)
            log2ceil((double)(GlobalParams::mesh_dim_x *
                              GlobalParams::mesh_dim_y));
    int dnode = 0;
    for (int i = 0; i < nbits - 1; i++)
        setBit(dnode, i + 1, getBit(local_id, i));
    setBit(dnode, 0, getBit(local_id, nbits - 1));

    Packet p;
    p.src_id = local_id;
    p.dst_id = dnode;

    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();

    return p;
}

Packet ProcessingElement::trafficButterfly()
{

    int nbits = (int)log2ceil((double)(GlobalParams::mesh_dim_x *
                                       GlobalParams::mesh_dim_y));
    int dnode = 0;
    for (int i = 1; i < nbits - 1; i++)
        setBit(dnode, i, getBit(local_id, i));
    setBit(dnode, 0, getBit(local_id, nbits - 1));
    setBit(dnode, nbits - 1, getBit(local_id, 0));

    Packet p;
    p.src_id = local_id;
    p.dst_id = dnode;

    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    p.size = p.flit_left = getRandomSize();

    return p;
}

void ProcessingElement::fixRanges(const Coord src,
                                  Coord &dst)
{
    // Fix ranges
    if (dst.x < 0)
        dst.x = 0;
    if (dst.y < 0)
        dst.y = 0;
    if (dst.x >= GlobalParams::mesh_dim_x)
        dst.x = GlobalParams::mesh_dim_x - 1;
    if (dst.y >= GlobalParams::mesh_dim_y)
        dst.y = GlobalParams::mesh_dim_y - 1;
}

int ProcessingElement::getRandomSize()
{
    return randInt(GlobalParams::min_packet_size,
                   GlobalParams::max_packet_size);
}

unsigned int ProcessingElement::getQueueSize() const
{
    return packet_queue.size();
}

// Data NoC - AddDate: 2023/04/30
unsigned int ProcessingElement::getDataQueueSize() const
{
    return datapacket_queue.size();
}