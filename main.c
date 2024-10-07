#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <termios.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include <dirent.h>
#include <stdbool.h>

#define SERVER_IP   "127.0.0.1"
#define PORT_NO     5120

static int sock;
static struct sockaddr_in addr;


void CreateSocket(bool server)
{
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        printf("Socket couldn't be created.\n");

    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT_NO);

    if (server) {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
            printf("Server socket couldn't be created.\n");
        listen(sock, 3);

        printf("Created server socket.\n");
    } else {
        char server_ip[] = SERVER_IP;
        struct hostent *host;

        if ((addr.sin_addr.s_addr = inet_addr(server_ip)) == INADDR_NONE) {
            if ((host = gethostbyname(server_ip)) == NULL)
                printf("Socket couldn't get host by name.\n");
            memcpy(&addr.sin_addr.s_addr, host->h_addr_list[0], host->h_length);
        }

        printf("Created client socket.\n");
    }
}

void *Server(void *param)
{
    int selected_core = 3;
    cpu_set_t cpu;
    CPU_ZERO(&cpu);
    CPU_SET(selected_core, &cpu);
    if (sched_setaffinity(syscall(SYS_gettid), sizeof(cpu), &cpu) == -1)
        printf("Core didn't select.\n");

    int cli_length = 0, res;
    struct sockaddr_in client = {0};
    const char *message = "Hello Client!";

    while (true) {
        printf("Waiting for client connection.\n");

        cli_length = sizeof(struct sockaddr_in);
        res = accept(sock, (struct sockaddr *)&client, (socklen_t*)&cli_length);
        if (res < 0)
            printf("Server accept failed.\n");
        else
            printf("Client connection accepted.\n");

        if (send(res, message, strlen(message), 0) < 0)
            printf("Server send failed.\n");

        usleep(1000);
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);

    return param;
}

void *Client(void *param)
{
    int selected_core = 3;
    cpu_set_t cpu;
    CPU_ZERO(&cpu);
    CPU_SET(selected_core, &cpu);
    if (sched_setaffinity(syscall(SYS_gettid), sizeof(cpu), &cpu) == -1)
        printf("Core didn't select.\n");

    bool conn_state = false;
    char data[255];
    int recv_count = 0;
    int recv_size = 12;

    while (true) {
        if (!conn_state) {
            struct timeval time;
            time.tv_sec = 1;
            time.tv_usec = 0;
            setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char *)&time, sizeof(time));

            if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
                printf("Disconnected.\n");
            } else {
                conn_state = true;
                printf("Client connected.\n");
            }

        } else {
            if ((recv_count = recv(sock, &data, recv_size, 0)) == -1) {
                if (errno == ECONNRESET)
                    printf("Client Socket Error : ECONNRESET");
                else if (errno == EBADF)
                    printf("Client Socket Error : EBADF");

                shutdown(sock, SHUT_RDWR);
                close(sock);
                conn_state = false;
            }

            if (recv_count > 0) {
                for (uint i = 0; i < recv_size; i++)
                    printf("%c", data[i]);
                printf("\n");
            }
        }

        usleep(1000);
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);

    return param;
}

int main(int argc, char *argv[])
{
    printf("TCP Socket Programming\n");

    if (argc >= 2) {

        if (atoi(argv[1]) == 0) {

            printf("Server\n");
            CreateSocket(true);
            pthread_t serv_thread;
            if (pthread_create(&serv_thread, NULL, *Server, NULL) == -1)
                printf("Server thread couldn't be created.\n");
            pthread_join(serv_thread, NULL);

        } else if (atoi(argv[1]) == 1) {

            printf("Client\n");
            CreateSocket(false);
            pthread_t cli_thread;
            if (pthread_create(&cli_thread, NULL, *Client, NULL) == -1)
                printf("Client thread couldn't be created.\n");
            pthread_join(cli_thread, NULL);

        } else {
            printf("Invalid parameter.\n");
        }
    }

    exit(0);
}
