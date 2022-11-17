#include <net/if.h>
#include <sys/ioctl.h>

#include "bridge.h"

static void inter_init(struct interface_bridge_t* inter,
                       const char* ifname,
                       clock_t timeout) {
  strcpy(inter->name, ifname);
  inter->timeout = timeout;
  inter->pcap = NULL;
}

static void inter_close(struct interface_bridge_t* inter) {
  if (inter->pcap) {
    pcap_close(inter->pcap);
    inter->pcap = NULL;
  }
}

static int get_mac(struct interface_bridge_t* inter) {
  int s;
  struct ifreq buffer;

  s = socket(PF_INET, SOCK_DGRAM, 0);

  memset(&buffer, 0x00, sizeof(buffer));

  strcpy(buffer.ifr_name, inter->name);

  ioctl(s, SIOCGIFHWADDR, &buffer);

  close(s);

  memcpy(inter->mac, buffer.ifr_hwaddr.sa_data, sizeof(inter->mac));

  return 0;
}

static int min(size_t x, size_t y) {
  if (x <= y) {
    return x;
  }
  return y;
}

int inter_open(struct interface_bridge_t* inter) {
  if (inter->pcap) {
    return 0;
  }

  if (get_mac(inter) < 0) {
    fprintf(stderr, "Can't get mac for %s\n", inter->name);
    return -1;
  }

  char eb[PCAP_ERRBUF_SIZE];
  inter->pcap = pcap_open_live(inter->name, 65535, 1, inter->timeout, eb);
  if (inter->pcap == 0) {
    fprintf(stderr, "pcap_open_live(%s) failed\n\t %s\n", inter->name, eb);
    return -1;
  }

  if (pcap_setdirection(inter->pcap, PCAP_D_IN) < 0) {
    fprintf(stderr, "pcap_set_direction(%s) failed\n", inter->name);
    goto aborting;
  }

  return 0;

aborting:
  inter_close(inter);
  return -1;
}

int inter_read(struct interface_bridge_t* inter,
               uint8_t* bytes,
               size_t size,
               enum type_pkt_t* type) {
  if (!inter->pcap || (bytes == 0) || (size == 0)) {
    return -1;
  }

  struct pcap_pkthdr phdr;
  struct pcap_pkthdr* pkt_header = &phdr;
  const uint8_t* pkt_data = 0;
  int ret = pcap_next_ex(inter->pcap, &pkt_header, &pkt_data);
  if (ret == 0) {
    return 0;
  }

  if ((ret < 0) || (pkt_header->len < sizeof(inter->mac))) {
    return -1;
  }

  int bytes_count = min((size_t)pkt_header->len, size);
  memcpy(bytes, pkt_data, bytes_count);

  uint8_t broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  uint8_t dst[6];
  memcpy(&dst, bytes, sizeof(dst));

  // Определяем тип сообщения по мас
  if (memcmp(&dst, &inter->mac, sizeof(inter->mac)) == 0) {
    *type = HOST;
  } else if (memcmp(&dst, &broadcast, sizeof(dst)) == 0) {
    *type = BROADCAST;
  } else if (dst[0] == 0x1) {
    *type = MULTICAST;
  } else {
    *type = OTHER;
  }

  return bytes_count;
}

int inter_write(struct interface_bridge_t* inter,
                const uint8_t* bytes,
                size_t size) {
  if (!inter->pcap || (bytes == 0) || (size == 0)) {
    return -1;
  }

  if (pcap_inject(inter->pcap, bytes, size) != (int)size) {
    return -1;
  }

  return 0;
}

void* inter_swap_ptk(void* thread_data) {
  struct bridge_tunnel_t* tunnel = thread_data;
  struct interface_bridge_t* inter_0 = tunnel->inter_0;
  struct interface_bridge_t* inter_1 = tunnel->inter_1;

  enum type_pkt_t type_pkt = UNKNOWN;
  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);
  while (*tunnel->terminated) {
    int bytes_count = inter_read(inter_0, buffer, buffer_size, &type_pkt);
    if ((bytes_count == 0) || (type_pkt == HOST)) {
      continue;
    }
    if (bytes_count == -1) {
      fprintf(stderr, "Can't read interface %s\n", inter_0->name);
      continue;
    }

    int res = inter_write(inter_1, buffer, bytes_count);
    if (res == -1) {
      fprintf(stderr, "Can't write interface %s\n", inter_1->name);
    }
  }
  free(tunnel);

  return NULL;
}

void bridge_init(struct bridge_t* bridge,
                 const char* ifname_0,
                 const char* ifname_1,
                 clock_t timeout) {
  inter_init(&bridge->inter_0, ifname_0, timeout);
  inter_init(&bridge->inter_1, ifname_1, timeout);
  bridge->terminated = false;
}

void bridge_close(struct bridge_t* bridge) {
  bridge->terminated = false;

  pthread_join(bridge->inter_0.thread, NULL);
  pthread_join(bridge->inter_1.thread, NULL);

  inter_close(&bridge->inter_0);
  inter_close(&bridge->inter_1);
}

int bridge_open(struct bridge_t* bridge) {
  int res = 0;
  res = inter_open(&bridge->inter_0);
  if (res == -1) {
    fprintf(stderr, "Can't open interface %s\n", bridge->inter_0.name);
    return -1;
  }
  res = inter_open(&bridge->inter_1);
  if (res == -1) {
    fprintf(stderr, "Can't open interface %s\n", bridge->inter_1.name);
    return -1;
  }

  return 0;
}

void bridge_run(struct bridge_t* bridge) {
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
