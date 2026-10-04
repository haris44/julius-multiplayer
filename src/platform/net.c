#include "net.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#define CLOSE_SOCKET closesocket
#define WOULD_BLOCK() (WSAGetLastError() == WSAEWOULDBLOCK)
#define IN_PROGRESS() (WSAGetLastError() == WSAEWOULDBLOCK)
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#define CLOSE_SOCKET close
#define WOULD_BLOCK() (errno == EAGAIN || errno == EWOULDBLOCK)
#define IN_PROGRESS() (errno == EINPROGRESS)
#endif

#include <stdio.h>
#include <string.h>

static int initialized;

static int init(void)
{
    if (initialized) {
        return 1;
    }
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return 0;
    }
#endif
    initialized = 1;
    return 1;
}

static void set_non_blocking(int s)
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) | O_NONBLOCK);
#endif
}

static void set_options(int s)
{
    int one = 1;
    // small messages every few milliseconds: never wait to group them
    setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *) &one, sizeof(one));
#ifdef SO_NOSIGPIPE
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, (const char *) &one, sizeof(one));
#endif
}

int net_listen(int port)
{
    if (!init()) {
        return NET_INVALID_SOCKET;
    }
    int s = (int) socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        return NET_INVALID_SOCKET;
    }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof(one));
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short) port);
    if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) != 0 || listen(s, 4) != 0) {
        CLOSE_SOCKET(s);
        return NET_INVALID_SOCKET;
    }
    set_non_blocking(s);
    return s;
}

int net_accept(int listener)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int s = (int) accept(listener, (struct sockaddr *) &addr, &len);
    if (s < 0) {
        return NET_INVALID_SOCKET;
    }
    set_options(s);
    set_non_blocking(s);
    return s;
}

int net_connect(const char *address, int port, int timeout_ms)
{
    if (!init()) {
        return NET_INVALID_SOCKET;
    }
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_text[16];
    snprintf(port_text, sizeof(port_text), "%d", port);
    if (getaddrinfo(address, port_text, &hints, &result) != 0) {
        return NET_INVALID_SOCKET;
    }
    int s = (int) socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        freeaddrinfo(result);
        return NET_INVALID_SOCKET;
    }
    set_non_blocking(s);
    int status = connect(s, result->ai_addr, (int) result->ai_addrlen);
    freeaddrinfo(result);
    if (status != 0) {
        if (!IN_PROGRESS()) {
            CLOSE_SOCKET(s);
            return NET_INVALID_SOCKET;
        }
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(s, &writable);
        struct timeval timeout = { timeout_ms / 1000, (timeout_ms % 1000) * 1000 };
        int error = 0;
        socklen_t len = sizeof(error);
        if (select(s + 1, 0, &writable, 0, &timeout) <= 0 ||
            getsockopt(s, SOL_SOCKET, SO_ERROR, (char *) &error, &len) != 0 || error != 0) {
            CLOSE_SOCKET(s);
            return NET_INVALID_SOCKET;
        }
    }
    set_options(s);
    return s;
}

int net_send(int socket, const void *data, int length)
{
    const char *bytes = data;
    while (length > 0) {
#ifdef MSG_NOSIGNAL
        int sent = (int) send(socket, bytes, length, MSG_NOSIGNAL);
#else
        int sent = (int) send(socket, bytes, length, 0);
#endif
        if (sent < 0) {
            if (WOULD_BLOCK()) {
                fd_set writable;
                FD_ZERO(&writable);
                FD_SET(socket, &writable);
                struct timeval timeout = { 1, 0 };
                if (select(socket + 1, 0, &writable, 0, &timeout) < 0) {
                    return 0;
                }
                continue;
            }
            return 0;
        }
        bytes += sent;
        length -= sent;
    }
    return 1;
}

int net_receive(int socket, void *data, int max_length)
{
    int received = (int) recv(socket, data, max_length, 0);
    if (received > 0) {
        return received;
    }
    if (received < 0 && WOULD_BLOCK()) {
        return 0;
    }
    return -1;
}

void net_close(int socket)
{
    if (socket >= 0) {
        CLOSE_SOCKET(socket);
    }
}

int net_udp_open(int port)
{
    if (!init()) {
        return NET_INVALID_SOCKET;
    }
    int s = (int) socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return NET_INVALID_SOCKET;
    }
    int one = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, (const char *) &one, sizeof(one));
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *) &one, sizeof(one));
#ifdef SO_REUSEPORT
    setsockopt(s, SOL_SOCKET, SO_REUSEPORT, (const char *) &one, sizeof(one));
#endif
    if (port) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons((unsigned short) port);
        if (bind(s, (struct sockaddr *) &addr, sizeof(addr)) != 0) {
            CLOSE_SOCKET(s);
            return NET_INVALID_SOCKET;
        }
    }
    set_non_blocking(s);
    return s;
}

static void send_datagram(int socket, unsigned long address, int port, const void *data, int length)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(address);
    addr.sin_port = htons((unsigned short) port);
    sendto(socket, data, length, 0, (struct sockaddr *) &addr, sizeof(addr));
}

void net_udp_broadcast(int socket, int port, const void *data, int length)
{
    send_datagram(socket, INADDR_BROADCAST, port, data, length);
    // broadcasts do not always come back to this computer: other windows of the game listen here too
    send_datagram(socket, INADDR_LOOPBACK, port, data, length);
}

int net_udp_receive(int socket, void *data, int max_length, char *from_address)
{
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    int received = (int) recvfrom(socket, data, max_length, 0, (struct sockaddr *) &addr, &len);
    if (received <= 0) {
        return 0;
    }
    snprintf(from_address, 16, "%s", inet_ntoa(addr.sin_addr));
    return received;
}

void net_local_address(char *address)
{
    snprintf(address, 16, "?");
    if (!init()) {
        return;
    }
    // "connecting" a UDP socket sends nothing: it only selects the interface towards the network
    int s = (int) socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        return;
    }
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(0x0a000001); // 10.0.0.1, never contacted
    addr.sin_port = htons(9);
    socklen_t len = sizeof(addr);
    if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) == 0 &&
        getsockname(s, (struct sockaddr *) &addr, &len) == 0 && addr.sin_addr.s_addr != 0) {
        snprintf(address, 16, "%s", inet_ntoa(addr.sin_addr));
    }
    CLOSE_SOCKET(s);
}

void net_sleep(int milliseconds)
{
#ifdef _WIN32
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000);
#endif
}

