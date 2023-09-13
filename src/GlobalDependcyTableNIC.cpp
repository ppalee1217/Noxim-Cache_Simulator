/*
 * Noxim - the NoC Simulator
 *
 * (C) 2005-2018 by the University of Catania
 * For the complete list of authors refer to file ../doc/AUTHORS.txt
 * For the license applied to these sources refer to file ../doc/LICENSE.txt
 *
 * This file contains the implementation of the global traffic table
 */

#include "GlobalDependcyTableNIC.h"

GlobalDependcyTableNIC::GlobalDependcyTableNIC()
{
    pthread_mutex_init(&mutex, NULL);
}

//TODO: Add dependency table
void GlobalDependcyTableNIC::addDependcy(tensorDependcyNIC tensorDependcyNIC){
    bool tensor_already_exist = false;
    for(int i=0;i<NIC_dependcy_table.size();i++){
        if(NIC_dependcy_table[i].tensor_id == tensorDependcyNIC.tensor_id && NIC_dependcy_table[i].nic_id == tensorDependcyNIC.nic_id){
            tensor_already_exist = true;
            break;
        }
    }
    if(!tensor_already_exist){
        printf("Add write tensor id %d to NIC %d\n", tensorDependcyNIC.tensor_id, tensorDependcyNIC.nic_id);
        pthread_mutex_lock(&mutex);
        NIC_dependcy_table.push_back(tensorDependcyNIC);
        pthread_mutex_unlock(&mutex);
    }
    return;
}

bool GlobalDependcyTableNIC::checkDependcyReturn(int tensor_id){
    if(NIC_dependcy_table.size() == 0)
        return true;
    bool check_flag = false;
    for(int i = 0; i < NIC_dependcy_table.size(); i++){
        if(NIC_dependcy_table[i].tensor_id == tensor_id){
            if(!NIC_dependcy_table[i].return_flag)
                return false;
            check_flag = true;
        }
    }
    if(check_flag)
        return true;
    // std::cerr << "The dependcy tensor " << tensor_id << " is not on the list." << std::endl;
    // printf("Dependcy table list: (checking tensor = %d)\n", tensor_id);
    // for(int i = 0;i<NIC_dependcy_table.size();i++){
    //     printf("tensor_id : %d, nic_id : %d, packet_count : %d, return_flag : %d\n", NIC_dependcy_table[i].tensor_id, NIC_dependcy_table[i].nic_id, NIC_dependcy_table[i].packet_count, NIC_dependcy_table[i].return_flag);
    // }
    // printf("--------\n");
    return false;
}

void GlobalDependcyTableNIC::reducePacketNum(int local_id, int tensor_id, int packet_num){
    pthread_mutex_lock(&mutex);
    for(int i = 0; i < NIC_dependcy_table.size(); i++){
        if(NIC_dependcy_table[i].tensor_id == tensor_id && NIC_dependcy_table[i].nic_id == local_id){
            if(NIC_dependcy_table[i].return_flag){
                std::cerr << "The dependcy tensor " << tensor_id << " is already returned while there is still packet sent back." << std::endl;
                std::cerr << "There must be a miscalculation of this tensor. Exiting the program. " << std::endl;
                exit(EXIT_FAILURE); // Terminate the program with a failure exit code
            }
            // printf("(NIC%d) Packet count of tensor id %d = %d (total = %d)\n", local_id%(int)log2(PE_NUM), tensor_id, NIC_dependcy_table[i].packet_count, packet_num);
            NIC_dependcy_table[i].packet_count--;
            if(NIC_dependcy_table[i].packet_count == 0){
                NIC_dependcy_table[i].return_flag = true;
                printf("(Write) Tensor id %d is ALL returned to NIC %d\n", tensor_id, local_id);
            }
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}