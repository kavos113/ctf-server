#ifndef SERVER_P_H
#define SERVER_P_H

#include <stdint.h>

#include "server.h"
#include "http_response.h"

void setup_shutdown(server_t *srv);

void listen_handler(const server_t *srv);
void client_handler(const server_t *srv, connection_t *conn);
void db_handler(const server_t *srv, connection_t *conn);

int add_connection(const server_t *server, connection_t *conn, uint32_t event_mask);
void remove_connection(const server_t *server, connection_t *conn);

// 1: success, 0: partial, -1: error
int connection_send_buffer(connection_t *conn);
void start_send_http_response(const server_t *server, connection_t *conn, http_response_t response);

#endif // SERVER_P_H