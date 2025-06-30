#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <stdio.h>

typedef struct s_client
{
    int id, fd, disconnect;
    char *buf;
    struct s_client *next;
} t_client;

typedef struct s_msg
{
    char *txt;
    int from;
    struct s_msg *next;
} t_msg;

int extract_message(char **buf, char **msg)
{
    *msg = 0;
    if (!*buf) return 0;
    for (int i = 0; (*buf)[i]; i++)
    {
        if ((*buf)[i] == '\n')
        {
            char *newbuf = malloc(strlen(*buf + i + 1) + 1);
            if (!newbuf) return -1;
            strcpy(newbuf, *buf + i + 1);
            (*buf)[i + 1] = 0;
            *msg = *buf;
            *buf = newbuf;
            return 1;
        }
    }
    return 0;
}

char *str_join(char *old, char *add)
{
    int len1 = old ? strlen(old) : 0;
    int len2 = strlen(add);
    char *newbuf = malloc(len1 + len2 + 1);
    if (!newbuf) return 0;
    newbuf[0] = 0;
    if (old) strcat(newbuf, old);
    strcat(newbuf, add);
    free(old);
    return newbuf;
}

void fatal(t_client *cl, t_msg *m, int sfd)
{
    close(sfd);
    while (cl)
    {
        t_client *next = cl->next;
        if (cl->buf) free(cl->buf);
        close(cl->fd);
        free(cl);
        cl = next;
    }
    while (m)
    {
        t_msg *next = m->next;
        free(m->txt);
        free(m);
        m = next;
    }
    write(2, "Fatal error\n", 12);
    exit(1);
}

int add_msg(t_msg **m, char *txt, int id)
{
    t_msg *new = malloc(sizeof(t_msg));
    if (!new) return 0;
    new->txt = str_join(NULL, txt);
    if (!new->txt) return (free(new), 0);
    new->from = id;
    new->next = 0;
    t_msg **cur = m;
    while (*cur)
        cur = &(*cur)->next;
    *cur = new;
    return 1;
}

int handle_client(t_client *c, t_msg **m)
{
    char buf[1024];
    ssize_t r = recv(c->fd, buf, sizeof(buf) - 1, 0);
    if (r <= 0)
    {
        c->disconnect = 1;
        char tmp[64];
        sprintf(tmp, "server: client %d just left\n", c->id);
        if (!add_msg(m, tmp, c->id)) return 0;
        return 1;
    }
    buf[r] = 0;
    c->buf = str_join(c->buf, buf);
    if (!c->buf) return 0;

    char *msg = 0;
    int ex;
    while ((ex = extract_message(&c->buf, &msg)) == 1)
    {
        char tmp[2000];
        sprintf(tmp, "client %d: %s", c->id, msg);
        if (!add_msg(m, tmp, c->id)) return (free(msg), 0);
        free(msg);
        msg = 0;
    }
    if (ex == -1) return 0;
    return 1;
}

int send_msgs(t_msg **m, t_client *cl)
{
    while (*m)
    {
        t_msg *next = (*m)->next;
        for (t_client *c = cl; c; c = c->next)
        {
            if (c->id != (*m)->from)
                if (send(c->fd, (*m)->txt, strlen((*m)->txt), 0) < 0)
                    return 0;
        }
        free((*m)->txt);
        free(*m);
        *m = next;
    }
    return 1;
}

void check_disconnect(t_client **cl, fd_set *fds)
{
    t_client **cur = cl;
    while (*cur)
    {
        if ((*cur)->disconnect)
        {
            t_client *tmp = *cur;
            if (tmp->buf) free(tmp->buf);
            FD_CLR(tmp->fd, fds);
            close(tmp->fd);
            *cur = tmp->next;
            free(tmp);
        }
        else
            cur = &(*cur)->next;
    }
}

int main(int ac, char **av)
{
    if (ac != 2)
        return (write(2, "Wrong number of arguments\n", 26), 1);

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) return (write(2, "Fatal error\n", 12), 1);

    struct sockaddr_in serv;
    bzero(&serv, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = htonl(2130706433);
    serv.sin_port = htons(atoi(av[1]));

    if (bind(sfd, (struct sockaddr *)&serv, sizeof(serv)) < 0 || listen(sfd, 10) < 0)
        return (close(sfd), write(2, "Fatal error\n", 12), 1);

    fd_set fds, rfds;
    FD_ZERO(&fds);
    FD_SET(sfd, &fds);
    t_client *cl = 0;
    t_msg *m = 0;
    int id = 0;

    while (1)
    {
        rfds = fds;
        if (select(FD_SETSIZE, &rfds, 0, 0, 0) < 0)
            fatal(cl, m, sfd);

        if (FD_ISSET(sfd, &rfds))
        {
            struct sockaddr_in cli;
            socklen_t len = sizeof(cli);
            int cfd = accept(sfd, (struct sockaddr *)&cli, &len);
            if (cfd < 0) fatal(cl, m, sfd);

            t_client *newc = calloc(1, sizeof(t_client));
            if (!newc) fatal(cl, m, sfd);
            newc->id = id++;
            newc->fd = cfd;
            newc->next = 0;
            FD_SET(cfd, &fds);

            t_client **ptr = &cl;
            while (*ptr) ptr = &(*ptr)->next;
            *ptr = newc;

            char tmp[64];
            sprintf(tmp, "server: client %d just arrived\n", newc->id);
            if (!add_msg(&m, tmp, newc->id)) fatal(cl, m, sfd);
        }

        for (t_client *c = cl; c; c = c->next)
            if (FD_ISSET(c->fd, &rfds) && !handle_client(c, &m))
                fatal(cl, m, sfd);

        if (m && !send_msgs(&m, cl))
            fatal(cl, m, sfd);

        check_disconnect(&cl, &fds);
    }
}
