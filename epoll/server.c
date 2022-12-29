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

pthread_t server;

void * thread_server(void * args)
{
    puts("server started\n");

    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_socket < 0)
    {
        perror("server < 0\n");
        return NULL;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket, (struct sockaddr*) &address, sizeof(address)) < 0)
    {
        perror("bind server < 0\n");
        close(server_socket);
        return NULL;
    }

    if (listen(server_socket, BACKLOG) < 0)
    {
        perror("listen < 0\n");
        close(server_socket);
        return NULL;
    }

    int epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd < 0)
    {
        perror("epoll_create1 < 0\n");
        close(server_socket);
        return NULL;
    }

    struct epoll_event epoll_temp, epoll_return_events[POLLSIZE];

    epoll_temp.events = EPOLLIN;
    epoll_temp.data.fd = server_socket;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_socket, &epoll_temp) < 0)
    {
        perror("epoll_ctl add listen < 0\n");
        close(epoll_fd);
        close(server_socket);
        return NULL;
    }

    while (1)
    {
        int epoll_size = epoll_wait(epoll_fd, epoll_return_events, POLLSIZE, 1000);
        if (epoll_size > 0)
        {
            for (int i = 0; i < epoll_size; ++i)
            {
                if (epoll_return_events[i].data.fd == server_socket)
                {
                    printf("new client got\n");
                    socklen_t size = sizeof(address);
                    int client = accept4(server_socket, (struct sockaddr *) &address, &size, SOCK_NONBLOCK | SOCK_CLOEXEC);
//                    int client = accept(server_socket, (struct sockaddr *) &address, &size);
                    if (client > 0)
                    {
                        printf("new client accepted %d <%s:%d>\n", client, inet_ntoa(address.sin_addr), ntohs(address.sin_port));
                        epoll_temp.events = EPOLLIN | EPOLLHUP | EPOLLRDHUP; // | EPOLLOUT
                        epoll_temp.data.fd = client;
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &epoll_temp) < 0)
                        {
                            perror("epoll_ctl add client < 0\n");
                            close(client);
                            break;
                        }
                    }
                    else
                    {
                        fprintf(stderr, "client fd wrong %d\n", client);
                    }
                }
                else
                {
                    if (epoll_return_events[i].events & EPOLLIN)
                    {
                        char receive_buffer[5];
                        int read = recv(epoll_return_events[i].data.fd, receive_buffer, sizeof(receive_buffer), 0);
                        printf("client %d EPOLLIN event read %d\n", epoll_return_events[i].data.fd, read);
                        if (read == 0)
                        {
                            printf("client %d closing\n", epoll_return_events[i].data.fd);
                            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, epoll_return_events[i].data.fd, NULL) < 0)
                            {
                                perror("epoll_ctl del client < 0\n");
                                close(epoll_return_events[i].data.fd);
                                break;
                            }
                            close(epoll_return_events[i].data.fd);
                        }
                        else if (read > 0)
                        {
                            printf("client %d: %*.*s\n", epoll_return_events[i].data.fd, read, read, receive_buffer);
//                            char send_buffer[] = "kirekhar injast koja miri";
//                            printf("server send %ld\n", send(epoll_return_events[i].data.fd, send_buffer, sizeof(send_buffer), 0));
                        }
                    }

                    if (epoll_return_events[i].events & EPOLLOUT)
                    {
                        printf("client %d EPOLLOUT\n", epoll_return_events[i].data.fd);
                    }

                    if (epoll_return_events[i].events & EPOLLHUP)
                    {
                        printf("client %d EPOLLHUP\n", epoll_return_events[i].data.fd);
                        if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, epoll_return_events[i].data.fd, NULL) < 0)
                        {
                            perror("epoll_ctl del client < 0\n");
                            close(epoll_return_events[i].data.fd);
                            break;
                        }
                        close(epoll_return_events[i].data.fd);
                    }

                    if (epoll_return_events[i].events & EPOLLRDHUP)
                    {
                        printf("client %d EPOLLRDHUP\n", epoll_return_events[i].data.fd);
                    }
                }
            }
        }
        else if (epoll_size == 0)
        {
//            printf("nothing happened\n");
        }
        else
        {
            perror("epoll_wait < 0\n");
            break;
        }
    }

    close(epoll_fd);
    close(server_socket);
    return NULL;
}

int main(int argc, char * argv[])
{
    pthread_create(&server, NULL, thread_server, NULL);
    pthread_join(server, NULL);
    return 0;
}
