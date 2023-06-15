#define __USE_GNU
#define _GNU_SOURCE

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define PORT                5000
#define BACKLOG               30
#define BUFFERSIZE            (64 * 1024)
#define MAXCLIENTS            10

struct client
{
    int socket_fd;
    struct sockaddr_in addr;
};

struct context
{
    struct client clients[MAXCLIENTS << 1];
    int count;
} ctx;

//int setnonblocking(int sfd)
//{
//    int flags, s;
//    flags = fcntl(sfd, F_GETFL, 0);
//    if (flags == -1) { perror("fcntl"); return -1; }
//    flags |= O_NONBLOCK;
//    s = fcntl(sfd, F_SETFL, flags);
//    if (s == -1) { perror("fcntl"); return -1; }
//    return 0;
//}

void * thread_accept_func(void * args)
{
    int socket = *(int *) args;
    struct sockaddr_in addr;
    socklen_t size = sizeof(addr);
    while (1)
    {
        int client_fd = accept4(socket, &addr, &size, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd > 0)
        {
            if (ctx.count >= MAXCLIENTS)
            {
                fprintf(stderr, "cannot accept new client %d.\n", client_fd);
                shutdown(client_fd, SHUT_RDWR);
                close(client_fd);
            }
            else
            {
                char addr_str[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &addr.sin_addr, addr_str, INET_ADDRSTRLEN);
                printf("Accept from (%d) %s:%u\n", client_fd, addr_str, ntohs(addr.sin_port));
                ctx.clients[ctx.count].addr = addr;
                ctx.clients[ctx.count].socket_fd = client_fd;
                ++ctx.count;
            }
        }
        else
        {
            printf("accept4: %d\n", client_fd);
        }
    }

    return NULL;
}

void close_client(int fd)
{
    printf("close event on %d\n", fd);
    shutdown(fd, SHUT_RDWR);
    close(fd);
    for (int j = 0; j < MAXCLIENTS; ++j)
    {
        if (ctx.clients[j].socket_fd == fd)
        {
            ctx.clients[j].socket_fd = -1;
            char addr_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ctx.clients[j].addr.sin_addr, addr_str, INET_ADDRSTRLEN);
            printf("Disconnect from (%d) %s:%u\n", fd, addr_str, ntohs(ctx.clients[j].addr.sin_port));
//                            memmove(&ctx.clients[j], &ctx.clients[j + 1], sizeof(struct client) * (ctx.count - j));
            --ctx.count;
            break;
        }
    }
}

int main(void)
{
    pthread_t thread_accept;

    printf("%s\n", ttyname(0));
//    signal(SIGCHLD, SIG_IGN);

    int server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (server_socket < 0)
    {
        fprintf(stderr, "failed to create socket: %d %s\n", errno, strerror(errno));
        return -1;
    }

    int yes_1 = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &yes_1, sizeof(yes_1)) < 0)
    {
        fprintf(stderr, "failed set reuse address: %d %s\n", errno, strerror(errno));
        close(server_socket);
        return -1;
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = INADDR_ANY;

    long arg = fcntl(server_socket, F_GETFL, NULL);
    if (arg < 0)
    {
        fprintf(stderr, "Error fcntl F_GETFL (%s)\n", strerror(errno));
        close(server_socket);
        return -1;
    }

    arg |= O_NONBLOCK;
    if (fcntl(server_socket, F_SETFL, arg) < 0)
    {
        fprintf(stderr, "Error fcntl F_SETFL (%s)\n", strerror(errno));
        close(server_socket);
        return -1;
    }

    if (bind(server_socket, (struct sockaddr*) &address, sizeof(address)) < 0)
    {
        fprintf(stderr, "failed to bind: %d %s\n", errno, strerror(errno));
        close(server_socket);
        return -1;
    }

    if (listen(server_socket, BACKLOG) < 0)
    {
        fprintf(stderr, "failed to listen: %d %s\n", errno, strerror(errno));
        close(server_socket);
        return -1;
    }

    printf("Listening (%d)....\n", server_socket);

    memset(&ctx, 0, sizeof(ctx));

//    pthread_create(&thread_accept, NULL, thread_accept_func, &server_socket);

    int loop = 1;
    while (loop)
    {
        struct pollfd poll_fds[(MAXCLIENTS << 1) + 1];
        int count = 1;

        poll_fds[0].fd = server_socket;
        poll_fds[0].events = POLLIN | POLLHUP;
        for (int i = 0; i < MAXCLIENTS; ++i)
        {
            if (ctx.clients[i].socket_fd > 0)
            {
//                printf("add %d on %d\n", ctx.clients[i].socket_fd, count);
                poll_fds[count].fd = ctx.clients[i].socket_fd;
                poll_fds[count].events = POLLIN | POLLRDHUP | POLLHUP | POLLERR | POLLREMOVE;
                ++count;
            }
        }

        printf("poll ... %d %d\n", count,  ctx.count);
        int nfd = poll(poll_fds, count, -1);
        if (nfd < 0)
        {
            fprintf(stderr, "failed to poll: %d %s\n", errno, strerror(errno));
            break;
        }
        else if (nfd == 0)
        {
            continue;
        }

        printf("poll done ... %d\n", nfd);
        for (int i = 0; i < count; ++i)
        {
            int fd = poll_fds[i].fd;
            int revent = poll_fds[i].revents;

            if (revent == 0)
            {
                continue;
            }

            printf("fd: %d, revent: %X -> %s%s%s%s%s%s%s%s%s%s%s%s%s\n", fd, revent,
                   (revent & POLLIN) ? "POLLIN ": "",
                   (revent & POLLPRI) ? "POLLPRI ": "",
                   (revent & POLLOUT) ? "POLLOUT ": "",
                   (revent & POLLRDNORM) ? "POLLRDNORM ": "",
                   (revent & POLLRDBAND) ? "POLLRDBAND ": "",
                   (revent & POLLWRNORM) ? "POLLWRNORM ": "",
                   (revent & POLLWRBAND) ? "POLLWRBAND ": "",
                   (revent & POLLMSG) ? "POLLMSG ": "",
                   (revent & POLLREMOVE) ? "POLLREMOVE ": "",
                   (revent & POLLRDHUP) ? "POLLRDHUP ": "",
                   (revent & POLLERR) ? "POLLERR ": "",
                   (revent & POLLHUP) ? "POLLHUP ": "",
                   (revent & POLLNVAL) ? "POLLNVAL ": "");

            if (fd == server_socket) // server sockets
            {
                if ((revent & POLLRDHUP) || (revent & POLLHUP))
                {
                    fprintf(stderr, "listener socket hanged up!!\n");
                    loop = 0;
                    break;
                }

                if (revent & POLLIN)
                {
                    struct sockaddr_in addr;
                    socklen_t size = sizeof(addr);
                    while (1)
                    {
                        int client_fd = accept4(server_socket, &addr, &size, SOCK_NONBLOCK | SOCK_CLOEXEC);
                        if (client_fd > 0)
                        {
                            if (ctx.count >= MAXCLIENTS)
                            {
                                fprintf(stderr, "cannot accept new client.\n");
                                shutdown(client_fd, SHUT_RDWR);
                                close(client_fd);
                            }
                            else
                            {
                                char addr_str[INET_ADDRSTRLEN];
                                inet_ntop(AF_INET, &addr.sin_addr, addr_str, INET_ADDRSTRLEN);
                                printf("Accept from (%d) %s:%u\n", client_fd, addr_str, ntohs(addr.sin_port));
                                ctx.clients[ctx.count].addr = addr;
                                ctx.clients[ctx.count].socket_fd = client_fd;
                                ++ctx.count;
                            }
                        }
                        else
                        {
                            printf("accept4: %d\n", client_fd);
                            break;
                        }
                    }
                }

                printf("un handled revent on listener: %X\n", revent);
            }
            else // client sockets
            {
                if ((revent & POLLRDHUP) || (revent & POLLHUP))
                {
                    close_client(fd);
                    continue;
                }

                if (revent & POLLIN)
                {
                    char buffer[BUFFERSIZE];
                    ssize_t read_size = recv(fd, buffer, sizeof(buffer), 0);
                    if (read_size > 0)
                    {
                        for (int j = 0; j < ctx.count; ++j)
                        {
                            if (ctx.clients[j].socket_fd != fd)
                            {
                                send(ctx.clients[j].socket_fd, buffer, read_size, MSG_NOSIGNAL);
                            }
                        }
                    }
                    else if (read_size == 0)
                    {
                        close_client(fd);
                    }

                    continue;
                }

                printf("un handled revent on client: %X\n", revent);
            }
        }
    }

//    pthread_cancel(thread_accept);
//    pthread_kill(thread_accept, SIGKILL);

    close(server_socket);
    for (int i = 0; i < ctx.count; ++i)
    {
        close(ctx.clients[i].socket_fd);
    }

    return 0;
}
