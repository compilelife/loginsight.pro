#include <iostream>
#include "stdout.h"
#include "controller.h"
#include "def.h"
#include "eventloop.h"

int main(int argc, char** argv)
{    
#ifdef EVTHREAD_USE_PTHREADS_IMPLEMENTED
    evthread_use_pthreads();
#elif EVTHREAD_USE_WINDOWS_THREADS_IMPLEMENTED
    evthread_use_windows_threads();
#endif

    Controller controller;
    controller.start();
    
    EventLoop::instance().start();

    return 0;
}
