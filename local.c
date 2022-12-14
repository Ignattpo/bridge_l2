#include "local.h"

#include <inttypes.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

struct bridge_tunnel_t {
  struct interface_bridge_t* inter_0;
  struct interface_bridge_t* inter_1;
  bool* terminated;
};

void* inter_swap_ptk(void* thread_data) {
  struct bridge_tunnel_t* tunnel = thread_data;
  struct interface_bridge_t* inter_0 = tunnel->inter_0;
  struct interface_bridge_t* inter_1 = tunnel->inter_1;
  bool* terminated = tunnel->terminated;
  free(tunnel);

  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);
  while (!*terminated) {
    int bytes_count = inter_read(inter_0, buffer, buffer_size);
    if (bytes_count == 0) {
      continue;
    }
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s can't read interface %s\n", __FUNCTION__,
              inter_0->name);
      continue;
    }

    int res = inter_write(inter_1, buffer, bytes_count);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s can't write interface %s\n", __FUNCTION__,
              inter_1->name);
    }
  }

  return NULL;
}

struct local_bridge_t* local_bridge_new(const char* ifname_0,
                                        const char* ifname_1,
                                        clock_t timeout) {
  struct local_bridge_t* bridge = malloc(sizeof(*bridge));
  if (!bridge) {
    fprintf(stderr, "ERROR> %s malloc\n", __FUNCTION__);
    return NULL;
  }

  inter_init(&bridge->inter_0, ifname_0, timeout);
  inter_init(&bridge->inter_1, ifname_1, timeout);
  bridge->terminated = false;

  return bridge;
}

void local_bridge_close(struct local_bridge_t* bridge) {
  pthread_join(bridge->inter_0.thread, NULL);
  pthread_join(bridge->inter_1.thread, NULL);

  inter_close(&bridge->inter_0);
  inter_close(&bridge->inter_1);
}

void local_bridge_free(struct local_bridge_t* bridge) {
  free(bridge);
}

int local_bridge_open(struct local_bridge_t* bridge) {
  int res = 0;
  res = inter_open(&bridge->inter_0);
  if (res == -1) {
    fprintf(stderr, "ERROR> %s can't open interface %s\n", __FUNCTION__,
            bridge->inter_0.name);
    return -1;
  }
  res = inter_open(&bridge->inter_1);
  if (res == -1) {
    fprintf(stderr, "ERROR> %s can't open interface %s\n", __FUNCTION__,
            bridge->inter_1.name);
    return -1;
  }

  return 0;
}

void local_bridge_run(struct local_bridge_t* bridge) {
  struct bridge_tunnel_t* tunnel = malloc(sizeof(*tunnel));
  tunnel->inter_0 = &bridge->inter_0;
  tunnel->inter_1 = &bridge->inter_1;
  tunnel->terminated = &bridge->terminated;
  pthread_create(&bridge->inter_0.thread, NULL, inter_swap_ptk, tunnel);

  tunnel = malloc(sizeof(*tunnel));
  tunnel->inter_0 = &bridge->inter_1;
  tunnel->inter_1 = &bridge->inter_0;
  tunnel->terminated = &bridge->terminated;
  pthread_create(&bridge->inter_1.thread, NULL, inter_swap_ptk, tunnel);
}

void local_bridge_stop(struct local_bridge_t* bridge) {
  bridge->terminated = true;
  pthread_cancel(bridge->inter_0.thread);
  pthread_cancel(bridge->inter_1.thread);
}
