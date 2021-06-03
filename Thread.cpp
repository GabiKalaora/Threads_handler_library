#include "Thread.h"


Thread::Thread() : _tid(0), state(RUNNING), quantum(1){}

Thread::Thread(int tid, void(*f)(void)) : _tid(tid), state(READY), quantum(0) // init list for fields
{

    address_t sp, pc;
    sp = (address_t)stack + STACK_SIZE - sizeof(address_t);
    pc = (address_t)f;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
}

int Thread::uthread_get_tid() const {
    return _tid;
}

sigjmp_buf& Thread::get_env() {
    return env;
}

State Thread::get_state() const {
    return state;
}

int Thread::get_quantum() const {
    return quantum;
}

int Thread::get_mutex_blocked_state() const {
    return blocked_by_mutex;
}

void Thread::set_state(State new_state) {
    state = new_state;
}

void Thread::inc_quantum() {
    quantum++;
}

void Thread::set_mutex_blocked_state(bool state) {
    blocked_by_mutex = state;
}

Thread::~Thread() {
    delete[] &stack;
}

