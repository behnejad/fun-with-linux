#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/epoll.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>

#define PORT        9000
#define BACKLOG     3
#define POLLSIZE    10

pthread_t client;

void * thread_client(void * args)
{
    puts("client started\n");
    sleep(1);

    int client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("client < 0\n");
        return NULL;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (connect(client_socket, (struct sockaddr*) &address, sizeof(address)) < 0)
    {
        perror("connect client < 0\n");
        close(client_socket);
        return NULL;
    }

    char send_buffer[] = "Hello World!";
    printf("client send %ld\n", send(client_socket, send_buffer, sizeof(send_buffer), 0));

    char receive_buffer[30];
    while (1)
    {
        ssize_t recv_size = recv(client_socket, receive_buffer, sizeof(receive_buffer) / 2, 0);
        if (recv_size == 0)
        {
            printf("server closed socket\n");
            break;
        }
        else if (recv_size > 0)
        {
            printf("server: %*.*s\n", recv_size, recv_size, receive_buffer);
        }
        else
        {
            perror("client read < 0\n");
        }
    }

    close(client_socket);
    return NULL;
}

int main(int argc, char * argv[])
{
    pthread_create(&client, NULL, thread_client, NULL);
    pthread_join(client, NULL);
    return 0;
}
