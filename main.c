#include "local.h"
#include "remote.h"

#include <signal.h>
#include <string.h>
#include <unistd.h>

#define ADDR "localhost"
#define PORT 8214

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

static int server_bridge(const char* inter_name,
                         const char* name_addr,
                         int port) {
  struct remote_t* server = server_init(inter_name, name_addr, port);
  if (!server) {
    fprintf(stderr, "ERROR > server_bridge server_init.\n");
    return 1;
  }

  int res = 0;
  res = server_run(server);
  if (res == -1) {
    fprintf(stderr, "ERROR > server_bridge server_run.\n");
    return 1;
  }

  printf("bridging %s <=> %s:%d \n", inter_name, name_addr, port);

  while (!terminated) {
    sleep(1);
  }

  server_stop(server);
  server_free(server);

  return 0;
}
static int client_bridge(const char* inter_name,
                         const char* serv_addr,
                         int serv_port) {
  struct remote_t* client = client_init(inter_name, serv_addr, serv_port);
  if (!client) {
    fprintf(stderr, "ERROR > client_bridge client_init.\n");
    return 1;
  }

  int res = 0;
  res = client_run(client);
  if (res == -1) {
    fprintf(stderr, "ERROR > client_bridge client_run.\n");
    return 1;
  }

  printf("bridging %s <=> %s:%d \n", inter_name, serv_addr, serv_port);
  while (!terminated) {
    sleep(1);
  }

  client_stop(client);
  client_free(client);

  return 0;
}

static int local_bridge(const char* inter_0, const char* inter_1) {
  if (!strcmp(inter_0, inter_1)) {
    fprintf(stderr, "Interfaces must not equal. %s == %s \n", inter_0, inter_1);
    return 1;
  }

  struct local_bridge_t* bridge = local_bridge_new(inter_0, inter_1, 1);

  int res = local_bridge_open(bridge);
  if (res == -1) {
    fprintf(stderr, "Bridge can't open.\n");
    return 1;
  }

  printf("bridging %s <=> %s\n", inter_0, inter_1);

  local_bridge_run(bridge);
  while (!terminated) {
    sleep(1);
  }
  local_bridge_stop(bridge);
  local_bridge_close(bridge);
  local_bridge_free(bridge);

  return 0;
}

int main(int argc, const char** argv) {
  if (geteuid() != 0) {
    fprintf(stderr, "You must be root!\n");
    return 1;
  }

  signals_init();

  int res = 0;
  if (!strcmp(argv[1], "server")) {
    const char* inter_name = NULL;
    const char* name_addr = ADDR;
    int port = PORT;

    if (argc < 3) {
      fprintf(stderr, "Not set name interface\n");
      return 1;
    }

    if (argc >= 3) {
      inter_name = argv[2];
    }
    if (argc >= 4) {
      name_addr = argv[3];
    }
    if (argc >= 5) {
      port = atoi(argv[4]);
    }

    res = server_bridge(inter_name, name_addr, port);
  } else if (!strcmp(argv[1], "client")) {
    const char* inter_name = NULL;
    const char* server_addr = ADDR;
    int server_port = PORT;

    if (argc < 3) {
      fprintf(stderr, "Not set name interface\n");
      return 1;
    }

    if (argc >= 3) {
      inter_name = argv[2];
    }
    if (argc >= 4) {
      server_addr = argv[3];
    }
    if (argc >= 5) {
      server_port = atoi(argv[4]);
    }

    res = client_bridge(inter_name, server_addr, server_port);
  } else {
    if (argc < 3) {
      fprintf(stderr, "Usage: bridge_l2 <if1> <if2>\n");
      return 1;
    }
    res = local_bridge(argv[1], argv[2]);
  }
  return res;
}
