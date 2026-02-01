//Exercice 3

#include <stdio.h>      // pour printf, perror, etc.
#include <stdlib.h>     // pour exit
#include <string.h>     // pour memset
#include <unistd.h>     // pour read, write, close
#include <netinet/in.h> // pour les structures sockaddr_in6, htons, etc.
#include <arpa/inet.h>  // pour inet_pton, etc.
#include <sys/socket.h> // pour socket, bind, listen, accept
#include <pthread.h>
#include <time.h>

// Variables globales
int nr_clients;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define BUFSIZE 1024


void init_clients(){
    nr_clients = 0;
}

typedef struct client_data_s {
    int sock;
    pthread_t thread;
}client_data;

client_data* alloc_client(int sock){
    client_data* c = malloc(sizeof(client_data));
    if (!c) {
        perror("malloc client_data");
        exit(EXIT_FAILURE);
    }
    c->sock = sock;

    pthread_mutex_lock(&mutex);
    nr_clients++;
    pthread_mutex_unlock(&mutex);
    return c;
}

void free_client(client_data* cli){
    free(cli);
    pthread_mutex_lock(&mutex);
    nr_clients--;
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    printf("Nombre de clients actifs: %d\n", nr_clients);
    pthread_mutex_unlock(&mutex);  
    
}

/*
int get_question(int sock, char* buffer, int* len, char* question){
    int r = -1; //Peu importe la valeur
    while ( (r=read(sock,buffer,*len)) >0) {
        buff[r]='\0';
        
        printf("Je vous entends : %s\n",buff);
    }
    return r;
}
*/

int get_question(int sock, char *buffer, int *len, char *question) {
    int i;
    while (1) {
        // Cherche un '\n' dans les données déjà lues
        for (i = 0; i < *len; i++) {
            if (buffer[i] == '\n') {
                // Si le caractère précédent est '\r', on le saute aussi
                int end = (i > 0 && buffer[i - 1] == '\r') ? i - 1 : i;

                // Copie la question dans le tampon 'question'
                memcpy(question, buffer, end);
                question[end] = '\0';

                // Décale les données restantes dans le buffer
                *len -= (i + 1);
                memmove(buffer, buffer + i + 1, *len);

                return 0; // Succès
            }
        }

        // Si on est ici, on n’a pas trouvé de '\n', on lit plus de données
        if (*len >= BUFSIZE) {
            fprintf(stderr, "Message trop long\n");
            return -1;
        }

        int r = read(sock, buffer + *len, BUFSIZE - *len);
        if (r <= 0) {
            return -1; // Erreur ou fermeture de la connexion
        }

        *len += r;
    }
}


int eval_quest(char *question, char *resp) {
    char *cmd, *args;
    cmd = strsep(&question, " ");//recup le string jusqu'a la delimitati
    args = question; //recup le reste

    if (cmd == NULL || strlen(cmd) == 0) {
        snprintf(resp, BUFSIZE, "fail empty command\n");
        return 0;
    }

    if (strcmp(cmd, "echo") == 0){
        if (args == NULL)
        args = "";  // Si pas d'arguments, on renvoie juste "ok"
        snprintf(resp, BUFSIZE, "ok %s\n", args);
        return 0;
    }
    else if (strcmp(cmd, "random") == 0){ //Pour random
        if(args != NULL){   
            int n = atoi(args);
            int r = rand()%n; //nombre compris entre 0 et n avec modulo
            snprintf(resp, BUFSIZE, "ok %d\n", r);
            return 0;
        }
        else {
            int r = rand();
            snprintf(resp, BUFSIZE, "ok %d\n", r);
            return 0;
        }
    }
    else if (strcmp(cmd, "time") == 0){
        time_t mnt = time(NULL);
        struct tm *t = localtime(&mnt);
    
        snprintf(resp, BUFSIZE, "ok %02d:%02d:%02d %d/%d/%d\n",
                 t->tm_hour, t->tm_min, t->tm_sec,
                 t->tm_mday, t->tm_mon + 1, 1900 + t->tm_year); //mois commence a 0 donc +1 et annee commence a 1900
        return 0;
    }

    else if (strcmp(cmd, "quit") == 0)
        return -1;

    else {
        snprintf(resp, BUFSIZE, "fail unknown command '%s'\n", cmd);
        return 0;
    }
}

int write_full(int sock, char* reponse, int len){
    if(write(sock, reponse, len)<0){
        perror("write erreur");
        return -1;
    }
    return 0;
}


void* worker(void* arg){
    char question[BUFSIZE], response[BUFSIZE],buf[BUFSIZE];
    client_data* cli = (client_data*) arg; //on recup les infos du clients

    int r = -1; //Peu importe la valeur
    int len = 0;
    while(1) {
        if(get_question(cli->sock,buf, &len, question) < 0)
            break; /* erreur: on deconnecte le client */
        if(eval_quest(question, response) < 0)
            break; /* erreur: on deconnecte le client */
        if(write_full(cli->sock, response, strlen(response)) < 0) {
            perror("could not send message");
            break; /* erreur: on deconnecte le client */
        }
    }

    
    printf("client parti\n");

    close(cli->sock); // Fermer le socket du client
    free_client(cli); // Libérer la mémoire et décrémenter nr_clients
    pthread_exit(NULL); // Terminer le thread
}

void client_arrived(int sock){
    client_data* cli = alloc_client(sock);

    if(pthread_create(&cli->thread,NULL,worker,(void*)cli) != 0){
        perror("Erreur pthread_create");
        close(sock);
        free_client(cli);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_lock(&mutex);
    printf("Nombre de clients actifs: %d\n", nr_clients);
    pthread_mutex_unlock(&mutex);    
}

void serveur(int port){
    //Création du socket serv
    int sock = socket(PF_INET6,SOCK_STREAM,0);
    if (sock < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    //On crée et associe la struct de la socket
    struct sockaddr_in6 sockaddr;
    memset(&sockaddr,0,sizeof(sockaddr)); //On met à zero les valeurs de la struct
    sockaddr.sin6_family=PF_INET6;
    sockaddr.sin6_port=htons(port);

    //On associe maintenant la socket
    int r = bind(sock,(struct sockaddr*) &sockaddr,sizeof(sockaddr));
    if(r<0){
        perror("bind"); close(sock);exit(EXIT_FAILURE);
    }
    
    r=listen(sock,1024);
    if(r<0){
        perror("listen");close(sock);exit(EXIT_FAILURE);
    }
    printf("j'attends mes clients\n");
    
        //Attente des clients
        while(1){
            int sock2=accept(sock,NULL,NULL);
            if (sock2<0){
                perror("accept"); continue;
            }
            printf("client arrivé\n");

            client_arrived(sock2);

            //close(sock2);
        }

}


int main(int argc, char** argv){
    if(argc != 2){
        perror("Syntaxe: ./server [port]");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    init_clients(); // initialiser le compteur clients

    serveur(port);

    return 0;
}