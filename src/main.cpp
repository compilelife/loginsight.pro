#include <iostream>
#include "stdout.h"
#include "controller.h"
#include "def.h"
#include "eventloop.h"

int main(int argc, char** argv)
{    
    Controller controller;
    controller.start();
    
    EventLoop::instance().start();

    return 0;
}
