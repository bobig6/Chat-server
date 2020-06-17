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

volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[NAME_LEN];
char groupName[NAME_LEN];

//function for UI in console
void str_overwrite_stdout(){
    printf("\n%s", "> ");
    fflush(stdout);
}

//removes \n from string
void remove_new_line(char* arr, int lenght){
    for (int i = 0; i < lenght; i++)
    {
        if(arr[i] == '\n'){
            arr[i] = '\0';
            break;
        }
    }
}
//sets a flag to send a message when client is disconected
void exits_the_program(){
    flag = 1;
}
//monitors receiving messages
void receive_thread(){
    char message[BUFFER_SZ] = {};
    while (1)
    {
        int receive = recv(sockfd, message, BUFFER_SZ, 0);

        if(receive > 0){
            printf("\nYou have recieved a message: \n");
            printf("%s ", message);
            str_overwrite_stdout();
        }else if (receive == 0)
        {
            perror("receive");
            break;
        }
        bzero(message, BUFFER_SZ);
    }
    
}

//monitors sending messages
void send_thread(){
    char recever[NAME_LEN];
    char message[BUFFER_SZ];

    while (1)
    {
        //gets username
        printf("Enter user to receive your message:");
        str_overwrite_stdout();
        fgets(recever, NAME_LEN, stdin);
        remove_new_line(recever, NAME_LEN);

        //checks if the user wants to exit
        if(strcmp(recever, "exit") == 0){
            break;
        }else{
            //send username to server
            send(sockfd, recever, strlen(recever), 0);
        }

        //gets message
        printf("Enter message:");
        str_overwrite_stdout();
        fgets(message, BUFFER_SZ, stdin);
        remove_new_line(message, BUFFER_SZ);

        //send message to server
        send(sockfd, message, strlen(message), 0);
        bzero(recever, NAME_LEN);
        bzero(message, BUFFER_SZ);

    }
    exits_the_program(2);
}


//monitors sending messages to group
void send_to_group_thread(){
    char message[BUFFER_SZ];

    while (1)
    {
        //gets message
        printf("Enter message:");
        str_overwrite_stdout();
        fgets(message, BUFFER_SZ, stdin);
        remove_new_line(message, BUFFER_SZ);

        //send message to server
        send(sockfd, message, strlen(message), 0);
        bzero(message, BUFFER_SZ);

    }
    exits_the_program(2);
}



int main(int argc, char **argv){
	

	char *ip = "127.0.0.1";
	int port = 4444;

    signal(SIGINT, exits_the_program);

    

    //gets name and sends it to the server
    printf("Enter your name: ");
    fgets(name, NAME_LEN, stdin);
    remove_new_line(name, strlen(name));
    //checks if name is correct
    if(strlen(name) > NAME_LEN - 1 || strlen(name) < 2){
        printf("Enter name correctly\n");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;

    //Server address setup
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);
    server_addr.sin_port = htons(port);
    //connecting to server
    int err = connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if(err == -1){
        perror("connect");
        return EXIT_FAILURE;
    }
    //sending name to server
    send(sockfd, name, NAME_LEN, 0);

    while (1)
    {
        char option[NAME_LEN];
        printf("Choose an option: \n");
        printf("c - create group \n");
        printf("j - join group \n");
        printf("m - message person \n");
        fgets(option, NAME_LEN, stdin);
        remove_new_line(option, strlen(option));

        if(strcmp(option, "c") == 0){
            //creates server
            char command[NAME_LEN] = "create";
            send(sockfd, command, NAME_LEN, 0);
            printf("Choose group name: \n");
            fgets(groupName, NAME_LEN, stdin);
            remove_new_line(groupName, strlen(groupName));
            if(strlen(groupName) > NAME_LEN - 1 || strlen(groupName) < 2){
                printf("Enter group name correctly\n");
                return EXIT_FAILURE;
            }
            send(sockfd, groupName, NAME_LEN, 0);
            pthread_t send_group_thread;
            if(pthread_create(&send_group_thread, NULL, (void*) send_to_group_thread, NULL) != 0){
                perror("pthread_create");
                return EXIT_FAILURE;
            }
            break;
        }else if (strcmp(option, "j") == 0)
        {
            //joins created server
            char command[NAME_LEN] = "join";
            send(sockfd, command, NAME_LEN, 0);
            printf("Enter group name: \n");
            fgets(groupName, NAME_LEN, stdin);
            remove_new_line(groupName, strlen(groupName));
            if(strlen(groupName) > NAME_LEN - 1 || strlen(groupName) < 2){
                printf("Enter group name correctly\n");
                return EXIT_FAILURE;
            }
            send(sockfd, groupName, NAME_LEN, 0);
            pthread_t send_group_thread;
            if(pthread_create(&send_group_thread, NULL, (void*) send_to_group_thread, NULL) != 0){
                perror("pthread_create");
                return EXIT_FAILURE;
            }
            break;
        }
        else if (strcmp(option, "m") == 0)
        {
            //messages to other people
            char command[NAME_LEN] = "message";
            send(sockfd, command, NAME_LEN, 0);
            //creating the thread to send messages
            pthread_t send_msg_thread;
            if(pthread_create(&send_msg_thread, NULL, (void*) send_thread, NULL) != 0){
                perror("pthread_create");
                return EXIT_FAILURE;
            }
            break;
        }else{
            printf("Didn't get that try again!");
        }
        
    }
    
    //creating the thread to monitor the received messages
    pthread_t recv_msg_thread;
    if(pthread_create(&recv_msg_thread, NULL, (void*) receive_thread, NULL) != 0){
        perror("pthread_create");
        return EXIT_FAILURE;
    }
    

    while(1){
        if(flag){
            printf("\nGoodbye :D\n");
            break;
        }
    }

    close(sockfd);

    return EXIT_SUCCESS;

}