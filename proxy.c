#include <net/if.h>
#include <sys/ioctl.h>

#include "proxy.h"

struct pipe_t* pipe_init(const char* ifname, clock_t timeout) {
  struct pipe_t* pipe = malloc(sizeof(*pipe));
  strcpy(pipe->ifname, ifname);
  pipe->timeout = timeout;
  pipe->pcap = NULL;

  return pipe;
}

void pipe_free(struct pipe_t* pipe) {
  free(pipe);
}

static int get_mac(struct pipe_t* pipe) {
  int s;
  struct ifreq buffer;

  s = socket(PF_INET, SOCK_DGRAM, 0);

  memset(&buffer, 0x00, sizeof(buffer));

  strcpy(buffer.ifr_name, pipe->ifname);

  ioctl(s, SIOCGIFHWADDR, &buffer);

  close(s);

  memcpy(pipe->mac, buffer.ifr_hwaddr.sa_data, sizeof(pipe->mac));

  return 0;
}

static int min(size_t x, size_t y) {
  if (x <= y) {
    return x;
  }
  return y;
}
/*
 * HalfPipe::open()
 * ----------------
 * Open the half pipe (before we can read/write)
 */
int pipe_open(struct pipe_t* pipe) {
  if (pipe->pcap) {
    return 0;
  }

  if (get_mac(pipe) < 0) {
    fprintf(stderr, "bummer: can't get mac for %s\n", pipe->ifname);
    return -1;
  }

  char eb[PCAP_ERRBUF_SIZE];
  pipe->pcap = pcap_open_live(pipe->ifname, 65535, 1, pipe->timeout, eb);
  if (pipe->pcap == 0) {
    fprintf(stderr, "bummer: pcap_open_live(%s) failed\n\t %s\n", pipe->ifname,
            eb);
    return -1;
  }

  // Ignore packets from this host (just need the incoming ones)
  if (pcap_setdirection(pipe->pcap, PCAP_D_IN) < 0) {
    fprintf(stderr, "bummer: pcap_set_direction(%s) failed\n", pipe->ifname);
    goto aborting;
  }

  return 0;

aborting:
  pipe_close(pipe);
  return -1;
}

void pipe_close(struct pipe_t* pipe) {
  if (pipe->pcap) {
    pcap_close(pipe->pcap);
    pipe->pcap = 0;
  }
}

/*
 * HalfPipe::read()
 * ----------------
 * Read a packet from the half pipe.
 */
int pipe_read(struct pipe_t* pipe,
              uint8_t* bytes,
              size_t size,
              enum type_pac_t* type) {
  if (pipe->pcap == 0 || bytes == 0 || size == 0)
    return -1;

  struct pcap_pkthdr phdr;
  struct pcap_pkthdr* php = &phdr;
  const uint8_t* bptr = 0;
  int ret = pcap_next_ex(pipe->pcap, &php, &bptr);
  if (ret == 0)
    return 0;

  // Truncated read
  if (ret < 0 || php->len < sizeof(pipe->mac))
    return -1;

  int nread = min((size_t)php->len, size);
  memcpy(bytes, bptr, nread);

  uint8_t broadcast[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
  uint8_t dst[6];
  memcpy(&dst, bytes, sizeof(dst));

  // Classify the packet based on the MAC address
  if (memcmp(&dst, &pipe->mac, sizeof(pipe->mac)) == 0) {
    *type = HOST;
  } else if (memcmp(&dst, &broadcast, sizeof(dst)) == 0) {
    *type = BROADCAST;
  } else if (dst[0] == 0x1) {
    *type = MULTICAST;
  } else {
    *type = OTHER;
  }

  return nread;
}

/*
 * HalfPipe::write()
 * -----------------
 * Write a packet through this half pipe into the interface
 */
int pipe_write(struct pipe_t* pipe, const uint8_t* bytes, size_t size) {
  if (pipe->pcap == 0 || bytes == 0 || size == 0)
    return -1;

  if (pcap_inject(pipe->pcap, bytes, size) != (int)size)
    return -1;

  return 0;
}
