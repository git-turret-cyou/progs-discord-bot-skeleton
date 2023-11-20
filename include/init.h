#ifndef __INIT_H
#define __INIT_H

typedef int (*initcall_t)(void);
typedef initcall_t initcall_entry_t;

#define __define_initcall(fn, id) \
    static initcall_t __initcall_##fn##id \
    __attribute__((used)) \
    __attribute__((section(".initcall" #id ".init"))) = fn

#define l1_initcall(fn) __define_initcall(fn, 1)
#define l2_initcall(fn) __define_initcall(fn, 2)
#define l3_initcall(fn) __define_initcall(fn, 3)
#define l4_initcall(fn) __define_initcall(fn, 4)
#define l5_initcall(fn) __define_initcall(fn, 5)

static inline initcall_t initcall_from_entry(initcall_entry_t *entry)
{
    return *entry;
}

#endif
