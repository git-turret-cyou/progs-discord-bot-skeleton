#ifndef __EVENT_H
#define __EVENT_H

#include <dbs/init.h>

enum Event {
#define E(ev_name, _) ev_name,
#include <dbs/bits/events.h>
#undef E
};

int ev_set_handler(enum Event event, int (*ev_handler)(cJSON*));

#define declare_event(event, handler) \
    void __decl_##event_##handler() { \
        ev_set_handler(event, handler); \
    } \
    l3_initcall(__decl_##event_##handler)

#endif
