//Exercice 4

#include <stdio.h>      // pour printf, perror, etc.
#include <stdlib.h>     // pour exit
#include <string.h>     // pour memset
#include <unistd.h>     // pour read, write, close
#include <netinet/in.h> // pour les structures sockaddr_in6, htons, etc.
#include <arpa/inet.h>  // pour inet_pton, etc.
#include <sys/socket.h> // pour socket, bind, listen, accept
#include <pthread.h>
#include <time.h>
#include <ctype.h> // pour isalnum
#include <signal.h>


//Struct

typedef struct s_msg {
    char sender[9]; //tableau statique
    char* text;
    struct s_msg* next;
}msg;

typedef struct s_mbox{
    msg* first;
    msg* last;
}mbox;

typedef struct client_data_s {
    int sock;
    pthread_t thread;
    struct client_data_s* next;
    char alias[9];
    mbox box;
}client_data;


//Fonctions TP5
int nbrDansChaine(char* chaine){
    int compteur = 0;
    int i = 0;
    
    while(chaine[i] != '\0'){
        compteur++;
        i++;
    }
    return compteur;
}
msg* create_msg(char* author, char* contents){
    msg* m = malloc(sizeof(msg));
    if (!m) return NULL;

    //m->sender = author; marche pas car tableau statique
    strncpy(m->sender,author,8);
    *(m->sender+8) = '\0'; //sender tableau !!

    //Transfert le param a notre msg
    int n = nbrDansChaine(contents); //taille de la chaine sans '0'
    m->text = malloc((n+1)*sizeof(char)); //on alloue la mmoire pour le text, plus 1 pour le '0' apres (avec le n pcq on veut plus un char)
    if (!m->text){
        free(m);
        return NULL;
    }
    strcpy(m->text, contents);
    // *(m->text+n+1) = '\0'; sert a rien avec strcpy

    return m;
}
void show_msg(msg* mess){
    printf("Autheur: %s\nContenu: %s\n",mess->sender,mess->text);
}
void destroy_msg(msg* mess){
    free(mess->text);
    free(mess);
}

void init_mbox(mbox* b){
    b->first = NULL;
    b->last = NULL;
}
void put(mbox* box, msg* mess){
    mess->next = NULL;
    
    if(box->first == NULL && box->last == NULL){
        box->first = mess;
        box->last = mess;
    }
    else{
        box->last->next = mess;
        box->last = mess;
        
    }
}

void afficheElemBoite(mbox* boite){
    msg* current = boite->first;
    while(current != NULL){
        show_msg(current);
        current = current->next;
        printf("\n");
    }
}

msg* get(mbox* boite){
    if(boite->last == NULL && boite->first == NULL){
        return NULL;
    }
    
    msg* m = boite->first;
    boite->first = boite->first->next;

    if(boite->first == NULL){
        boite->last = NULL;
    }

    return m;

}

void send_mess(mbox* box, char* author, char* contents){
    msg* new = create_msg(author,contents);
    put(box,new);
}

void recieve_mess(mbox* box){
    if (!(box->last == NULL && box->first == NULL)){
        msg* recu = get(box);
        show_msg(recu);
        destroy_msg(recu);
    }
    else{
        printf("Aucun Message dans la boite");
    }
}


// Variables globales
int nr_clients;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#define BUFSIZE 1024

client_data* first;
client_data* last;

void init_clients(){
    nr_clients = 0;
    first = NULL;
    last = NULL;
}

int valid_alias(char* alias) {
    int len = strlen(alias);
    if (len < 1 || len > 8) return 0;

    for (int i = 0; i < len; i++) {
        if (!isalnum(alias[i])) return 0;
    }

    return 1;
}


client_data* alloc_client(int sock){
    client_data* c = malloc(sizeof(client_data));
    if (!c) {
        perror("malloc client_data");
        exit(EXIT_FAILURE);
    }
    c->sock = sock;
    c->next = NULL;

    pthread_mutex_lock(&mutex);
    nr_clients++;

    if(first == NULL){
        //cas liste vide donc premier client
        first = c;
        last = c;
    } else {
        last->next = c;
        last = c;
    }

    c->alias[0] = '\0';
    init_mbox(&c->box);


    pthread_mutex_unlock(&mutex);
    return c;
}

client_data* search_client(char* alias) {
    pthread_mutex_lock(&mutex);
    client_data* curr = first;
    while (curr != NULL) {
        if (strcmp(curr->alias, alias) == 0) {
            pthread_mutex_unlock(&mutex);
            return curr;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&mutex);
    return NULL;
}

void free_client(client_data* cli){
    pthread_mutex_lock(&mutex);
    // Cas où la liste est vide
    if (first == NULL || cli == NULL) {
        pthread_mutex_unlock(&mutex);
        return;
    }else if (cli == first){
        first = cli->next; //Si c'est le premier ou redefinie le premier
    }
    
    else {
        //on va chercher l'élément précédant
        client_data* prev = first;
        while(prev->next != cli){
            prev = prev->next;
        }
        prev->next = cli->next;
        if(last == cli){ //Si on a loop jusqu'au dernier
            last = prev;
        }
        
    }

    msg* m;
    while ((m = get(&cli->box)) != NULL) {
        destroy_msg(m);
    }
    
    close(cli->sock);
    free(cli);
    
    nr_clients--;
    pthread_mutex_unlock(&mutex);

    pthread_mutex_lock(&mutex);
    printf("Nombre de clients actifs: %d\n", nr_clients);
    pthread_mutex_unlock(&mutex);  
    
}

int do_list(char* resp) {
    pthread_mutex_lock(&mutex);

    strcpy(resp, "ok");

    client_data* curr = first;
    while (curr != NULL) {
        if (curr->alias[0] != '\0') { // seulement les clients avec alias
            strncat(resp, " ", BUFSIZE - strlen(resp) - 1);
            strncat(resp, curr->alias, BUFSIZE - strlen(resp) - 1);
        }
        curr = curr->next;
    }

    strncat(resp, "\n", BUFSIZE - strlen(resp) - 1);
    pthread_mutex_unlock(&mutex);
    return 0;
}


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

//Modifications de eval_quest
int do_alias(char* arg, char* resp, client_data* sender) {
    char alias[100];
    if (arg == NULL || sscanf(arg, "%s", alias) != 1) {
        snprintf(resp, BUFSIZE, "fail alias missing or invalid\n");
        return 0;
    }

    if (!valid_alias(alias)) {
        snprintf(resp, BUFSIZE, "fail invalid alias\n");
        return 0;
    }

    printf("alias reçu: %s\n", alias);

    pthread_mutex_lock(&mutex);

    // On parcourt manuellement ici au lieu d'appeler search_client()
    client_data* curr = first;
    while (curr != NULL) {
        if (strcmp(curr->alias, alias) == 0) {
            pthread_mutex_unlock(&mutex);
            snprintf(resp, BUFSIZE, "fail alias unavailable\n");
            return 0;
        }
        curr = curr->next;
    }

    strncpy(sender->alias, alias, 8);
    sender->alias[8] = '\0';
    pthread_mutex_unlock(&mutex);

    snprintf(resp, BUFSIZE, "ok %s\n", sender->alias);

    return 0;
}

int do_send(char* arg, char* resp, client_data* sender) {
    char target_alias[100], message[BUFSIZE];

    if (arg == NULL || sscanf(arg, "%s %[^\n]", target_alias, message) < 2) {
        snprintf(resp, BUFSIZE, "fail send missing or invalid arguments\n");
        return 0;
    }

    pthread_mutex_lock(&mutex);

    // Remplacer search_client() par un parcours manuel
    client_data* curr = first;
    while (curr != NULL) {
        if (strcmp(curr->alias, target_alias) == 0) {
            break;
        }
        curr = curr->next;
    }

    if (curr == NULL) {
        pthread_mutex_unlock(&mutex);
        snprintf(resp, BUFSIZE, "fail unknown alias\n");
        return 0;
    }

    if (strlen(message) == 0) {
        pthread_mutex_unlock(&mutex);
        snprintf(resp, BUFSIZE, "fail empty message\n");
        return 0;
    }

    send_mess(&curr->box, sender->alias, message);
    pthread_mutex_unlock(&mutex);

    snprintf(resp, BUFSIZE, "ok\n");
    return 0;
}


int do_recv(char* resp, client_data* receiver) {
    pthread_mutex_lock(&mutex);
    msg* m = get(&receiver->box);
    pthread_mutex_unlock(&mutex);

    if (m == NULL) {
        snprintf(resp, BUFSIZE, "ok\n");
        return 0;
    }

    snprintf(resp, BUFSIZE, "ok %s %s\n", m->sender, m->text);
    destroy_msg(m);
    return 0;
}


int eval_quest(char *question, char *resp, client_data* sender,int sock) {
    char *cmd, *args;
    cmd = strsep(&question, " ");//recup le string jusqu'a la delimitati
    args = question; //recup le reste

    if ((strcmp(cmd, "send") == 0 || strcmp(cmd, "recv") == 0 || strcmp(cmd, "list") == 0) && sender->alias[0] == '\0') {
    snprintf(resp, BUFSIZE, "fail alias required\n");
    return 0;
}


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
        
    else if (strcmp(cmd, "list") == 0){
        return do_list(resp);
    } else if (strcmp(cmd, "alias") == 0) {
        return do_alias(args, resp, sender);
    } else if (strcmp(cmd, "send") == 0) {
        return do_send(args, resp, sender);
    } else if (strcmp(cmd, "recv") == 0) {
        return do_recv(resp, sender);
    }
    
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

void* worker(void* arg) {
    signal(SIGPIPE, SIG_IGN);

    char question[BUFSIZE], response[BUFSIZE], buf[BUFSIZE];
    client_data* cli = (client_data*) arg;
    int len = 0;

    while (1) {
        if (get_question(cli->sock, buf, &len, question) < 0)
            break;

        if (eval_quest(question, response, cli, cli->sock) < 0)
            break;


        if (write_full(cli->sock, response, strlen(response)) < 0) {
            perror("could not send message");
            break;
        } else {
            printf("Message envoyé avec succès\n");
            fflush(stdout);
        }
    }

    printf("client %s parti\n", cli->alias[0] ? cli->alias : "(anonyme)");
    fflush(stdout);

    close(cli->sock);
    free_client(cli);
    pthread_exit(NULL);
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