#include <unistd.h>

#include <init.h>
#include <log.h>
#include <subsys.h>

int net_subsystem(void)
{
    print(LOG_NOTICE "net: starting net subsystem");
    usleep(10000); // do net stuff
    return 0;
}

void net_initcall()
{
    start_subsystem(net_subsystem);
}
l1_initcall(net_initcall);
