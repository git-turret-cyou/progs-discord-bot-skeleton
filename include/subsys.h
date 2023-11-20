#ifndef __SUBSYS_H
#define __SUBSYS_H

int __impl_start_subsystem(char *name, int (*fn)(void));
#define start_subsystem(fn) __impl_start_subsystem(#fn, fn)

#endif
