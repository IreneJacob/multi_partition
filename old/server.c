/**
 * @file: server.c
 * @brief: Stores key,value pairs (sent by client) in partitioned database
 * @author: Irene Jacob
 **/
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_memory.h>
#include <rte_per_lcore.h>

#ifndef MAXBUFSIZE
#define MAXBUFSIZE 1024
#endif

#ifndef PORT
#define PORT 0
#endif

volatile sig_atomic_t stop;

// Forward declarations
// void init_sockaddr(struct hostent *server_name, int port,
// struct sockaddr_in *server_addr);
void sigint_handler(int signal);

// Global variables

// TODO: put common functions in a seperate file
/**
 * FIXME: this information is not really useful - read docs and rewrite
 * saves domain name, server name and port number information for server
 * @param server_name pointer to server we are trying to connect to
 * @param port        port number for server we are trying to connect to
 * @param server_addr where we store the information
 */
// void init_sockaddr(struct hostent *server_name, int port,
//                    struct sockaddr_in *server_addr) {
//   server_addr->sin_family = AF_INET; /* IPv4 Internet Domain */
//   // server_addr->sin_addr = *(struct in_addr * )server_name->h_addr;
//   // REVIEW: what's the difference between the above line and the memcpy one?
//   memcpy(&server_addr->sin_addr, server_name->h_addr, server_name->h_length);
//   server_addr->sin_port = htons(port);
// }

static int lcore_hello(__attribute__((unused)) void *arg) {
  unsigned lcore_id;
  lcore_id = rte_lcore_id();
  printf("hello from core %u\n", lcore_id);
  return 0;
}
void sigint_handler(__attribute__((unused)) int signal)
{
  printf("%s\n", "Ctrl-C pressed.");
  stop = 1;
}

// void sigint_handler(__attribute__((unused)) int signal)
// {
//   printf("%s\n", "Ctrl-C pressed.");
//   stop = 1;
//   printf("%i", stop);
//   // exit(0);
// }

/**
 * listens for client requests and handles single/multi partition commands
 * @param  argc TODO
 * @param  argv TODO
 * @return      0 for success
 */
int main(int argc, char **argv) {
  int ret;
  unsigned lcore_id;
  int sock; /* socket file descriptor */
  char buffer[MAXBUFSIZE];
  struct sockaddr_in server_addr, client_addr;
  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons((u_short)PORT);

  // gracefully exit on user's signal (Ctrl-c)
  struct sigaction act;
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);
  stop = 0;
  
  // signal(SIGINT, sigint_handler); // gracefully exit on user's signal (Ctrl-c)

  // TODO: EAL init
  sock = socket(AF_INET, SOCK_DGRAM,0); 
  /* SOCK_DGRAM = udp communication type, AF_INET = IPv4
                       Protocol communication domain */
  if (sock < 0) {
    printf("%s\n", "could not create socket"); /* REVIEW: Use correct function
                                                  to print error messages */
    exit(EXIT_FAILURE);
  }
  // int bind_status = bind(sock,(const struct sockaddr *)&server_addr,
  // sizeof(server_addr)); if (bind_status < 0) {
  //   printf("%s\n", "could not bind to server" );
  //   exit(EXIT_FAILURE);
  // }
  // initialise environment abstraction layer for DPDK
  ret = rte_eal_init(argc, argv);
  if (ret < 0) {
    rte_panic("Cannot init EAL\n");
  }

  /* call lcore_hello() on every slave lcore
  TODO: this should be where we initialise worker threads
  */
  RTE_LCORE_FOREACH_SLAVE(lcore_id) {
    rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
  }

  /* call it on master lcore too */
  lcore_hello(NULL);
  rte_eal_mp_wait_lcore();
  int n = 0;
  socklen_t server_addr_len = 0;
  printf("Awaiting client request \n");
  while (!stop) {
    // this is a blocking call
    n = recvfrom(sock, (char *)buffer, MAXBUFSIZE, MSG_WAITALL,
                 (struct sockaddr *)&client_addr, &server_addr_len);
    buffer[n] = '\0';
    // TODO: multipartition commands etc handled here for rocksdb
    printf("Client : %s\n", buffer);
    // send reply to client
    sendto(sock, (const char *)buffer, strlen(buffer), 0,
           (const struct sockaddr *)&client_addr, server_addr_len);
    printf("Hello message sent.\n");
  }
  close(sock);
  return 0;
}
