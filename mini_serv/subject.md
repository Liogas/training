# MINI_SERV

- test1.c -> brouillon pas compilable
- test2.c -> brouillon, connexion et deconnexion ok mais envoie de msg trop compliqué à implémenter
- test3.c -> brouillon, pas loin du but mais finalement encore trop loin
- test4.c -> version fonctionnel, se servir de cette logique pour l'exam (pas d'utilisation de writefds, peut etre problématique)

30/06
- training1.c -> 1h15 mais quelques erreurs presentent lors de la compilation
- training2.c -> Depart : 11h30 - Fin : 12h52 - problemes leaks + oublie pour disconnect
- training3.c -> Depart : 13h30 - Fin : 14h25 - erreurs d'innatention (FD_SET avec readfds au lieu de allfds, ajout du message de depart au mauvais endroit)
- training4.c -> Depart : 14h48 - Fin : 15h45 - petit oublie de readfds = allfds mais sinon parfait
- training5.c -> Depart : 16h04 - Fin : 16h56: - (OBJECTIF finir en 45 min a peu pres) -> 52min mais erreur de merde, FD_ISSET sur allfds au lieu de readfds

31/06
Objectif : avoir le temps de le faire 2 fois avant l'exam et le finir en 45min au deuxieme essai.
- training6.c -> Depart : 8h25 - Fin : 9h15 - grosse erreur d'innatention qui a pris du temps a etre vu tmp->next = tmp au lieu de tmp->next = client lors de l'ajout d'un client dans la liste

## Assignment name  : mini_serv
## Expected files   : mini_serv.c
## Allowed functions: 
- write
- close
- select
- socket
- accept
- listen
- send
- recv
- bind
- strstr
- malloc
- realloc
- free
- calloc
- bzero
- atoi
- sprintf
- strlen
- exit
- strcpy
- strcat
- memset

----------------------------------------------------------------------------

### Write a program that will listen for client to connect on a certain port on 127.0.0.1 and will let clients to speak with each other

### This program will take as first argument the port to bind to
If no argument is given, it should write in stderr "Wrong number of arguments" followed by a \n and exit with status 1
If a System Calls returns an error before the program start accepting connection, it should write in stderr "Fatal error" followed by a \n and exit with status 1
If you cant allocate memory it should write in stderr "Fatal error" followed by a \n and exit with status 1

### Your program must be non-blocking but client can be lazy and if they don't read your message you must NOT disconnect them...

### Your program must not contains #define preproc
### Your program must only listen to 127.0.0.1
### The fd that you will receive will already be set to make 'recv' or 'send' to block if select hasn't be called before calling them, but will not block otherwise. 

### When a client connect to the server:
- the client will be given an id. the first client will receive the id 0 and each new client will received the last client id + 1
- %d will be replace by this number
- a message is sent to all the client that was connected to the server: "server: client %d just arrived\n"

### clients must be able to send messages to your program.
- message will only be printable characters, no need to check
- a single message can contains multiple \n
- when the server receive a message, it must resend it to all the other client with "client %d: " before every line!

### When a client disconnect from the server:
- a message is sent to all the client that was connected to the server: "server: client %d just left\n"

### Memory or fd leaks are forbidden

To help you, you will find the file main.c with the beginning of a server and maybe some useful functions. (Beware this file use forbidden functions or write things that must not be there in your final program)

Warning our tester is expecting that you send the messages as fast as you can. Don't do un-necessary buffer.

Evaluation can be a bit longer than usual...

Hint: you can use nc to test your program
Hint: you should use nc to test your program
Hint: To test you can use fcntl(fd, F_SETFL, O_NONBLOCK) but use select and NEVER check EAGAIN (man 2 send)
