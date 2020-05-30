#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include <strings.h>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define NAME_LEN 32

static int cli_count = 0;
static int uid = 10;

//Struct to hold client information
typedef struct
{
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[NAME_LEN];
} client_t;

client_t *clients[MAX_CLIENTS];

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;


//Adds client to clients array
void queue_add(client_t *cl){
    pthread_mutex_lock(&client_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(!clients[i]){
            clients[i] = cl;
            break;
        }
    }
    pthread_mutex_unlock(&client_mutex);
}

//remove client from clients array
void queue_remove(int uid){
    pthread_mutex_lock(&client_mutex);

    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i]){
            if(clients[i]->uid == uid){
                clients[i] = NULL;
                break;
            }
        }
    }

    pthread_mutex_unlock(&client_mutex);
}

//function to send message to a user
void send_message(char *username, char *s){
    int is_sent = 0;
    pthread_mutex_lock(&client_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++){
        if(clients[i]){
            if(strcmp(clients[i]->name, username) == 0){
                is_sent = 1;
                if(write(clients[i]->sockfd, s, strlen(s)) < 0){
                    perror("write");
                    break;
                }
                printf("Message sent to %s", username);
            }
        }
    }
    if(is_sent == 0){
        printf("%s doesn't exist\n", username);
    }
    pthread_mutex_unlock(&client_mutex);
}


//thread function for receiving from every client
void *clientThread(void *arg){
    char buffer[BUFFER_SZ];
    char name[NAME_LEN];
    int leave_flag = 0;
    cli_count++;

    client_t *cli = (client_t*) arg;
    //receives name of client and checks if it's correct
    if(recv(cli->sockfd, name, NAME_LEN, 0) <= 0 || strlen(name) < 2 || strlen(name) >= NAME_LEN - 1){
        printf("Enter the name correctly\n");
        leave_flag = 1;
    }else{
        strcpy(cli->name, name);
        printf("Name entered corectly\n");
    }


    while(1){
        if(leave_flag){
            break;
        }
        char recever[100], message[100];

        //getting receiver name
        int receveRecever = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
        if(receveRecever > 0){
            if(strlen(buffer) > 0){
                //gets recever             
                strcpy(recever, buffer);
            }
        }else if (receveRecever == 0 || strcmp(buffer, "exit") == 0)
        {
            leave_flag = 1;
        }else
        {
            perror("server");
            leave_flag = 1;
        }
        bzero(buffer, BUFFER_SZ);




        //getting message to send
        int receveMessage = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
        if(receveMessage > 0){
            if(strlen(buffer) > 0){
                //gets message            
                strcpy(message, buffer);

            }
        }else if (receveMessage == 0 || strcmp(buffer, "exit") == 0)
        {
            leave_flag = 1;
        }else
        {
            perror("server");
            leave_flag = 1;
        }
        bzero(buffer, BUFFER_SZ);


        //sends message to recever
        send_message(recever, message);

    }
    close(cli->sockfd);
    queue_remove(cli->uid);
    free(cli);
    cli_count--;
    pthread_detach(pthread_self());

    return NULL;
}

int main(int argc, char **argv) {
    char *ip = "127.0.0.1";
    int port = 4444;

    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    //Socket setup
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    signal(SIGPIPE, SIG_IGN);

    if(setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0 ){
        perror("setsockopt");
        return EXIT_FAILURE;
    }

    if(bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
        perror("bind");
        return EXIT_FAILURE;
    }

    if(listen(listenfd, 10) < 0){
        perror("listen");
        return EXIT_FAILURE;
    }
    
    while (1)
    {
        //accepting clients
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        if((cli_count+1) == MAX_CLIENTS){
            printf("Max clients are conected\n");
            close(connfd);
            continue;
        }
        //setting client information
        client_t *cli = (client_t *)malloc(sizeof(client_t));
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        queue_add(cli);
        //creating thread for the client
        pthread_create(&tid, NULL, &clientThread, (void*)cli);
        //for lower cpu usage
        sleep(1);
    }
    
    
    
    return EXIT_SUCCESS;
}