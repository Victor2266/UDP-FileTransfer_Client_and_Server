#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h> // For close() function
#include <arpa/inet.h>
#include <sys/stat.h>  // For lstat

struct pdu {
    char type;
    int length;
    char data[100];
};

#define SERVER_UDP_PORT 8080 /* Default UDP port */

int main(int argc, char *argv[]) {
    struct sockaddr_in fsin;  /* Client address structure */
    struct sockaddr_in sin;   /* Server address structure */
    int s, port, alen, filelen, index, count;
    struct pdu data_in, data_out;
    struct stat file_stat;    // For retrieving file metadata
    char buf[100];
    
    // Determine port based on command-line arguments
    switch (argc) {
    case 1:
        port = SERVER_UDP_PORT;
        break;
    case 2:
        port = atoi(argv[1]);
        break;
    default:
        fprintf(stderr, "Usage: %s [port]\n", argv[0]);
        exit(1);
    }

    /* Set up the server address */
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);

    /* Create UDP socket */
    s = socket(AF_INET, SOCK_DGRAM, 0);
    if (s < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    /* Bind the socket to the server address */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
        perror("Socket binding failed");
        exit(1);
    }

    alen = sizeof(fsin);  // Set client address length for receiving messages

    // Server loop to handle file transfer requests
    while (1) {
        printf("Waiting for filename request on IP %s and port %d...\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));

        // Receive filename request from client
        if (recvfrom(s, &data_in, sizeof(data_in), 0, (struct sockaddr *)&fsin, &alen) < 0) {
            perror("recvfrom error");
            continue;
        }

        // Validate request type
        if (data_in.type != 'C') {
            fprintf(stderr, "Error: Invalid request type\n");
            continue;
        }

        printf("---> Received filename: %s\n", data_in.data);

        // Use lstat to get file information without opening it
        if (lstat(data_in.data, &file_stat) < 0) {
            perror("Error with lstat on file");
            data_out.type = 'E';
            data_out.length = 1;
            data_out.data[0] = 0xFF;
            sendto(s, &data_out, sizeof(data_out), 0, (struct sockaddr *)&fsin, alen);
            continue;
        }

        filelen = file_stat.st_size;  // Get file size from lstat result

        // Open the requested file
        FILE *fptr = fopen(data_in.data, "r");
        if (fptr == NULL) {
            printf("Error opening file\n\n");

            data_out.type = 'E';
            data_out.length = 1;
            data_out.data[0] = 0xFF;
            sendto(s, &data_out, sizeof(data_out), 0, (struct sockaddr *)&fsin, alen);
            continue;
        }

        fprintf(stderr, "[Starting file transfer]\n");

        // Send file content in chunks
        index = 0;
        while (index < filelen) {

            float percentage = (float)index/filelen * 100;
            printf("\rTransfer progress: (%3.0f%%)", percentage);
            fflush(stdout);

            if (filelen - index <= sizeof(data_out.data)) {
                count = filelen - index;
                data_out.type = 'F';  // 'F' for final chunk
            } else {
                count = sizeof(data_out.data);
                data_out.type = 'D';  // 'D' for data chunk
            }

            data_out.length = count;
            fread(data_out.data, sizeof(char), count, fptr);

            if (sendto(s, &data_out, sizeof(data_out), 0, (struct sockaddr *)&fsin, alen) < 0) {
                perror("Error sending data");
                break;
            }
            index += count;
        }

        fprintf(stderr, "\n[File transfer completed]\n\n");
        fclose(fptr);
    }

    close(s);  // Close the server socket
    return 0;
}
