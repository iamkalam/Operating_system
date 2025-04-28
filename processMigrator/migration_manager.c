#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 5000
#define THRESHOLD 80.0 // CPU or memory usage threshold for migration

void handle_client(int client_sock) {
    char buffer[128];
    int bytes_read;

    // Receive data from the client
    bytes_read = recv(client_sock, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read < 0) {
        perror("Failed to receive data");
        close(client_sock);
        return;
    }

    buffer[bytes_read] = '\0';
    printf("Received: %s\n", buffer);

    // Parse resource usage
    float cpu_usage, memory_usage;
    sscanf(buffer, "CPU: %f, Memory: %f", &cpu_usage, &memory_usage);

    // Decide if migration is needed
    if (cpu_usage > THRESHOLD || memory_usage > THRESHOLD) {
        printf("Node is overloaded. Triggering migration.\n");
        // Add migration logic here
    }

    close(client_sock);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(1);
    }

    // Listen for connections
    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(1);
    }

    printf("Migration Manager is running on port %d...\n", PORT);

    // Accept and handle clients
    while (1) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        handle_client(client_sock);
    }

    close(server_sock);
    return 0;
}