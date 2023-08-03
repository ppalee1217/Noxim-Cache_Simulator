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
    }
    else
    {
        if (req_rx.read() == 1 - current_level_rx)
        {
            Flit flit_tmp = flit_rx.read();
            current_level_rx = 1 - current_level_rx; // Negate the old value for Alternating Bit Protocol (ABP)
            if (flit_tmp.flit_type == FLIT_TYPE_HEAD){
                cout << "(Request)Cache NIC " << local_id << " received a HEAD flit from PE " << flit_tmp.src_id << endl;
                cout << "Packet ID: " << flit_tmp.packet_id << endl;
                cout << "Store request packet to packet buffer" << endl;
                cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                Packet packet_tmp;
                packet_tmp.src_id = flit_tmp.src_id;
                packet_tmp.dst_id = flit_tmp.dst_id;
                packet_tmp.vc_id = flit_tmp.vc_id;
                packet_tmp.packet_id = flit_tmp.packet_id;
                packet_tmp.timestamp = flit_tmp.timestamp;
                packetBuffers.push_back(packet_tmp);
            }
            if (flit_tmp.flit_type == FLIT_TYPE_BODY){
                cout << "(Request)Cache NIC " << local_id << " received a BODY flit from PE " << flit_tmp.src_id << endl;
                cout << "Packet ID: " << flit_tmp.packet_id << endl;
                cout << "Received request: 0x" << std::hex << flit_tmp.payload.data << std::dec << endl;
                cout << "Received request seq no.: " << flit_tmp.sequence_no << endl;
                cout << "Received request type(read = 1): " << flit_tmp.payload.read << endl;
                cout << "Received request size: " << flit_tmp.payload.request_size << endl;
                cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                for(int i=0;i<packetBuffers.size();i++){
                    if(packetBuffers[i].packet_id == flit_tmp.packet_id){
                        packetBuffers[i].payload.push_back(flit_tmp.payload);
                        break;
                    }
                }
            }
            if (flit_tmp.flit_type == FLIT_TYPE_TAIL){
                cout << "(Request)Cache NIC " << local_id << " received a TAIL flit from PE " << flit_tmp.src_id << endl;
                cout << "Packet ID: " << flit_tmp.packet_id << endl;
                cout << "Store request packet from packet buffer to received packets" << endl;
                cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                for(int i=0;i<packetBuffers.size();i++){
                    if(packetBuffers[i].packet_id == flit_tmp.packet_id){
                        received_packets.push_back(packetBuffers[i]);
                        packetBuffers.erase(packetBuffers.begin()+i);
                    }
                }
            }
            cout << endl;
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
            packet_queue.push(packet);
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
    cout << "==========" << endl;
    cout << "CacheNIC " << std::dec << local_id << " sent a request flit to PE " << flit.dst_id << endl;
    cout << "Flit type: " << flit.flit_type << endl;
    cout << "Sequence number: " << flit.sequence_no << endl;
    cout << "Sent request: 0x" << std::hex << flit.payload.data << std::dec << endl;
    cout << "==========" << endl;
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
    }
    else
    {
        if (datareq_rx.read() == 1 - datacurrent_level_rx)
        {
            DataFlit flit_tmp = dataflit_rx.read();
            datacurrent_level_rx = 1 - datacurrent_level_rx;
            if (flit_tmp.flit_type == FLIT_TYPE_HEAD){
                cout << "(Data)Cache NIC " << local_id << " received a HEAD flit from PE " << flit_tmp.src_id << endl;
                cout << "Packet ID: " << flit_tmp.packet_id << endl;
                cout << "Store data packet to packet buffer" << endl;
                cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                Packet packet_tmp;
                packet_tmp.src_id = flit_tmp.src_id;
                packet_tmp.dst_id = flit_tmp.dst_id;
                packet_tmp.vc_id = flit_tmp.vc_id;
                packet_tmp.packet_id = flit_tmp.packet_id;
                packet_tmp.timestamp = flit_tmp.timestamp;
                packetBuffers.push_back(packet_tmp);
            }
            if (flit_tmp.flit_type == FLIT_TYPE_BODY){
                cout << "(Data)Cache NIC " << local_id << " received a BODY flit from PE " << flit_tmp.src_id << endl;
                cout << "Packet ID: " << flit_tmp.packet_id << endl;
                cout << "Received request seq no.: " << flit_tmp.sequence_no << endl;
                cout << "Received data: 0x" << std::hex << flit_tmp.payload.data << std::dec << endl;
                cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                for(int i=0;i<packetBuffers.size();i++){
                    if(packetBuffers[i].packet_id == flit_tmp.packet_id){
                        packetBuffers[i].payload.push_back(flit_tmp.payload);
                        break;
                    }
                }
            }
            if (flit_tmp.flit_type == FLIT_TYPE_TAIL){
                cout << "(Data)Cache NIC " << local_id << " received a TAIL flit from PE " << flit_tmp.src_id << endl;
                cout << "Packet ID: " << flit_tmp.packet_id << endl;
                cout << "Store request packet from packet buffer to received packets" << endl;
                cout << "Timestamp of flit: " << flit_tmp.timestamp << endl;
                for(int i=0;i<packetBuffers.size();i++){
                    if(packetBuffers[i].packet_id == flit_tmp.packet_id){
                        received_datapackets.push_back(packetBuffers[i]);
                        packetBuffers.erase(packetBuffers.begin()+i);
                    }
                }
            }
            dataack_rx.write(datacurrent_level_rx);
            cout << endl;
        }
    }
}

void CacheNIC::checkCachePackets()
{
    if (reset.read())
    {
        
    }
    else
    {

    }
    return;
}

void CacheNIC::checkNoCPackets()
{
    if (reset.read())
    {
        received_packets.clear();
        received_datapackets.clear();
        packetBuffers.clear();
    }
    else
    {
        if(received_packets.size()!=0){
            for(int i=0;i<received_datapackets.size();i++){
                //* Here is a assumption that the data packet of same request will be received before other data packet from the same PE
                if(received_packets[0].src_id == received_datapackets[i].src_id){
                    cout << "CacheNIC " << local_id << " received a Data & Req packet from PE " << received_datapackets[i].src_id << endl;
                    cout << "Request type is (Read is 1) " << received_packets[0].payload[0].read << endl;
                    cout << "Request size is " << received_packets[0].payload[0].request_size << endl;
                    cout << "Request address is 0x" << std::hex << received_packets[0].payload[0].data << received_packets[0].payload[1].data << std::dec << endl;
                    if(received_packets[0].payload[0].read != 1){
                        for(int j=0;j<received_datapackets[i].payload.size();j++){
                            cout << "Data flit " << j << " is 0x" << std::hex << received_datapackets[i].payload[j].data << std::dec << endl;
                        }
                    }
                    cout << endl;
                    transaction(received_packets[0], received_datapackets[i]);
                    received_packets.erase(received_packets.begin());
                    received_datapackets.erase(received_datapackets.begin()+i);
                    //* For debug test (if error occurs when packet number is huge)
                    // cout << "received packets size: " << received_packets.size() << endl;
                    // cout << "received datapackets size: " << received_datapackets.size() << endl;
                    // cout << "packetBuffers size: " << packetBuffers.size() << endl;
                    cout << endl;
                    break;
                }
            }
        }
    }
}

void CacheNIC::transaction(Packet & req_packet, Packet & data_packet){
    cout << "<<Start transaction with IPC>>" << endl;
    std::string shm_name_tmp = "cache_nic" + std::to_string(local_id);
    // src_id(32) | dst_id(32) | packet_id(32) | req(32*2) | data(32*8) | read(32) | request_size(32) | READY(1) | VALID(1)
    char *shm_name = new char[shm_name_tmp.size()+1];
    std::strcpy (shm_name, shm_name_tmp.c_str());
    cout << "SHM_NAME: " << shm_name << endl;
    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    ftruncate(fd, SHM_SIZE);
    uint32_t* ptr = (uint32_t*)mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    uint32_t ready, valid, ack, data_test;
    cout << "Waiting for IPC channel to set ready signal." << endl;
    while(CHECKREADY(ptr) != 1){
        // getchar();
        // ready = CHECKREADY(ptr);
        // data_test = GETTEST(ptr, 15);
        // cout << "Checking again..." << endl;
        // cout << "ready = " << ready << endl;
        // cout << "data_test = 0x" << std::hex << data_test << std::dec << endl;
    }
    cout << "IPC channel is ready to read." << endl;
    //* Setting the valid bit and size
    // set src_id
    setIPC_Data(ptr, req_packet.src_id, 0, 0);
    // set dst_id
    setIPC_Data(ptr, req_packet.dst_id, 1, 0);
    // set packet_id
    setIPC_Data(ptr, req_packet.packet_id, 2, 0);
    // set request addr
    for(int i=0;i<(REQ_PACKET_SIZE/32);i++){
        setIPC_Data(ptr, req_packet.payload[i].data, 3, i);
    }
    // set request data (write only)
    if(req_packet.payload[0].read != 1){
        for(int i=0;i<(DATA_PACKET_SIZE/32);i++){
            setIPC_Data(ptr, data_packet.payload[i].data, 5, i);
        }
    }
    // set request size (read only)
    else{
        setIPC_Data(ptr, req_packet.payload[0].request_size, 14, 0);
    }
    // set read
    setIPC_Data(ptr, req_packet.payload[0].read, 13, 0);
    setIPC_Valid(ptr);
    cout << "Wait for IPC channel to set ack signal." << endl;
    while(CHECKACK(ptr) != 1){
        ack = CHECKACK(ptr);
        cout << "ack = " << ack << endl;
    }
    resetIPC_Ack(ptr);
    cout << "IPC channel ack signal is sent back." << endl;
    cout << "<Transaction completed!>" << endl;
    resetIPC_Valid(ptr);
    // munmap(ptr, SHM_SIZE);
    // shm_unlink(shm_name);
    return;
}

void CacheNIC::resetIPC_Ack(uint32_t *ptr){
    *(ptr + 15) = (*(ptr + 15) & ~(0b1 << 29));
    return;
}

void CacheNIC::setIPC_Data(uint32_t *ptr, uint32_t data, int const_pos, int varied_pos){
    *(ptr + const_pos + varied_pos) = data;
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
    cout << "==========" << endl;
    cout << "CacheNIC " << std::dec << local_id << " sent a data flit to PE " << flit.dst_id << endl;
    cout << "Flit type: " << flit.flit_type << endl;
    cout << "Sequence number: " << flit.sequence_no << endl;
    cout << "Sent data: 0x" << std::hex << flit.payload.data << std::dec << endl;
    cout << "==========" << endl;
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

        int dstfromTable;
        int remaining_traffic = traffic_table->getPacketinCommunication(local_id, dstfromTable, isReqt);
        // cout << "in NoC " << isReqt << " remaining Traffic in PE: " << remaining_traffic << endl;
        // getchar();
        if (remaining_traffic > 0)
        {
            int vc = randInt(0, GlobalParams::n_virtual_channels - 1);
            packet.make(local_id, dstfromTable, vc, now, getRandomSize());
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
            cout << "Read data from cache" << endl << endl;
        }
    }

    p.dst_id = dst;
    p.timestamp = sc_time_stamp().to_double() / GlobalParams::clock_period_ps;
    //! Modified
    p.size = p.flit_left = (packet_size + 2); //* 2 flits for header and tail
    p.packet_id = randInt(0, 1000);
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