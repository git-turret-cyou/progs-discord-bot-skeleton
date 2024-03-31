#ifndef __SUBSYS_H
#define __SUBSYS_H

#include <dbs/init.h>

int __impl_start_subsystem(char *name, int (*fn)(void));
#define start_subsystem(fn) __impl_start_subsystem(#fn, fn)
#define declare_subsystem(fn) \
    void subsys_start_##fn(void) { \
        start_subsystem(fn); \
    } \
    l5_initcall(subsys_start_##fn)

#endif
