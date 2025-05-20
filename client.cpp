#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <vector>
#include <string>
#include <cstdlib>
#include <iostream>
#include <sstream>

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

static int32_t write_all(int fd, const char *buf, size_t n)
{
    while (n > 0)
    {
        ssize_t rv = write(fd, buf, n);
        if (rv <= 0)
        {
            return -1; // error
        }
        assert((size_t)rv <= n);
        n -= (size_t)rv;
        buf += rv;
    }
    return 0;
}

static int32_t send_req(int fd, const std::vector<std::string> &cmd)
{
    uint32_t len = 4;
    for (const std::string &s : cmd)
    {
        len += 4 + s.size();
    }
    if (len > K_MAX_MSG)
    {
        return -1;
    }

    char wbuf[4 + K_MAX_MSG];
    memcpy(&wbuf[0], &len, 4); // assume little endian
    uint32_t n = cmd.size();
    memcpy(&wbuf[4], &n, 4);
    size_t cur = 8;
    for (const std::string &s : cmd)
    {
        uint32_t p = (uint32_t)s.size();
        memcpy(&wbuf[cur], &p, 4);
        memcpy(&wbuf[cur + 4], s.data(), s.size());
        cur += 4 + s.size();
    }
    return write_all(fd, wbuf, 4 + len);
}

static int32_t read_res(int fd)
{
    char rbuf[4 + K_MAX_MSG + 1];
    errno = 0;
    int32_t err = read_full(fd, (uint8_t *)rbuf, 4);
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
    memcpy(&len, rbuf, 4);
    if (len > K_MAX_MSG)
    {
        msg("too long");
        return -1;
    }

    err = read_full(fd, (uint8_t *)&rbuf[4], len);
    if (err)
    {
        msg("read() error");
        return err;
    }

    uint32_t rescode = 0;
    if (len < 4)
    {
        msg("bad response");
        return -1;
    }
    memcpy(&rescode, &rbuf[4], 4);

    // print result
    printf("status: %u\n", rescode);
    printf("len: %u\n", len);
    if (len > 4)
    {
        printf("data: %.*s\n", len - 4, &rbuf[8]);
    }
    else
    {
        printf("data: <empty>\n");
    }

    return 0;
}

int main(int argc, char **argv)
{
    int sockfd = socket(AF_INET6, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        die("server socket()");
    }

    struct sockaddr_in6 addr = {};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(1234);
    if (inet_pton(AF_INET6, "::ffff:server", &addr.sin6_addr) <= 0)
    {
        die("invalid address");
    }

    int32_t err = connect(sockfd, (const struct sockaddr *)&addr, sizeof(addr));
    if (err)
    {
        die("connect()");
    }

    if (argc == 1)
    {
        std::string line;
        while (true)
        {
            std::cout << "> ";
            if (!std::getline(std::cin, line))
                break;

            std::istringstream iss(line);
            std::vector<std::string> cmd;
            std::string token;
            while (iss >> token)
            {
                cmd.push_back(token);
            }

            if (cmd.empty())
                continue;
            if (cmd[0] == "exit" || cmd[0] == "quit")
                break;

            err = send_req(sockfd, cmd);
            if (err)
                break;

            err = read_res(sockfd);
            if (err)
                break;
        }
    }
    else
    {
        std::vector<std::string> cmd;
        for (int i = 1; i < argc; ++i)
        {
            cmd.push_back(argv[i]);
        }
        err = send_req(sockfd, cmd);
        if (!err)
            read_res(sockfd);
    }

    close(sockfd);
    return 0;
}
