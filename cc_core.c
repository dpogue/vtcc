
#include "cc_core.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>


struct _CyanChat {
    CCClient* ui;
    int socket;
    char* last_userlist;
    char* nickname;
    char* tmp_nickname;
};

struct _CCUser {
    int level;
    char* nickname;
    char* address;
};

/* Private function prototypes */
static void cc_send_welcome(CyanChat* cc);
static void cc_send_logout(CyanChat* cc);

CyanChat* cc_new(CCClient* ui) {
    CyanChat* cc = NULL;

    cc = malloc(sizeof(CyanChat));
    cc->ui = ui;
    cc->socket = -1;
    cc->last_userlist = NULL;
    cc->nickname = NULL;
    cc->tmp_nickname = NULL;

    return cc;
}

void cc_free(CyanChat** cc) {
    if (*cc == NULL) {
        return;
    }

    if ((*cc)->socket != -1) {
        cc_send_logout(*cc);

        shutdown((*cc)->socket, SHUT_RDWR);
        close((*cc)->socket);
        (*cc)->socket = -1;
    }

    if ((*cc)->tmp_nickname != NULL) {
        free((*cc)->tmp_nickname);
    }

    if ((*cc)->nickname != NULL) {
        free((*cc)->nickname);
    }

    free(*cc);
    *cc = NULL;
}

int cc_connect(CyanChat* cc, const char* host, unsigned short port) {
    struct hostent* hp;
    struct sockaddr_in server;
    int sd = -1;

    if (cc == NULL || host == NULL) {
        return 0;
    }

    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        return 0;
    }

    memset(&server, 0, sizeof(struct sockaddr_in));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);

    if ((hp = gethostbyname(host)) == NULL) {
        return 0;
    }

    memcpy((char*)&server.sin_addr, hp->h_addr, hp->h_length);

    if (connect(sd, (struct sockaddr*)&server, sizeof(server)) == -1) {
        return 0;
    }

    cc->socket = sd;

    cc_send_welcome(cc);

    return 1;
}

void cc_set_socket(CyanChat* cc, int fd) {
    if (cc == NULL) {
        return;
    }

    cc->socket = fd;

    cc_send_welcome(cc);
}

int cc_get_socket(CyanChat* cc) {
    if (cc == NULL) {
        return -1;
    }

    return cc->socket;
}

void cc_parse(CyanChat* cc, char* packet) {
    char* token;
    char* parsable;
    char* tmp;
    int cmd;

    if (cc == NULL || packet == NULL) {
        return;
    }

    tmp = parsable = strdup(packet);

    token = strtok(parsable, "|");
    cmd = atoi(token);

    switch (cmd) {
        case 10:
        {
            if (cc->tmp_nickname != NULL) {
                free(cc->tmp_nickname);
            }
            cc->tmp_nickname = NULL;
            break;
        }
        case 11:
        {
            if (cc->tmp_nickname == NULL) {
                /* This is bad */
                break;
            }
            cc->nickname = cc->tmp_nickname;
            cc->tmp_nickname = NULL;
            break;
        }
        case 21:
        {
            char* username = strtok(NULL, "|");
            CCUser* user = cc_user_from_username(username);
            char* message = strtok(NULL, "\r\n");
            int type = message[1] - '0';
            message += 2;

            if (cc->ui->on_private_msg != NULL) {
                cc->ui->on_private_msg(user, type, message);
            }
            cc_user_free(&user);
            break;
        }
        case 31:
        {
            char* username = strtok(NULL, "|");
            CCUser* user = cc_user_from_username(username);
            char* message = strtok(NULL, "\r\n");
            int type = message[1] - '0';
            message += 2;

            if (cc->ui->on_broadcast_msg != NULL) {
                cc->ui->on_broadcast_msg(user, type, message);
            }
            cc_user_free(&user);
            break;
        }
        case 35:
        {
            size_t count = 0;
            size_t i = 0;
            CCUser** users = NULL;

            if (cc->last_userlist == NULL) {
                cc->last_userlist = strdup(packet);
            } else {
                if (strcmp(cc->last_userlist, packet) == 0) {
                    break;
                } else {
                    free(cc->last_userlist);
                    cc->last_userlist = strdup(packet);
                }
            }

            for (i = 0; i < strlen(packet); i++) {
                /* Count the number of user. It's the number of | characters */
                if (packet[i] == '|') {
                    count++;
                }
            }

            users = (CCUser**)malloc(count * sizeof(CCUser*));

            /* Parse the users */
            i = 0;
            token = strtok(NULL, "|");
            while (token != NULL) {
                users[i] = cc_user_from_username(token);
                i++;
                token = strtok(NULL, "|");
            }

            if (cc->ui->on_userlist != NULL) {
                cc->ui->on_userlist(i, users);
            }

            for (; i > 0; i--) {
                cc_user_free(&(users[i-1]));
            }
            free(users);
            break;
        }
        case 40:
        {
            token = strtok(NULL, "\r\n");
            if (token[0] == '1') {
                token++;
            }

            if (cc->ui->on_broadcast_msg != NULL) {
                CCUser* user = malloc(sizeof(CCUser));
                user->level = 2;
                user->nickname = strdup("ChatServer");
                user->address = NULL;

                cc->ui->on_broadcast_msg(user, 1, token);

                cc_user_free(&user);
            }
            break;
        }
        default:
            fprintf(stderr, "2 - %s\n", packet);
    }

    free(tmp);
}

void cc_set_nickname(CyanChat* cc, const char* nick) {
    char* msg = NULL;

    if (cc == NULL || cc->socket == -1 || nick == NULL) {
        return;
    }

    msg = malloc(strlen(nick) + 16);
    sprintf(msg, "10|%s\r\n", nick);

    cc->tmp_nickname = strdup(nick);

    write(cc->socket, msg, strlen(msg));

    free(msg);
}

int cc_has_nickname(CyanChat* cc) {
    if (cc == NULL) {
        return 0;
    }

    return (cc->nickname == NULL) ? 0 : 1;
}

void cc_send_broadcast(CyanChat* cc, const char* line) {
    char* msg = NULL;

    if (cc == NULL || cc->socket == -1 || line == NULL) {
        return;
    }

    msg = malloc(strlen(line) + 16);
    sprintf(msg, "30|^1%s\r\n", line);

    write(cc->socket, msg, strlen(msg));

    free(msg);
}

/* Private functions */
void cc_send_welcome(CyanChat* cc) {
    char* msg = NULL;

    if (cc == NULL || cc->socket == -1) {
        return;
    }

    msg = malloc(16);
    sprintf(msg, "40|1\r\n");

    write(cc->socket, msg, strlen(msg));

    free(msg);
}

void cc_send_logout(CyanChat* cc) {
    char* msg = NULL;

    if (cc == NULL || cc->socket == -1 || cc->nickname == NULL) {
        return;
    }

    msg = malloc(16);
    sprintf(msg, "15\r\n");

    write(cc->socket, msg, strlen(msg));

    free(msg);
}


/***************************************************************************** 
 ** CyanChat User Functions
 *****************************************************************************/
CCUser* cc_user_from_username(const char* username) {
    char* user = NULL;
    char* tmp = NULL;
    int level = 0;
    char* nick = NULL;
    char* addr = NULL;
    size_t i = 0;
    CCUser* ccuser = NULL;

    if (username == NULL) {
        return NULL;
    }

    tmp = user = strdup(username); /* Work on a copy of the string */

    level = (int)(tmp[0] - '0');
    tmp++;

    for (i = 0; tmp[i] != ',' && tmp[i] != '\0'; i++) { }
    nick = strndup(tmp, i);
    if (i++ != strlen(tmp)) {
        tmp += i;
        addr = strdup(tmp);
    }

    free(user);

    ccuser = (CCUser*)malloc(sizeof(CCUser));
    ccuser->level = level;
    ccuser->nickname = nick;
    ccuser->address = addr;

    return ccuser;
}

void cc_user_free(CCUser** user) {
    if (*user == NULL) {
        return;
    }

    if ((*user)->nickname) {
        free((*user)->nickname);
    }

    if ((*user)->address) {
        free((*user)->address);
    }

    free(*user);
    *user = NULL;
}

int cc_user_get_level(CCUser* user) {
    if (user == NULL) {
        return -1;
    }

    return user->level;
}

const char* cc_user_get_nickname(CCUser* user) {
    if (user == NULL) {
        return NULL;
    }

    return user->nickname;
}

const char* cc_user_get_address(CCUser* user) {
    if (user == NULL) {
        return NULL;
    }

    return user->address;
}

const char* cc_user_get_username(CCUser* user) {
    size_t length;
    char* username;

    if (user == NULL) {
        return NULL;
    }

    length = strlen(user->nickname) + strlen(user->address) + 2;
    username = malloc(length);

    sprintf(username, "%d%s,%s", user->level, user->nickname, user->address);

    return username;
}
