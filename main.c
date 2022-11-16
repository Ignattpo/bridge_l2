#include "proxy.h"

#include <stdbool.h>

int main(int argc, const char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: l2bridge <if1> <if2>\n");
    return 1;
  }

  if (geteuid() != 0) {
    fprintf(stderr, "bummer: You must be root!\n");
    return 1;
  }

  // Open the pipes on the two interfaces
  const char* ifn0 = argv[1];
  const char* ifn1 = argv[2];
  if (!strcmp(ifn0, ifn1)) {
    fprintf(stderr, "bummer: You are kidding right?\n");
    return 1;
  }

  const char* ifns[2] = {ifn0, ifn1};
  struct pipe_t* p1 = pipe_init(ifns[0], 1);
  struct pipe_t* p2 = pipe_init(ifns[1], 1);
  if (pipe_open(p1) < 0 || pipe_open(p2) < 0)
    return 1;

  struct pipe_t* pipes[2] = {p1, p2};
  uint8_t bytes[64 * 1024];
  uint64_t counts[2] = {0, 0};

  printf("bridging %s <=> %s\n", p1->ifname, p2->ifname);

  // And bridge them. Packet in on 0, out on 1 and vice-versa
  while (true) {
    for (size_t n = 0; n < 2; ++n) {
      enum type_pac_t type_pac;
      int nread = pipe_read(pipes[n], bytes, sizeof(bytes), &type_pac);
      if (nread <= 0 || type_pac == HOST) {
        continue;
      }
      pipe_write(pipes[1 - n], bytes, nread);
      counts[n] += nread;
    }
  }

  return 0;
}
