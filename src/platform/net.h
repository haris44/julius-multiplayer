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

/**
 * Opens a non-blocking UDP socket that can send and receive broadcasts: several programs of the same
 * computer can listen on the same port (game discovery on the local network)
 * @param port Port to receive on, or 0 to only send
 * @return Socket, or NET_INVALID_SOCKET
 */
int net_udp_open(int port);

/**
 * Sends a datagram to every computer of the local network, and to this computer
 */
void net_udp_broadcast(int socket, int port, const void *data, int length);

/**
 * Receives a waiting datagram without blocking
 * @param from_address Filled with the IP address of the sender (at least 16 characters)
 * @return Number of bytes received, 0 when nothing is waiting
 */
int net_udp_receive(int socket, void *data, int max_length, char *from_address);

/**
 * IP address of this computer on the local network, to tell the other players
 * @param address Filled with the address (at least 16 characters), "?" if unknown
 */
void net_local_address(char *address);

/**
 * Waits without using the processor
 */
void net_sleep(int milliseconds);

#endif // PLATFORM_NET_H
