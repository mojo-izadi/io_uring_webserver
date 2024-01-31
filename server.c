#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <liburing.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    // Create a socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return 1;
    }

    // Set socket options
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        return 1;
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        return 1;
    }

    // Listen for incoming connections
    if (listen(server_fd, 3) < 0) {
        perror("listen failed");
        return 1;
    }

    printf("Server listening on port %d\n", port);

    struct io_uring ring;
    if (io_uring_queue_init(128, &ring, 0) < 0) {
        perror("io_uring_queue_init failed");
        return 1;
    }

    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    struct iovec iov;
    char buffer[BUFFER_SIZE];

    while (1) {
        // Accept a new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("accept failed");
            return 1;
        }

        printf("New connection accepted\n");

        // Read file path from the socket
        memset(buffer, 0, BUFFER_SIZE);
        iov.iov_base = buffer;
        iov.iov_len = BUFFER_SIZE;

        sqe = io_uring_get_sqe(&ring);
        io_uring_prep_readv(sqe, new_socket, &iov, 1, 0);
        io_uring_submit(&ring);

        // Wait for the read operation to complete
        io_uring_wait_cqe(&ring, &cqe);
        io_uring_cqe_seen(&ring, cqe);

        printf("Requested file: %s\n", buffer);

        // Open the file
        FILE *file = fopen(buffer, "r");
        if (file == NULL) {
            // File not found
            write(new_socket, "NOT FOUND", 9);
        } else {
            // Read the file contents and write to the socket
            while (!feof(file)) {
                size_t bytesRead = fread(buffer, 1, BUFFER_SIZE, file);
                if (bytesRead > 0) {
                    write(new_socket, buffer, bytesRead);
                }
            }
            fclose(file);
        }

        // Close the socket
        close(new_socket);
        printf("Connection closed\n");
    }

    io_uring_queue_exit(&ring);
    close(server_fd);

    return 0;
}