#include "bridge.h"

#include <signal.h>

static volatile bool terminated = 0;

void sigint_cb(int sig) {
  if (!terminated) {
    terminated = 1;
  }
}

static void signals_init(void) {
  struct sigaction sa_hup;
  memset(&sa_hup, 0, sizeof(sa_hup));
  sa_hup.sa_handler = sigint_cb;
  sa_hup.sa_flags = SA_RESTART;
  sigaction(SIGINT, &sa_hup, 0);
}

int main(int argc, const char** argv) {
  if (argc < 3) {
    fprintf(stderr, "Usage: bridge_l2 <if1> <if2>\n");
    return 1;
  }

  if (geteuid() != 0) {
    fprintf(stderr, "You must be root!\n");
    return 1;
  }

  const char* inter_0 = argv[1];
  const char* inter_1 = argv[2];
  if (!strcmp(inter_0, inter_1)) {
    fprintf(stderr, "Interfaces must not equal. %s == %s \n", inter_0, inter_1);
    return 1;
  }

  signals_init();

  struct bridge_t bridge;
  bridge_init(&bridge, inter_0, inter_1, 1);

  int res = bridge_open(&bridge);
  if (res == -1) {
    fprintf(stderr, "Bridge can't open.\n");
    return 1;
  }

  printf("bridging %s <=> %s\n", inter_0, inter_1);

  bridge_run(&bridge);
  while (!terminated) {
    sleep(1);
  }
  bridge_close(&bridge);

  return 0;
}
