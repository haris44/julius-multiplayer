#ifndef PLATFORM_NET_H
#define PLATFORM_NET_H

/**
 * @file
 * Minimal TCP sockets for local network games: one host, a few clients.
 * Sockets are non-blocking once connected; messages are framed by the caller.
 */

#define NET_INVALID_SOCKET -1

/**
 * Starts listening for clients
 * @param port TCP port
 * @return Listening socket, or NET_INVALID_SOCKET
 */
int net_listen(int port);

/**
 * Accepts a waiting client without blocking
 * @return Client socket, or NET_INVALID_SOCKET when nobody is waiting
 */
int net_accept(int listener);

/**
 * Connects to a host, waiting at most timeout_ms
 * @param address IP address or host name
 * @return Connected socket, or NET_INVALID_SOCKET
 */
int net_connect(const char *address, int port, int timeout_ms);

/**
 * Sends all bytes (blocks until they are handed to the system)
 * @return 1 on success, 0 when the connection is lost
 */
int net_send(int socket, const void *data, int length);

/**
 * Receives available bytes without blocking
 * @return Number of bytes received, 0 when nothing is available, -1 when the connection is closed
 */
int net_receive(int socket, void *data, int max_length);

void net_close(int socket);

#endif // PLATFORM_NET_H
