#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/queue.h>

#include <rte_memory.h>
#include <rte_launch.h>
#include <rte_eal.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_debug.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define PORT 0
#define MAXBUFSIZE 1024
 int done;

static int
lcore_hello(__attribute__((unused)) void *arg)
{
    unsigned lcore_id;
    lcore_id = rte_lcore_id();
    printf("hello from core %u\n", lcore_id);
    return 0;
}

int main(int argc, char *argv[])
{
    int ret;
    unsigned lcore_id;
    int sockfd;
    char buffer[MAXBUFSIZE];
    const char *msg = "Hello from server";
    struct sockaddr_in servaddr, cliaddr;

    // initialize the Hashmap
    // hmap = create_map();

    // Creating socket file descriptor
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }
    memset(&servaddr, 0, sizeof(servaddr));
    memset(&cliaddr, 0, sizeof(cliaddr));

    // Filling server information
    servaddr.sin_family    = AF_INET; // IPv4
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);

    // Bind the socket with the server address
    if ( bind(sockfd, (const struct sockaddr *)&servaddr,
              sizeof(servaddr)) < 0 )
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
    {
        rte_panic("Cannot init EAL\n");
    }

    /* call lcore_hello() on every slave lcore */
    RTE_LCORE_FOREACH_SLAVE(lcore_id)
    {
        rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
    }

    /* call it on master lcore too */
    lcore_hello(NULL);

    rte_eal_mp_wait_lcore();
    // initialize_readline();
    // init_rocksdb_txdb();

    // char *line;
    // char *s;

    // while (!done)
    // {
    //     line = readline("rocks> ");
    //     if (!line)
    //     {
    //         break;
    //     }
    //
    //     s = stripwhite(line);
    //
    //     if (*s)
    //     {
    //         add_history(s);
    //         execute_line(s);
    //     }
    //
    //     free(line);
    // }

    // txdb_deconstruct();
    // destroy_map(hmap);

    int len, n;
    // n = recvfrom(sockfd, (char *)buffer, MAXBUFSIZE,
    //              MSG_WAITALL, ( struct sockaddr *) &cliaddr,
    //              &len);
    n = recvfrom(sockfd, (char *) buffer , MAXBUFSIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, (socklen_t *) &len);

    if (n < 0)
      perror("ERROR in recvfrom");

    buffer[n] = '\0';
    printf("Client : %s\n", buffer);
    sendto(sockfd, (const char *)msg, strlen(msg),
           MSG_CONFIRM, (const struct sockaddr *) &cliaddr,
           len);
    printf("Hello message sent.\n");

    return 0;
}
