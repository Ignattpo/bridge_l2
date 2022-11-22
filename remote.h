#ifndef REMOTE_H
#define REMOTE_H

#include "interface.h"

#include <inttypes.h>
#include <pcap.h>
#include <pthread.h>
#include <stdbool.h>

struct base_t {
  struct sockaddr_in sock_addr;
  socklen_t addr_len;
  char* name_addr;
  int port;
  int socket;
  bool terminated;
  pthread_t read_thread;
  pthread_t write_thread;
};

struct server_t {
  struct base_t base;
  pthread_t wait_thread;
  int fd;
};

struct client_t {
  struct base_t base;
  struct interface_bridge_t inter;
};

struct client_t* client_init(const char* inter_name,
                             const char* serv_addr,
                             int serv_port);
int client_run(struct client_t* client);
void client_stop(struct client_t* client);
void client_free(struct client_t* client);

struct server_t* server_init(const char* inter_name,
                             const char* name_addr,
                             int port);
int server_run(struct server_t* server);
void server_stop(struct server_t* server);
void server_free(struct server_t* server);

#endif  // REMOTE_H
