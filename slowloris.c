//
// Created by hooman on 6/13/23.
//

#define __USE_GNU
#define _GNU_SOURCE

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <getopt.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/poll.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <fcntl.h>

void print_help()
{
    printf("options\n"
           "\t -i, --ip                 *server ip address\n"
           "\t -p, --port               *server port\n"
           "\t -c, --connections        *number of total connections\n"
           "\t -s, --sleep              sleep in each loop in us\n"
           "\t -b, --bytes              number of bytes send in each loop\n"
           "\t -d, --data               payload in hex-string format\n"
           "\t -t, --timeout            poll timeout in ms\n"
           );
}

static struct option long_options[] =
{
        {"ip", required_argument, 0, 'i'},
        {"port", required_argument, 0, 'p'},
        {"sleep", required_argument, 0, 's'},
        {"connections", required_argument, 0, 'c'},
        {"bytes", required_argument, 0, 'b'},
        {"data", required_argument, 0, 'd'},
        {"timeout", required_argument, 0, 't'},
        {0, 0, 0, 0},
};

struct connection
{
    int socket_fd;
    int connected;
    int buffer_index;
};

static struct
{
    const char * ip;
    const char * port;
    unsigned int sleep;
    int bytes;
    const char * payload;
    char * buffer;
    int buffer_len;
    struct connection * connections;
    int connection_count;
    struct pollfd * poll;
    int poll_count;
    int poll_timeout;
    int * poll_connection_map;
    struct sockaddr_in serv_addr;
} state;

//void * thread_connection(void * arg)
//{
//    return NULL;
//}

void clear_state()
{
    if (state.connections)
    {
        for (int i = 0; i < state.connection_count; ++i)
        {
            if (state.connections[i].socket_fd)
            {
                close(state.connections[i].socket_fd);
            }
        }
    }

    free(state.buffer);
    free(state.poll);
    free(state.connections);
}

void sigHandle(int signal)
{
    static const int STACK_TRACE = 30;
    void * array[STACK_TRACE];

    fprintf(stderr, "signal: %d\n", signal);
    int size = backtrace(array, STACK_TRACE);
    char ** strings = (char **) backtrace_symbols(array, size);

    for (int i = 0; i < size; ++i)
    {
        fprintf(stderr, "%02i %s\n", i, strings[i]);
    }

    free(strings);
    clear_state();
    exit(-1);
}

char ascii2bcd(char i)
{
    if (i >= '0' && i <= '9')
    {
        return i - '0';
    }
    else if (i >= 'A' && i <= 'F')
    {
        return i - 'A' + 10;
    }
    else if (i >= 'a' && i <= 'f')
    {
        return i - 'a' + 10;
    }
    else
    {
        return 0;
    }
}

void unhexify(char * dst, const char * src, int len)
{
    for (int i_src = 0, i_dst = 0; i_src < len; i_src += 2, ++i_dst)
    {
        dst[i_dst] = (char) (((ascii2bcd(src[i_src]) << 4) | ascii2bcd(src[i_src + 1])));
    }
}


int resolve_ip(const char * domain, const char * port, int family, int sockettype, int protocol, struct sockaddr_in * out)
{
    struct addrinfo * addr;
    struct addrinfo hint = {0};

    hint.ai_family = family;
    hint.ai_socktype = sockettype;
    hint.ai_protocol = protocol;

    if (getaddrinfo(domain, port, &hint, &addr) == 0)
    {
        struct addrinfo * ptr = addr;
        while (ptr != NULL)
        {
            if (out != NULL)
            {
                memcpy(out, ptr->ai_addr, sizeof(struct sockaddr_in));
                break;
            }

            struct sockaddr_in * adin = ptr->ai_addr;
            printf("ai_flags: %d, ai_family: %d, ai_socktype: %d, ai_protocol: %d, ai_addrlen: %d, "
                   "sin_family: %d, sin_port: %d, sin_addr: %u, inet_ntoa: %s\n",
                   ptr->ai_flags, ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol, ptr->ai_addrlen,
                   adin->sin_family, ntohs(adin->sin_port), adin->sin_addr.s_addr, inet_ntoa(adin->sin_addr));
            ptr = ptr->ai_next;
        }
    }
    else
    {
        return -1;
    }

    freeaddrinfo(addr);
    return 0;
}

int make_async(int socket_fd)
{
    long arg = fcntl(socket_fd, F_GETFL, NULL);
    if (arg < 0)
    {
        fprintf(stderr, "Error fcntl F_GETFL (%s)\n", strerror(errno));
        return -1;
    }

    arg |= O_NONBLOCK;
    if (fcntl(socket_fd, F_SETFL, arg) < 0)
    {
        fprintf(stderr, "Error fcntl F_SETFL (%s)\n", strerror(errno));
        return -1;
    }

    return 0;
}

int main(int argc, const char * argv[])
{
    signal(SIGILL, sigHandle);
    signal(SIGABRT, sigHandle);
    signal(SIGFPE, sigHandle);
    signal(SIGSEGV, sigHandle);
    signal(SIGBUS, sigHandle);
    signal(SIGSTKFLT, sigHandle);

    char buffer[1024];
    int param_count = 0;
    int opt;

    state.bytes = 10;
    state.sleep = 10000;
    state.poll_timeout = 1000;

    while ((opt = getopt_long(argc, argv, "i:p:s:c:b:d:t:", long_options, NULL)) != -1)
    {
        switch (opt)
        {
            case 'i':
                state.ip = optarg;
                ++param_count;
                break;
            case 'p':
                state.port = optarg;
                ++param_count;
                break;
            case 's':
                state.sleep = (unsigned int) atoi(optarg);
                break;
            case 'c':
                state.connection_count = atoi(optarg);
                ++param_count;
                break;
            case 'b':
                state.bytes = atoi(optarg);
                break;
            case 'd':
                state.payload = optarg;
                break;
            case 't':
                state.poll_timeout = atoi(optarg);
                break;
            default:
                print_help();
                return -1;
        }
    }

    if (param_count != 3)
    {
        fprintf(stderr, "required parameters not found.\n");
        free(state.buffer);
        return -1;
    }

    state.poll_connection_map = calloc(state.connection_count, sizeof(int));
    if (state.poll_connection_map == NULL)
    {
        perror("cannot allocate poll_connection_map.\n");
        clear_state();
        return -1;
    }

    state.connections = calloc(state.connection_count, sizeof(struct connection));
    if (state.connections == NULL)
    {
        perror("cannot allocate connections.\n");
        clear_state();
        return -1;
    }

    state.poll = calloc(state.connection_count, sizeof(struct pollfd));
    if (state.poll == NULL)
    {
        perror("cannot allocate poll.\n");
        clear_state();
        return -1;
    }

    if (state.payload)
    {
        state.buffer_len = strlen(state.payload) >> 1;
        state.buffer = calloc(state.buffer_len, 1);
        if (state.buffer == NULL)
        {
            perror("cannot allocate buffer.\n");
            clear_state();
            return -1;
        }
        unhexify(state.buffer, state.payload, state.buffer_len << 1);
    }

    if (resolve_ip(state.ip, state.port, AF_INET, SOCK_STREAM, IPPROTO_TCP, &state.serv_addr) != 0)
    {
        perror("ERROR, no such host");
        clear_state();
        return -1;
    }

    while (1)
    {
        state.poll_count = 0;
        for (int i = 0; i < state.connection_count; ++i)
        {
            if (state.connections[i].connected == 0)
            {
                if (state.connections[i].socket_fd <= 0)
                {
                    int soc = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (soc < 0)
                    {
                        perror("ERROR opening socket\n");
                        clear_state();
                        return -1;
                    }
                    else
                    {
                        state.connections[i].socket_fd = soc;
                        if (make_async(state.connections[i].socket_fd) < 0)
                        {
                            clear_state();
                            return -1;
                        }
                    }
                }

                int res = connect(state.connections[i].socket_fd, (struct sockaddr *) &state.serv_addr, sizeof(state.serv_addr));
                if (res < 0)
                {
                    if (errno != EINPROGRESS)
                    {
                        fprintf(stderr, "cannot connect socket(%d) %d (%s)\n", state.connections[i].socket_fd,  errno, strerror(errno));
                        clear_state();
                        return -1;
                    }
                }

                state.connections[i].connected = 1;
                printf("socket %d connected\n", state.connections[i].socket_fd);
            }

            if (state.connections[i].socket_fd && state.connections[i].connected != 0)
            {
                state.poll[i].fd = state.connections[i].socket_fd;
                state.poll[i].events = POLLIN | POLLHUP | POLLRDHUP | (state.connections[i].connected == 2 ? 0 : POLLOUT);
                state.poll[i].revents = 0;
                state.poll_connection_map[state.poll_count++] = i;

                if (state.connections[i].connected == 2 && state.buffer_len != 0)
                {
                    int remain = state.buffer_len - state.connections[i].buffer_index;
                    if (remain > state.bytes)
                    {
                        send(state.connections[i].socket_fd, state.buffer + state.connections[i].buffer_index, state.bytes, MSG_NOSIGNAL);
                        state.connections[i].buffer_index += state.bytes;
                    }
                    else
                    {
                        send(state.connections[i].socket_fd, state.buffer + state.connections[i].buffer_index, remain, MSG_NOSIGNAL);
                        state.connections[i].buffer_index = 0;
                    }
                }
            }
        }

        int count = poll(state.poll, state.poll_count, state.poll_timeout);
        if (count < 0)
        {
            fprintf(stderr, "failed to poll: %d %s\n", errno, strerror(errno));
            break;
        }
        else if (count == 0)
        {
            continue;
        }

        for (int i = 0; i < state.poll_count; ++i)
        {
            int revent = state.poll[i].revents;
            printf("fd: %d, revent: %X -> %s%s%s%s%s%s%s%s%s%s%s%s%s\n",
                   state.poll[i].fd, revent,
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

            assert(state.connections[state.poll_connection_map[i]].socket_fd == state.poll[i].fd);
            if ((revent & POLLRDHUP) || (revent & POLLHUP))
            {
                int socket_index = state.poll_connection_map[i];
                printf("socket %d disconnected\n", state.connections[socket_index].socket_fd);
                shutdown(state.connections[socket_index].socket_fd, SHUT_RDWR);

                close(state.connections[socket_index].socket_fd);
                state.connections[socket_index].socket_fd = 0;
                state.connections[socket_index].connected = 0;
                continue;
            }

            if ((revent & (POLLIN | POLLOUT)) == (POLLIN | POLLOUT))
            {
//                    while (1)
//                    {
//                        int r = recv(state.poll[i].fd, buffer, sizeof(buffer), 0);
//                        if (r <= 0)
//                        {
//                            break;
//                        }
//                    }
                while (recv(state.poll[i].fd, buffer, sizeof(buffer), 0) > 0);
            }

            if (revent & POLLOUT)
            {
                int value = 1;
                struct linger linger = {.l_onoff = 0, .l_linger = 0};
                state.connections[state.poll_connection_map[i]].connected = 2;
                setsockopt(state.poll[i].fd, SOL_SOCKET, SO_KEEPALIVE, &value, sizeof(value));
                setsockopt(state.poll[i].fd, SOL_SOCKET, SO_LINGER, &linger, sizeof(linger));
            }
        }

        usleep(state.sleep);
    }

    clear_state();
    return 0;
}
