#include <iostream>
#include "event.h"
#include "stdout.h"
#include "monitorlog.h"
#include "eventloop.h"
#include "pingtask.h"

using namespace std;

int main(int argc, char** argv)
{    
    auto& evloop = EventLoop::instance();

    auto evbase = evloop.base();
    evloop.start();
    return 0;
}
