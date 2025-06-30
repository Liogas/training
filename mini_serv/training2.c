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
    int diconnect;
    char *buffer;
    struct s_client *next;
}   t_client;

typedef struct s_msg
{
    char *text;
    int sendId;
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

int getMaxFd(t_client *clients, int sockfd)
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

void fatalError(int sockfd, t_client *clients, t_msg *msgs)
{
    close(sockfd);
    while (clients)
    {
        t_client *next = clients->next;
        if (clients->buffer)
            free(clients->buffer);
        close(clients->fd);
        free(clients);
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

int addMsg(t_msg **msgs, char *msg, int id)
{
    t_msg *new = malloc(sizeof(t_msg));
    if (!new)
        return (0);
    new->next = NULL;
    new->sendId = id;
    new->text = msg;
    if (!*msgs)
        *msgs = new;
    else
    {
        t_msg *tmp = *msgs;
        while (tmp && tmp->next)
            tmp = tmp->next;
        tmp->next = new;
    }
    return (1);
}

int handleClient(t_client *client, t_msg **msgs, fd_set *allfds)
{
    char buffer[1025];
    int read = recv(client->fd, buffer, sizeof(buffer), 0);
    if (read < 0)
        return (0);
    if (read == 0)
    {
        client->diconnect = 1;
        char formated[1000];
        sprintf(formated, "server: client %d just left\n", client->id);
        char *msg = str_join(NULL, formated);
        if (!msg)
            return (0);
        if (!addMsg(msgs, msg, client->id))
            return (free(msg), 0);
        FD_CLR(client->fd, allfds);
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
        char formated[1000];
        sprintf(formated, "client %d: %s", client->id, msg);
        char *old = msg;
        msg = str_join(NULL, formated);
        free(old);
        if (!msg || !addMsg(msgs, msg, client->id))
            return (0);
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

int sendMsgs(t_msg **msgs, t_client *clients)
{
    while (*msgs)
    {
        t_msg *next = (*msgs)->next;
        t_client *tmp = clients;
        while (tmp)
        {
            if (tmp->id != (*msgs)->sendId &&
                send(tmp->fd, (*msgs)->text, strlen((*msgs)->text), 0) < 0)
                return (0);
            tmp = tmp->next;
        }
        if ((*msgs)->text)
            free((*msgs)->text);
        free(*msgs);
        *msgs = next;
    }
    return (1);
}

void checkDisconnect(t_client **clients)
{
    t_client *before = NULL;
    t_client *tmp = *clients;

    while (tmp)
    {
        if (tmp->diconnect)
        {
            t_client *next = tmp->next;
            if (tmp->buffer)
                free(tmp->buffer);
            close(tmp->fd);
            free(tmp);
            if (!before)
                *clients = next;
            else
                before->next = next;
            tmp = next;
            continue ;
        }
        before = tmp;
        tmp = tmp->next;
    }
}

int main(int ac, char **av) {
	int sockfd;
	struct sockaddr_in servaddr; 

    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

	// socket create and verification 
	sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) 
    {
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
		exit(1); 
	}

    fd_set allfds, readfds;
    int id = 0;
    t_client *clients = NULL;
    t_msg *msgs = NULL;

    FD_ZERO(&allfds);
    FD_ZERO(&readfds);
    FD_SET(sockfd, &allfds);

    while (1)
    {
        readfds = allfds;
        if (select(getMaxFd(clients, sockfd) + 1, &readfds, NULL, NULL, NULL) < 0)
            fatalError(sockfd, clients, msgs);


        if (FD_ISSET(sockfd, &readfds))
        {
            struct sockaddr_in cli;
            int len = sizeof(cli);
            int connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);
            if (connfd < 0)
                fatalError(sockfd, clients, msgs);
            t_client *new = malloc(sizeof(t_client));
            if (!new)
                (close(connfd), fatalError(sockfd, clients, msgs));
            new->fd = connfd;
            new->id = id++;
            new->buffer = NULL;
            new->next = NULL;
            new->diconnect = 0;
            if (!clients)
                clients = new;
            else
            {
                t_client *tmp = clients;
                while (tmp && tmp->next)
                    tmp = tmp->next;
                tmp->next = new;
            }
            char formated[1000];
            sprintf(formated, "server: client %d just arrived\n", new->id);
            char *msg = str_join(NULL, formated);
            if (!msg)
                fatalError(sockfd, clients, msgs);
            if (!addMsg(&msgs, msg, new->id))
                fatalError(sockfd, clients, msgs);
            FD_SET(new->fd, &allfds);
        }

        t_client *client = clients;
        while (client)
        {
            if (FD_ISSET(client->fd, &readfds) && !handleClient(client, &msgs, &allfds))
                fatalError(sockfd, clients, msgs);
            client = client->next;
        }

        if (msgs && !sendMsgs(&msgs, clients))
            fatalError(sockfd, clients, msgs);
        checkDisconnect(&clients);
    }
    return (1);
}