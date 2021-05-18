#include "Mutex.h"


Mutex::Mutex(int tid) : id_thread_lock(tid), state(LOCK){}

int Mutex::get_id_thread_lock() const{
    return id_thread_lock;
}

mutex_state Mutex::get_state() const{
    return state;
}

void Mutex::set_state(mutex_state new_state){
    state = new_state;
}

void Mutex::set_id_thread_lock(int id){
    id_thread_lock = id;
}

//Thread::~Thread() {}
