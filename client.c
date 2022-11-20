#include "client.h"

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
  struct client_t* client = thread_data;

  enum type_pkt_t type_pkt = UNKNOWN;
  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);

  while (!client->terminated) {
    ssize_t bytes_count =
        inter_read(&client->inter, buffer, buffer_size, &type_pkt);
    if ((bytes_count == 0) || (type_pkt == HOST)) {
      continue;
    }
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR>%s inter_read %s\n", __FUNCTION__,
              client->inter.name);
      continue;
    }

    int res = sendto(client->socket, buffer, bytes_count, 0, &client->sock_addr,
                     sizeof(client->sock_addr));
    if (res == -1) {
      fprintf(stderr, "ERROR>%s sendto %s\n", __FUNCTION__, client->name_addr);
    }
  }

  return 0;
}

static void* recv_thread(void* thread_data) {
  struct client_t* client = thread_data;

  uint8_t buffer[64 * 1024];
  size_t buffer_size = sizeof(buffer);
  struct sockaddr addr;
  socklen_t addrlen;

  while (!client->terminated) {
    ssize_t bytes_count =
        recvfrom(client->socket, buffer, buffer_size, 0, &addr, &addrlen);
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s recvfrom %s\n", __FUNCTION__,
              client->name_addr);
      continue;
    }

    if (memcmp(&addr, (struct sockaddr*)&client->sock_addr, sizeof(addr))) {
      fprintf(stderr, "ERROR> %s package incorrect source address\n",
              __FUNCTION__);
      continue;
    }

    int res = inter_write(&client->inter, buffer, buffer_size);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s inter_write %s\n", __FUNCTION__,
              client->inter.name);
    }
  }

  return 0;
}

struct client_t* client_init(const char* inter_name,
                             const char* server_addr,
                             int server_port) {
  struct client_t* client = malloc(sizeof(*client));
  if (!client) {
    fprintf(stderr, "ERROR> %s malloc %s\n", __FUNCTION__, server_addr);
    goto aborting;
  }
  int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (client_socket < 0) {
    goto aborting;
  }

  client->name_addr = strdup(server_addr);
  client->port = server_port;
  client->socket = client_socket;
  client->terminated = 0;

  inter_init(&client->inter, inter_name, 1);
  int res = inter_open(&client->inter);
  if (res == -1) {
    goto aborting_client;
  }

  struct hostent* hosten = gethostbyname(server_addr);
  if (!hosten) {
    fprintf(stderr, "ERROR> %s gethostbyname", __FUNCTION__);
    return NULL;
  }

  struct in_addr** addr_list = (struct in_addr**)hosten->h_addr_list;
  if (addr_list[0] == NULL) {
    fprintf(stderr, "ERROR> %s incorrect addres %s\n", __FUNCTION__,
            server_addr);
    return NULL;
  }

  client->sock_addr.sin_family = AF_INET;
  client->sock_addr.sin_port = htons(server_port);
  client->sock_addr.sin_addr = *addr_list[0];

  return client;
aborting_client:
  free(client->name_addr);
aborting:
  close(client_socket);

  return NULL;
}

int client_run(struct client_t* client) {
  int res = pthread_create(&client->read_thread, NULL, recv_thread, client);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create recv_thread addres: %s port: %d \n",
            __FUNCTION__, client->name_addr, client->port);
  }

  res = pthread_create(&client->write_thread, NULL, sendto_thread, client);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create sendto_thread addres: %s port: %d \n",
            __FUNCTION__, client->name_addr, client->port);
  }

  return 0;
}

int client_stop(struct client_t* client) {
  client->terminated = true;
  pthread_cancel(client->write_thread);
  pthread_cancel(client->read_thread);

  pthread_join(client->write_thread, NULL);
  pthread_join(client->read_thread, NULL);

  close(client->socket);
  inter_close(&client->inter);

  return 0;
}

void client_free(struct client_t* client) {
  free(client->name_addr);
  free(client);
}
