/**
 * @file: client.c
 * @brief: Sends read/write requests to a server
 * @author: Irene Jacob
 **/

#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef MAXWAITTIME
#define MAXWAITTIME 1
#endif

#ifndef REQUESTBUFSIZE
#define REQUESTBUFSIZE 100
#endif

// Forward declarations
void init_sockaddr(struct hostent *server_name, int port,
                   struct sockaddr_in *server_addr);
void sigint_handler(int signal);
void check_timeout(clock_t start_time);
int parse_request(char *req);

// Global variables
volatile sig_atomic_t stop; // 0 to keep program running, 1 to stop

int parse_request(char *req)
{
  return 1; // 1 is valid, 0 is invalid
}

/**
 * changes status to terminated if a timeout occurs
 * @param start_time time when timer was last reset
 * @param end_time   current time
 */
void check_timeout(clock_t start_time)
{
  clock_t end_time = clock();
  // printf("%li\n", (end_time - start_time) / CLOCKS_PER_SEC);
  if ((end_time - start_time) / CLOCKS_PER_SEC > MAXWAITTIME)
  {
    /* server has not responded in MAXWAITTIME seconds -> timeout
                   occurred */
    stop = 1;
  }
}

/**
 * Signal handler for SIGINT (when user wants to terminate process)
 * @param signal an integer representing the type of signal sent
 * HACK:
 * https://stackoverflow.com/questions/6731317/c-exit-from-infinite-loop-on-keypress
 */
void sigint_handler(int signal)
{
  printf("%s\n", "Ctrl-C pressed.");
  stop = 1;
}
/**
 * FIXME: this information is not really useful - read docs and rewrite
 * saves domain name, server name and port number information for server
 * @param server_name pointer to server we are trying to connect to
 * @param port        port number for server we are trying to connect to
 * @param server_addr where we store the information
 */
void init_sockaddr(struct hostent *server_name, int port,
                   struct sockaddr_in *server_addr)
{
  server_addr->sin_family = AF_INET; /* IPv4 Internet Domain */
  // server_addr->sin_addr = *(struct in_addr * )server_name->h_addr;
  // REVIEW: what's the difference between the above line and the memcpy one?
  memcpy(&server_addr->sin_addr, server_name->h_addr, server_name->h_length);
  server_addr->sin_port = htons(port);
}

/**
 * connects to the server node and sends read/write requests to the server
 * @param  argc [description]
 * @param  argv[1] IP address of the server
 * @param  argv[2] Port of the server
 * @return      an int suggesting whether the requests were successfully sent
 * and the response was received in time
 */

int main(int argc, char const *argv[])
{
  struct hostent *server;
  struct sockaddr_in server_addr;
  int sock; /* socket file descriptor */
  int port;
  clock_t timer_start;
  char request_buf[REQUESTBUFSIZE];
  char response_buf[REQUESTBUFSIZE];
  int valid_request; // 1 for valid, 0 for invalid

  // REVIEW: is the check for hostname parameter being supplied unnecessary? -
  // maybe something we should check after getting the basic implementation
  // working
  if (argc != 3 || argv[1] == '\0') /* REVIEW: should there be an optional arg
                                       to pass a file with requests */
  {
    printf("Usage: %s <server address> <server port>\n", argv[0]);
    exit(EXIT_FAILURE);
  }
  // create socket
  server = gethostbyname(argv[1]);
  if (!server)
  {
    printf("%s\n", "host not found");
    exit(EXIT_FAILURE);
  }
  port = atoi(argv[2]);
  memset(&server_addr, 0, sizeof(struct sockaddr_in));
  // setsockopt can be used to enable reusing address and port (optional)
  init_sockaddr(server, port, &server_addr);
  sock = socket(AF_INET, SOCK_DGRAM,
                0); /* SOCK_DGRAM = udp communication type, AF_INET = IPv4
                       Protocol communication domain */
  if (sock < 0)
  {
    printf("%s\n", "could not create socket"); /* REVIEW: Use correct function
                                                  to print error messages */
    exit(EXIT_FAILURE);
  }
  // connect with server
  connect(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
  // gracefully exit on user's signal (Ctrl-c)
  struct sigaction act;
  act.sa_handler = sigint_handler;
  sigaction(SIGINT, &act, NULL);

  stop = 0;
  timer_start = clock();

  int n, len;
  n = 0;
  len = 0;
  while (!stop)
  {
    printf("reached here \n");
    // check timeout
    check_timeout(timer_start);
    // read client's request from stdin
    printf("Type in request: \n");
    fgets(request_buf, REQUESTBUFSIZE, stdin);
    valid_request = parse_request(request_buf); // TODO: parse requests properly
    if (valid_request)
    {
      // send message to server

      sendto(sock, (const char *)request_buf, strlen(request_buf), 0,
             (const struct sockaddr *)&server_addr, sizeof(server_addr));
      /**
       *
       * TODO: where are you storing the requests?
       *
       * Are there multiple requests?
       *
       * Do you need to check for a particular response to each particular
       * request
       */
      // await server's response
      // check_timeout(timer_start, timer_end);
      check_timeout(timer_start);
      if (!stop)
      {
        n = recvfrom(sock, (char *)response_buf, REQUESTBUFSIZE, MSG_WAITALL,
                     (struct sockaddr *)&server_addr, &len);
      }
      
      // n = recvfrom(sock, (char *)response_buf, REQUESTBUFSIZE, MSG_WAITALL,
      //              (struct sockaddr *)&server_addr, &len);
      response_buf[n] = '\0';
      printf("Server : %s\n", response_buf);
      printf("n: %i", n);
      if (n > 100)
      {
        timer_start = clock();
      }
    }
    // timer_end = clock();
    else{
      // restart timer to avoid timeout - not server's fault 
      timer_start = clock();
    }
    
  }
  // close socket
  close(sock); // REVIEW: which should we use, close or shutdown
  // shutdown(sock, SHUT_RDWR);
  return 0;
}
