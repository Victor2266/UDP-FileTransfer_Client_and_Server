#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define SERVER_TCP_PORT 8080 /* Default TCP port */
#define BUFLEN 102           /* Buffer length */

struct pdu {
    char type;
    int length;
    char data[100];
};

int main(int argc, char **argv) {
    char *host = "localhost";
    int port = SERVER_TCP_PORT;
    struct hostent *phe;
    struct sockaddr_in sin;
    int s, n;
    struct pdu data_in, data_out;

    // Set host and port based on command-line arguments
    switch (argc) {
    case 1:
        break;
    case 2:
        host = argv[1];
        break;
    case 3:
        host = argv[1];
        port = atoi(argv[2]);
        break;
    default:
        fprintf(stderr, "usage: ./client [host [port]]\n");
        exit(1);
    }

    // Configure server address
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if ((phe = gethostbyname(host)) != NULL) {
        memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
    } else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
        fprintf(stderr, "Can't get host entry \n");
        exit(1);
    }

    // Main loop to handle multiple file requests
    while (1) {
        printf("Sending to IP: %s Port: %d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
        printf("Enter filename or ctrl+d to quit: \n");
        memset(data_out.data, 0, 100);
        
        // Read filename input from user
        if ((n = read(0, data_out.data, 100)) > 0) {
            data_out.data[n - 1] = '\0'; // Ensure the filename is null-terminated
        } else {
            printf("No filename entered - closing connection\n");
            break;
        }

        // Create UDP socket
        s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            fprintf(stderr, "Can't create socket \n");
            continue;
        }

        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
            fprintf(stderr, "Can't connect to %s on port %d\n", host, port);
            close(s);
            continue;
        }

        data_out.type = 'C';
        data_out.length = strlen(data_out.data) + 1;

        // Send filename to server
        if (write(s, &data_out, sizeof(data_out)) < 0) {
            perror("Error sending filename");
            close(s);
            continue;
        }

        // Create file to save incoming data
        FILE *fptr = fopen(data_out.data, "w");
        if (fptr == NULL) {
            perror("Error creating file");
            close(s);
            continue;
        }

        fprintf(stderr, "---> Beginning file transfer\n");

        // Receive file data in chunks
        while ((n = read(s, &data_in, sizeof(data_in))) > 0) {
            if (data_in.type == 'E') {
                printf("Error: File not found or cannot access\n\n");
                remove(data_out.data); // Delete file on error
                break;
            } else if (data_in.type == 'F') {
                fwrite(data_in.data, sizeof(char), data_in.length, fptr);
                printf("---> File transfer complete\n\n");
                break;
            } else if (data_in.type == 'D') {
                fwrite(data_in.data, sizeof(char), data_in.length, fptr);
            } else {
                printf("Unspecified error occurred\n\n");
                remove(data_out.data); // Delete file on error
                break;
            }
        }

        fclose(fptr);
        close(s);  // Close client socket
    }
    return 0;
}
