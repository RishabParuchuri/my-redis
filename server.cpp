#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const size_t K_MAX_MSG = 4096;

static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg)
{
    int err = errno;
    fprintf(stderr, "[%d] %s\n", err, msg);
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

static int32_t request(int connfd)
{
    char rbuf[4 + K_MAX_MSG];
    errno = 0;
    int32_t err = read_full(connfd, rbuf, 4);
    if (err)
    {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    int32_t len = 0;
    memcpy(&len, rbuf, 4);
    if (len > (int32_t)K_MAX_MSG)
    {
        msg("Message too long!");
        return -1;
    }

    // get the body of the request
    err = read_full(connfd, &rbuf[4], len);
    if (err)
    {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }

    printf("client says: %.*s\n", len, &rbuf[4]);

    const char reply[] = "hi back to you!!!";
    char wbuf[4 + sizeof(reply)];
    len = (int32_t)strlen(reply);
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], reply, len);
    return write_full(connfd, wbuf, len + 4);
}

int main()
{
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        die("server socket()");
    }

    int val = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(1234);
    addr.sin6_addr = in6addr_any;

    // binding reserves an ip address and port on computer for the socket
    int err = bind(sockfd, (const struct sockaddr *)&addr, sizeof(addr));
    if (err)
    {
        die("bind()");
    }

    // listening tells the kernal that the socket can accept and queue incoming connections.
    // SOMAXCONN is 4096 connections in queue
    err = listen(sockfd, SOMAXCONN);
    if (err)
    {
        die("listen()");
    }

    while (true)
    {
        // accept connections
        struct sockaddr_in6 client_addr = {};
        socklen_t addrlen = sizeof(client_addr);
        int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        if (connfd < 0)
        {
            continue;
        }

        // implement business logic
        while (true)
        {
            int32_t err = request(connfd);
            if (err)
            {
                break;
            }
        }
        close(connfd);
    }
}