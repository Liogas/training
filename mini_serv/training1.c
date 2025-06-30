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
    int id;
    int fd;
    char *buffer;
    int disconnect;
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

void fatalError(t_client *clients, t_msg *msgs, int sockfd)
{
    close(sockfd);
    while (clients)
    {
        t_client *tmp = clients->next;
        if (clients->buffer)
            free(clients->buffer);
        close(clients->fd);
        free(clients);
        clients = tmp;
    }
    while (msgs)
    {
        t_msg *tmp = msgs->next;
        if (msgs->text)
            free(msgs->text);
        free(msgs);
        msgs = tmp;
    }
}

int addMsg(t_msg **msgs, char *msg, int id)
{
    t_msg *new = malloc(sizeof(t_msg));
    if (!new)
        return (0);
    new->text = str_join(NULL, msg);
    if (!new->text)
        return (free(new), 0);
    new->sendId = id;
    new->next = NULL;
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

int handleClient(t_client *client, t_msg **msgs)
{
    char buffer[1024];

    ssize_t read = recv(client->fd, buffer, sizeof(buffer), 0);
    if (read < 0)
        return (0);
    if (read == 0)
    {
        client->disconnect = 1;
        char msg[1000];
        sprintf(msg, "server: client %d just left\n", client->id);
        if (!addMsg(msgs, msg, client->id))
            return (0);
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
        if (!addMsg(msgs, formated, client->id))
            return (free(msg), 0);
        free(msg);
        msg = NULL;
        val = extract_message(&client->buffer, &msg);
    }
    if (val == -1)
        return (0);
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
            if (tmp->id != (*msgs)->sendId)
            {
                ssize_t size = send(tmp->fd, (*msgs)->text, strlen((*msgs)->text), 0);
                if (size < 0)
                    return (0);
            }
            tmp = tmp->next;
        }
        if ((*msgs)->text)
            free((*msgs)->text);
        free(*msgs);
        *msgs = next;
    }
    return (1);
}

void checkDisconnect(t_client **clients, fd_set *allfds)
{
    t_client *before = NULL;
    t_client *tmp = *clients;
    while (tmp)
    {
        if (tmp->disconnect)
        {
            t_client *next = tmp->next;
            if (tmp->buffer)
                free(tmp->buffer);
            FD_CLR(tmp->fd, allfds);
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
		exit(1); 
	}

    fd_set readfds, allfds;
    int id = 0;
    t_msg *msgs = NULL;
    t_client *clients = NULL;

    FD_ZERO(&allfds);
    FD_ZERO(&readfds);
    FD_SET(sockfd, &allfds);

    while (1)
    {
        readfds = allfds;
        if (select(getMaxFd(clients, sockfd) + 1, &readfds, NULL, NULL, NULL) < 0)
            fatalError(clients, msgs, sockfd);

        if (FD_ISSET(sockfd, &readfds))
        {
            struct sockaddr_in cli;
            int len = sizeof(cli);
            int connfd = accept(sockfd, (struct sockaddr *)&cli, (socklen_t *)&len);
            if (connfd < 0)
                fatalError(clients, msgs, sockfd);
            
            t_client *client = malloc(sizeof(t_client));
            if (!client)
                (close(connfd), fatalError(clients, msgs, sockfd));
            client->fd = connfd;
            client->id = id++;
            client->next = NULL;
            client->buffer = NULL;
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
            FD_SET(client->fd, &allfds);
            char msg[64];
            sprintf(msg, "server: client %d just arrived\n", client->id);
            if (!addMsg(&msgs, msg, client->id))
                fatalError(clients, msgs, sockfd);
        }

        t_client *tmp = clients;
        while (tmp)
        {
            if (FD_ISSET(tmp->fd, &readfds) && !handleClient(tmp, &msgs))
                fatalError(clients, msgs, sockfd);
            tmp = tmp->next;
        }

        if (msgs && !sendMsgs(&msgs, clients))
            fatalError(clients, msgs, sockfd);
        checkDisconnect(&clients, &allfds);
    }
}