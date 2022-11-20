#include "server.h"

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void* sendto_thread(void* thread_data) {
  struct server_t* server = thread_data;

  enum type_pkt_t type_pkt = UNKNOWN;
  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);

  while (!server->terminated) {
    ssize_t bytes_count =
        inter_read(&server->inter, buffer, buffer_size, &type_pkt);
    if ((bytes_count == 0) || (type_pkt == HOST)) {
      continue;
    }
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s can't read interface %s\n", __FUNCTION__,
              server->inter.name);
      continue;
    }

    int res = sendto(server->socket, buffer, bytes_count, 0,
                     (struct sockaddr*)&server->src_addr, server->addr_len);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s can't send addr %s:%d\n", __FUNCTION__,
              inet_ntoa(server->src_addr.sin_addr), server->src_addr.sin_port);
    }
  }

  return 0;
}

static void* recv_thread(void* thread_data) {
  struct server_t* server = thread_data;

  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);
  struct sockaddr_in src_addr;
  socklen_t addrlen = sizeof(src_addr);

  while (!server->terminated) {
    ssize_t bytes_count = recvfrom(server->socket, buffer, buffer_size, 0,
                                   (struct sockaddr*)&src_addr, &addrlen);
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s can't recvfrom\n", __FUNCTION__);
      continue;
    }
    if (memcmp(&src_addr, (struct sockaddr*)&server->src_addr, addrlen)) {
      fprintf(stderr, "ERROR> %s package incorrect source address\n",
              __FUNCTION__);
      continue;
    }

    int res = inter_write(&server->inter, buffer, buffer_size);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s can't write interface %s\n", __FUNCTION__,
              server->inter.name);
    }
  }

  return 0;
}

static void* wait_for_client_thread(void* thread_data) {
  struct server_t* server = thread_data;

  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);

  struct sockaddr_in src_addr;
  socklen_t addrlen = sizeof(src_addr);

  ssize_t bytes_count = recvfrom(server->socket, buffer, buffer_size, 0,
                                 (struct sockaddr*)&src_addr, &addrlen);
  if (bytes_count == -1) {
    fprintf(stderr, "ERROR> %s can't recvfrom\n", __FUNCTION__);
    return 0;
  }

  server->addr_len = addrlen;
  memcpy(&server->src_addr, &src_addr, addrlen);

  int res = inter_write(&server->inter, buffer, buffer_size);
  if (res == -1) {
    fprintf(stderr, "ERROR> %s can't write interface %s\n", __FUNCTION__,
            server->inter.name);
  }

  res = pthread_create(&server->recv_thread, NULL, recv_thread, server);
  if (res != 0) {
    fprintf(stderr, "ERROR> %s pthread_create recv_thread\n", __FUNCTION__);
  }

  res = pthread_create(&server->send_thread, NULL, sendto_thread, server);
  if (res != 0) {
    fprintf(stderr, "ERROR> %s pthread_create sendto_thread\n", __FUNCTION__);
  }

  return 0;
}

struct server_t* server_init(const char* inter_name,
                             const char* name_addr,
                             int port) {
  int server_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (server_socket < 0) {
    goto aborting;
  }

  struct hostent* hosten = gethostbyname(name_addr);
  if (!hosten) {
    fprintf(stderr, "ERROR> %s gethostbyname", __FUNCTION__);
    return NULL;
  }

  struct in_addr** addr_list = (struct in_addr**)hosten->h_addr_list;
  if (addr_list[0] == NULL) {
    fprintf(stderr, "ERROR> %s incorrect addres %s\n", __FUNCTION__, name_addr);
    return NULL;
  }

  struct sockaddr_in sock_addr;
  sock_addr.sin_family = AF_INET;
  sock_addr.sin_port = htons(port);
  sock_addr.sin_addr = *addr_list[0];

  int res =
      bind(server_socket, (struct sockaddr*)&sock_addr, sizeof(sock_addr));
  if (res != 0) {
    fprintf(stderr, "ERROR> %s bind", __FUNCTION__);
    goto aborting;
  }

  struct server_t* server = malloc(sizeof(*server));
  if (!server) {
    fprintf(stderr, "ERROR> %s malloc %s\n", __FUNCTION__, name_addr);
    goto aborting;
  }

  server->name_addr = strdup(name_addr);
  server->port = port;
  server->socket = server_socket;
  server->terminated = 0;

  inter_init(&server->inter, inter_name, 1);
  res = inter_open(&server->inter);
  if (res == -1) {
    free(server->name_addr);
    goto aborting;
  }

  return server;

aborting:
  close(server_socket);

  return NULL;
}

int server_run(struct server_t* server) {
  int res = pthread_create(&server->wait_thread, NULL, wait_for_client_thread,
                           server);
  if (res != 0) {
    fprintf(stderr, "ERROR> %s pthread_create wait_for_client_thread",
            __FUNCTION__);
  }

  return 0;
}

int server_stop(struct server_t* server) {
  server->terminated = true;
  pthread_cancel(server->wait_thread);
  pthread_cancel(server->recv_thread);
  pthread_cancel(server->send_thread);

  pthread_join(server->wait_thread, NULL);
  pthread_join(server->recv_thread, NULL);
  pthread_join(server->send_thread, NULL);

  close(server->socket);
  inter_close(&server->inter);

  return 0;
}

void server_free(struct server_t* server) {
  free(server->name_addr);
  free(server);
}
