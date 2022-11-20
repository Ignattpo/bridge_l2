#ifndef BRIDGE_SERVER_H
#define BRIDGE_SERVER_H

#include "interface.h"

#include <inttypes.h>
#include <pcap.h>
#include <pthread.h>
#include <stdbool.h>

struct server_t {
  struct sockaddr_in src_addr;
  socklen_t addr_len;
  char* name_addr;
  int port;
  int socket;
  bool terminated;
  pthread_t wait_thread;
  pthread_t recv_thread;
  pthread_t send_thread;
  struct interface_bridge_t inter;
};

struct server_t* server_init(const char* inter_name,
                             const char* name_addr,
                             int port);
int server_run(struct server_t* server);
int server_stop(struct server_t* server);
void server_free(struct server_t* server);

#endif  // BRIDGE_SERVER_H
