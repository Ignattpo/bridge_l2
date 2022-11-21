#ifndef REMOTE_H
#define REMOTE_H

#include "interface.h"

#include <inttypes.h>
#include <pcap.h>
#include <pthread.h>
#include <stdbool.h>

struct remote_t {
  struct sockaddr_in sock_addr;
  socklen_t addr_len;
  char* name_addr;
  int port;
  int socket;
  bool terminated;
  pthread_t wait_thread;
  pthread_t read_thread;
  pthread_t write_thread;
  struct interface_bridge_t inter;
};

struct remote_t* client_init(const char* inter_name,
                             const char* serv_addr,
                             int serv_port);
int client_run(struct remote_t* client);
void client_stop(struct remote_t* client);
void client_free(struct remote_t* client);

struct remote_t* server_init(const char* inter_name,
                             const char* name_addr,
                             int port);
int server_run(struct remote_t* server);
void server_stop(struct remote_t* server);
void server_free(struct remote_t* server);

#endif  // REMOTE_H
