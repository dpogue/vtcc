
#ifndef CC_CORE_H
#define CC_CORE_H

#include <stdio.h>
#include <stdlib.h>

typedef struct _CyanChat    CyanChat;
typedef struct _CCClient    CCClient;
typedef struct _CCUser      CCUser;

struct _CCClient {
    void (*on_connect)();

    void (*on_userlist)(size_t, CCUser**);

    void (*on_broadcast_msg)(CCUser*, int, const char*);
    void (*on_private_msg)(CCUser*, int, const char*);
};


CyanChat* cc_new(CCClient* ui);

void cc_free(CyanChat** cc);

int cc_connect(CyanChat* cc, const char* host, unsigned short port);

void cc_set_socket(CyanChat* cc, int fd);

int cc_get_socket(CyanChat* cc);

void cc_parse(CyanChat* cc, char* packet);

void cc_set_nickname(CyanChat* cc, const char* nick);

int cc_has_nickname(CyanChat* cc);

void cc_send_broadcast(CyanChat* cc, const char* line);


/***************************************************************************** 
 ** CyanChat User Functions
 *****************************************************************************/
CCUser* cc_user_from_username(const char* username);

void cc_user_free(CCUser** user);

int cc_user_get_level(CCUser* user);

const char* cc_user_get_nickname(CCUser* user);

const char* cc_user_get_address(CCUser* user);

/* You must free the result! */
const char* cc_user_get_username(CCUser* user);

#endif
