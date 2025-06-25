#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/select.h>
#include <sys/socket.h>

#include <string.h>
#include <strings.h>

#include <netinet/in.h>


typedef struct
{
    int id;
    int fd;
    struct sockaddr_in addr;
    socklen_t addrLen;
    char    *buffer;
    t_client *next;
}   t_client;


typedef struct
{
    char    *msg;
    int     id;
    int     remaining;
    int     type;
    t_msg   *next;
}   t_msg;

t_msg   *createMsg(int id, char *msg, int nbClient, int type)
{
    t_msg   *new = malloc(sizeof(t_msg));
    if (!new)
        return (NULL);
    new->id = id;
    new->msg = msg;
    new->next = NULL;
    new->type = type;
    new->remaining = nbClient - 1;
    return (new);
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

void    freeMsgs(t_msg *msgs)
{
    t_msg *tmp = msgs;
    while (tmp)
    {
        tmp = msgs->next;
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
            if (!before && msg->next)
                *msgs = NULL;
            else if (!before)
                *msgs = msg->next;
            else
                before->next = msg->next;
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

char *cutMsg(t_client *client)
{
    int     pos = 0;
    char    *tmp = NULL;
    while (client->buffer[pos] != '\n')
        pos++;
    pos++;
    char *msg = malloc( (pos + 1) * sizeof(char));
    if (!msg)
        return (NULL);
    int i = -1;
    while (++i <= pos)
        msg[i] = client->buffer[i];
    if (client->buffer[pos + 1] != '\0')
    {
        tmp = malloc((strlen(client->buffer - pos) * sizeof(char)));
        if (!tmp)
            return (free(msg), NULL);
        int i = -1;
        while (client->buffer[++pos])
            tmp[++i] = client->buffer[pos];
    }
    free(client->buffer);
    client->buffer = tmp;
    return (msg);
}

int handleMsg(t_msg **msgs, t_client *client, char *buffer, int nbClient)
{
    if (client->buffer)
        client->buffer = realloc(client->buffer, strlen(client->buffer) + strlen(buffer) + 1);
    else
        client->buffer = malloc( (strlen(buffer) + 1) * sizeof(char));
    if (!client->buffer)
        return (0);
    strcat(client->buffer, buffer);
    if (!client->buffer)
        return (0);
    int run = 1;
    while (run)
    {
        char *tmp = strstr(client->buffer, "\n");
        if (tmp)
        {
            char *msgTmp = cutMsg(client);
            if (!msgTmp)
                return (0);
            t_msg   *msg = createMsg(client->fd, msgTmp, nbClient, 3);
            if (!msg)
                return (free(msgTmp), 0);
            addMsg(&msgs, msg);
        }
        else
            run = 0;
    }
    return (1);
}

void    exitFatalError(t_client *clients, t_msg *msgs, int serv)
{
    freeClients(clients);
    freeMsgs(msgs);
    write(2, "Fatal error\n", 12);
    close(serv);
    exit(1);
}

int sendAllMsg(t_msg *msgs, t_client *client)
{
    while (msgs)
    {
        msgs->remaining--;
        msgs = msgs->next;
    }
    return (1);
}

int main(int ac, char **av)
{
    printf("=== MINI SERV ===\n");
    if (ac != 2)
    {
        write(2, "Wrong number of arguments\n", 26);
        exit(1);
    }
    int port = atoi(av[1]);
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
    int serv = socket(AF_INET, SOCK_STREAM, 0);
    if (serv == -1)
    {
        write(2, "Fatal error\n", 12);
        exit(1);
    }
    if (bind(serv, (const struct sockaddr *)&servAddr, sizeof(servAddr)) != 0 ||
        listen(serv, 50) != 0)
    {
        write(2, "Fatal error\n", 12);
        close(serv);
        exit(1);
    }

    char    buffer[50];
    sprintf(buffer, "%d", port);
    write(1, "Serv listening at 127.0.0.1:", 28);
    write(1, buffer, strlen(buffer));
    write(1, "\n", 1);

    fd_set allFds;
    fd_set readFds, writeFds;

    t_client *clients = NULL;
    t_msg    *msgs = NULL;

    int maxFd = serv;
    int idClient = 0;
    int nbClient = 0;

    FD_ZERO(&allFds);
    FD_ZERO(&writeFds);

    FD_SET(serv, &allFds);

    while (1)
    {
        readFds = allFds;
        if (msgs)
            writeFds = allFds;
        else
            FD_ZERO(&writeFds);
        select(maxFd, &readFds, &writeFds, NULL, NULL);
        for (int i = 0; i <= maxFd; i++)
        {
            if (FD_ISSET(i, &readFds))
            {
                // PRET POUR LA LECTURE
                if (i == serv)
                {
                    // NOUVEAU CLIENT
                    t_client *client = malloc(sizeof(t_client));
                    if (!client)
                        exitFatalError(clients, msgs, serv);
                    bzero(&client->addr, sizeof(client->addr));
                    client->buffer = NULL;
                    client->addrLen = sizeof(client->addr);
                    client->fd = accept(serv, (struct sockaddr *)&client->addr, &client->addrLen);
                    if (client->fd == -1)
                        exitFatalError(clients, msgs, serv);
                    else if (client->fd  = maxFd)
                        maxFd = client->fd;
                    client->id = idClient;
                    idClient++;
                    nbClient++;
                    addClient(&clients, client);
                    // AJOUTER MSG SERVEUR
                    t_msg *msg = createMsg(client->id, NULL, nbClient, 1);
                    if (!msg)
                        exitFatalError(clients, msgs, serv);
                    addMsg(&msgs, msg);
                }
                else
                {
                    // CLIENT A ENVOYE UN MSG
                    t_client *client = getClient(i, clients);
                    if (!client)
                        exitFatalError(clients, msgs, serv);
                    char buffer[1000];
                    ssize_t read;
                    read = recv(client->fd, buffer, sizeof(buffer), MSG_DONTWAIT);
                    if (read == -1)
                        exitFatalError(clients, msgs, serv);
                    else if (read == 0)
                    {
                        FD_CLR(client->fd, &allFds);
                        freeClient(client, clients);
                        nbClient--;
                        // AJOUTER MSG SERVEUR
                        t_msg *msg = createMsg(client->id, NULL, nbClient, 2);
                        if (!msg)
                            exitFatalError(clients, msgs, serv);
                        addMsg(&msgs, msg);
                    }
                    else if (!handleMsg(&msgs, client, buffer, nbClient))
                        exitFatalError(clients, msgs, serv);
                }
            }
            if (FD_ISSET(i, &writeFds))
            {
                t_client *client = getClient(i, clients);
                if (!client)
                    exitFatalError(clients, msgs, serv);
                if (!sendAllMsg(msgs, client))
                    exitFatalError(clients, msgs, serv);
            }
        }
    }

    close(serv);
    return (0);
}