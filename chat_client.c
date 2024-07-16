#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>

#define SERVER_IP "127.0.0.1"
#define PORT 9000
#define BUFFER_SIZE 1024

void *receive_messages(void *arg) {
    int client_fd = *((int *)arg);
    char buffer[BUFFER_SIZE];

    while (1) {
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            printf("Server disconnected\n");
            break;
        }

        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    }

    close(client_fd);
    pthread_exit(NULL);
}

int main() {
    int client_fd;
    struct sockaddr_in server_addr;
    pthread_t tid;
    char buffer[BUFFER_SIZE];
    int clientID;

    // Create socket
    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    server_addr.sin_port = htons(PORT);

    // Connect to server
    if (connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    // Receive welcome message
    int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received > 0) {
        buffer[bytes_received] = '\0';
        printf("%s", buffer);
    }

    // Thread to receive messages from server
    pthread_create(&tid, NULL, receive_messages, &client_fd);

    // Main loop to send messages
    while (1) {
        fgets(buffer, BUFFER_SIZE, stdin);

        // Send message to server
        send(client_fd, buffer, strlen(buffer), 0);
    }

    pthread_cancel(tid);  // Cancel receive thread
    pthread_join(tid, NULL);  // Wait for receive thread to terminate

    close(client_fd);

    return 0;
}
