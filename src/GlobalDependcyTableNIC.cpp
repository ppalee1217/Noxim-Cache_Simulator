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