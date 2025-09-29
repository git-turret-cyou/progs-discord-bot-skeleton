#ifndef __DBS_ABSTRACT_H
#define __DBS_ABSTRACT_H

int interaction_reply(cJSON *i, char *content, int raw);
int interaction_defer_reply(cJSON *i, int hidden);
int interaction_edit_reply(cJSON *i, char *content, int raw);

#endif
