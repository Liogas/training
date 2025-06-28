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
    struct sockaddr_in addr;
    char    *buffer;
    int     disconnect;
    struct s_client *next;
}   t_client;

typedef struct s_msg
{
    char *text;
    int   clientId;
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

char *str_join(char *buf, char *add, int shouldFree)
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
    if (shouldFree)
	    free(buf);
	strcat(newbuf, add);
	return (newbuf);
}

void fatalError(t_client *clients, int fd, t_msg *msgs)
{
    close(fd);
    while (clients)
    {
        t_client *tmp = clients->next;
        close(clients->fd);
        if (clients->buffer)
            free(clients->buffer);
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
    write(2, "Fatal error\n", 12);
    exit(1);
}

int buildMsg(int id, char *text, t_msg **msgs)
{
    t_msg *new = malloc(sizeof(t_msg));
    if (!new)
        return (0);
    new->text = malloc(100 * sizeof(char));
    if (!new->text)
        return (free(new), 0);
    sprintf(new->text, "server: client %d %s\n", id, text);
    new->text = str_join(NULL, new->text, 0);
    new->next = NULL;
    new->clientId = id;
    if (!new->text)
        return (free(new), 0);
    if (!*msgs)
        *msgs = new;
    else
    {
        t_msg *tmp = *msgs;
        while (tmp && tmp->next)
            tmp = tmp->next;
        if (!tmp)
            return (free(new->text), (free(new)), 0);
        tmp->next = new;
    }
    return (1);
}

int gestRead(int fd, t_client **clients, t_msg **msgs)
{
    char        buffer[1000];
    t_client    *client = *clients;
    ssize_t     read;

    while (client && client->fd != fd)
     client = client->next;
    if (!client)
        return (0);
    read = recv(fd, buffer, sizeof(buffer), 0);
    if (read < 0)
        return (0);
    if (read == 0)
    {
        if (!buildMsg(client->id, "just left", msgs))
            return (0);
        return (1);
    }
    // MESSAGE ENVOYE PAR LE CLIENT
    t_msg   *msg = malloc(sizeof(t_msg));
    if (!msg)
        return (0);
    msg->text = malloc(100 * sizeof(char));
    if (!msg->text)
        return (free(msg), 0);
    sprintf(msg->text, "client %d: ", client->id);
    msg->next = NULL;
    msg->clientId = client->id;
    client->buffer = str_join(client->buffer, buffer, 1);
    if (!client->buffer)
        return (free(msg), 0);
    char    *msgTmp = NULL;
    if (extract_message(&client->buffer, &msgTmp) == -1)
    {
        if (msgTmp)
            free(msgTmp);
        if (msg->text)
            free(msg->text);
        return (free(msg), 0);
    }
    msg->text = str_join(msg->text, msgTmp, 1);
    if (!msg->text)
    {
        if (msgTmp)
            free(msgTmp);
        if (msg->text)
            free(msg->text);
        return (free(msg), 0);
    }
    free(msgTmp);
    if (!*msgs)
        *msgs = msg;
    else
    {
        t_msg *tmp = *msgs;
        while (tmp && tmp->next)
            tmp = tmp->next;
        if (!tmp)
            return (free(msg->text), free(msg), 0);
        tmp->next = msg;
    }
    return (1);
}

int gestWrite(int fd, t_msg **msgs, t_client **clients, int maxFd)
{
    if (!*msgs)
        return (1);
    t_client *client = *clients;
    while (client && client->fd != fd)
        client = client->next;
    if (!client)
        return (0);
    // ENVOIE DU MESSAGE AU CLIENT
    if ((*msgs)->clientId == client->id)
        return (1);
    if (send(fd, (*msgs)->text, strlen((*msgs)->text), 0) < 0)
        return (0);
    if (fd == maxFd)
    {
        t_msg *tmp = *msgs;
        *msgs = (*msgs)->next;
        free(tmp->text);
        free(tmp);
    }
    return (1);
}

int getMaxfd(int sockfd, t_client *clients)
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

int main(int ac, char **av) {
	int sockfd;
	struct sockaddr_in servaddr;
    t_client    *clients = NULL;
    t_msg       *msgs = NULL;
    int id = 0;

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
	else
		write(1, "Socket successfully created..\n", 30); 
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
	else
		write(1, "Socket successfully binded..\n", 29);
	if (listen(sockfd, 10) != 0) {
		write(2, "Fatal error\n", 12); 
        close(sockfd); 
		exit(1); 
	}
    else
        write(1, "Socket successfully listening...\n", 33);
        
    fd_set  allfds, writefds, readfds;

    FD_ZERO(&allfds);
    FD_ZERO(&writefds);
    FD_ZERO(&readfds);

    FD_SET(sockfd, &allfds);

    while (1)
    {
        readfds = writefds = allfds;
        FD_CLR(sockfd, &writefds);

        t_client *client = clients;
        t_client *before = NULL;
        while (client)
        {
            if (client->disconnect)
            {
                if (!before)
                    clients = client->next;
                else
                    before->next = client->next;
                if (client->buffer)
                    free(client->buffer);
                FD_CLR(client->fd, &allfds);
                close(client->fd);
                free(client);
                client = clients;
                continue ;
            }
            before = client;
            client = client->next;
        }

        int maxfd = getMaxfd(sockfd, clients);
        if (select(maxfd + 1, &readfds, &writefds, NULL, NULL) < 0)
            fatalError(clients, sockfd, msgs);

        int i = -1;
        while (++i <= maxfd)
        {
            if (FD_ISSET(i, &readfds))
            {
                if (i == sockfd)
                {
                    printf("Un nouveau client cherche a se connecter\n");
                    // NOUVEAU CLIENT
                    t_client *new = malloc(sizeof(t_client));
                    if (!new)
                        fatalError(clients, sockfd, msgs);
                    new->id = id;
                    new->next = NULL;
                    new->disconnect = 0;
                    id++;
                    socklen_t  len = sizeof(new->addr);
                    new->fd = accept(sockfd, (struct sockaddr *)&new->addr, &len);
                    if (new->fd < 0)
                        (free(new), fatalError(clients, sockfd, msgs));
                    if (maxfd < new->fd)
                        maxfd = new->fd;
                    FD_SET(new->fd, &allfds);
                    if (!clients)
                        clients = new;
                    else
                    {
                        t_client *tmp = clients;
                        while (tmp->next)
                            tmp = tmp->next;
                        tmp->next = new;
                    }
                    printf("client->id = %d\n client->fd = %d\n", new->id, new->fd);
                    if (!buildMsg(i, "just arrived", &msgs))
                        fatalError(clients, sockfd, msgs);
                }
                else if (!gestRead(i, &clients, &msgs))
                    fatalError(clients, sockfd, msgs);
            }
            if (FD_ISSET(i, &writefds) && !gestWrite(i, &msgs, &clients, maxfd))
                    fatalError(clients, sockfd, msgs);
        }
    }
    return (0);
}