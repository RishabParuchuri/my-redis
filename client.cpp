#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>

const size_t K_MAX_MSG = 32 << 20;
static void msg(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
}

static void die(const char *msg)
{
    fprintf(stderr, "[%d] %s\n", errno, msg);
    abort();
}

static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

static int32_t read_full(int fd, uint8_t *buf, size_t n)
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

static int32_t write_all(int fd, const uint8_t *buf, size_t n)
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

static int32_t send_req(int fd, const uint8_t *text, size_t len)
{
    if (len > K_MAX_MSG)
    {
        return -1;
    }
    std::vector<uint8_t> wbuf;
    buf_append(wbuf, (const uint8_t *)&len, 4);
    buf_append(wbuf, text, len);
    return write_all(fd, wbuf.data(), wbuf.size());
}

static int32_t read_res(int fd)
{
    // 4 bytes header
    std::vector<uint8_t> rbuf;
    rbuf.resize(4);
    errno = 0;
    int32_t err = read_full(fd, &rbuf[0], 4);
    if (err)
    {
        if (errno == 0)
        {
            msg("EOF");
        }
        else
        {
            msg("read() error");
        }
        return err;
    }

    uint32_t len = 0;
    memcpy(&len, rbuf.data(), 4);
    if (len > K_MAX_MSG)
    {
        msg("too long");
        return -1;
    }

    // reply body
    rbuf.resize(4 + len);
    err = read_full(fd, &rbuf[4], len);
    if (err)
    {
        msg("read() error");
        return err;
    }

    printf("len:%u data:%.*s\n", len, len < 100 ? len : 100, &rbuf[4]);
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

    // multiple pipelined requests
    std::vector<std::string> query_list = {
        "hello1",
        "hello2",
        "hello3",
        std::string(K_MAX_MSG, 'z'),
        "hello5",
    };
    for (const std::string &s : query_list)
    {
        int32_t err = send_req(sockfd, (uint8_t *)s.data(), s.size());
        if (err)
        {
            goto L_DONE;
        }
    }
    for (size_t i = 0; i < query_list.size(); ++i)
    {
        int32_t err = read_res(sockfd);
        if (err)
        {
            goto L_DONE;
        }
    }

L_DONE:
    close(sockfd);
    return 0;
}