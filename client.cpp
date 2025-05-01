#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

static void die(const char *msg)
{
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

int main()
{
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        die("server socket()");
    }

    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = ntohs(1234);
    addr.sin6_addr = in6addr_loopback;
    int err = connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr));
    if (err)
    {
        die("connect()");
    }

    char msg[] = "hello to the server";
    send(sockfd, msg, strlen(msg), 0);

    char rbuf[64] = {};
    ssize_t n = recv(sockfd, rbuf, sizeof(rbuf) - 1, 0);
    if (n < 0)
    {
        die("read");
    }
    printf("server says: %s\n", rbuf);
    close(sockfd);
}