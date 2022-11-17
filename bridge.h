#ifndef BRIDGE_H
#define BRIDGE_H

#include <inttypes.h>
#include <pcap.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum type_pkt_t { UNKNOWN, HOST, BROADCAST, MULTICAST, OTHER };

struct interface_bridge_t {
  pthread_t thread;
  char name[255];
  clock_t timeout;
  struct pcap* pcap;
  uint8_t mac[6];
};

struct bridge_t {
  struct interface_bridge_t inter_0;
  struct interface_bridge_t inter_1;
  bool terminated;
};

struct bridge_tunnel_t {
  struct interface_bridge_t* inter_0;
  struct interface_bridge_t* inter_1;
  bool* terminated;
};

void bridge_init(struct bridge_t* bridge,
                 const char* ifname_0,
                 const char* ifname_1,
                 clock_t timeout);
void bridge_close(struct bridge_t* bridge);
int bridge_open(struct bridge_t* bridge);
void bridge_run(struct bridge_t* bridge);

#endif  // BRIDGE_H
