#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVER_IP "192.168.1.100" // Replace with the central node's IP
#define SERVER_PORT 5000

void get_resource_usage(float *cpu_usage, float *memory_usage) {
    FILE *fp;
    char buffer[128];

    // Get CPU usage
    fp = popen("top -bn1 | grep 'Cpu(s)' | awk '{print $2 + $4}'", "r");
    fgets(buffer, sizeof(buffer), fp);
    *cpu_usage = atof(buffer);
    pclose(fp);

    // Get memory usage
    fp = popen("free | grep Mem | awk '{print $3/$2 * 100.0}'", "r");
    fgets(buffer, sizeof(buffer), fp);
    *memory_usage = atof(buffer);
    pclose(fp);
}

void send_data_to_server(float cpu_usage, float memory_usage) {
    int sock;
    struct sockaddr_in server_addr;
    char message[128];

    // Create socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Connect to server
    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection to server failed");
        close(sock);
        exit(1);
    }

    // Send resource usage data
    snprintf(message, sizeof(message), "CPU: %.2f, Memory: %.2f", cpu_usage, memory_usage);
    send(sock, message, strlen(message), 0);

    close(sock);
}

int main() {
    float cpu_usage, memory_usage;

    while (1) {
        get_resource_usage(&cpu_usage, &memory_usage);
        printf("CPU: %.2f%%, Memory: %.2f%%\n", cpu_usage, memory_usage);
        send_data_to_server(cpu_usage, memory_usage);
        sleep(5);
    }

    return 0;
}