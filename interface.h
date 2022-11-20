#ifndef BRIDGE_INTERFACE_H
#define BRIDGE_INTERFACE_H

#include <pcap.h>

enum type_pkt_t { UNKNOWN, HOST, BROADCAST, MULTICAST, OTHER };

struct interface_bridge_t {
  pthread_t thread;
  char name[255];
  clock_t timeout;
  struct pcap* pcap;
  uint8_t mac[6];
};

void inter_init(struct interface_bridge_t* inter,
                const char* ifname,
                clock_t timeout);
void inter_close(struct interface_bridge_t* inter);
int inter_open(struct interface_bridge_t* inter);
int inter_read(struct interface_bridge_t* inter,
               uint8_t* bytes,
               size_t size,
               enum type_pkt_t* type);
int inter_write(struct interface_bridge_t* inter,
                const uint8_t* bytes,
                size_t size);

#endif  // BRIDGE_INTERFACE_H
