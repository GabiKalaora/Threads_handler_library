//
// Created by hoday on 11/05/2021.
//

#ifndef OS_MUTEX_H
#define OS_MUTEX_H

enum mutex_state{
    LOCK, UNLOCK
};

class Mutex{

private:
    int id_thread_lock;
    mutex_state state;


public:

    // constructor
    Mutex(int id);

//    ~Thread();

    int get_id_thread_lock() const;

    mutex_state get_state() const;

    void set_state(mutex_state new_state);

    void set_id_thread_lock(int id);

};
#endif //OS_MUTEX_H
