//
// Created by Kolane Steve on 25-3-4.
//

#include "msg_que.h"

sem_t msg_que::m_sem;
mutex msg_que::m_mutex;

msg_que::msg_que(const char* p_name, int p_block) {
    if (p_name != NULL) {
        m_name = (char*)malloc(strlen(p_name) + 1);
        strcpy(m_name, p_name);
        m_block = p_block;
        sem_init(&msg_que::m_sem, 0, 0);
    }
}

msg_que::~msg_que() {

}

void msg_que::send_msg(const char* dest_name, elements* msg) {
    memset(msg->dest_name, 0, sizeof(msg->dest_name));
    char* p_name = msg->dest_name;
    while (*dest_name != '\0') {
        *p_name++ = *dest_name++;
    }

    m_mutex.lock();
    m_queue.push_back(*msg);
    m_mutex.unlock();
    sem_post(&msg_que::m_sem);
}

bool msg_que::recv_msg(const char* this_name, elements* msg) {
    bool result;
    elements queue_element;
    memset(&queue_element,0x00,sizeof(elements));

    if(m_block == 1) sem_wait(&m_sem);
    else{
        if(sem_trywait(&m_sem) == -1)
            return false;
    }
    m_mutex.lock();
    if(m_queue.empty()) {
        m_mutex.unlock();
        return false;
    }
    queue_element = m_queue.front();
    if(strcmp(queue_element.dest_name,this_name)!=0){
        m_mutex.unlock();
        sem_post(&m_sem);
        return false;
    }
    m_queue.pop_front();

    memcpy(msg,&queue_element,sizeof(elements));

    m_mutex.unlock();
    return true;
}

void msg_que::send_msg(elements *msg)
{
    m_mutex.lock();
    m_queue.push_back(*msg);

    m_mutex.unlock();
    sem_post(&m_sem);
}

bool msg_que::recv_msg(elements *msg)
{
    bool result;

    if(m_block == 1) sem_wait(&m_sem);
    else{
        if(sem_trywait(&m_sem) == -1)
            return false;
    }
    m_mutex.lock();
    if(m_queue.empty()) {
        m_mutex.unlock();
        return false;
    }
    *msg = m_queue.front();
    m_queue.pop_front();

    m_mutex.unlock();
    return true;
}
