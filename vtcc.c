
#include "vtcc.h"

static void set_up_screen(void);
static void add_line(const char* line);
static const char* get_colour(int level);
static size_t textlen(const char* str);

int running = 1;
CyanChat* cc = NULL;
struct winsize w;

CCClient client = {
    NULL,
    &on_userlist,
    &on_broadcast_msg,
    &on_private_msg
};

int main(int argc, char** argv) {
    char* host = "cho.cyan.com";
    unsigned short port = 1813;
    int c;
    fd_set set;

    set_up_screen();

    while ((c = getopt(argc, argv, "h:p:")) != -1) {
        switch(c) {
            case 'h':
                host = optarg;
                break;
            case 'p':
                port = atoi(optarg);
                break;
            default:
                break;
        }
    }

    cc = cc_new(&client);
    cc_connect(cc, host, port);

    rl_callback_handler_install(NULL, &on_input);

    while (running) {
        int ccsock = cc_get_socket(cc);
        FD_ZERO(&set);
        FD_SET(ccsock, &set);
        FD_SET(STDIN_FILENO, &set);

        select(ccsock + 1, &set, NULL, NULL, NULL);

        if (FD_ISSET(STDIN_FILENO, &set)) {
            rl_callback_read_char();
        }
        if (FD_ISSET(ccsock, &set)) {
            int nread = 0;
            char* buf = malloc(1024);
            char* bp = buf;
            int bsize = 1024;

            while (bsize && (nread = read(ccsock, bp, bsize)) > 0) {
                bp += nread;
                bsize -= nread;

                if (*(bp - 1) == '\n' || bsize == 0) {
                    *bp = '\0';
                    break;
                }
            }

            if (nread == 0) {
            } else if (nread > 0) {
                char* saveptr = NULL;
                char* line = strtok_r(buf, "\n", &saveptr);
                while (line != NULL) {
                    cc_parse(cc, line);
                    line = strtok_r(saveptr, "\n", &saveptr);
                }
            }

            free(buf);
        }
    }

    rl_callback_handler_remove();

    printf("\033[H");
    printf("\033[2J");
    fflush(stdout);

    return 0;
}

void set_up_screen(void) {
    size_t i;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    printf("\033[?6l");
    printf("\033[?7h");
    printf("\033[H");
    printf("\033[2J");
    printf("\033[?4h");
    printf("\033[3;%dr", w.ws_row);
    printf("\033[B");

    for (i = 0; i < w.ws_col; i++) {
        printf("─");
    }

    printf("\033[H");
    printf("\033[2K");

    fflush(stdout);
}

void add_line(const char* line) {
    size_t length = 0;
    int lines = 1;

    length = textlen(line);
    if (length > w.ws_col) {
        lines = 1 + (length / w.ws_col);
    }

    printf("\033[s");
    printf("\033[3;1H");
    printf("\033[%dL", lines);
    printf("\033[3;1H");

    printf("%s", line);

    printf("\033[u");
    fflush(stdout);
}

const char* get_colour(int level) {
    switch (level) {
        case 0:
            return kWhite;
        case 1:
            return kCyan;
        case 2:
            return kGreen;
        case 4:
            return kGold;
        default:
            return kRed;
    }
}

size_t textlen(const char* str) {
    size_t count = 0;
    size_t i = 0;
    int in_escape = 0;

    for (i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\033') {
            in_escape = 1;
        } else if (in_escape && str[i] == 'm') {
            in_escape = 0;
        } else if (in_escape) {
            /* Do NOT increment count */
        } else {
            count++;
        }
    }

    return count;
}

void on_input(char* line) {
    if (line == NULL) {
        running = 0;
        return;
    }

    printf("\033[H");
    printf("\033[2K");
    fflush(stdout);

    if (strlen(line) == 0) {
        return;
    }

    if (strstr(line, "/nick ") == line) {
        cc_set_nickname(cc, line + 6);
    } else if (strstr(line, "/quit") == line) {
        running = 0;
        return;
    } else {
        if (!cc_has_nickname(cc)) {
            add_line("Not Logged in!");
        } else {
            cc_send_broadcast(cc, line);
        }
    }
}

/* CyanChat callbacks */
void on_userlist(size_t count, CCUser** users) {
    size_t i = 0;
    char* output = NULL;

    if (count == 0) {
        return;
    }

    output = malloc((count * 56) + 16);

    sprintf(output, "Chat Users:");

    for (i = 0; i < count; i++) {
        const char* nickname = cc_user_get_nickname(users[i]);
        int level = cc_user_get_level(users[i]);
        char* printed = malloc(56);

        sprintf(printed, " %s[%s]%s", 
                get_colour(level), nickname, kNormal);

        strncat(output, printed, 56);

        free(printed);
    }

    add_line(output);
    free(output);
}

void on_broadcast_msg(CCUser* user, int type, const char* msg) {
    const char* nick = NULL;
    int level = 0;
    char* output = NULL;

    nick = cc_user_get_nickname(user);
    level = cc_user_get_level(user);

    output = malloc(strlen(nick) + strlen(msg) + 92);

    switch (type) {
        case 2:
            sprintf(output, "%s\\\\\\\\\\ %s[%s]%s %s %s/////%s",
                    kGreen, get_colour(level), nick, kNormal, msg,
                    kGreen, kNormal);
            break;
        case 3:
            sprintf(output, "%s///// %s[%s]%s %s %s\\\\\\\\\\%s",
                    kGreen, get_colour(level), nick, kNormal, msg,
                    kGreen, kNormal);
            break;
        default:
            sprintf(output, "%s[%s]%s %s",
                    get_colour(level), nick, kNormal, msg);
    }

    add_line(output);
    free(output);
}

void on_private_msg(CCUser* user, int type, const char* msg) {
    const char* nick = NULL;
    int level = 0;
    char* output = NULL;
    const char* prefix = "Private Message from";

    nick = cc_user_get_nickname(user);
    level = cc_user_get_level(user);

    output = malloc(strlen(prefix) + strlen(nick) + strlen(msg) + 92);

    sprintf(output, "%s%s %s[%s]%s: %s",
            kMagenta, prefix, get_colour(level), nick, kNormal, msg);

    add_line(output);
    free(output);
}
