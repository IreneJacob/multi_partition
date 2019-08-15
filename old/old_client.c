#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <netdb.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <string.h>
#include <netinet/in.h>
#include <time.h>

struct client_context
{
    struct sockaddr_in server_addr;
};

int main(int argc, char const *argv[])
{

    char buf[64];
    clock_t start_t, end_t;
    int pinged_res;
    /* argv[1] is internet address of server argv[2] is port of server.
      * Convert the port from ascii to integer and then from host byte
      * order to network byte order.
      */
    if (argc != 3)
    {
        printf("Usage: %s <host address> <port> \n", argv[0]);
        exit(1);
    }

    // The name of the server is passed as an argument
    struct hostent *server;
    if ((server = gethostbyname(argv[1])) == NULL)
    {
        printf("ERROR: host %s not found", argv[1]);
        return -1;
    }
    /* Set up the server name */
    struct client_context ctx;
    memset(&ctx.server_addr, 0, sizeof ctx.server_addr);
    ctx.server_addr.sin_family = AF_INET; /* Internet Domain    */

    /* Server's Address   */
    memcpy((char *)&(ctx.server_addr.sin_addr.s_addr), (char *)server->h_addr_list[0], server->h_length);
    ctx.server_addr.sin_port = htons(atoi(argv[2])); /* Server Port        */

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        perror("New DGRAM socket");
        return -1;
    }
    strcpy(buf, "Hello, World!");
    int n = sendto(sock, buf, strlen(buf) + 1, 0, (struct sockaddr *)&ctx.server_addr, sizeof(ctx.server_addr));
    if (n < 0)
    {
        perror("sendto");
    }
    int done = 0;
    buf[0] = 0;
    start_t = clock();

    // Listen for the return
    while (done != 1)
    {
        /* print the server's reply */
        n = recvfrom(sock, buf, strlen(buf), 0, (struct sockaddr *)&ctx.server_addr, (socklen_t *)sizeof(ctx.server_addr));
        if (n < 0)
            perror("ERROR in recvfrom");
        end_t = clock();
        printf("Echo from server: %s", buf);
        pinged_res = strcmp(buf, "Hello, World!");
        /* if the server's response is the same */
        if (pinged_res == 0)
        {
            done = 1;
        }
        else if ((end_t - start_t) / CLOCKS_PER_SEC > 10)
        {
            printf("Timeout occurred");
            done = 1;
        }
        else
        {
            done = 0;
        }
    }
    shutdown(sock, SHUT_RDWR);
    return 0;
}
