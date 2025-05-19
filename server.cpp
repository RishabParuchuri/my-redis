#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#include <iostream>
#include <map>

const size_t K_MAX_MSG = 32 << 20;
static std::map<std::string, std::string> g_data;

enum
{
    RES_OK = 0,
    RES_ERR = 1, // error
    RES_NX = 2,  // key not found
};

struct Conn
{
    int fd = -1;
    // the purpose of the application
    bool wantRead = false;
    bool wantWrite = false;
    bool wantClose = false;
    // buffers for input and output
    std::vector<uint8_t> incoming;
    std::vector<uint8_t> outgoing;
};

struct Response
{
    uint32_t status = 0;
    std::vector<uint8_t> data;
};

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

static void setFdToNonBlocking(int fd)
{
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
}

// append to the back
static void
buf_append(std::vector<uint8_t> &buf, const uint8_t *data, size_t len)
{
    buf.insert(buf.end(), data, data + len);
}

// remove from the front
static void buf_consume(std::vector<uint8_t> &buf, size_t n)
{
    buf.erase(buf.begin(), buf.begin() + n);
}

static Conn *handle_accept(int fd)
{
    // accept connection
    struct sockaddr_in6 client_addr = {};
    socklen_t addrlen = sizeof(client_addr);
    int connfd = accept(fd, (struct sockaddr *)&client_addr, &addrlen);
    if (connfd < 0)
    {
        return NULL;
    }

    setFdToNonBlocking(fd);
    // create a `struct Conn`
    Conn *conn = new Conn();
    conn->fd = connfd;
    conn->wantRead = true; // read the 1st request
    return conn;
}

static bool read_u32(const uint8_t *&cur, const uint8_t *end, uint32_t &out)
{
    if (cur + 4 > end)
    {
        return false;
    }
    memcpy(&out, cur, 4);
    cur += 4;
    return true;
}

static bool
read_str(const uint8_t *&cur, const uint8_t *end, size_t n, std::string &out)
{
    if (cur + n > end)
    {
        return false;
    }
    out.assign(cur, cur + n);
    cur += n;
    return true;
}

static int32_t parse_request(const uint8_t *data, size_t size, std::vector<std::string> &out)
{
    const uint8_t *end = data + size;
    uint32_t numStrings = 0;
    if (!read_u32(data, end, numStrings))
    {
        return -1;
    }
    if (numStrings > K_MAX_MSG)
    {
        return -1;
    }

    while (out.size() < numStrings)
    {
        uint32_t len = 0;
        if (!read_u32(data, end, len))
        {
            return -1;
        }
        out.push_back(std::string());
        if (!read_str(data, end, len, out.back()))
        {
            return -1;
        }
    }
    if (data != end)
    {
        return -1;
    }
    return 0;
}

static void make_response(uint32_t status, std::string_view data, std::vector<uint8_t> &out)
{
    uint32_t resp_len = 4 + (uint32_t)data.size();
    buf_append(out, (const uint8_t *)&resp_len, 4);
    buf_append(out, (const uint8_t *)&status, 4);
    buf_append(out, (const uint8_t *)data.data(), data.size());
}

static void make_response(const Response &resp, std::vector<uint8_t> &out)
{
    uint32_t resp_len = 4 + (uint32_t)resp.data.size();
    buf_append(out, (const uint8_t *)&resp_len, 4);
    buf_append(out, (const uint8_t *)&resp.status, 4);
    buf_append(out, resp.data.data(), resp.data.size());
}

static bool do_request(std::vector<std::string> &cmd, std::vector<uint8_t> &out)
{
    if (cmd.size() == 2 && cmd[0] == "get")
    {
        auto it = g_data.find(cmd[1]);
        if (it == g_data.end())
        {
            make_response(RES_NX, "", out);
        }
        else
        {
            printf("get called: %s\n", it->second.c_str());
            make_response(RES_OK, it->second, out); // direct write
        }
        return true;
    }

    Response resp;

    if (cmd.size() == 3 && cmd[0] == "set")
    {
        g_data[cmd[1]].swap(cmd[2]);
        printf("set called\n");
        resp.status = RES_OK;
    }
    else if (cmd.size() == 2 && cmd[0] == "del")
    {
        g_data.erase(cmd[1]);
        printf("del called\n");
        resp.status = RES_OK;
    }
    else
    {
        resp.status = RES_ERR;
    }

    make_response(resp, out);
    return true;
}

static bool try_one_request(Conn *conn)
{
    // try to parse buffer
    if (conn->incoming.size() < 4)
    {
        return false;
    }
    uint32_t len = 0;
    memcpy(&len, conn->incoming.data(), 4);
    if (4 + len > conn->incoming.size())
    {
        return false;
    }
    const uint8_t *request = &conn->incoming[4];

    std::vector<std::string> cmd;
    if (parse_request(request, len, cmd) < 0)
    {
        conn->wantClose = true;
        return false; // error
    }
    Response res;
    do_request(cmd, conn->outgoing);
    make_response(res, conn->outgoing);

    // remove the message from incoming
    buf_consume(conn->incoming, 4 + len);
    return true; // success
}

static void handle_read(Conn *conn)
{
    uint8_t buffer[64 * 1024];
    ssize_t bytesRead = read(conn->fd, buffer, sizeof(buffer));
    if (bytesRead < 0)
    {
        conn->wantClose = true;
        return;
    }
    // add data to incoming buffer
    buf_append(conn->incoming, buffer, (size_t)bytesRead);
    // parse the buffer, process message, and remove from incoming buffer
    while (try_one_request(conn))
    {
    };
    if (conn->outgoing.size() > 0)
    {
        // response
        conn->wantRead = false;
        conn->wantWrite = true;
    }
}

static void handle_write(Conn *conn)
{
    assert(conn->outgoing.size() > 0);
    ssize_t bytesWrote = write(conn->fd, conn->outgoing.data(), conn->outgoing.size());
    if (bytesWrote < 0 && errno == EAGAIN)
    {
        return;
    }
    if (bytesWrote < 0)
    {
        conn->wantClose = true;
        return;
    }
    buf_consume(conn->outgoing, (size_t)bytesWrote);
    if (conn->outgoing.size() == 0)
    { // all data written
        conn->wantRead = true;
        conn->wantWrite = false;
    }
}

// static int32_t read_full(int fd, char *buffer, size_t n)
// {
//     // Basically continuously decrement until whole stream is read
//     while (n > 0)
//     {
//         ssize_t bytesRead = read(fd, buffer, n);
//         if (bytesRead <= 0)
//         {
//             return -1;
//         }
//         assert((size_t)bytesRead <= n);
//         n -= (size_t)bytesRead;
//         buffer += (size_t)bytesRead;
//     }
//     return 0;
// }

// static int32_t write_full(int fd, char *buffer, size_t n)
// {
//     while (n > 0)
//     {
//         ssize_t bytesRead = write(fd, buffer, n);
//         if (bytesRead <= 0)
//         {
//             return -1;
//         }
//         assert((size_t)bytesRead <= n);
//         n -= (size_t)bytesRead;
//         buffer += (size_t)bytesRead;
//     }
//     return 0;
// }

// static int32_t request(int connfd)
// {
//     char readBuffer[4 + K_MAX_MSG];
//     errno = 0;
//     int32_t err = read_full(connfd, readBuffer, 4);
//     if (err)
//     {
//         msg(errno == 0 ? "EOF" : "read() error");
//         return err;
//     }

//     int32_t len = 0;
//     memcpy(&len, readBuffer, 4);
//     if (len > (int32_t)K_MAX_MSG)
//     {
//         msg("Message too long!");
//         return -1;
//     }

//     // get the body of the request
//     err = read_full(connfd, &readBuffer[4], len);
//     if (err)
//     {
//         msg(errno == 0 ? "EOF" : "read() error");
//         return err;
//     }

//     printf("client says: %.*s\n", len, &readBuffer[4]);

//     const char reply[] = "hi back to you!!!";
//     char writeBuffer[4 + sizeof(reply)];
//     len = (int32_t)strlen(reply);
//     memcpy(writeBuffer, &len, 4);
//     memcpy(&writeBuffer[4], reply, len);
//     return write_full(connfd, writeBuffer, len + 4);
// }

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

    // make a map of all the client connections
    std::vector<Conn *> fdToConnection;

    std::vector<struct pollfd> pollArgs;

    while (true)
    {
        // empty the poll args vector
        pollArgs.clear();

        // insert the listening socket into the the vector
        struct pollfd listening = {sockfd, POLLIN, 0};
        pollArgs.push_back(listening);

        // get all the connection sockets
        for (Conn *conn : fdToConnection)
        {
            if (!conn)
            {
                continue;
            }
            struct pollfd conn_pollfd = {conn->fd, POLLERR, 0};
            // set the proper poll flags
            if (conn->wantRead)
            {
                conn_pollfd.events |= POLLIN;
            }

            if (conn->wantWrite)
            {
                conn_pollfd.events |= POLLOUT;
            }
            pollArgs.push_back(conn_pollfd);
        }
        // wait for readiness from sockets
        int readyCount = poll(pollArgs.data(), (nfds_t)pollArgs.size(), -1);
        if (readyCount < 0 && errno == EINTR)
        {
            continue;
        }

        if (readyCount < 0)
        {
            die("poll() error");
        }
        // listening socket
        if (pollArgs[0].revents)
        {
            if (Conn *conn = handle_accept(sockfd))
            {
                if (fdToConnection.size() <= (size_t)conn->fd)
                {
                    fdToConnection.resize(conn->fd + 1);
                }
                fdToConnection[conn->fd] = conn;
            }
        }
        // goes through all the client connections
        for (size_t i = 1; i < pollArgs.size(); ++i)
        {
            uint32_t ready = pollArgs[i].revents;
            Conn *conn = fdToConnection[pollArgs[i].fd];
            if (ready & POLLIN)
            {
                handle_read(conn);
            }

            if (ready & POLLOUT)
            {
                handle_write(conn);
            }

            if ((ready & POLLERR) || conn->wantClose)
            {
                (void)close(conn->fd);
                fdToConnection[conn->fd] = NULL;
                delete conn;
            }
        }

        // // accept connections
        // struct sockaddr_in6 client_addr = {};
        // socklen_t addrlen = sizeof(client_addr);
        // int connfd = accept(sockfd, (struct sockaddr *)&client_addr, &addrlen);
        // if (connfd < 0)
        // {
        //     continue;
        // }

        // // implement business logic
        // while (true)
        // {
        //     int32_t err = request(connfd);
        //     if (err)
        //     {
        //         break;
        //     }
        // }
        // close(connfd);
    }
}