#include <iostream>
#include "uthreads.h"
#include "Mutex.h"
#include "Mutex.cpp"
#include "Thread.cpp"
#include <map>
#include <list>
#include <queue>
#include <algorithm>


#define SUCCESS (0)
#define FAILURE (-1)
#define MAIN_TID (0)
#define INVALID_QUANTUM ("quantum needs to be positive int")
#define SIGNAL_MASKING_ERROR ("failed masking signal")
#define SIGNAL_UNMASKING_ERROR ("failed unmasking signal")
#define TIMER_ERROR ("failed setting timer")
#define SIGNAL_ACTION_ERROR ("examine and change a signal action failed")
#define ERROR_GETTING_VALID_FREE_ID ("max num of id is reached")
#define INVALID_TID ("given id is invalid")
#define MAIN_TID_BLOCK ("cant block main tid")
#define INVALID_ID ("invalid thread id")
#define INVALID_LOCK ("the mutex is already locked by the current thread")
#define INVALID_UNLOCK ("the mutex is already unlock")


using namespace std;

int next_thread_num;
int curr_running_thread_id;
int quantum_size;
int total_quantums = 1;
sigset_t cur_set;
map<int, Thread*> all_threads;
list<int> ready_thread_id;

list<int> threads_want_mutex_list;

priority_queue <int, vector<int>, greater<int>> min_thread_id;

Mutex* mutex;

struct sigaction sa;
struct itimerval timer;



void sys_error_handler(const string& error_msg){
    cerr << "system error: " + error_msg << endl;
    exit(1);
}

void lib_error_handler(const string& error_msg){
    cerr << "thread library error: " + error_msg << endl;
}


void block_signal(){
    int err_return_value = -1;

    if (sigprocmask(SIG_SETMASK, &cur_set, nullptr) == err_return_value){
        sys_error_handler(SIGNAL_MASKING_ERROR);
        exit(1);
    }
}


void unblock_signal(){
    int did_unblock = sigprocmask(SIG_UNBLOCK, &cur_set, nullptr);

    if (did_unblock == -1){
        sys_error_handler(SIGNAL_UNMASKING_ERROR);
        exit(1);
    }
}


bool invalid_tid(int tid){
    if (tid < 0 || tid > MAX_THREAD_NUM) {
        return true;
    }
    if (all_threads.find(tid) == all_threads.end()){
        return true;
    }
    return false;
}


void delete_entire_process(){
//    for (auto & it : all_threads)
//    {
//        delete it.second;
//    }
}


void delete_tid(int tid){
// TODO: dont forget to free memory

 //   auto temp = all_threads[tid];
//    delete[] temp;
    all_threads.erase(tid);
    min_thread_id.push(tid);
    if (find(ready_thread_id.begin(), ready_thread_id.end(), tid) != ready_thread_id.end()){
        ready_thread_id.remove(tid);
    }

    if (find(threads_want_mutex_list.begin(), threads_want_mutex_list.end(), tid) != threads_want_mutex_list.end()) {
        threads_want_mutex_list.remove(tid);
    }
}


int get_min_available_ind(){
    int min_id = next_thread_num;
    if (!min_thread_id.empty()){
        min_id = min_thread_id.top();
        min_thread_id.pop();
    }

    if (min_id == next_thread_num){
        next_thread_num++;
    }
    return min_id;
}


void thread_handler(int signal_id){
    block_signal();
    int ret_val = sigsetjmp(all_threads[curr_running_thread_id]->get_env(), 1);

    if (ret_val == 1) {
        return;
    }


    if (all_threads[curr_running_thread_id]->get_state() == RUNNING){
        // take curr running thread and add to ready_thread queue and change its state to ready
        all_threads[curr_running_thread_id]->set_state(READY);
        ready_thread_id.push_back(curr_running_thread_id);
        ready_thread_id.pop_front();
        curr_running_thread_id = ready_thread_id.front();
        all_threads[curr_running_thread_id]->set_state(RUNNING);
    }

    else if (all_threads[curr_running_thread_id]->get_state() == BLOCKED){
        ready_thread_id.pop_front();
        curr_running_thread_id = ready_thread_id.front();
        all_threads[curr_running_thread_id]->set_state(RUNNING);

    }

    else if (all_threads[curr_running_thread_id]->get_state() == TERMINATED){
        ready_thread_id.pop_front();
        delete_tid(curr_running_thread_id);
        curr_running_thread_id = ready_thread_id.front();
        all_threads[curr_running_thread_id]->set_state(RUNNING);
    }
    else if(all_threads[curr_running_thread_id]->get_state() == READY){
        curr_running_thread_id = ready_thread_id.front();
        all_threads[curr_running_thread_id]->set_state(RUNNING);
    }

    all_threads[curr_running_thread_id]->inc_quantum();
    total_quantums++;

    unblock_signal();
    setitimer(ITIMER_VIRTUAL, &timer, nullptr);
    siglongjmp(all_threads[curr_running_thread_id]->get_env(), 1);
}


void set_timer() {
    // TODO: do I need to take care of seconds timer ?
    timer.it_interval.tv_usec = quantum_size % 1000000;
    timer.it_interval.tv_sec = quantum_size / 1000000;

    timer.it_value.tv_usec = quantum_size % 1000000;
    timer.it_value.tv_sec = quantum_size / 1000000;

    if (setitimer(ITIMER_VIRTUAL, &timer, nullptr) < 0){
        sys_error_handler(TIMER_ERROR);
    }

}


int uthread_init(int quantum_usecs){
    quantum_size = quantum_usecs;

    block_signal();
    if (quantum_usecs <= 0){
        lib_error_handler(INVALID_QUANTUM);
        unblock_signal();
        return FAILURE;
    }

    // init main thread
    all_threads[MAIN_TID] = new Thread();
    curr_running_thread_id = MAIN_TID;
    ready_thread_id.push_back(MAIN_TID);
    next_thread_num = 1;


    sigemptyset(&cur_set);
    sigaddset(&cur_set, SIGVTALRM);
    sa.sa_handler = &thread_handler;

    if (sigaction(SIGVTALRM, &sa, nullptr) < 0){
        sys_error_handler(SIGNAL_ACTION_ERROR);
    }
    // init clock
    set_timer();

    /** init mutex from H **/
    mutex = new Mutex();

    unblock_signal();
    return SUCCESS;
}


int uthread_spawn(void (*f)(void)){
    block_signal();
    int new_id = get_min_available_ind();

    // if id is in valid
    if (new_id >= MAX_THREAD_NUM){
        lib_error_handler(ERROR_GETTING_VALID_FREE_ID);
        unblock_signal();
        return FAILURE;
    }

    all_threads[new_id] = new Thread(new_id, f);
    ready_thread_id.push_back(new_id);
    unblock_signal();
    return new_id;
}


int uthread_terminate(int tid){
    block_signal();

    // check if tid valid
    if (invalid_tid(tid)){
        lib_error_handler(INVALID_TID);
        unblock_signal();
        return FAILURE;
    }
    /** unlock mutex if thread to terminate is blocking mutex from H**/
    if (mutex->get_id_thread_lock() == tid)
    {
        uthread_mutex_unlock();
    }

    if (tid == MAIN_TID){
        delete_entire_process();
        unblock_signal();
        exit(SUCCESS);
    }

    if (tid == curr_running_thread_id){
        // TODO: if this thread is mutex blocking then unlock mutex and then delete
        all_threads[curr_running_thread_id]->set_state(TERMINATED);
        unblock_signal();
        thread_handler(SIGVTALRM);
        delete_tid(tid);
    }

    // thread is valid and is not main_tid or running tid
    else{
        delete_tid(tid);
    }

    /** unlock mutex if thread to terminate is blocking mutex from H**/
    if (mutex->get_id_thread_lock() == tid)
    {
        uthread_mutex_unlock();
    }

    unblock_signal();
    return SUCCESS;
}


int uthread_block(int tid){
    block_signal();

    // tid is invalid
    if (invalid_tid(tid)){
        lib_error_handler(INVALID_TID);
        unblock_signal();
        return FAILURE;
    }
    // tid is  MAIN_TID
    else if (tid == MAIN_TID){
        lib_error_handler(MAIN_TID_BLOCK);
        unblock_signal();
        return FAILURE;
    }

    // tid is curr_running_thread
    else if (tid == curr_running_thread_id){
        all_threads[curr_running_thread_id]->set_state(BLOCKED);
        unblock_signal();
        thread_handler(SIGVTALRM);
    }
    // tid is valid and its not MAIN_TID nor curr_running_tid hence all is left is to remove from ready queue
    else{
        all_threads[tid]->set_state(BLOCKED);
        if (find(ready_thread_id.begin(), ready_thread_id.end(), tid) != ready_thread_id.end()){
            ready_thread_id.remove(tid);
        }
    }
    unblock_signal();
    return SUCCESS;
}


int uthread_resume(int tid){
    block_signal();

    //tid is invalid
    if (invalid_tid(tid)){
        lib_error_handler(INVALID_ID);
        unblock_signal();
        return FAILURE;
    }

    // that's the only interesting case: when tid is valid and in blocked state
    if (all_threads[tid]->get_state() == BLOCKED){
        all_threads[tid]->set_state(READY);
        if (!all_threads[tid]->get_mutex_blocked_state()) {
            ready_thread_id.push_back(tid);
        }
    }

    unblock_signal();
    return SUCCESS;
}


int uthread_get_tid(){
    return curr_running_thread_id;
}


int uthread_get_total_quantums(){
    return total_quantums;
}


int uthread_get_quantums(int tid){
    if (invalid_tid(tid)) {
        lib_error_handler(INVALID_ID);
        return FAILURE;
    }
    return all_threads[tid]->get_quantum();
}


int uthread_mutex_lock() {
    block_signal();
    int thread_id_want_block = curr_running_thread_id;

    if (mutex->get_id_thread_lock() == thread_id_want_block){
        lib_error_handler(INVALID_LOCK);
        unblock_signal();
        return FAILURE;
    }

    if (mutex->get_mutex_state() == LOCK){
        all_threads[thread_id_want_block]->set_mutex_blocked_state(true);


        bool in_ready_list = (find(ready_thread_id.begin(), ready_thread_id.end(),
                                   thread_id_want_block) != ready_thread_id.end());
        if (in_ready_list) {
            ready_thread_id.remove(thread_id_want_block);
        }

        if (find(threads_want_mutex_list.begin(), threads_want_mutex_list.end(), thread_id_want_block)
                == threads_want_mutex_list.end()){
            threads_want_mutex_list.push_back(thread_id_want_block);
        }
        all_threads[thread_id_want_block]->set_state(READY);
        unblock_signal();
        thread_handler(SIGVTALRM);

    }

    // if mutex is unlocked just lock it and continue with life
    else if (mutex->get_mutex_state() == UNLOCK){
        mutex->set_state(LOCK);
        mutex->set_id_thread_lock(thread_id_want_block);
        unblock_signal();
        return SUCCESS;
    }


    unblock_signal();
    return SUCCESS;
}


int uthread_mutex_unlock() {

    block_signal();
    if (mutex->get_mutex_state() == UNLOCK || mutex->get_id_thread_lock() != curr_running_thread_id) {
        lib_error_handler(INVALID_UNLOCK);
        unblock_signal();
        return FAILURE;
    }

    if (threads_want_mutex_list.empty()) {
        mutex->set_state(UNLOCK);
        mutex->set_id_thread_lock(-1);
    }

    else {
        int next_thread_use_mutex = threads_want_mutex_list.front();
        threads_want_mutex_list.pop_front();
        all_threads[next_thread_use_mutex]->set_mutex_blocked_state(false);

        if (all_threads[next_thread_use_mutex]->get_state() == READY) {
            ready_thread_id.push_back(next_thread_use_mutex);
        }
    }

    unblock_signal();
    return SUCCESS;
}

