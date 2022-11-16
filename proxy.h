#ifndef PROXY_H
#define PROXY_H

#include <inttypes.h>
#include <pcap.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum type_pac_t { HOST, BROADCAST, MULTICAST, OTHER };

struct pipe_t {
  char ifname[255];
  clock_t timeout;
  struct pcap* pcap;
  uint8_t mac[6];
};

struct pipe_t* pipe_init(const char* ifname, clock_t timeout);
void pipe_free(struct pipe_t* pipe);
int pipe_open(struct pipe_t* pipe);
int pipe_read(struct pipe_t* pipe,
              uint8_t* bytes,
              size_t size,
              enum type_pac_t* type_pac);
int pipe_write(struct pipe_t* pipe, const uint8_t* bytes, size_t size);
void pipe_close(struct pipe_t* pipe);

#endif  // PROXY_H
