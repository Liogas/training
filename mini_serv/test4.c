#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>

typedef struct s_client
{
    int id;
    int fd;
    char *buffer;
    int disconnect;
    struct s_client *next;
}   t_client;

typedef struct s_msg
{
    char *text;
    int sender_fd;
    struct s_msg *next;
}   t_msg;

void fatal_error(t_client *clients, int sockfd, t_msg *msgs)
{
    t_client *tmp;
    while (clients)
    {
        tmp = clients->next;
        close(clients->fd);
        if (clients->buffer)
            free(clients->buffer);
        free(clients);
        clients = tmp;
    }
    while (msgs)
    {
        t_msg *tmpm = msgs->next;
        if (msgs->text)
            free(msgs->text);
        free(msgs);
        msgs = tmpm;
    }
    close(sockfd);
    write(2, "Fatal error\n", 12);
    exit(1);
}

int extract_message(char **buf, char **msg)
{
	char	*newbuf;
	int	i;

	*msg = 0;
	if (*buf == 0)
		return (0);
	i = 0;
	while ((*buf)[i])
	{
		if ((*buf)[i] == '\n')
		{
			newbuf = calloc(1, sizeof(*newbuf) * (strlen(*buf + i + 1) + 1));
			if (newbuf == 0)
				return (-1);
			strcpy(newbuf, *buf + i + 1);
			*msg = *buf;
			(*msg)[i + 1] = 0;
			*buf = newbuf;
			return (1);
		}
		i++;
	}
	return (0);
}

char *str_join(char *buf, char *add)
{
	char	*newbuf;
	int		len;

	if (buf == 0)
		len = 0;
	else
		len = strlen(buf);
	newbuf = malloc(sizeof(*newbuf) * (len + strlen(add) + 1));
	if (newbuf == 0)
		return (0);
	newbuf[0] = 0;
	if (buf != 0)
		strcat(newbuf, buf);
	free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void add_msg(t_msg **msgs, char *text, int sender_fd)
{
    t_msg *new = malloc(sizeof(t_msg));
    if (!new)
        return;
    new->text = str_join(NULL, text);
    new->sender_fd = sender_fd;
    new->next = NULL;

    if (!*msgs)
        *msgs = new;
    else
    {
        t_msg *tmp = *msgs;
        while (tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
}

int get_max_fd(int sockfd, t_client *clients)
{
    int max = sockfd;
    while (clients)
    {
        if (clients->fd > max)
            max = clients->fd;
        clients = clients->next;
    }
    return (max);
}

void remove_disconnected(t_client **clients, fd_set *allfds)
{
    t_client *curr = *clients;
    t_client *prev = NULL;

    while (curr)
    {
        if (curr->disconnect)
        {
            if (prev)
                prev->next = curr->next;
            else
                *clients = curr->next;
            FD_CLR(curr->fd, allfds);
            close(curr->fd);
            if (curr->buffer)
                free(curr->buffer);
            t_client *tmp = curr;
            curr = curr->next;
            free(tmp);
        }
        else
        {
            prev = curr;
            curr = curr->next;
        }
    }
}

int read_from_client(t_client *client, t_msg **msgs)
{
    char buffer[1025];
    ssize_t bytes_read = recv(client->fd, buffer, 1024, 0);

    if (bytes_read <= 0)
    {
        char leave_msg[64];
        sprintf(leave_msg, "server: client %d just left\n", client->id);
        add_msg(msgs, leave_msg, -1);
        client->disconnect = 1;
        return 1;
    }

    buffer[bytes_read] = '\0';
    client->buffer = str_join(client->buffer, buffer);
    if (!client->buffer)
        return 0;

    char *msg = NULL;
    int val = extract_message(&client->buffer, &msg);
    while (val == 1)
    {
        char formatted[1500];
        sprintf(formatted, "client %d: %s", client->id, msg);
        add_msg(msgs, formatted, client->fd);
        free(msg);
        msg = NULL;
        val = extract_message(&client->buffer, &msg);
    }
    if (val == -1)
        return (0);
    return 1;
}

void send_to_all_clients(t_msg **msgs, t_client *clients)
{
    t_msg *curr = *msgs;
    while (curr)
    {
        t_client *c = clients;
        while (c)
        {
            if (!c->disconnect && c->fd != curr->sender_fd)
            {
                if (send(c->fd, curr->text, strlen(curr->text), 0) < 0)
                    c->disconnect = 1;
            }
            c = c->next;
        }
        t_msg *tmp = curr;
        curr = curr->next;
        if (tmp->text)
            free(tmp->text);
        free(tmp);
    }
    *msgs = NULL;
}

int main(int ac, char **av)
{
    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

    int sockfd, port = atoi(av[1]);
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        fatal_error(NULL, sockfd, NULL);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(2130706433);
    servaddr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        fatal_error(NULL, sockfd, NULL);

    if (listen(sockfd, 10) < 0)
        fatal_error(NULL, sockfd, NULL);

    fd_set allfds, readfds;
    FD_ZERO(&allfds);
    FD_SET(sockfd, &allfds);

    t_client *clients = NULL;
    t_msg *msgs = NULL;
    int next_id = 0;

    while (1)
    {
        readfds = allfds;
        if (select(get_max_fd(sockfd, clients) + 1, &readfds, NULL, NULL, NULL) < 0)
            fatal_error(clients, sockfd, msgs);

        if (FD_ISSET(sockfd, &readfds))
        {
            struct sockaddr_in cli_addr;
            socklen_t len = sizeof(cli_addr);
            int connfd = accept(sockfd, (struct sockaddr *)&cli_addr, &len);
            if (connfd < 0)
                continue;

            t_client *new = malloc(sizeof(t_client));
            if (!new)
                fatal_error(clients, sockfd, msgs);

            new->id = next_id++;
            new->fd = connfd;
            new->buffer = NULL;
            new->disconnect = 0;
            new->next = NULL;

            FD_SET(connfd, &allfds);

            if (!clients)
                clients = new;
            else
            {
                t_client *tmp = clients;
                while (tmp->next)
                    tmp = tmp->next;
                tmp->next = new;
            }

            char msg_arrival[64];
            sprintf(msg_arrival, "server: client %d just arrived\n", new->id);
            add_msg(&msgs, msg_arrival, new->fd); // <-- On exclut le nouveau client
        }

        t_client *curr = clients;
        while (curr)
        {
            if (FD_ISSET(curr->fd, &readfds))
            {
                if (!read_from_client(curr, &msgs))
                    fatal_error(clients, sockfd, msgs);
            }
            curr = curr->next;
        }

        if (msgs)
            send_to_all_clients(&msgs, clients);

        remove_disconnected(&clients, &allfds);
    }

    return 0;
}
