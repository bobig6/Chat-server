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
#define MAX_GROUPS 100
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

//Struct to hold groups information
typedef struct
{
    char name[NAME_LEN];
    client_t *clients[MAX_CLIENTS];
    int last_client;
} group_t;


client_t *clients[MAX_CLIENTS];
group_t *groups[MAX_CLIENTS];

pthread_mutex_t client_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t groups_mutex = PTHREAD_MUTEX_INITIALIZER;


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


//Adds group to groups array
void add_group(char name[NAME_LEN]){
    pthread_mutex_lock(&groups_mutex);
    for(int i = 0; i < MAX_GROUPS; i++){
        if(!groups[i]){
            group_t *gr = (group_t *)malloc(sizeof(group_t));
            strcpy(gr->name, name);
            gr->last_client = 0;
            groups[i] = gr;
            break;
        }
    }
    pthread_mutex_unlock(&groups_mutex);
}

//Adds user to group array
void add_user_to_group(client_t *cl, char name[NAME_LEN]){
    pthread_mutex_lock(&groups_mutex);
    for(int j = 0; j < MAX_GROUPS; j++){
        if(groups[j]){
            if(strcmp(groups[j]->name, name) == 0){
                for(; groups[j]->last_client < MAX_CLIENTS;){
                    if(groups[j]->clients[groups[j]->last_client]->name != NULL){}
                    groups[j]->clients[groups[j]->last_client] = cl;
                    printf("%s joined %s with id %d\n", groups[j]->clients[groups[j]->last_client]->name, groups[j]->name, groups[j]->last_client);
                    groups[j]->last_client++;
                    break;
                    
                }
            }
        }
    }
    pthread_mutex_unlock(&groups_mutex);
}


//remove client from group
void remove_user_from_group(int uid, char name[NAME_LEN]){
    pthread_mutex_lock(&groups_mutex);

    for(int j = 0; j < MAX_GROUPS; j++){
        for(int i = 0; i < MAX_CLIENTS; i++){
            if(groups[j]){
                if(strcmp(groups[j]->name, name) == 0){
                    if(!groups[j]->clients[i]){
                        if(groups[j]->clients[i]->uid == uid){
                            clients[i] = NULL;
                            break;
                        }
                    }
                }
            }
        }
    }

    pthread_mutex_unlock(&groups_mutex);
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



//function to send message to a group
void send_message_to_group(char *group_name, char *s, client_t *cli){
    int is_sent = 0;
    pthread_mutex_lock(&groups_mutex);

    for(int j = 0; j < MAX_GROUPS; j++){
        if(groups[j]){
            if(strcmp(groups[j]->name, group_name) == 0){
                for(int i = 0; i < groups[j]->last_client; i++){
                    if(groups[j]->clients[i]){
                        if(groups[j]->clients[i]->uid != cli->uid){
                            char *message = malloc(sizeof(char)* 1000);
                            sprintf(message, "%s> %s", cli->name, s);
                            if(write(groups[j]->clients[i]->sockfd, message, strlen(message)) < 0){
                                perror("ERROR: write to descriptor failed");
                                break;
                            }
                            is_sent = 1;
                        }
                    }
                }
        
            }

        }
    }

    if(is_sent == 0){
        printf("%s doesn't exist\n", group_name);
    }
    pthread_mutex_unlock(&groups_mutex);
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

    char option[100];

    //getting option
    int receveOption = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
    if(receveOption > 0){
        if(strlen(buffer) > 0){
            //gets recever             
            strcpy(option, buffer);
        }
    }else if (receveOption == 0 || strcmp(buffer, "exit") == 0)
    {
        close(cli->sockfd);
        queue_remove(cli->uid);
        free(cli);
        cli_count--;
        pthread_detach(pthread_self());
        return NULL;
    }else
    {
        perror("server");
        close(cli->sockfd);
        queue_remove(cli->uid);
        free(cli);
        cli_count--;
        pthread_detach(pthread_self());

        return NULL;
    }
    bzero(buffer, BUFFER_SZ);

    if(strcmp(option, "create") == 0){
        char group_name[100];
        int receveName = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
        if(receveName > 0){
            if(strlen(buffer) > 0){
                //gets recever             
                strcpy(group_name, buffer);
            }
        }else if (receveName == 0)
        {
            close(cli->sockfd);
            queue_remove(cli->uid);
            free(cli);
            cli_count--;
            pthread_detach(pthread_self());
            return NULL;
        }else
        {
            perror("server");
            close(cli->sockfd);
            queue_remove(cli->uid);
            free(cli);
            cli_count--;
            pthread_detach(pthread_self());
            return NULL;
        }
        bzero(buffer, BUFFER_SZ);
        add_group(group_name);
        add_user_to_group(cli, group_name);

        //send messages
        while(1){
            if(leave_flag){
                break;
            }
            char message[100];

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
            send_message_to_group(group_name, message, cli);

        }
        remove_user_from_group(cli->uid, group_name);


    }else if (strcmp(option, "join") == 0)
    {
        //get group name
        char group_name[100];
        int receveName = recv(cli->sockfd, buffer, BUFFER_SZ, 0);
        if(receveName > 0){
            if(strlen(buffer) > 0){
                //gets group             
                strcpy(group_name, buffer);
            }
        }else if (receveName == 0)
        {
            close(cli->sockfd);
            queue_remove(cli->uid);
            free(cli);
            cli_count--;
            pthread_detach(pthread_self());
            return NULL;
        }else
        {
            perror("server");
            close(cli->sockfd);
            queue_remove(cli->uid);
            free(cli);
            cli_count--;
            pthread_detach(pthread_self());
            return NULL;
        }
        bzero(buffer, BUFFER_SZ);
        add_user_to_group(cli, group_name);

        //send messages
        while(1){
            if(leave_flag){
                break;
            }
            char message[100];

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
            send_message_to_group(group_name, message, cli);

        }
        remove_user_from_group(cli->uid, group_name);

    }else if (strcmp(option, "message") == 0)
    {
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