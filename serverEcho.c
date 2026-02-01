#include <stdio.h>      // pour printf, perror, etc.
#include <stdlib.h>     // pour exit
#include <string.h>     // pour memset
#include <unistd.h>     // pour read, write, close
#include <netinet/in.h> // pour les structures sockaddr_in6, htons, etc.
#include <arpa/inet.h>  // pour inet_pton, etc.
#include <sys/socket.h> // pour socket, bind, listen, accept


void client_arrived(int sock){
    //Lecture des messages
    char* buff = malloc(100 * sizeof(char));
    if (!buff) {
        perror("echec allocation du buffer du msg");
        return;
    }

    int r = -1; //Peu importe la valeur
    while ( (r=read(sock,buff,99)) >0) {
        buff[r]='\0';
        
        printf("Je vous entends : %s\n",buff);
    }
    free(buff);
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

            close(sock2);
            printf("client parti\n");
        }

}


int main(int argc, char** argv){
    if(argc != 2){
        perror("Syntaxe: ./server [port]");
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);

    serveur(port);
}