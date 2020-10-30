#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "xuake.h"
#include "ft.h"
#include "term.h"

static struct xuake_server *s = NULL;

void
handle_chld(int sig)
{
    pid_t p;
    char **args;
    char *prog;
    int r;

    p = waitpid(-1, &r, WNOHANG);

    fprintf(stderr, "SIGCHLD recieved! (%d;%d)\n", p, s ? s->xkt.child : -1);
    if (!s->initialized) {
        fprintf(stderr, "Exiting!\n");
        exit(0);
    }

    if (s && p == s->xkt.child) {
        // XXX: need rate limiting
        if (s->conf.xkt.respawn) {
            fprintf(stderr, "Respawning!\n");

            p = xkterm_forkpty(&s->xkt);

            if (p == 0) {
                exec_startcmd(s->conf.xkt.cmd); // never returns;
                exit(125);
            }

            s->xkt.child = p;
        } else {
            fprintf(stderr, "Exiting!\n");
            wl_display_terminate(s->wl_display);
        }
    } else
        fprintf(stderr, "Not Exiting!\n");
    signal(SIGCHLD, handle_chld);
}

void
handle_segv(int sig)
{
    static struct xuake_server *stmp;
    fprintf(stderr, "SIGSEGV recieved!\n");
    if (!s)
        exit(255);
    stmp = s;
    s = NULL; // prevent infinite segfault loop
    wl_display_terminate(stmp->wl_display);
    wl_display_destroy_clients(stmp->wl_display);
    wl_display_destroy(stmp->wl_display);
    exit(127);
}

void
handle_usr1(int sig)
{
    if (s)
        s->conf.xkt.respawn = 0;
    signal(SIGUSR1, handle_usr1);
}

void
init_signals(struct xuake_server *server)
{
    s = server;

    //signal(SIGSEGV, handle_segv);
    signal(SIGCHLD, handle_chld);
    signal(SIGUSR1, handle_usr1);
}
