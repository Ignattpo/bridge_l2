#include "remote.h"

#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static int remote_run(struct remote_t* remote);

static void* sendto_thread(void* thread_data) {
  struct remote_t* remote = thread_data;

  enum type_pkt_t type_pkt = UNKNOWN;
  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);
  while (!remote->terminated) {
    ssize_t bytes_count =
        inter_read(&remote->inter, buffer, buffer_size, &type_pkt);
    if ((bytes_count == 0) || (type_pkt == HOST)) {
      continue;
    }
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR>%s inter_read %s\n", __FUNCTION__,
              remote->inter.name);
      continue;
    }

    int res = sendto(remote->socket, buffer, bytes_count, 0,
                     (struct sockaddr*)&remote->sock_addr, remote->addr_len);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s can't send addr %s:%d\n", __FUNCTION__,
              inet_ntoa(remote->sock_addr.sin_addr),
              remote->sock_addr.sin_port);
      perror("sendto:");
    }
  }

  return 0;
}

static void* recv_thread(void* thread_data) {
  struct remote_t* remote = thread_data;

  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);
  struct sockaddr_in addr;
  socklen_t addrlen;
  while (!remote->terminated) {
    ssize_t bytes_count = recvfrom(remote->socket, buffer, buffer_size, 0,
                                   (struct sockaddr*)&addr, &addrlen);
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s recvfrom %s\n", __FUNCTION__,
              remote->name_addr);
      continue;
    }

    if (memcmp(&addr, (struct sockaddr*)&remote->sock_addr, addrlen)) {
      fprintf(stderr,
              "ERROR> %s package incorrect source address received:%s:%d "
              "expected:%s:%d\n",
              __FUNCTION__, inet_ntoa(addr.sin_addr), addr.sin_port,
              inet_ntoa(remote->sock_addr.sin_addr),
              remote->sock_addr.sin_port);
      continue;
    }

    int res = inter_write(&remote->inter, buffer, buffer_size);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s inter_write %s\n", __FUNCTION__,
              remote->inter.name);
    }
  }

  return 0;
}

static void* wait_for_client_thread(void* thread_data) {
  struct remote_t* server = thread_data;

  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);

  struct sockaddr_in src_addr;
  socklen_t addrlen = sizeof(src_addr);

  ssize_t bytes_count = recvfrom(server->socket, buffer, buffer_size, 0,
                                 (struct sockaddr*)&src_addr, &addrlen);
  if (bytes_count == -1) {
    fprintf(stderr, "ERROR> %s can't recvfrom\n", __FUNCTION__);
    return 0;
  }

  int res = inter_write(&server->inter, buffer, buffer_size);
  if (res == -1) {
    fprintf(stderr, "ERROR> %s inter_write %s\n", __FUNCTION__,
            server->inter.name);
  }

  server->addr_len = addrlen;
  memcpy(&server->sock_addr, &src_addr, addrlen);

  remote_run(server);

  return 0;
}

static struct remote_t* remote_init(const char* inter_name,
                                    const char* addr,
                                    int server_port) {
  struct remote_t* remote = malloc(sizeof(*remote));
  if (!remote) {
    fprintf(stderr, "ERROR> %s malloc %s\n", __FUNCTION__, addr);
    return NULL;
  }

  int remote_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (remote_socket < 0) {
    goto aborting;
  }

  remote->name_addr = strdup(addr);
  remote->port = server_port;
  remote->socket = remote_socket;
  remote->terminated = false;
  remote->addr_len = sizeof(remote->sock_addr);
  remote->read_thread = 0;
  remote->wait_thread = 0;
  remote->write_thread = 0;

  inter_init(&remote->inter, inter_name, 1);
  int res = inter_open(&remote->inter);
  if (res == -1) {
    goto aborting_socket;
  }

  struct hostent* hosten = gethostbyname(addr);
  if (!hosten) {
    fprintf(stderr, "ERROR> %s gethostbyname", __FUNCTION__);
    return NULL;
  }

  struct in_addr** addr_list = (struct in_addr**)hosten->h_addr_list;
  if (addr_list[0] == NULL) {
    fprintf(stderr, "ERROR> %s incorrect addres %s\n", __FUNCTION__, addr);
    return NULL;
  }

  remote->sock_addr.sin_family = AF_INET;
  remote->sock_addr.sin_port = htons(server_port);
  remote->sock_addr.sin_addr = *addr_list[0];

  return remote;

aborting_socket:
  close(remote_socket);
  free(remote->name_addr);
aborting:
  free(remote);

  return NULL;
}

static int remote_run(struct remote_t* remote) {
  int res = pthread_create(&remote->read_thread, NULL, recv_thread, remote);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create recv_thread addres: %s port: %d \n",
            __FUNCTION__, remote->name_addr, remote->port);
  }

  res = pthread_create(&remote->write_thread, NULL, sendto_thread, remote);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create sendto_thread addres: %s port: %d \n",
            __FUNCTION__, remote->name_addr, remote->port);
  }

  return 0;
}

static void remote_stop(struct remote_t* remote) {
  remote->terminated = true;

  pthread_cancel(remote->read_thread);
  pthread_cancel(remote->write_thread);

  pthread_join(remote->read_thread, NULL);
  pthread_join(remote->write_thread, NULL);
}

static void remote_free(struct remote_t* remote) {
  free(remote->name_addr);
  free(remote);
}

struct remote_t* client_init(const char* inter_name,
                             const char* server_addr,
                             int server_port) {
  return remote_init(inter_name, server_addr, server_port);
}

struct remote_t* server_init(const char* inter_name,
                             const char* name_addr,
                             int port) {
  struct remote_t* server = remote_init(inter_name, name_addr, port);
  if (!server) {
    return NULL;
  }

  int res = bind(server->socket, (struct sockaddr*)&server->sock_addr,
                 server->addr_len);
  if (res != 0) {
    fprintf(stderr, "ERROR> %s bind", __FUNCTION__);
    remote_free(server);
    return NULL;
  }

  return server;
}

int client_run(struct remote_t* client) {
  int res = remote_run(client);
  return res;
}

int server_run(struct remote_t* server) {
  int res = pthread_create(&server->wait_thread, NULL, wait_for_client_thread,
                           server);
  if (res != 0) {
    fprintf(stderr, "ERROR> %s pthread_create wait_for_client_thread",
            __FUNCTION__);
  }

  return 0;
}

void server_stop(struct remote_t* server) {
  remote_stop(server);

  pthread_cancel(server->wait_thread);
  pthread_join(server->wait_thread, NULL);

  close(server->socket);
  inter_close(&server->inter);
}

void client_stop(struct remote_t* client) {
  remote_stop(client);

  close(client->socket);
  inter_close(&client->inter);
}

void server_free(struct remote_t* server) {
  remote_free(server);
}

void client_free(struct remote_t* client) {
  remote_free(client);
}
