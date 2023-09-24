#include "CacheNIC.h"

void* CacheNIC::new2d(int h, int w, int size)
{
    int i;
    void **p;

    p = (void**)new char[h*sizeof(void*) + h*w*size];
    for(i = 0; i < h; i++)
        p[i] = ((char *)(p + h)) + i*w*size;

    return p;
}

int CacheNIC::getTrafficSize(){
    return traffic_table_NIC.size();
}

void CacheNIC::getPacketinCommunication(const int src_id, CommunicationNIC* comm)
{
	for (unsigned int i = 0; i < traffic_table_NIC.size(); i++)
	{
		if (traffic_table_NIC[i].src == src_id && !traffic_table_NIC[i].used)
		{
            get_req_count++;
            traffic_table_NIC[i].used = true;
            comm->src = traffic_table_NIC[i].src;
            comm->dst = traffic_table_NIC[i].dst;
            comm->packet_id = traffic_table_NIC[i].packet_id;
            comm->tensor_id = traffic_table_NIC[i].tensor_id;
            for(int j=0;j<traffic_table_NIC[i].addr.size();j++){
                comm->addr.push_back(traffic_table_NIC[i].addr[j]);
                comm->req_type.push_back(traffic_table_NIC[i].req_type[j]);
                comm->flit_word_num.push_back(traffic_table_NIC[i].flit_word_num[j]);
                vector <int> tmp_Data;
                for(int k=0;k<traffic_table_NIC[i].flit_word_num[j];k++){
                    tmp_Data.push_back(traffic_table_NIC[i].data[j][k]);
                }
                comm->data.push_back(tmp_Data);
            }
            comm->pir = traffic_table_NIC[i].pir;
            comm->por = traffic_table_NIC[i].por;
            comm->t_on = traffic_table_NIC[i].t_on;
            comm->t_off = traffic_table_NIC[i].t_off;
            comm->t_period = traffic_table_NIC[i].t_period;
		    return;
        }
    }
    comm->src = -1;
	return;
}

void CacheNIC::addTraffic(uint32_t src_id, uint32_t dst_id, uint32_t packet_id, uint32_t tensor_id,uint64_t* addr, uint32_t* req_type, uint32_t* flit_word_num, int** data, int packet_size){
    CommunicationNIC comm;
    comm.src = src_id;
    comm.dst = dst_id;
    // Default values
    comm.packet_id = packet_id;
    comm.tensor_id = tensor_id;
    for(int i=0;i<packet_size;i++){
        comm.addr.push_back(addr[i]);
        comm.req_type.push_back(req_type[i]);
        comm.flit_word_num.push_back(flit_word_num[i]);
        vector <int> tmp_data;
        for(int j=0;j<flit_word_num[i];j++){
            tmp_data.push_back(data[i][j]);    
        }
        comm.data.push_back(tmp_data);
    }
    comm.pir = GlobalParams::packet_injection_rate;
    comm.por = comm.pir;
    comm.t_on = 0;
    comm.t_off = GlobalParams::reset_time + GlobalParams::simulation_time;
    comm.t_period = GlobalParams::reset_time + GlobalParams::simulation_time;
    comm.used = false;
    traffic_table_NIC.push_back(comm);
    add_traffic_count++;
    return;
}

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
            }
        }
        ack_rx.write(current_level_rx);
    }
}

void CacheNIC::txProcess()
{
    if (reset.read())
    {
        get_req_count = 0;
        req_tx.write(0);
        current_level_tx = 0;
        transmittedAtPreviousCycle = false;
    }
    else
    {
        Packet packet;

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
    flit.hub_relay_node = NOT_VALID;
    packetQueue_mutex.lock();
    packet_queue.front().flit_left--;
    if (packet_queue.front().flit_left == 0)
        packet_queue.pop();
    packetQueue_mutex.unlock();
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
                        data_mutex.lock();
                        received_datapackets.push_back(dataPacketBuffers[i]);
                        data_mutex.unlock();
                        dataPacketBuffers.erase(dataPacketBuffers.begin()+i);
                    }
                }
                dataBuffer_mutex.unlock();
            }
            dataack_rx.write(datacurrent_level_rx);
        }
    }
}

void CacheNIC::checkCachePackets()
{
    while(true){
        if (reset.read())
        {
            traffic_table_NIC.clear();
            add_traffic_count = 0;
        }
        else
        {
            int nic_id = (local_id) % (int)log2(PE_NUM);
            std::string shm_name_tmp = "cache_nic" + std::to_string(nic_id) + "_CACHE";
            char *shm_name = new char[shm_name_tmp.size()+1];
            std::strcpy (shm_name, shm_name_tmp.c_str());
            //* Creating shared memory object
            int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
            //* Setting file size
            ftruncate(fd, SHM_SIZE);
            //* Mapping shared memory object to process address space
            uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            memset(ptr, 0, SHM_SIZE);
            uint32_t src_id, dst_id, packet_id, tensor_id, packet_num, packet_size;
            uint64_t* addr = (uint64_t*)malloc(16*sizeof(uint64_t));
            uint32_t* req_type = (uint32_t*)malloc(16*sizeof(uint32_t));
            uint32_t* flit_word_num = (uint32_t*)malloc(16*sizeof(uint32_t));
            int** data = NEW2D(16, 8, int);
            bool ready, valid, ack;
            //* Reading from shared memory object
            while(CHECKVALID(ptr) != 1){
                setIPC_Ready(ptr); // Set ready signal
                wait();
            }
            ready = CHECKREADY(ptr);
            valid = CHECKVALID(ptr);
            resetIPC_Ready(ptr);
            src_id = GETSRC_ID(ptr);
            dst_id = GETDST_ID(ptr);
            packet_id = GETPACKET_ID(ptr);
            packet_num = GETPACKET_NUM(ptr);
            tensor_id = GETTENSOR_ID(ptr);
            packet_size = GETPACKET_SIZE(ptr);
            for(int i=0;i<16;i++){
                addr[i] = GETADDR(ptr, i);
                req_type[i] = GETREQ_TYPE(ptr, i);
                flit_word_num[i] = GETFLIT_WORD_NUM(ptr, i);
                for(int j=0;j<8;j++){
                    data[i][j] = GETDATA(ptr, i, j);
                }
            }
            //* If read request, add traffic to traffic table
            //* Else, reduce the packet number of write request in dependency table
            transcation_count++;
            if(req_type[0]){
                addTraffic(src_id, dst_id, packet_id, tensor_id, addr, req_type, flit_word_num, data, packet_size);
            }
            else{
                dependcy_table_NIC->reducePacketNum(local_id, tensor_id, packet_num);
            }
            setIPC_Ack(ptr);
            while(CHECKACK(ptr)!=0){
                wait();
            }
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
            if(received_packets.size()!=0 && received_datapackets.size()!=0){
                bool break_flag = false;
                for(int j=0; j<received_packets.size();j++){
                    for(int i=0;i<received_datapackets.size();i++){
                        if(received_packets[j].packet_id == received_datapackets[i].packet_id){
                            //* Get packet from received_packets and received_datapackets
                            req_mutex.lock();
                            data_mutex.lock();
                            Packet req_packet = received_packets[j];
                            Packet data_packet = received_datapackets[i];
                            data_mutex.unlock();
                            req_mutex.unlock();
                            _packet_count++;
                            //* Check dependency table of this packet (when request is read)
                            if(req_packet.payload[0].read){
                                bool continue_flag = false;
                                for(int k=0;k<req_packet.depend_tensor_id.size();k++){
                                    //* If one of the dependency tensor is not return, then this packet cannot be sent
                                    if(!dependcy_table_NIC->checkDependcyReturn(req_packet.depend_tensor_id[k]))
                                        continue_flag = true;
                                }
                                if(continue_flag){
                                    wait();
                                    continue;
                                }
                            }
                            //* break flag is used to check only one packet can be sent at a time
                            break_flag = true;
                            //* Erase packet from received_packets and received_datapackets
                            req_mutex.lock();
                            data_mutex.lock();
                            received_packets.erase(received_packets.begin()+j);
                            received_datapackets.erase(received_datapackets.begin()+i);
                            data_mutex.unlock();
                            req_mutex.unlock();
                            //* Run coalescing unit to repack the packet (when request is read)
                            if(req_packet.payload[0].read)
                                runCoalescingUnit(&req_packet, &data_packet);
                            else{
                                //* Add request to dependency table of this packet (when request is write)
                                tensorDependcyNIC tmp_td;
                                tmp_td.nic_id = local_id;
                                tmp_td.tensor_id = req_packet.tensor_id;
                                tmp_td.packet_count = req_packet.packet_num;
                                tmp_td.return_flag = false;
                                dependcy_table_NIC->addDependcy(tmp_td);
                            }
                            if(req_packet.payload.size() == 0){
                                break;
                            }
                            //* Start transaction with Cache
                            transaction(req_packet, data_packet);
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

void CacheNIC::runCoalescingUnit(Packet *packet, Packet *data_packet){
    //* Unpack the packet to search which flits need to be sent
    //* And then repack the packet
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
    for(int i=0;i<packet->payload.size();i++){
        cache_set_index = ((packet->payload[i].addr >> (cache_num_offset_bits+2)) & index_mask);
        if(ADDR_MAPPING_MODE == 0){
            unsigned int choose_bank = cache_set_index % BANK_NUM;
            int nic_id = (local_id) % (int)log2(PE_NUM);
            if(choose_bank/4 != nic_id){
                packet->payload.erase(packet->payload.begin()+i);
                data_packet->data_payload.erase(data_packet->data_payload.begin()+i);
            }
        }
        else if(ADDR_MAPPING_MODE == 1){
            unsigned int address_partition = ((unsigned int)-1) / BANK_NUM;
            unsigned int choose_bank = (packet->payload[i].addr / address_partition);
            int nic_id = (local_id) % (int)log2(PE_NUM);
            if(choose_bank/4 != nic_id){
                packet->payload.erase(packet->payload.begin()+i);
                data_packet->data_payload.erase(data_packet->data_payload.begin()+i);
            }
        }
    }
}

void CacheNIC::transaction(Packet req_packet, Packet data_packet){
    //* valid, ready, ack (3-bit) and reserved (29-bit)
    //* src id(int), dst id(int), packet id(int), packet_num(int), tensor_id(int), 
    //! 6 * int = 6 word
    //* Payload: addr(uint64_t), req_type(int), flit_word_num(int) * 16
    //* DataPayload: data(int * 8) * 16
    //! 16 * (uint64_t + 10 * int) = 192 word
    //! Total: 198 word = 6336 bit
    int nic_id = (local_id) % (int)log2(PE_NUM);
    std::string shm_name_tmp = "cache_nic" + std::to_string(nic_id) + "_NOC";
    char *shm_name = new char[shm_name_tmp.size()+1];
    std::strcpy (shm_name, shm_name_tmp.c_str());
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, SHM_SIZE);
    uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint32_t ready, valid, ack;
    memset(ptr, 0, SHM_SIZE);
    while(CHECKREADY(ptr) != 1){
        wait();
    }
    // set src_id
    setIPC_Data(ptr, req_packet.src_id, 1, 0);
    // set dst_id
    setIPC_Data(ptr, req_packet.dst_id, 2, 0);
    // set packet_id
    setIPC_Data(ptr, req_packet.packet_id, 3, 0);
    // set packet_num
    setIPC_Data(ptr, req_packet.packet_num, 4, 0);
    // set tensor_id
    setIPC_Data(ptr, req_packet.tensor_id, 5, 0);
    // set packet size
    setIPC_Data(ptr, req_packet.payload.size(), 6, 0);
    for(int i=0;i<req_packet.payload.size();i++){
        // set request addr
        setIPC_Addr(ptr, req_packet.payload[i].addr, i);
        // set request type
        setIPC_Data(ptr, req_packet.payload[i].read, 8, i);
        // set flit_word_num
        setIPC_Data(ptr, req_packet.payload[i].flit_word_num, 9, i);
        for(int j=0;j<req_packet.payload[i].flit_word_num;j++){
            // set request data
            setIPC_Data(ptr, data_packet.data_payload[i].data[j], 10+j, i);
        }
    }
    //* set valid
    setIPC_Valid(ptr);
    while(CHECKACK(ptr) != 1){
        wait();
    }
    resetIPC_Ack(ptr);
    resetIPC_Valid(ptr);
    return;
}

void CacheNIC::setIPC_Data(uint32_t *ptr, int data, int const_pos, int index){
    *(ptr + const_pos + 12*index) = data;
    return;
}

void CacheNIC::setIPC_Addr(uint32_t *ptr, uint64_t data, int index){
    uint64_t *dataPtr = reinterpret_cast<uint64_t*>(ptr + 7 + 12 * index);
    *dataPtr = data;
    return;
}

void CacheNIC::setIPC_Ready(uint32_t *ptr){
    *ptr = (*ptr | (0b1 << 31));
    return;
}

void CacheNIC::resetIPC_Ready(uint32_t *ptr){
    *ptr = (*ptr & ~(0b1 << 31));
    return;
}

void CacheNIC::setIPC_Valid(uint32_t *ptr){
    *ptr = (*ptr | (0b1 << 30));
    return;
}

void CacheNIC::resetIPC_Valid(uint32_t *ptr){
    *ptr = (*ptr & ~(0b1 << 30));
    return;
}

void CacheNIC::setIPC_Ack(uint32_t *ptr){
    *ptr = (*ptr | (0b1 << 29));
    return;
}

void CacheNIC::resetIPC_Ack(uint32_t *ptr){
    *ptr = (*ptr & ~(0b1 << 29));
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

DataFlit CacheNIC::nextDataFlit()
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
    dataPacketQueue_mutex.lock();
    datapacket_queue.front().flit_left--;
    if (datapacket_queue.front().flit_left == 0)
        datapacket_queue.pop();
    dataPacketQueue_mutex.unlock();
    return flit;
}

bool CacheNIC::canShot(Packet *packet, int isReqt)
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

        CommunicationNIC comm;
        getPacketinCommunication(local_id, &comm);
        if (comm.src != -1)
        {
            int vc = randInt(0, GlobalParams::n_virtual_channels - 1);
            int packet_size = comm.addr.size();
            int read = comm.req_type[0];
            std::vector<int> emptyDependTensorId;
            uint64_t* addr = (uint64_t*)malloc(packet_size*sizeof(uint64_t));
            int* req_type = (int*)malloc(packet_size*sizeof(int));
            int* flit_word_num = (int*)malloc(packet_size*sizeof(int));
            int** data = NEW2D(packet_size, 8, int);
            for(int i =0;i<packet_size;i++){
                addr[i] = comm.addr[i];
                req_type[i] = comm.req_type[i];
                flit_word_num[i] = comm.flit_word_num[i];
                for(int j=0;j<flit_word_num[i];j++){
                    data[i][j] = comm.data[i][j];
                }
            }
            Packet packet;
            packet.make(comm.src, comm.dst, vc, now, packet_size, read, comm.tensor_id, addr, NULL, flit_word_num, 1, comm.packet_id, emptyDependTensorId, 0);
            Packet data_packet;
            data_packet.make(comm.src, comm.dst, vc, now, packet_size, read, comm.tensor_id, addr, data, flit_word_num, 0, comm.packet_id, emptyDependTensorId, 0);
            packetQueue_mutex.lock();
            dataPacketQueue_mutex.lock();
            packet_queue.push(packet);
            datapacket_queue.push(data_packet);
            packetQueue_mutex.unlock();
            dataPacketQueue_mutex.unlock();
            sendback_count++;
            shot = true;
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

//! For testing only (Deprecated)
Packet CacheNIC::trafficTest(int isReqt)
{
    Packet p;
//     p.src_id = local_id;
//     int dst = 0;
//     int packet_size = 0;
//     bool read;
//     (isReqt==1) ? cout << "<<Request channel>>" << endl : cout << "<<Data channel>>" << endl;
//     std::string data = "";
//     cout << "Current CacheNIC id: " << local_id << endl;
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
//             input_data_size = DATA_PACKET_SIZE/4 - input_data_size;
//             std::string paddingZero = "";
//             for(int i = 0; i < input_data_size; i++){
//                 paddingZero += "0";
//             }
//             data = data + paddingZero;
//             packet_size = DATA_PACKET_SIZE/32;
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
//             cout << "Read data from PE" << endl << endl;
//         }
//     }

//     p.dst_id = dst;
//     p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
//     //! Modified
//     p.size = p.flit_left = (packet_size + 2); //* 2 flits for header and tail
//     srand (time(NULL));
//     p.packet_id = rand();
//     //!
//     p.vc_id = randInt(0, GlobalParams::n_virtual_channels - 1);

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