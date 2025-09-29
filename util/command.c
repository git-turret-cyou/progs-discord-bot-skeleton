#include <stddef.h>

#include <cJSON.h>
#include <stb_ds.h>

#include <dbs/commands.h>
#include <dbs/init.h>
#include <dbs/log.h>

Command *commands = NULL;

void register_commands() {
    for(int i = 0; i < arrlen(commands); ++i) {
        print("command %s: %s (@%x)", commands[i].name, commands[i].description, (long long)commands[i].callback);
    }
}
l4_initcall(register_commands);
