#include <stdio.h>      // pour printf, perror, etc.
#include <stdlib.h>     // pour exit
#include <string.h>     // pour memset
#include <unistd.h>     // pour read, write, close
#include <netinet/in.h> // pour les structures sockaddr_in6, htons, etc.
#include <arpa/inet.h>  // pour inet_pton, etc.
#include <sys/socket.h> // pour socket, bind, listen, accept
#include <pthread.h>

// Variables globales
int nr_clients;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

void* worker(void* arg){
    client_data* cli = (client_data*) arg; //on recup les infos du clients

    //Lecture des messages
    char* buff = malloc(100 * sizeof(char)); // 99 + 1 pour \0
    if (!buff) {
        perror("echec allocation du buffer du msg");
        close(cli->sock);
        free_client(cli);
        pthread_exit(NULL);
    }

    int r = -1; //Peu importe la valeur
    while ( (r=read(cli->sock,buff,99)) >0) {
        buff[r]='\0';
        
        printf("Je vous entends : %s\n",buff);
    }

    
    printf("client parti\n");

    free(buff);
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