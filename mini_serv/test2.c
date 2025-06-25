#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/socket.h>

#include <string.h>
#include <strings.h>

#include <netinet/in.h>

typedef struct s_client
{
    int id;
    int fd;
    struct sockaddr_in addr;
    socklen_t addrLen;
    char    *buffer;
    struct s_client *next;
}   t_client;



void    freeClients(t_client *clients)
{
    t_client *tmp = clients;
    while (tmp)
    {
        tmp = clients->next;
        if (clients->fd != -1)
            close(clients->fd);
        if (clients->buffer)
            free(clients->buffer);
        free(clients);
        clients = tmp;
    }
}

void    freeClient(t_client *client, t_client **clients)
{
    t_client   *tmp = *clients;
    t_client   *before = NULL;
    while (tmp)
    {
        if (client == tmp)
        {
            if (!before && client->next == NULL)
                *clients = client->next;
            else if (!before)
                *clients = NULL;
            else
                before->next = client->next;
            if (client->fd != -1)
                close(client->fd);
            if (client->buffer)
                free(client->buffer);
            free(client);
            return ;
        }
        before = tmp;
        tmp = tmp->next;
    }
}

void    addClient(t_client **clients, t_client *client)
{
    if (!(*clients))
        *clients = client;
    else
    {
        t_client *tmp = *clients;
        while (tmp->next)
            tmp = tmp->next;
        client->next = NULL;
        tmp->next = client;
    }
}

int acceptClient(int server, fd_set *allFds, t_client **clients, int *maxFd, int *idClient)
{
    t_client    *new = malloc(sizeof(t_client));
    if (!new)
        return (-1);
    bzero(&new->addr, sizeof(new->addr));
    new->buffer = NULL;
    new->addrLen = sizeof(new->addr);
    new->fd = accept(server, (struct sockaddr *)&new->addr, &new->addrLen);
    if (new->fd == -1)
        return (-1);
    if (*maxFd < new->fd)
        *maxFd = new->fd;
    new->id = *idClient;
    new->next = NULL;
    *idClient += 1;
    addClient(clients, new);
    FD_SET(new->fd, allFds);
    return (new->id);
}


int startServer(int *server, char *arg)
{
    int port = atoi(arg);
    struct sockaddr_in servAddr;
    bzero(&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    /*
        Connaitre la valeur correspondant a l'adresse IP 127.0.0.1
        printf("%x\n%x\n%x\n%x\n", 127, 0, 0 ,1);
        printf("%d\n", 0x7f000001);
    */
    servAddr.sin_addr.s_addr = htonl(2130706433);
    servAddr.sin_port = htons(port);
    *server = socket(AF_INET, SOCK_STREAM, 0);
    if (*server == -1)
    {
        write(2, "Fatal error\n", 12);
        return (0);
    }
    int opt = 1;
    setsockopt(*server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (bind(*server, (const struct sockaddr *)&servAddr, sizeof(servAddr)) != 0 ||
        listen(*server, 50) != 0)
    {
        write(2, "Fatal error\n", 12);
        close(*server);
        return (0);
    }
    return (1);
}

void    exitFatalError(t_client *clients, int serv)
{
    freeClients(clients);
    // freeMsgs(msgs);
    write(2, "Fatal error\n", 12);
    close(serv);
    exit(1);
}

int main(int ac, char **av)
{

    int server;
    int idClient = 0;

    printf("=== MINI SERV ===\n");
    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }

    if (!startServer(&server, av[1]))
        return (1);

    fd_set allFds, readFds, writeFds;
    int maxFd = server;

    t_client *clients = NULL;

    FD_ZERO(&allFds);
    FD_ZERO(&writeFds);
    FD_ZERO(&readFds);

    FD_SET(server, &allFds);

    while (1)
    {
        readFds = allFds;
        select (maxFd + 1, &readFds, &writeFds, NULL, NULL);

        int i = -1;
        while (++i <= maxFd)
        {
            if (FD_ISSET(i, &readFds))
            {
                if (i == server)
                {
                    int val = acceptClient(server, &allFds, &clients, &maxFd, &idClient);
                    if (val == -1)
                        exitFatalError(clients, server);
                    printf("New client connected : %d\n", val);
                    printf("=== CLIENTS CONNECTED === \n");
                    t_client *tmp = clients;
                    while (tmp)
                    {
                        printf("[%d] -> %d\n", tmp->id, tmp->fd);
                        tmp = tmp->next;
                    }
                }
            }

            if (FD_ISSET(i, &writeFds))
            {
                printf("ECRITURE POUR %d PRETE\n", i);
            }
        }
    }

    freeClients(clients);
    close(server);

    return (0);
}