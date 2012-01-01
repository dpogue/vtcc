
#ifndef VTCC_H
#define VTCC_H

#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <readline/readline.h>
#include "cc_core.h"

#define kNormal     "\033[m"
#define kWhite      "\033[1;37m"
#define kCyan       "\033[1;36m"
#define kGreen      "\033[1;32m"
#define kRed        "\033[1;31m"
#define kGold       "\033[1;33m"
#define kMagenta    "\033[1;35m"

void on_input(char* line);

void on_userlist(size_t count, CCUser** users);
void on_broadcast_msg(CCUser* user, int type, const char* msg);
void on_private_msg(CCUser* user, int type, const char* msg);

#endif
