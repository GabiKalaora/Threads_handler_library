#include <stdio.h>
#include <setjmp.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#ifndef EX2_OS_THREADS_H
#define EX2_OS_THREADS_H

#define STACK_SIZE 8192

#ifdef __x86_64__

#define JB_SP 6
#define JB_PC 7
typedef unsigned long address_t;

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}
#else

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
		"rol    $0x9,%0\n"
                 : "=g" (ret)
                 : "0" (addr));
    return ret;
}

#endif

/**
 * A translation is required when using an address of a variable.
 * @param addr address to translate
 * @return valid address to use
 */


enum State{
    RUNNING, READY, BLOCKED, TERMINATED
};

class Thread{

private:
    int _tid;
    char stack[STACK_SIZE];
    sigjmp_buf env;
    State state;
    int quantum;
    bool mutex_block;


public:
    // constructor for main thread with id == 0
    Thread();

    // constructor
    Thread(int tid, void(*f)(void));// init list for fields

    // destructor
    ~Thread();

    // getters
    int uthread_get_tid() const;

    sigjmp_buf& get_env();

    State get_state() const;

    int get_quantum() const;

    int get_mutex_blocked_state() const;


    //setters and adders
    void set_state(State new_state);

    void inc_quantum();

    void set_mutex_blocked_state(bool state);
};

#endif //EX2_OS_THREADS_H
