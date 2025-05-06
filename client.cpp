#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const size_t K_MAX_MSG = 4096;

static void die(const char *msg)
{
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

static int32_t read_full(int fd, char *buf, size_t n)
{
    // Basically continuously decrement until whole stream is read
    while (n > 0)
    {
        ssize_t rv = read(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += (size_t)rv;
    }
    return 0;
}

static int32_t write_full(int fd, char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1;
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += (size_t)rv;
    }
    return 0;
}

static int32_t query(int fd, char *text)
{
    uint32_t len = (u_int32_t)strlen(text);
    if (len > maxk)
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