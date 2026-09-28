#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <uk/assert.h>
#include <unistd.h>
#include <errno.h>
#include <flexos/isolation.h>

int run_server(int RECVBUFFERSIZE)
{
    int sockfd, connfd, rc;
    int len __attribute__((flexos_whitelist));
    struct sockaddr_in cli __attribute__((flexos_whitelist));
    struct sockaddr_in servaddr __attribute__((flexos_whitelist));
    char *buf = flexos_malloc_whitelist(1024 * 512 * sizeof(char));

    flexos_gate_r(liblwip, sockfd, socket, AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        flexos_gate(libc, printf, FLEXOS_SHARED_LITERAL("Socket failed\n"));
        return 0;
    }

    for (size_t i = 0; i < sizeof(servaddr); i++)
        *((char *) &servaddr) = 0;

    servaddr.sin_family = AF_INET;
    /* no need for gates here, these are simple calculation macros */
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(12345);

    flexos_gate_r(liblwip, rc, bind, sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
    if (rc != 0) {
        flexos_gate(libc, printf, FLEXOS_SHARED_LITERAL("Bind failed\n"));
        return 0;
    }

    flexos_gate_r(liblwip, rc, listen, sockfd, 5);
    if (rc != 0) {
        flexos_gate(libc, printf, FLEXOS_SHARED_LITERAL("Listen failed\n"));
        return 0;
    }

    len = sizeof(cli);

    flexos_gate_r(liblwip, connfd, accept, sockfd, (struct sockaddr *) &cli, &len);

    if (connfd < 0) {
        flexos_gate(libc, printf, FLEXOS_SHARED_LITERAL("Accept failed\n"));
        return 0;
    }

    while (1) {
	flexos_gate_r(liblwip, rc, recv, connfd, buf, RECVBUFFERSIZE, 0);

        if (rc < 0) {
            flexos_gate(libc, printf, FLEXOS_SHARED_LITERAL("Read failed with %d\n"), rc);
            break;
        }
    }

    return 0;
}
