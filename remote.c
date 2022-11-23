#include "remote.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/if_arp.h>
#include <linux/if_tun.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/udp.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static void* server_sendto_thread(void* thread_data) {
  struct server_t* server = thread_data;
  struct base_t* base = &server->base;

  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);

  while (!base->terminated) {
    ssize_t bytes_count = read(server->fd, buffer, buffer_size);
    if (bytes_count == 0) {
      continue;
    }
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR>%s read \n", __FUNCTION__);
      perror("read:");
      continue;
    }

    int res = sendto(base->socket, buffer, bytes_count, 0,
                     (struct sockaddr*)&base->sock_addr, base->addr_len);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s can't send addr %s:%d\n", __FUNCTION__,
              inet_ntoa(base->sock_addr.sin_addr), base->sock_addr.sin_port);
      perror("sendto:");
    }
  }

  return 0;
}

static void* server_recv_thread(void* thread_data) {
  struct server_t* server = thread_data;
  struct base_t* base = &server->base;

  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);
  struct sockaddr_in addr;
  socklen_t addrlen = 0;
  while (!base->terminated) {
    ssize_t bytes_count = recvfrom(base->socket, buffer, buffer_size, 0,
                                   (struct sockaddr*)&addr, &addrlen);
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s recvfrom %s\n", __FUNCTION__, base->name_addr);
      continue;
    }

    if (memcmp(&addr, (struct sockaddr*)&base->sock_addr, addrlen)) {
      //        inet_ntoa создает статический буффер в который записывается
      //        строка адресса повторный вызов inet_ntoa перезаписывает этот
      //        буффер поэтому вывод ошибки разбит на 2 функции. вызывает
      //        опасение использование этой функции внутри потока, но другого
      //        варианта приведения адресса не нашел.
      fprintf(stderr,
              "ERROR> %s package incorrect source address received:%s:%d ",
              __FUNCTION__, inet_ntoa(addr.sin_addr), addr.sin_port);
      fprintf(stderr, "expected:%s:%d\n ", inet_ntoa(base->sock_addr.sin_addr),
              base->sock_addr.sin_port);
      continue;
    }

    bytes_count = write(server->fd, buffer, bytes_count);
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR>%s write \n", __FUNCTION__);
      perror("write:");
      continue;
    }
  }

  return 0;
}

static void* sendto_thread(void* thread_data) {
  struct client_t* client = thread_data;
  struct base_t* base = &client->base;

  enum type_pkt_t type_pkt = UNKNOWN;
  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);
  while (!base->terminated) {
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

    int res = sendto(base->socket, buffer, bytes_count, 0,
                     (struct sockaddr*)&base->sock_addr, base->addr_len);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s can't send addr %s:%d\n", __FUNCTION__,
              inet_ntoa(base->sock_addr.sin_addr), base->sock_addr.sin_port);
      perror("sendto:");
    }
  }

  return 0;
}

static void* recv_thread(void* thread_data) {
  struct client_t* client = thread_data;
  struct base_t* base = &client->base;

  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);
  struct sockaddr_in addr;
  socklen_t addrlen;
  while (!base->terminated) {
    ssize_t bytes_count = recvfrom(base->socket, buffer, buffer_size, 0,
                                   (struct sockaddr*)&addr, &addrlen);
    if (bytes_count == -1) {
      fprintf(stderr, "ERROR> %s recvfrom %s\n", __FUNCTION__, base->name_addr);
      continue;
    }

    if (memcmp(&addr, (struct sockaddr*)&base->sock_addr, addrlen)) {
      //        inet_ntoa создает статический буффер в который записывается
      //        строка адресса повторный вызов inet_ntoa перезаписывает этот
      //        буффер поэтому вывод ошибки разбит на 2 функции. вызывает
      //        опасение использование этой функции внутри потока, но другого
      //        варианта приведения адресса не нашел.
      fprintf(stderr,
              "ERROR> %s package incorrect source address received:%s:%d ",
              __FUNCTION__, inet_ntoa(addr.sin_addr), addr.sin_port);
      fprintf(stderr, "expected:%s:%d\n ", inet_ntoa(base->sock_addr.sin_addr),
              base->sock_addr.sin_port);
      continue;
    }

    int res = inter_write(&client->inter, buffer, bytes_count);
    if (res == -1) {
      fprintf(stderr, "ERROR> %s inter_write %s\n", __FUNCTION__,
              client->inter.name);
    }
  }

  return 0;
}

static void* wait_for_client_thread(void* thread_data) {
  struct server_t* server = thread_data;

  uint8_t buffer[1600];
  size_t buffer_size = sizeof(buffer);

  struct base_t* base = &server->base;
  ssize_t bytes_count =
      recvfrom(server->base.socket, buffer, buffer_size, 0,
               (struct sockaddr*)&base->sock_addr, &base->addr_len);
  if (bytes_count == -1) {
    fprintf(stderr, "ERROR> %s can't recvfrom\n", __FUNCTION__);
    return 0;
  }

  bytes_count = write(server->fd, buffer, bytes_count);
  if (bytes_count == -1) {
    fprintf(stderr, "ERROR>%s write \n", __FUNCTION__);
    perror("write:");
  }

  int res =
      pthread_create(&base->read_thread, NULL, server_recv_thread, server);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create recv_thread addres: %s port: %d \n",
            __FUNCTION__, base->name_addr, base->port);
    return 0;
  }

  res = pthread_create(&base->write_thread, NULL, server_sendto_thread, server);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create sendto_thread addres: %s port: %d \n",
            __FUNCTION__, base->name_addr, base->port);
    return 0;
  }

  return 0;
}

static int base_init(struct base_t* base, const char* addr, int server_port) {
  int base_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (base_socket < 0) {
    return -1;
  }

  base->name_addr = strdup(addr);
  base->port = server_port;
  base->socket = base_socket;
  base->terminated = false;
  base->addr_len = sizeof(base->sock_addr);
  base->read_thread = 0;
  base->write_thread = 0;

  struct hostent* hosten = gethostbyname(addr);
  if (!hosten) {
    fprintf(stderr, "ERROR> %s gethostbyname\n", __FUNCTION__);
    return -1;
  }

  struct in_addr** addr_list = (struct in_addr**)hosten->h_addr_list;
  if (addr_list[0] == NULL) {
    fprintf(stderr, "ERROR> %s incorrect addres %s\n", __FUNCTION__, addr);
    return -1;
  }

  base->sock_addr.sin_family = AF_INET;
  base->sock_addr.sin_port = htons(server_port);
  base->sock_addr.sin_addr = *addr_list[0];

  return 0;
}

static void base_stop(struct base_t* base) {
  base->terminated = true;

  pthread_cancel(base->read_thread);
  pthread_cancel(base->write_thread);

  pthread_join(base->read_thread, NULL);
  pthread_join(base->write_thread, NULL);
}

static void base_free(struct base_t* base) {
  free(base->name_addr);
}

struct client_t* client_init(const char* inter_name,
                             const char* server_addr,
                             int server_port) {
  if ((strlen(inter_name) >= 4) && (inter_name[0] == 't') &&
      (inter_name[1] == 'a') && (inter_name[2] == 'p')) {
    fprintf(stderr, "ERROR>client not expects tap(%s) interface\n", inter_name);
    return NULL;
  }
  if ((strlen(inter_name) >= 4) && (inter_name[0] == 't') &&
      (inter_name[1] == 'u') && (inter_name[2] == 'n')) {
    fprintf(stderr, "ERROR>client not expects tun(%s) interface\n", inter_name);
    return NULL;
  }
  struct client_t* client = malloc(sizeof(*client));
  if (!client) {
    fprintf(stderr, "ERROR> %s malloc %s\n", __FUNCTION__, server_addr);
    return NULL;
  }
  int res = base_init(&client->base, server_addr, server_port);
  if (res == -1) {
    goto aborting;
  }

  inter_init(&client->inter, inter_name, 1);
  res = inter_open(&client->inter);
  if (res == -1) {
    goto aborting;
  }

  return client;

aborting:
  close(client->base.socket);
  base_free(&client->base);
  free(client);

  return NULL;
}

struct server_t* server_init(const char* inter_name,
                             const char* name_addr,
                             int port) {
  if ((strlen(inter_name) < 4) || (inter_name[0] != 't') ||
      (inter_name[1] != 'a') || (inter_name[2] != 'p')) {
    fprintf(stderr, "ERROR>server expects tap(%s) interface\n", inter_name);
    return NULL;
  }

  struct server_t* server = malloc(sizeof(*server));
  if (!server) {
    fprintf(stderr, "ERROR> %s malloc %s\n", __FUNCTION__, name_addr);
    return NULL;
  }

  int res = base_init(&server->base, name_addr, port);
  if (res == -1) {
    goto aborting;
  }

  res = bind(server->base.socket, (struct sockaddr*)&server->base.sock_addr,
             server->base.addr_len);
  if (res != 0) {
    fprintf(stderr, "ERROR> %s bind", __FUNCTION__);
    goto aborting;
  }
  server->wait_thread = 0;
  //  TODO настройка интерфейса server

  server->fd = open("/dev/net/tun", O_RDWR);
  if (server->fd == -1) {
    fprintf(stderr, "ERROR> %s open", __FUNCTION__);
    goto aborting;
  }

  struct ifreq ifr = {0};
  ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
  strncpy(ifr.ifr_name, inter_name, IFNAMSIZ);
  res = ioctl(server->fd, TUNSETIFF, &ifr);
  if (res == -1) {
    fprintf(stderr, "ERROR> %s ioctl", __FUNCTION__);
    goto aborting;
  }

  return server;

aborting:
  close(server->base.socket);
  base_free(&server->base);
  free(server);

  return NULL;
}

int client_run(struct client_t* client) {
  struct base_t* base = &client->base;
  int res = pthread_create(&base->read_thread, NULL, recv_thread, client);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create recv_thread addres: %s port: %d \n",
            __FUNCTION__, base->name_addr, base->port);
    return res;
  }

  res = pthread_create(&base->write_thread, NULL, sendto_thread, client);
  if (res != 0) {
    fprintf(stderr,
            "ERROR> %s pthread_create sendto_thread addres: %s port: %d \n",
            __FUNCTION__, base->name_addr, base->port);
    return res;
  }

  return res;
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

void server_stop(struct server_t* server) {
  base_stop(&server->base);

  pthread_cancel(server->wait_thread);
  pthread_join(server->wait_thread, NULL);

  close(server->base.socket);
}

void client_stop(struct client_t* client) {
  base_stop(&client->base);

  close(client->base.socket);
  inter_close(&client->inter);
}

void server_free(struct server_t* server) {
  base_free(&server->base);
  free(server);
}

void client_free(struct client_t* client) {
  base_free(&client->base);
  free(client);
}
