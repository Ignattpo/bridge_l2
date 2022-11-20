#ifndef BRIDGE_CLIENT_H
#define BRIDGE_CLIENT_H

#include "interface.h"

#include <inttypes.h>
#include <pcap.h>
#include <pthread.h>
#include <stdbool.h>

struct client_t {
  struct sockaddr_in sock_addr;
  char* name_addr;
  int port;
  int socket;
  bool terminated;
  pthread_t read_thread;
  pthread_t write_thread;
  struct interface_bridge_t inter;
};

struct client_t* client_init(const char* inter_name,
                             const char* serv_addr,
                             int serv_port);
int client_run(struct client_t* client);
int client_stop(struct client_t* client);
void client_free(struct client_t* client);

#endif  // BRIDGE_CLIENT_H
