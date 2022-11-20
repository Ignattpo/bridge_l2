#ifndef BRIDGE_H
#define BRIDGE_H

#include "interface.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

struct local_bridge_t {
  struct interface_bridge_t inter_0;
  struct interface_bridge_t inter_1;
  bool terminated;
};

struct local_bridge_t* local_bridge_new(const char* ifname_0,
                                        const char* ifname_1,
                                        clock_t timeout);
void local_bridge_close(struct local_bridge_t* bridge);
void local_bridge_free(struct local_bridge_t* bridge);

int local_bridge_open(struct local_bridge_t* bridge);
void local_bridge_run(struct local_bridge_t* bridge);
void local_bridge_stop(struct local_bridge_t* bridge);

#endif  // BRIDGE_H
