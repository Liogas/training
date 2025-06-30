#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

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
    int sendid;
    char *text;
    struct s_msg *next;
}   t_msg;

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

int getmaxfd(int sockfd, t_client *clients)
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

void fatalerror(int sockfd, t_client *clients, t_msg *msgs)
{
    close(sockfd);
    while (clients)
    {
        t_client *next = clients->next;
        close(clients->fd);
        if (clients->buffer)
            free(clients->buffer);
        free(clients->buffer);
        clients = next;
    }
    while (msgs)
    {
        t_msg *next = msgs->next;
        if (msgs->text)
            free(msgs->text);
        free(msgs);
        msgs = next;
    }
}

int addmsg(t_msg **msgs, char *msg, int id)
{
    t_msg *new = malloc(sizeof(t_msg));
    if (!new)
        return (0);
    new->sendid = id;
    new->next = NULL;
    new->text = msg;
    if (!*msgs)
        *msgs = new;
    else
    {
        t_msg *tmp = *msgs;
        while (tmp && tmp->next)
            tmp = tmp->next;
        tmp->text = msg;
    }
    return (1);
}

int handleclient(t_client *client, t_msg **msgs, fd_set *allfds)
{
    char buffer[2000];
    int read = recv(client->fd, buffer, sizeof(buffer), 0);
    if (read < 0)
        return (0);
    if (read == 0)
    {
        client->disconnect = 1;
        FD_CLR(client->fd, allfds);
        char formated[2000];
        sprintf(formated, "server: client %d just left\n", client->id);
        char *msg = str_join(NULL, formated);
        if (!msg)
            return (0);
        if (!addmsg(msgs, msg, client->id))
            return (free(msg), 0);
        return (1);
    }
    buffer[read] = '\0';
    client->buffer = str_join(client->buffer, buffer);
    if (!client->buffer)
        return (0);
    char *msg = NULL;
    int val = extract_message(&client->buffer, &msg);
    while (val == 1)
    {
        char formated[2000];
        sprintf(formated, "client %d: %s", client->id, msg);
        char *old = msg;
        msg = str_join(NULL, formated);
        if (!msg)
            return (0);
        free(old);
        if (!addmsg(msgs, msg, client->id))
            return (free(msg), 0);
        msg = NULL;
        val = extract_message(&client->buffer, &msg);
    }
    if (val == -1)
    {
        if (msg)
            free(msg);
        return (0);
    }
    return (1);
}

int sendmsgs(t_msg **msgs, t_client *clients)
{
    while (*msgs)
    {
        t_client *client = clients;
        while (client)
        {
            if (!client->disconnect && client->id != (*msgs)->sendid)
            {
                if (send(client->fd, (*msgs)->text, strlen((*msgs)->text), 0) < 0)
                    return (0);
            }
            client = client->next;
        }
        t_msg *next = (*msgs)->next;
        if ((*msgs)->text)
            free((*msgs)->text);
        free(*msgs);
        *msgs = next;
    }
    return (1);
}

void checkdisconnect(t_client **clients)
{
    t_client *before = NULL;
    t_client *client = *clients;
    while (client)
    {
        if (client->disconnect)
        {
            t_client *next = client->next;
            if (client->buffer)
                free(client->buffer);
            close(client->fd);
            free(client);
            if (!before)
                *clients = next;
            else
                before->next = next;
            client = *clients;
            continue ;
        }
        before = client;
        client = client->next;
    }
}

int main(int ac, char **av) {
	int sockfd, connfd, len;
	struct sockaddr_in servaddr, cli; 

    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		write(2, "Fatal error\n", 12);
		exit(1);
	} 
	bzero(&servaddr, sizeof(servaddr)); 

	// assign IP, PORT 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(2130706433); //127.0.0.1
	servaddr.sin_port = htons(atoi(av[1])); 
  
	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr))) != 0) { 
		write(2, "Fatal error\n", 12);
        close(sockfd);
		exit(1); 
	} 
	if (listen(sockfd, 10) != 0) {
		write(2, "Fatal error\n", 12);
        close(sockfd);
		exit(1); 
	}
    fd_set allfds, readfds;
    int id = 0;
    t_client *clients = NULL;
    t_msg   *msgs = NULL;
    FD_ZERO(&allfds);
    FD_ZERO(&readfds);
    FD_SET(sockfd, &allfds);

    while (1)
    {
        readfds = allfds;
        if (select(getmaxfd(sockfd, clients) + 1, &readfds, NULL, NULL, NULL) < 0)
            fatalerror(sockfd, clients, msgs);
        if (FD_ISSET(sockfd, &readfds))
        {
            len = sizeof(cli);
	        connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);
	        if (connfd < 0)
                fatalerror(sockfd, clients, msgs);
            t_client *client = malloc(sizeof(t_client));
            if (!client)
                (close(connfd), fatalerror(sockfd, clients, msgs));
            client->id = id++;
            client->fd = connfd;
            client->buffer = NULL;
            client->next = NULL;
            client->disconnect = 0;
            if (!clients)
                clients = client;
            else
            {
                t_client *tmp = clients;
                while (tmp && tmp->next)
                    tmp = tmp->next;
                tmp->next = client;
            }
            char formatted[2000];
            sprintf(formatted, "server: client %d just arrived\n", client->id);
            char *msg = str_join(NULL, formatted);
            if (!msg)
                fatalerror(sockfd, clients, msgs);
            if (!addmsg(&msgs, msg, client->id))
                fatalerror(sockfd, clients, msgs);
            FD_SET(client->fd, &allfds);
        }
        t_client *client = clients;
        while (client)
        {
            if (FD_ISSET(client->fd, &readfds) && !handleclient(client, &msgs, &allfds))
                fatalerror(sockfd, clients, msgs);
            client = client->next;
        }
        if (msgs && !sendmsgs(&msgs, clients))
            fatalerror(sockfd, clients, msgs);
        checkdisconnect(&clients);
    }
}
