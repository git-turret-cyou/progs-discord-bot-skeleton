#ifndef DBS_COMMANDS_H
#define DBS_COMMANDS_H

enum CommandType {
    COMMAND_CHAT_INPUT = 1,
    COMMAND_USER = 2,
    COMMAND_MESSAGE = 3,
    COMMAND_PRIMARY_ENTRY_POINT = 4
};

enum CommandOptionType {
    OPTION_SUB_COMMAND = 1,
    OPTION_SUB_COMMAND_GROUP = 2,
    OPTION_STRING = 3,
    OPTION_INTEGER = 4,
    OPTION_BOOLEAN = 5,
    OPTION_USER = 6,
    OPTION_CHANNEL = 7,
    OPTION_ROLE = 8,
    OPTION_MENTIONABLE = 9,
    OPTION_NUMBER = 10,
    OPTION_ATTACHMENT = 11
};

struct command_option {
    enum CommandOptionType type;
    char *name;
    char *description;
    int required;
    char **choices;
    int **choices_int;
    struct command_option **options;
    int channel_types;
    int min_value;
    int max_value;
    int min_length;
    int max_length;
    int autocomplete;
};
typedef struct command_option CommandOption;

struct command {
    enum CommandType type;
    char *name;
    char *description;
    CommandOption *options;
    void (*callback)(cJSON*);
};
typedef struct command Command;

extern Command *commands;

#define declare_command(command) \
    void command_register_##command(void) { \
        command ## _command.callback = &command; \
        arrput(commands, command ## _command); \
    } \
    l3_initcall(command_register_##command)

#endif
