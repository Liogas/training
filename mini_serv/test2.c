#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/socket.h>

#include <string.h>
#include <strings.h>

#include <netinet/in.h>

#include <errno.h>



typedef struct s_msg
{
    char    *text;
    int     offset;
    struct s_msg *next;
}   t_msg;

typedef struct s_client
{
    int id;
    int fd;
    int disconnect;
    struct sockaddr_in addr;
    socklen_t addrLen;
    char    *buffer;
    t_msg   *msgs;
    struct s_client *next;
}   t_client;

void    freeMsgs(t_msg *msgs)
{
    t_msg *tmp = msgs;
    while (tmp)
    {
        tmp = msgs->next;
        if (msgs->text)
            free(msgs->text);
        free(msgs);
        msgs = tmp;
    }
}

void    freeMsg(t_msg *msg, t_msg **msgs)
{
    t_msg   *tmp = *msgs;
    t_msg   *before = NULL;
    while (tmp)
    {
        if (msg == tmp)
        {
            if (!before)
                *msgs = msg->next;
            else
                before->next = msg->next;
            if (msg->text)
                free(msg->text);
            free(msg);
            return ;
        }
        before = tmp;
        tmp = tmp->next;
    }
}

void    addMsg(t_msg **msgs, t_msg *msg)
{
    if (!(*msgs))
        *msgs = msg;
    else
    {
        t_msg *tmp = *msgs;
        while (tmp->next)
            tmp = tmp->next;
        msg->next = NULL;
        tmp->next = msg;
    }
}


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
        freeMsgs(clients->msgs);
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
            if (client->msgs)
                freeMsgs(client->msgs);
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

t_client *getClient(int fd, t_client *clients)
{
    while (clients)
    {
        if (clients->fd == fd)
            return (clients);
        clients = clients->next;
    }
    return (NULL);
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
    new->msgs = NULL;
    new->disconnect = 0;
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
    t_client *tmp = clients;
    printf("ETAT DE CLIENTS\n");
    while (tmp)
    {
        printf("[%d] -> %d\n", tmp->id, tmp->fd);
        tmp = tmp->next;
    }
    freeClients(clients);
    write(2, "Fatal error\n", 12);
    close(serv);
    exit(1);
}

int addConnectedMsg(t_client *clients, int id)
{
    while (clients)
    {
        if (clients->id != id)
        {
            t_msg   *new = malloc(sizeof(t_msg));
            if (!new)
                return (0);
            new->next = NULL;
            new->offset = 0;
            new->text = malloc(16 * sizeof(char));
            if (!new->text)
                return (freeMsgs(new), 0);
            strcpy(new->text, "server: client ");
            char idString[4];
            sprintf(idString, "%d", id);
            new->text = realloc(new->text, strlen(new->text) + strlen(idString) + strlen(" just arrived\n") + 1);
            if (!new->text)
                return (freeMsgs(new), 0);
            strcat(new->text, idString);
            strcat(new->text, " just arrived\n");
            addMsg(&clients->msgs, new);
        }
        clients = clients->next;
    }
    return (1);
}

int addDisconnectedMsg(t_client *clients, int id)
{
    while (clients)
    {
        if (clients->id != id)
        {
            t_msg   *new = malloc(sizeof(t_msg));
            if (!new)
                return (0);
            new->next = NULL;
            new->offset = 0;
            new->text = malloc(16 * sizeof(char));
            if (!new->text)
                return (freeMsgs(new), 0);
            strcpy(new->text, "server: client ");
            char idString[4];
            sprintf(idString, "%d", id);
            new->text = realloc(new->text, strlen(new->text) + strlen(idString) + strlen(" just left\n") + 1);
            if (!new->text)
                return (0);
            strcat(new->text, idString);
            strcat(new->text, " just left\n");
            addMsg(&clients->msgs, new);
            printf("msg -> %s\n", new->text);
        }
        clients = clients->next;
    }
    return (1);
}

void initWriteFds(fd_set *writeFds, t_client *clients)
{
    FD_ZERO(writeFds);

    while (clients)
    {
        if (clients->msgs && !clients->disconnect)
            FD_SET(clients->fd, writeFds);
        clients = clients->next;
    }
}

void    cleanClients(t_client **clients, fd_set *allFds)
{
    t_client *tmp = *clients;
    t_client *previous = NULL;
    t_client *tmp2;
    while (tmp)
    {
        if (tmp->disconnect)
        {
            if (!previous)
                *clients = tmp->next;
            else
                previous->next = tmp->next;
            tmp2 = tmp->next;
            FD_CLR(tmp->fd, allFds);
            close(tmp->fd);
            freeMsgs(tmp->msgs);
            free(tmp);
            tmp = tmp2;
        }
        else
        {
            previous = tmp;
            tmp = tmp->next;
        }
    }
}

int sendMsgs(t_client *client)
{
    t_msg *msg = client->msgs;
    while (msg)
    {
        int len = (int)strlen(msg->text);
        ssize_t sent = send(client->fd, msg->text + msg->offset, len - msg->offset, 0);
        if (sent < 0)
            return (0);
        msg->offset += sent;
        if (msg->offset >= (int)len)
        {
            t_msg *tmp = msg->next; 
            freeMsg(msg, &client->msgs);
            msg = tmp; 
        }
        else
            break ;
    }
    return (1);
}

int handleClient(int fd, t_client **clients)
{
    char buffer[1000];
    t_client *client = getClient(fd, *clients);
    if (!client)
        return (0);
    ssize_t read = recv(client->fd, buffer, sizeof(buffer), 0);
    if (read < 0)
        return (0);
    if (read == 0)
    {
        printf("Un client veut se deconnecter\n");
        if (!addDisconnectedMsg(*clients, client->id))
            return (0);
        client->disconnect = 1;
        return (1);
    }
    return (1);
}

int getMaxFd(int server, t_client *clients)
{
    int max = server;
    while (clients)
    {
        if (clients->fd > max)
            max = clients->fd;
        clients = clients->next;
    }
    return (max);
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

        initWriteFds(&writeFds, clients);
        cleanClients(&clients, &allFds);
        readFds = allFds;

        printf("getMaxFd -> %d\n", getMaxFd(server, clients));
        if (select (getMaxFd(server, clients) + 1, &readFds, &writeFds, NULL, NULL) < 0)
        {
            perror("select:");
            printf("Sortie 3\n");
            exitFatalError(clients, server);
        }

        int i = -1;
        while (++i <= maxFd)
        {
            if (FD_ISSET(i, &readFds))
            {
                if (i == server)
                {
                    int val = acceptClient(server, &allFds, &clients, &maxFd, &idClient);
                    if (val == -1)
                    {
                        printf("Sortie 4\n");
                        exitFatalError(clients, server);
                    }
                    if (!addConnectedMsg(clients, val))
                    {
                        printf("Sortie 5\n");
                        exitFatalError(clients, server);
                    }
                }
                else
                {
                    if (!handleClient(i, &clients))
                    {
                        printf("Sortie 0\n");                        
                        exitFatalError(clients, server);
                    }
                }
            }

            if (FD_ISSET(i, &writeFds))
            {
                t_client *client = getClient(i, clients);
                if (!client)
                {
                        printf("Sortie 1\n");                        
                        exitFatalError(clients, server);
                }
                if (!sendMsgs(client))
                {
                    printf("Sortie 2\n");                        
                    exitFatalError(clients, server);
                }
            }
        }
    }

    freeClients(clients);
    close(server);

    return (0);
}