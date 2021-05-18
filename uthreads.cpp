#include <iostream>
#include "uthreads.h"
#include "Mutex.h"
#include "Mutex.cpp"
#include "Thread.h"
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
map<int, Thread*> all_threads;
list<int> ready_thread_id;
list<int> blocked_by_mutex_list;
priority_queue <int, vector<int>, greater<int>> min_thread_id;
Mutex *mutex;

struct sigaction sa;
struct itimerval timer;


void sys_error_handler(const string& error_msg){
    cerr << "system error: " + error_msg << endl;
    exit(EXIT_FAILURE);
}

void lib_error_handler(const string& error_msg){
    cerr << "thread library error: " + error_msg << endl;
}


void block_signal(){
    int did_block = sigprocmask(SIG_BLOCK, &sa.sa_mask, NULL);

    if (did_block == -1){
        sys_error_handler(SIGNAL_MASKING_ERROR);
    }
}


void unblock_signal(){
    int did_unblock = sigprocmask(SIG_UNBLOCK, &sa.sa_mask, NULL);

    if (did_unblock == -1){
        sys_error_handler(SIGNAL_UNMASKING_ERROR);
    }
}


//int set_env_thread(int thread_to_activate_id){
//    int ret_env_assignment = sigsetjmp(all_threads[thread_to_activate_id]->get_env(), 1);
//    if (ret_env_assignment == 1){
//        unblock_signal();
//        return 1;
//    }
//    return 0;
//}



bool invalid_tid(int tid){
    //TODO: check if id is initialized and in all_thread
    if (all_threads.find(tid) == all_threads.end()){
        return true;
    }
    return false;
}


void delete_entire_process(){

}


void delete_tid(int tid){
    // TODO: erase tid thread from all_threads and from ready threads and add this id to free id's
    auto temp = all_threads[tid];
    all_threads.erase(tid);
    delete temp;
    min_thread_id.push(tid);
    if (find(ready_thread_id.begin(), ready_thread_id.end(), tid) != ready_thread_id.end()){
        ready_thread_id.remove(tid);
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


void update_next_ready_to_curr_thread(){
    // take first ready thread in ready_thread queue and change its state and update it to be curr running thread
    if (!ready_thread_id.empty()){
        curr_running_thread_id = ready_thread_id.front();
        ready_thread_id.pop_front();
        all_threads[curr_running_thread_id]->set_state(RUNNING);
    }
}


void thread_handler(int signal_id){
// TODO: we'll call this func in one of the following cases happen:
//       1. a quantum passes and we want to procceed with next thread in queue
//       2. user called uthread_block(tid) and tid == cur_running_tid
//       3. user called uthread_terminate(tid) and tid == cur_running_tid

// TODO: we'll like that our thread handler will take care of all 2 cases as follows:
//       1. when a thread is in running stage we'll want to extract this thread id from ready queue hence
//          well ask wat is thread state if its running we'll push this thread to the back of the ready queue
//          take the thread from the front of ready queue change it state to running and extract it from
//          ready queue and call siglongjmp on this thread. (THIS CASE IS INVOKED BY CLOCK SIGNAL)
//       2. after a uthread_block(tid) is called and tid == cur_running_tid we'll change state of curr_running_thread
//          to block and call thread_handler(WHAT SIGNAL SHOULD I USE ???) since its the running thread and as
//          explained in section 1. each thread that its state is changed from ready to running we'll extract it
//          from ready queue hence this tid should not be in ready queue, so we reset the timer and call siglongjmp on
//          the front thread in ready queue and as described in section 1. we'll change it state to running and extract
//          the front thread id from ready queue
//       3. similar to section 2. just ni that case we'll need to handle deleting all recourses that we allocated for
//          this thread and as in section 2. we'll need to take care of updating new curr_running_thread and extract it
//          from ready queue etc.

    block_signal();
    total_quantums++;

    if (all_threads[curr_running_thread_id]->get_state() == RUNNING){
        // take curr running thread and add to ready_thread queue and change its state to ready
        ready_thread_id.push_back(curr_running_thread_id);
        // update curr_running_id to the head of ready queue
        update_next_ready_to_curr_thread();
    }

    if (all_threads[curr_running_thread_id]->get_state() == BLOCKED){
        // take first ready thread in ready_thread queue and change its state and update
        // it to be curr running thread
        update_next_ready_to_curr_thread();
    }

    if (all_threads[curr_running_thread_id]->get_state() == TERMINATED){
        // add curr_running_thread_id to available id's
        min_thread_id.push(curr_running_thread_id);
        update_next_ready_to_curr_thread();
    }

    // if quantum is calculated upper value
    all_threads[curr_running_thread_id]->inc_quantum();
    unblock_signal();
    siglongjmp(all_threads[curr_running_thread_id]->get_env(), 1);
}


void set_timer() {
    // TODO: do I need to take care of seconds timer ?
    timer.it_interval.tv_usec = quantum_size;
    timer.it_interval.tv_sec = 0;
    timer.it_value.tv_usec = quantum_size;
    timer.it_value.tv_sec = 0;

    if (setitimer(ITIMER_VIRTUAL, &timer, NULL)){
        unblock_signal(); // needed ?
        sys_error_handler(TIMER_ERROR);
    }

    if (sigaction(SIGVTALRM, &sa, NULL)){
        sys_error_handler(SIGNAL_ACTION_ERROR);
    }
}


int uthread_init(int quantum_usecs){
    block_signal();
    if (quantum_usecs <= 0){
        lib_error_handler(INVALID_QUANTUM);
        unblock_signal();
        return FAILURE;
    }

    // init main thread
    all_threads[0] = new Thread();
    curr_running_thread_id = MAIN_TID;
    ready_thread_id.push_back(MAIN_TID);
    next_thread_num = 1;

    sa.sa_handler = &thread_handler;
    // init clock
    set_timer();
    // assign threads handler algorithm to sa.sa_handler
    quantum_size = quantum_usecs;
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

    if (tid == MAIN_TID){
        delete_entire_process();
        unblock_signal();
        exit(SUCCESS);
    }


    if (tid == curr_running_thread_id){
        // TODO: not sure what I should do in this case
        delete_tid(tid);
        unblock_signal();
        set_timer();
        thread_handler(SIGVTALRM); // should i call this func here or it will be activated auto
    }

    // thread is valid and is not main_tid or running tid
    else{
        delete_tid(tid);
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
    all_threads[tid]->set_state(BLOCKED);
    // tid is  MAIN_TID
    if (tid == MAIN_TID){
        lib_error_handler(MAIN_TID_BLOCK);
        unblock_signal();
        return FAILURE;
    }
    // tis is curr_running_ thread
    if (tid == curr_running_thread_id){
        // TODO: not sure what I should do in this case
        unblock_signal();
        set_timer();
        thread_handler(SIGVTALRM);
    }
    // tid is valid and its not MAIN_TID nor curr_running_tid hence all is left is to remove from ready queue
    else{
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
        ready_thread_id.push_back(tid);
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
    return all_threads[tid]->get_quantum();
}

// TODO:

// TODO: MUTEX QUESTIONS !!
//     1. synchronize between regular block and mutex blocked same for unblocking
//     2. what if i tearminated the id that is mutex blocking threads

int uthread_mutex_lock() {
    block_signal();
    int thread_id_want_block = curr_running_thread_id;

    if (mutex->get_state() == UNLOCK){
        mutex->set_state(LOCK);
        mutex->set_id_thread_lock(thread_id_want_block);
        unblock_signal();
        return SUCCESS;
    }

    if (mutex->get_id_thread_lock() == thread_id_want_block){
        lib_error_handler(INVALID_LOCK);
        unblock_signal();
        return FAILURE;
    }

    all_threads[thread_id_want_block]->set_mutex_blocked_state(true);
    blocked_by_mutex_list.push_back(thread_id_want_block);
    //TODO: "In the future when this thread will be back to RUNNING state,
    // it will try again to acquire the mutex."
    unblock_signal();
    return SUCCESS;
}


int uthread_mutex_unlock() {
    block_signal();
    if (mutex->get_state() == UNLOCK) {
        lib_error_handler(INVALID_UNLOCK);
        unblock_signal();
        return FAILURE;
    }

    int thread_id = blocked_by_mutex_list.front();
    blocked_by_mutex_list.pop_front();

    // ???
    uthread_resume(thread_id);

    if (blocked_by_mutex_list.empty()) {
        mutex->set_state(UNLOCK);
    }

    unblock_signal();
    return SUCCESS;
}


//
//int main() {
//    cout << total_quantums << endl;
//    uthread_init(100000);
//    int i = 0;
//
//
//    for (;;) {
////        cout <<(all_threads[0]->get_state())<<endl;
////        all_threads[i]->set_state(TERMINATED);
//        cout << i << endl;
//        uthread_spawn(f);
//        cout <<(all_threads[i]->get_state())<<endl;
//        std::cout << "ex2_OS testing" << std::endl;
//        if (i % 2 == 0){
//            all_threads[i]->set_state(BLOCKED);
//        }
////        if (i % 3 == 0) {
////            uthread_terminate(i);
////        }
//
//        cout <<(all_threads[i]->get_state())<<endl;
//        i++;
//        cout << endl;
//        siglongjmp(all_threads[i]->get_env(), 1);
//    }
//
//    return 0;
//}