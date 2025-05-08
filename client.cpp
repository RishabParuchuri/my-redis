#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

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

static int32_t query(int fd, const char *text)
{
    uint32_t len = (u_int32_t)strlen(text);
    if (len > K_MAX_MSG)
    {
        return -1;
    }

    // send request
    char wbuf[4 + K_MAX_MSG];
    memcpy(wbuf, &len, 4);
    memcpy(&wbuf[4], text, len);
    int32_t err = write_full(fd, wbuf, len + 4);
    if (err)
    {
        return err;
    }

    char rbuf[4 + K_MAX_MSG + 1];
    errno = 0;
    err = read_full(fd, rbuf, 4);
    if (err)
    {
        msg(errno == 0 ? "EOF" : "read() error");
        return err;
    }
    memcpy(&len, rbuf, 4);
    if (len > K_MAX_MSG)
    {
        msg("message too long");
        return -1;
    }

    err = read_full(fd, &rbuf[4], len);
    if (err)
    {
        msg("read() error");
        return err;
    }

    printf("server says: %.*s\n", len, &rbuf[4]);
    return 0;
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
    int32_t err = connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr));
    if (err)
    {
        die("connect()");
    }

    // messages

    err = query(sockfd, "Hello Server");
    if (err)
    {
        goto L_DONE;
    }
    err = query(sockfd, "Hello again Server");
    if (err)
    {
        goto L_DONE;
    }
    err = query(sockfd, "Ok bye Server");
    if (err)
    {
        goto L_DONE;
    }

L_DONE:
    close(sockfd);
    return 0;
}