/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the processing element
 */

#include "CacheNIC.h"

int CacheNIC::randInt(int min, int max)
{
    return min +
           (int)((double)(max - min + 1) * rand() / (RAND_MAX + 1.0));
}

void CacheNIC::rxProcess()
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
                // cout << "(Request)Cache NIC " << local_id << " received a HEAD flit from PE " << flit_tmp.src_id << endl;
                // cout << "Packet ID: " << flit_tmp.packet_id << endl;
                // cout << "Store request packet to packet buffer" << endl;
                // cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                fprintf(_log_receive, "------------\n");
                fprintf(_log_receive, "(Req) NIC %d reveice a HEAD flit from PE %d\n", local_id, flit_tmp.src_id);
                fprintf(_log_receive, "Packet ID: %d\n", flit_tmp.packet_id);
                fprintf(_log_receive, "Timestamp of flit: %d\n", flit_tmp.timestamp);
                Packet packet_tmp;
                packet_tmp.src_id = flit_tmp.src_id;
                packet_tmp.dst_id = flit_tmp.dst_id;
                packet_tmp.vc_id = flit_tmp.vc_id;
                packet_tmp.packet_id = flit_tmp.packet_id;
                packet_tmp.finish = flit_tmp.finish;
                packet_tmp.timestamp = flit_tmp.timestamp;
                packet_tmp.payload.clear();
                reqBuffer_mutex.lock();
                packetBuffers.push_back(packet_tmp);
                reqBuffer_mutex.unlock();
            }
            if (flit_tmp.flit_type == FLIT_TYPE_BODY){
                // cout << "(Request)Cache NIC " << local_id << " received a BODY flit from PE " << flit_tmp.src_id << endl;
                // cout << "Packet ID: " << flit_tmp.packet_id << endl;
                // cout << "Received request: 0x" << std::hex << flit_tmp.payload.data << std::dec << endl;
                // cout << "Received request seq no.: " << flit_tmp.sequence_no << endl;
                // cout << "Received request type(read = 1): " << flit_tmp.payload.read << endl;
                // cout << "Received request size: " << flit_tmp.payload.request_size << endl;
                // cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                fprintf(_log_receive,"----------\n");
                fprintf(_log_receive, "(Req) NIC %d reveice a BODY flit from PE %d\n", local_id, flit_tmp.src_id);
                fprintf(_log_receive, "Packet ID: %d\n", flit_tmp.packet_id);
                fprintf(_log_receive, "Addr: 0x%08x\n", flit_tmp.payload.data);
                fprintf(_log_receive, "Timestamp of flit: %d\n", flit_tmp.timestamp);
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
                // cout << "(Request)Cache NIC " << local_id << " received a TAIL flit from PE " << flit_tmp.src_id << endl;
                // cout << "Packet ID: " << flit_tmp.packet_id << endl;
                // cout << "Store request packet from packet buffer to received packets" << endl;
                // cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                fprintf(_log_receive,"----------\n");
                fprintf(_log_receive, "(Req) NIC %d reveice a TAIL flit from PE %d\n", local_id, flit_tmp.src_id);
                fprintf(_log_receive, "Packet ID: %d\n", flit_tmp.packet_id);
                fprintf(_log_receive, "Timestamp of flit: %d\n", flit_tmp.timestamp);
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
            }
            // cout << endl;
        }
        ack_rx.write(current_level_rx);
    }
}

void CacheNIC::txProcess()
{
    if (reset.read())
    {
        req_tx.write(0);
        current_level_tx = 0;
        transmittedAtPreviousCycle = false;
    }
    else
    {
        Packet packet;

        if (canShot(packet, 1))
        {
            // printf("===========\n");
            // printf("Sent a packet from CacheNIC %d to PE %d\n", local_id, packet.dst_id);
            // printf("Packet ID: %d\n", packet.packet_id);
            // printf("Packet size: %d\n", packet.size);
            // printf("Packet flit left: %d\n", packet.flit_left);
            // for(int i=0;i<packet.payload.size();i++){
            //     printf("Packet req %d: 0x%08x\n", i, packet.payload[i].data);
            // }
            // printf("Packet finish: %d\n", packet.finish);
            // printf("Packet req type: %d\n", packet.payload[0].read);
            // packet_queue.push(packet);
            transmittedAtPreviousCycle = true;
        }
        else
            transmittedAtPreviousCycle = false;

        if (ack_tx.read() == current_level_tx)
        {
            if (!packet_queue.empty())
            {
                Flit flit = nextFlit();                  // Generate a new flit
                flit_tx->write(flit);                    // Send the generated flit
                current_level_tx = 1 - current_level_tx; // Negate the old value for Alternating Bit Protocol (ABP)
                req_tx.write(current_level_tx);
            }
        }
    }
}

Flit CacheNIC::nextFlit()
{
    Flit flit;
    Packet packet = packet_queue.front();

    flit.src_id = packet.src_id;
    flit.dst_id = packet.dst_id;
    flit.vc_id = packet.vc_id;
    //! Modified
    flit.packet_id = packet.packet_id;
    flit.finish = packet.finish;
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
    //! Modified
    if(packet.size == packet.flit_left || packet.flit_left == 1){
        flit.payload = packet.payload[0];
    }
    else{
        flit.payload = packet.payload[flit.sequence_no-1];
    }
    // cout << "==========" << endl;
    // cout << "CacheNIC " << std::dec << local_id << " sent a request flit to PE " << flit.dst_id << endl;
    // cout << "Flit type: " << flit.flit_type << endl;
    // cout << "Sequence number: " << flit.sequence_no << endl;
    // cout << "Sent request: 0x" << std::hex << flit.payload.data << std::dec << endl;
    // cout << "==========" << endl;
    //! Modified
    flit.hub_relay_node = NOT_VALID;

    packet_queue.front().flit_left--;
    if (packet_queue.front().flit_left == 0)
        packet_queue.pop();

    return flit;
}

void CacheNIC::datarxProcess()
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
                // cout << "(Data)Cache NIC " << local_id << " received a HEAD flit from PE " << flit_tmp.src_id << endl;
                // cout << "Packet ID: " << flit_tmp.packet_id << endl;
                // cout << "Store data packet to packet buffer" << endl;
                // cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                fprintf(_log_receive, "------------\n");
                fprintf(_log_receive, "(Data) NIC %d reveice a HEAD flit from PE %d\n", local_id, flit_tmp.src_id);
                fprintf(_log_receive, "Packet ID: %d\n", flit_tmp.packet_id);
                fprintf(_log_receive, "Timestamp of flit: %d\n", flit_tmp.timestamp);
                Packet packet_tmp;
                packet_tmp.src_id = flit_tmp.src_id;
                packet_tmp.dst_id = flit_tmp.dst_id;
                packet_tmp.vc_id = flit_tmp.vc_id;
                packet_tmp.packet_id = flit_tmp.packet_id;
                packet_tmp.finish = flit_tmp.finish;
                packet_tmp.timestamp = flit_tmp.timestamp;
                packet_tmp.payload.clear();
                dataBuffer_mutex.lock();
                dataPacketBuffers.push_back(packet_tmp);
                dataBuffer_mutex.unlock();
            }
            if (flit_tmp.flit_type == FLIT_TYPE_BODY){
                // cout << "(Data)Cache NIC " << local_id << " received a BODY flit from PE " << flit_tmp.src_id << endl;
                // cout << "Packet ID: " << flit_tmp.packet_id << endl;
                // cout << "Received request seq no.: " << flit_tmp.sequence_no << endl;
                // cout << "Received data: 0x" << std::hex << flit_tmp.payload.data << std::dec << endl;
                // cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                fprintf(_log_receive,"----------\n");
                fprintf(_log_receive, "(Data) NIC %d reveice a BODY flit from PE %d\n", local_id, flit_tmp.src_id);
                fprintf(_log_receive, "Packet ID: %d\n", flit_tmp.packet_id);
                fprintf(_log_receive, "Data: 0x%08x\n", flit_tmp.payload.data);
                fprintf(_log_receive, "Timestamp of flit: %d\n", flit_tmp.timestamp);
                dataBuffer_mutex.lock();
                for(int i=0;i<dataPacketBuffers.size();i++){
                    if(dataPacketBuffers[i].packet_id == flit_tmp.packet_id){
                        dataPacketBuffers[i].payload.push_back(flit_tmp.payload);
                        break;
                    }
                }
                dataBuffer_mutex.unlock();
            }
            if (flit_tmp.flit_type == FLIT_TYPE_TAIL){
                // cout << "(Data)Cache NIC " << local_id << " received a TAIL flit from PE " << flit_tmp.src_id << endl;
                // cout << "Packet ID: " << flit_tmp.packet_id << endl;
                // cout << "Store request packet from packet buffer to received packets" << endl;
                // cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                fprintf(_log_receive,"----------\n");
                fprintf(_log_receive, "(Data) NIC %d reveice a TAIL flit from PE %d\n", local_id, flit_tmp.src_id);
                fprintf(_log_receive, "Packet ID: %d\n", flit_tmp.packet_id);
                fprintf(_log_receive, "Timestamp of flit: %d\n", flit_tmp.timestamp);
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
            // cout << endl;
        }
    }
}

void CacheNIC::checkCachePackets()
{
    while(true){
        if (reset.read())
        {
        }
        else
        {
            std::string shm_name_tmp = "cache_nic" + std::to_string(local_id) + "_CACHE";
            char *shm_name = new char[shm_name_tmp.size()+1];
            std::strcpy (shm_name, shm_name_tmp.c_str());
            cout << "=====================" << endl;
            cout << "<< Start transaction with Cache (Reader: " << local_id/5 << ") >>" << endl;
            // cout << "SHM_NAME_CACHE: " << shm_name << endl;
            // Creating shared memory object
            int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
            // Setting file size
            ftruncate(fd, SHM_SIZE);
            // Mapping shared memory object to process address space
            uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            memset(ptr, 0, SHM_SIZE);
            uint32_t src_id, dst_id, packet_id, req, read, request_size;
            uint32_t* data;
            uint32_t data_test;
            uint32_t ready, valid, ack;
            bool pause =false;
            // cout << "Wait for IPC channel to set valid signal." << endl << endl;
            // Reading from shared memory object
            while(CHECKVALID(ptr) != 1){
                setIPC_Ready(ptr); // Set ready signal
                wait();
            }
            // cout << "<<Start transaction with IPC (Read)>>" << endl;
            ready = CHECKREADY(ptr);
            valid = CHECKVALID(ptr);
            resetIPC_Ready(ptr);
            src_id = GETSRC_ID(ptr);
            dst_id = GETDST_ID(ptr);
            packet_id = GETPACKET_ID(ptr);
            // cout << "====================" << endl;
            // cout << "<<Request from Reader " << local_id/5 << " received!>>" << endl;
            // cout << "src_id = " << src_id << endl;
            // cout << "dst_id = " << dst_id << endl;
            fprintf(_log_r, "------------\n");
            fprintf(_log_r, "src_id is: %d\n", src_id);
            fprintf(_log_r, "dst_id is: %d\n", dst_id);
            fprintf(_log_r, "packet_id is: %u\n", packet_id);
            for(int i = 0; i < 2; i++) {
                req = GETREQ(ptr, i);
                fprintf(_log_r, "addr[%d] is: 0x%08x\n", i, req);
                // cout << "req[" << i << "]  = 0x" << std::hex << std::setw(8) << req << std::dec << endl;
            }
            data = (uint32_t *)malloc(8*sizeof(uint32_t));
            for(int i = 0; i < 8; i++) {
                *(data+i) = GETDATA(ptr, i);
                fprintf(_log_r, "data[%d] is: 0x%08x\n", i, *(data+i));
                // cout << "data[" << i << "] = " << std::hex << std::setw(8) << data[i] << std::dec << endl;
            }
            read = GETREAD(ptr);
            // cout << "read (1 = true): " << read << endl;
            request_size = GETREQUEST_SIZE(ptr);
            fprintf(_log_r, "request size is: %d\n", request_size);
            // cout << "request_size = " << request_size << endl;
            setIPC_Ack(ptr);
            cout << "<<Reader " << local_id/5 << " Transaction completed!>>" << endl << endl;
            while(CHECKACK(ptr)!=0){
                wait();
            }
            traffic_table_NIC->addTraffic(src_id, dst_id, 1, request_size, req, NULL, read);
            cout << "Add request traffic from NIC " <<  local_id << " to traffic table" << endl;
            traffic_table_NIC->addTraffic(src_id, dst_id, 0, request_size, req, data, read);
            cout << "Add data traffic from NIC " <<  local_id << " to traffic table" << endl;
            cout << "Traffic Table NIC current size = " << traffic_table_NIC->getTrafficSize() << endl;
            cout << "=====================" << endl;
        }
        wait();
    }
}

void CacheNIC::checkNoCPackets()
{
    while(true){
        if (reset.read())
        {
            received_packets.clear();
            received_datapackets.clear();
        }
        else
        {
            // cout << "NIC " << local_id << ":" << endl;
            // cout << "received packets size: " << received_packets.size() << endl;
            // cout << "received datapackets size: " << received_datapackets.size() << endl;
            // cout << "packetBuffers size: " << packetBuffers.size() << endl;
            // cout << "dataPacketBuffers size: " << dataPacketBuffers.size() << endl;
            // cout << "----------" << endl;
            // cout << endl;
            if(received_packets.size()!=0){
                for(int i=0;i<received_datapackets.size();i++){
                    //* Here is a assumption that the data packet of same request will be received before other data packet from the same PE
                    if(received_packets[0].src_id == received_datapackets[i].src_id){
                        // cout << "CacheNIC " << local_id << " received a Data & Req packet from PE " << received_datapackets[i].src_id << endl;
                        // cout << "Request type is (Read is 1) " << received_packets[0].payload[0].read << endl;
                        // cout << "Request size is " << received_packets[0].payload[0].request_size << endl;
                        // cout << "Request address is 0x" << std::hex << received_packets[0].payload[0].data << received_packets[0].payload[1].data << std::dec << endl;
                        // cout << "Request data packet size is " << received_datapackets[i].payload.size() << endl;
                        // if(received_datapackets[i].payload.size() != 0){
                        //     for(int j=0;j<received_datapackets[i].payload.size();j++){
                        //         cout << "Data flit " << j << " is 0x" << std::hex << received_datapackets[i].payload[j].data << std::dec << endl;
                        //     }
                        // }
                        // cout << endl;
                        //!
                        req_mutex.lock();
                        data_mutex.lock();
                        Packet req_packet = received_packets[0];
                        Packet data_packet = received_datapackets[i];
                        received_packets.erase(received_packets.begin());
                        received_datapackets.erase(received_datapackets.begin()+i);
                        data_mutex.unlock();
                        req_mutex.unlock();
                        transaction(req_packet, data_packet);
                        //* For debug test (if error occurs when packet number is huge)
                        // cout << "NIC " << local_id << ":" << endl;
                        // cout << "received packets size: " << received_packets.size() << endl;
                        // cout << "received datapackets size: " << received_datapackets.size() << endl;
                        // cout << "packetBuffers size: " << packetBuffers.size() << endl;
                        // cout << "dataPacketBuffers size: " << dataPacketBuffers.size() << endl;
                        // cout << "----------" << endl;
                        // cout << endl;
                        break;
                    }
                }
            }
        }
        // }
        // getchar();
        wait();
    }
}

void CacheNIC::transaction(Packet req_packet, Packet data_packet){
    // transactionFlag = true;
    std::string shm_name_tmp = "cache_nic" + std::to_string(local_id) + "_NOC";
    char *shm_name = new char[shm_name_tmp.size()+1];
    std::strcpy (shm_name, shm_name_tmp.c_str());
    // cout << "=====================" << endl;
    cout << "<< Start transaction with Cache (Writer: " << local_id/5 << ") >>" << endl;
    // cout << "SHM_NAME_CACHE: " << shm_name << endl;
    // cout << "SHM_NAME_NOC: " << shm_name << endl;
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, SHM_SIZE);
    uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint32_t ready, valid, ack, data_test;
    memset(ptr, 0, SHM_SIZE);
    // cout << "Waiting for IPC channel to set ready signal." << endl;
    while(CHECKREADY(ptr) != 1){
        wait();
    }
    // cout << "IPC channel is ready to read." << endl;
    //* Setting the valid bit and size
    // printf("=============\n");
    // printf("NIC %d:\n", local_id);
    // set src_id
    setIPC_Data(ptr, req_packet.src_id, 0, 0);
    printf("(%dW) src_id is: %d\n", local_id/5, req_packet.src_id);
    // set dst_id
    setIPC_Data(ptr, req_packet.dst_id, 1, 0);
    printf("(%dW) dst_id is: %d\n", local_id/5, req_packet.dst_id);
    // set packet_id
    setIPC_Data(ptr, req_packet.packet_id, 2, 0);
    // printf("(%dW) packet_id is: %u\n", local_id/5, req_packet.packet_id);
    // set request addr
    for(int i=0;i<(REQ_PACKET_SIZE/32);i++){
        setIPC_Data(ptr, req_packet.payload[i].data, 3, i);
        printf("(%dW) addr[%d] is: 0x%08x\n", local_id/5, i, req_packet.payload[i].data);
    }
    // set request data
    if(!req_packet.payload[0].read){
        for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
            setIPC_Data(ptr, data_packet.payload[i].data, 5, i);
            // printf("data[%d] is: 0x%08x\n", i, data_packet.payload[i].data);
        }
    }
    else{
        for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
            setIPC_Data(ptr, 0, 5, i);
            // printf("data[%d] is: 0x%08x\n", i, 0);
        }
    }
    // set request size
    setIPC_Data(ptr, req_packet.payload[0].request_size, 14, 0);
    // printf("request size is: %d\n", req_packet.payload[0].request_size);
    // set read
    setIPC_Data(ptr, req_packet.payload[0].read, 13, 0);
    // printf("read is: %d\n", req_packet.payload[0].read);
    // set finish
    // if(req_packet.finish){
    //     setIPC_Finish(ptr);
    //     printf("------------\n");
    // cout << "=====================" << endl;
    // printf("(%dW) src_id is: %d\n", local_id, req_packet.src_id);
    // printf("(%dW) dst_id is: %d\n", local_id, req_packet.dst_id);
    // printf("(%dW) packet_id is: %u\n", local_id, req_packet.packet_id);
    // for(int i=0;i<(REQ_PACKET_SIZE/32);i++){
    //     printf("(%dW) addr[%d] is: 0x%08x\n", local_id, i, req_packet.payload[i].data);
    // }
    // for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
    //     printf("(%dW) data[%d] is: 0x%08x\n", local_id, i, data_packet.payload[i].data);
    // }
    // printf("(%dW) finish is: %d\n", local_id, req_packet.finish);
    // printf("Noxim input is finished (by NIC %d)!\n", local_id);
    printf("(%dW) Transaction count = %d\n", local_id/5, transcation_count);
    //     getchar();
    // }
    
    fprintf(_log_w, "------------\n");
    fprintf(_log_w, "src_id is: %d\n", req_packet.src_id);
    fprintf(_log_w, "dst_id is: %d\n", req_packet.dst_id);
    fprintf(_log_w, "packet_id is: %u\n", req_packet.packet_id);
    for(int i=0;i<(REQ_PACKET_SIZE/32);i++){
        fprintf(_log_w, "addr[%d] is: 0x%08x\n", i, req_packet.payload[i].data);
    }
    // for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
    //     fprintf(_log_w, "data[%d] is: 0x%08x\n", i, data_packet.payload[i].data);
    // }
    fprintf(_log_w, "finish is: %d\n", req_packet.finish);
    fprintf(_log_w, "packet timestamp is: %d\n", req_packet.timestamp);
    fprintf(_log_w, "datapacket timestamp is: %d\n", data_packet.timestamp);
    fprintf(_log_w, "(%d) Transaction count = %d\n", local_id, transcation_count);
    // getchar();
    setIPC_Valid(ptr);
    // cout << "Wait for IPC channel to set ack signal." << endl;
    // cout << "(Writer) Address = " << std::hex << ptr << std::dec << endl;
    // getchar();
    while(CHECKACK(ptr) != 1){
        wait();
    }
    resetIPC_Ack(ptr);
    // cout << "IPC channel ack signal is sent back." << endl;
    cout << "<Writer " << local_id/5 << " Transaction completed!>" << endl;
    // cout << "=====================" << endl;
    resetIPC_Valid(ptr);
    transcation_count++;
    return;
}

void CacheNIC::setIPC_Data(uint32_t *ptr, uint32_t data, int const_pos, int varied_pos){
    *(ptr + const_pos + varied_pos) = data;
    return;
}

void CacheNIC::setIPC_Ready(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 31));
    return;
}

void CacheNIC::resetIPC_Ready(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 31));
    return;
}

void CacheNIC::setIPC_Valid(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 30));
    return;
}

void CacheNIC::resetIPC_Valid(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 30));
    return;
}

void CacheNIC::setIPC_Ack(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 29));
    return;
}

void CacheNIC::resetIPC_Ack(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 29));
    return;
}

void CacheNIC::setIPC_Finish(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) | (0b1 << 28));
    return;
}

void CacheNIC::datatxProcess()
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

        if (canShot(packet, 0))
        {
            // printf("===========\n");
            // printf("Sent a datapacket from CacheNIC %d to PE %d\n", local_id, packet.dst_id);
            // printf("Packet ID: %d\n", packet.packet_id);
            // printf("Packet size: %d\n", packet.size);
            // printf("Packet flit left: %d\n", packet.flit_left);
            // for(int i=0;i<packet.payload.size();i++){
            //     printf("Packet data %d: 0x%08x\n", i, packet.payload[i].data);
            // }
            // printf("Packet finish: %d\n", packet.finish);
            // printf("Packet req type: %d\n", packet.payload[0].read);
            datapacket_queue.push(packet);
            datatransmittedAtPreviousCycle = true;
        }
        else
            datatransmittedAtPreviousCycle = false;

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

DataFlit CacheNIC::nextDataFlit()
{
    DataFlit flit;
    Packet packet = datapacket_queue.front();

    flit.src_id = packet.src_id;
    flit.dst_id = packet.dst_id;
    flit.vc_id = packet.vc_id;
    //! Modified
    flit.finish = packet.finish;
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
    //! Modified
    if(packet.size == packet.flit_left || packet.flit_left == 1){
        flit.payload = packet.payload[0];
    }
    else{
        flit.payload = packet.payload[flit.sequence_no-1];
    }
    // cout << "==========" << endl;
    // cout << "CacheNIC " << std::dec << local_id << " sent a data flit to PE " << flit.dst_id << endl;
    // cout << "Flit type: " << flit.flit_type << endl;
    // cout << "Sequence number: " << flit.sequence_no << endl;
    // cout << "Sent data: 0x" << std::hex << flit.payload.data << std::dec << endl;
    // cout << "==========" << endl;
    //!

    datapacket_queue.front().flit_left--;

    if (datapacket_queue.front().flit_left == 0)
        datapacket_queue.pop();

    return flit;
}

bool CacheNIC::canShot(Packet &packet, int isReqt)
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
        // shot = (local_id == 19);
        // shot = (((double)rand()) / RAND_MAX < threshold) && (local_id == 19);
        if (shot)
        {
            if (GlobalParams::traffic_distribution == TRAFFIC_RANDOM)
                packet = trafficRandom();
            else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE1)
                packet = trafficTranspose1();
            else if (GlobalParams::traffic_distribution == TRAFFIC_TRANSPOSE2)
                packet = trafficTranspose2();
            else if (GlobalParams::traffic_distribution == TRAFFIC_BIT_REVERSAL)
                packet = trafficBitReversal();
            else if (GlobalParams::traffic_distribution == TRAFFIC_SHUFFLE)
                packet = trafficShuffle();
            else if (GlobalParams::traffic_distribution == TRAFFIC_BUTTERFLY)
                packet = trafficButterfly();
            else if (GlobalParams::traffic_distribution == TRAFFIC_LOCAL)
                packet = trafficLocal();
            else if (GlobalParams::traffic_distribution == TRAFFIC_TEST)
                packet = trafficTest(isReqt);
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

        Communication comm = traffic_table_NIC->getPacketinCommunication(local_id, isReqt);
        if (comm.src != -1)
        {
            uint32_t packet_id = rand();
            int vc = randInt(0, GlobalParams::n_virtual_channels - 1);
            packet.make(local_id, comm.dst, vc, now, (isReqt) ? (REQ_PACKET_SIZE/32 + 2) : (DATA_PACKET_SIZE/32 + 2), comm.req_type, comm.req_addr, comm.req_data, comm.finish, comm.req_size, isReqt, packet_id);
            shot = true;
            printf("CacheNIC %d: %s packet to PE %d\n", local_id, (isReqt) ? "request" : "data", comm.dst);
        }
        else
            shot = false;
    }

    return shot;
}

Packet CacheNIC::trafficLocal()
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

int CacheNIC::findRandomDestination(int id, int hops)
{
    assert(GlobalParams::topology == TOPOLOGY_MESH);

    int inc_y = rand() % 2 ? -1 : 1;
    int inc_x = rand() % 2 ? -1 : 1;

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

Packet CacheNIC::trafficRandom()
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
Packet CacheNIC::trafficTest(int isReqt)
{
    Packet p;
    p.src_id = local_id;
    int dst = 0;
    int packet_size = 0;
    bool read;
    (isReqt==1) ? cout << "<<Request channel>>" << endl : cout << "<<Data channel>>" << endl;
    std::string data = "";
    cout << "Current CacheNIC id: " << local_id << endl;
    cout << "Enter destination PE id: ";
    cin >> dst;
    cout << "Is request Read(1) or Write(0): ";
    cin >> read;
    int request_size = 0;
    if(isReqt){
        //* Request channel
        cout << "Enter request address (in HEX): ";
        cin >> data;
        if(read){
            cout << "Enter request data size (in flits): ";
            cin >> request_size;
        }
        int input_data_size = data.size();
        input_data_size = REQ_PACKET_SIZE/4 - input_data_size;
        std::string paddingZero = "";
        for(int i = 0; i < input_data_size; i++){
            paddingZero += "0";
        }
        data = data + paddingZero;
        packet_size = REQ_PACKET_SIZE/32;
        cout << "Packet size is : " << packet_size << ", each flit is 32-bits" << endl;
        for(int i = 0; i < packet_size; i++){
            string data_payload = data.substr(i*8, 8);
            cout << "Flit body " << i << ": " << data_payload << endl;
            Payload pld_tmp;
            pld_tmp.data = std::stoul(data_payload, nullptr, 16);
            pld_tmp.read = read;
            pld_tmp.request_size = request_size;
            p.payload.push_back(pld_tmp);
        }
        cout << endl;
    }
    else{
        //* Data channel
        if(!read){
            cout << "Enter write data (in HEX): ";
            cin >> data;
            int input_data_size = data.size();
            input_data_size = DATA_PACKET_SIZE/4 - input_data_size;
            std::string paddingZero = "";
            for(int i = 0; i < input_data_size; i++){
                paddingZero += "0";
            }
            data = data + paddingZero;
            packet_size = DATA_PACKET_SIZE/32;
            cout << "Packet size is : " << packet_size << ", each flit is 32-bits" << endl;
            for(int i = 0; i < packet_size; i++){
                string data_payload = data.substr(i*8, 8);
                cout << "Flit body " << i << ": " << data_payload << endl;
                Payload pld_tmp;
                pld_tmp.data = std::stoul(data_payload, nullptr, 16);
                pld_tmp.read = read;
                pld_tmp.request_size = request_size;
                p.payload.push_back(pld_tmp);
            }
            cout << endl;
        }
        else{
            Payload pld_tmp;
            pld_tmp.data = 0;
            pld_tmp.read = read;
            pld_tmp.request_size = request_size;
            p.payload.push_back(pld_tmp);
            cout << "Read data from PE" << endl << endl;
        }
    }

    p.dst_id = dst;
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    //! Modified
    p.size = p.flit_left = (packet_size + 2); //* 2 flits for header and tail
    srand (time(NULL));
    p.packet_id = rand();
    //!
    p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);

    return p;
}

Packet CacheNIC::trafficTranspose1()
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

Packet CacheNIC::trafficTranspose2()
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

void CacheNIC::setBit(int &x, int w, int v)
{
    int mask = 1 << w;

    if (v == 1)
        x = x | mask;
    else if (v == 0)
        x = x & ~mask;
    else
        assert(false);
}

int CacheNIC::getBit(int x, int w)
{
    return (x >> w) & 1;
}

inline double CacheNIC::log2ceil(double x)
{
    return ceil(log(x) / log(2.0));
}

Packet CacheNIC::trafficBitReversal()
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

Packet CacheNIC::trafficShuffle()
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

Packet CacheNIC::trafficButterfly()
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

void CacheNIC::fixRanges(const Coord src,
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

int CacheNIC::getRandomSize()
{
    return randInt(GlobalParams::min_packet_size,
                   GlobalParams::max_packet_size);
}

unsigned int CacheNIC::getQueueSize() const
{
    return packet_queue.size();
}

// Data NoC - AddDate: 2023/04/30
unsigned int CacheNIC::getDataQueueSize() const
{
    return datapacket_queue.size();
}