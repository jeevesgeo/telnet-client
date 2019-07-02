/*
 *  telnetc.c - A simple telnet client program
 *  June, 2019.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

// #defines
#define IAC             255
#define DONT            254
#define DO              253
#define WONT            252
#define WILL            251
#define SB              250
#define SE              240
#define NAWS            31
#define DEFAULT_PORT    23


// telnet negotiation function
static void do_negotiate(int sock, unsigned char *buff, int bufflen)
{
    int i;

    if (buff[1] == DO && buff[2] == NAWS) {
        unsigned char willnaws[] = {255, 251, 31};
        if (send(sock, willnaws, 3, 0) < 0)
            exit(1);

        unsigned char sbnaws[] = {255, 250, 31, 0, 80, 0, 24, 255, 240};
        if (send(sock, sbnaws, 3, 0) < 0)
            exit(1);
        return;
    }

    for (i=0; i < bufflen; i++) {
        if (buff[i] == DO)
            buff[i] = WONT;
        else if (buff[i] == WILL)
            buff[i] = DO;
    }

    if (send(sock, buff, bufflen, 0) < 0)
        exit(1);
}

/*
 * main entry point
 *
 */
int main(int argc, char *argv[])
{
    int telnet_port = DEFAULT_PORT;
    int server_sock, rc, len;
    unsigned char buf[32];
	struct addrinfo hints, *serverinfo, *p;
    struct timeval tv;
    tv.tv_sec = 1; // 1second
    tv.tv_usec = 0;

    // check for correct usage
    if (argc < 2 || argc > 3) {
        printf("Usage: %s address [port]\n", argv[0]);
        return 1;
    }

    // if port is given, use it instead of the default
    if (argc == 3)
        telnet_port = atoi(argv[2]);

    // get ip address from url
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rc = getaddrinfo(argv[1], "telnet", &hints, &serverinfo)) != 0) {
		printf("Failed to retrieve the ip address!\n");
		return 1;
	}

	// loop through the addrinfo results and connect
	for (p = serverinfo; p!= NULL; p = p->ai_next) {
		// create socket
		if ((server_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
			continue;

		// connect to the telnet server
		if (connect(server_sock, p->ai_addr, p->ai_addrlen) == -1) {
		    close(server_sock);
		    continue;
		}
		break;
	}

	if (p == NULL) {
		printf("Failed to connect to the server!\n");
        return 1;
    }

    // infinite loop
    while(1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        if (server_sock != 0)
            FD_SET(server_sock, &rfds);
        FD_SET(0, &rfds);

        // wait for input data
        int numready = select(server_sock + 1, &rfds, NULL, NULL, &tv);
        if (numready < 0) {
            printf("Error waiting for input!\n");
            return 1;
        } else if (numready == 0) {
            tv.tv_sec = 1; // 1second
            tv.tv_usec = 0;
        } else if (server_sock != 0 && FD_ISSET(server_sock, &rfds)) {
            // first read a single byte
            len = recv(server_sock, buf, 1, 0);
            if (len < 0) {
                printf("Error reading from socket!\n");
                return 1;
            } else if (len == 0) {
                printf("Server hung up!\n");
                return 0;
            }

            if (buf[0] == IAC) {
                // read two more bytes
                len = recv(server_sock, &buf[1], 2, 0);
                if (len < 0) {
                    printf("Error reading from socket!\n");
                    return 1;
                } else if (len == 0) {
                    printf("Server hung up!\n");
                    return 0;
                }

                do_negotiate(server_sock, buf, 3);

            } else {
                buf[1] = '\0';
                printf("%s", buf);
                fflush(0);
            }
        } else if (FD_ISSET(0, &rfds)) {
            buf[0] = getc(stdin);
            if (send(server_sock, buf, 1, 0) < 0)
                return 1;
            if (buf[0] == '\n')
                putchar('\r');  // force an LF
        }
    }

    close(server_sock);
    return 0;
}
