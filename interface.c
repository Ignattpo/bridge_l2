#include "interface.h"

#include <inttypes.h>
#include <net/if.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

void inter_init(struct interface_bridge_t* inter,
                const char* ifname,
                clock_t timeout) {
  strcpy(inter->name, ifname);
  inter->timeout = timeout;
  inter->pcap = NULL;
}

void inter_close(struct interface_bridge_t* inter) {
  if (inter->pcap) {
    pcap_close(inter->pcap);
    inter->pcap = NULL;
  }
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

  char eb[PCAP_ERRBUF_SIZE];
  inter->pcap = pcap_open_live(inter->name, 65535, 1, inter->timeout, eb);
  if (inter->pcap == 0) {
    fprintf(stderr, "ERROR> %s pcap_open_live(%s) failed\n\t %s\n",
            __FUNCTION__, inter->name, eb);
    return -1;
  }

  if (pcap_setdirection(inter->pcap, PCAP_D_IN) < 0) {
    fprintf(stderr, "ERROR> %s pcap_setdirection(%s) failed\n", __FUNCTION__,
            inter->name);
    goto aborting;
  }

  return 0;

aborting:
  inter_close(inter);
  return -1;
}

int inter_read(struct interface_bridge_t* inter, uint8_t* bytes, size_t size) {
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

  if (ret < 0) {
    return -1;
  }

  int bytes_count = min((size_t)pkt_header->len, size);
  memcpy(bytes, pkt_data, bytes_count);

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
