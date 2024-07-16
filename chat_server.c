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
#define MAX_CLIENTS 10
#define BUFFER_SIZE 1024

typedef struct {
    int id;
    int fd;
    char room_name[50];
} Client;

typedef struct {
    int id;
    char room_name[50];
    char password[50];
} Room;

Client clients[MAX_CLIENTS];
Room rooms[MAX_CLIENTS];
int num_clients = 0;
int num_rooms = 0;

int find_room_by_name(char *room_name) {
    for (int i = 0; i < num_rooms; i++) {
        if (strcmp(rooms[i].room_name, room_name) == 0) {
            return i;
        }
    }
    return -1;
}

int find_client_by_fd(int fd) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].fd == fd) {
            return i;
        }
    }
    return -1;
}

void send_message_to_room(char *room_name, char *message, int sender_id) {
    char send_buffer[BUFFER_SIZE];
    snprintf(send_buffer, BUFFER_SIZE, "clientID[%d]: %s", sender_id, message);
    for (int i = 0; i < num_clients; i++) {
        if (clients[i].fd != sender_id && strcmp(clients[i].room_name, room_name) == 0) {
            send(clients[i].fd, send_buffer, strlen(send_buffer), 0);
        }
    }
}


void *handle_client(void *arg) {
    Client *client = (Client *)arg;
    int client_fd = client->fd;

    char buffer[BUFFER_SIZE];
    char send_buffer[BUFFER_SIZE];

    snprintf(send_buffer, BUFFER_SIZE, "Welcome to the chat server! Client[%d]!\nEnter /create <room_name> <password> to create a new room.\nOr Enter /join <room_name> <password> to join the room\n", client_fd);
    send(client_fd, send_buffer, strlen(send_buffer), 0);

    while (1) {
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        int client_index = find_client_by_fd(client_fd);
        if (bytes_received <= 0) {
            printf("Client %d disconnected\n", client_fd);
            close(client_fd);
            
            if (client_index != -1) {
                clients[client_index] = clients[--num_clients];
            }
            pthread_exit(NULL);
        }

        buffer[bytes_received] = '\0';

        // Check if client wants to create a new room
        if (strncmp(buffer, "/create", 7) == 0) {
            char room_name[50];
            char password[50];
            sscanf(buffer + 8, "%s %s", room_name, password);
            
            // Check if the room already exists
            int room_index = find_room_by_name(room_name);
            if (room_index != -1) {
                snprintf(send_buffer, BUFFER_SIZE, "Room '%s' already exists. Please choose a different name.\n", room_name);
                send(client_fd, send_buffer, strlen(send_buffer), 0);
                continue;
            }

            // Add the room to the list
            Room new_room;
            new_room.id = num_rooms++;
            strcpy(new_room.room_name, room_name);
            strcpy(new_room.password, password);
            rooms[new_room.id] = new_room;

            snprintf(send_buffer, BUFFER_SIZE, "Room '%s' created successfully.\n", room_name);
            send(client_fd, send_buffer, strlen(send_buffer), 0);
            continue;
        }

        // Check if client wants to join a room
        if (strncmp(buffer, "/join", 5) == 0) {
            char room_name[50];
            char password[50];
            sscanf(buffer + 6, "%s %s", room_name, password);

            int room_index = find_room_by_name(room_name);
            if (room_index == -1) {
                snprintf(send_buffer, BUFFER_SIZE, "Room '%s' does not exist. Please check the room name.\n", room_name);
                send(client_fd, send_buffer, strlen(send_buffer), 0);
                continue;
            }

            // Check if password matches
            if (strcmp(password, rooms[room_index].password) != 0) {
                snprintf(send_buffer, BUFFER_SIZE, "Incorrect password for room '%s'. Please try again.\n", room_name);
                send(client_fd, send_buffer, strlen(send_buffer), 0);
                continue;
            }
            strcpy(password, "");
            // Join the room
            strcpy(clients[client_index].room_name, room_name);
            snprintf(send_buffer, BUFFER_SIZE, "Joined room '%s' successfully.\n", room_name);
            send(client_fd, send_buffer, strlen(send_buffer), 0);
            continue;
        }

        // Check if client wants to leave a room
        if (strncmp(buffer, "/leave", 6) == 0) {
            snprintf(send_buffer, BUFFER_SIZE, "Left room '%s' successfully.\n", clients[client_index].room_name);
            strcpy(clients[client_index].room_name, "");
            send(client_fd, send_buffer, strlen(send_buffer), 0);
            continue;
        }

        // Handle chat messages
        if (strlen(clients[client_index].room_name) > 0) {
            send_message_to_room(clients[client_index].room_name, buffer, client_fd);
        } else {
            snprintf(send_buffer, BUFFER_SIZE, "You are not in any room. Use /join <room_name> <password> to join a room.\n");
            send(client_fd, send_buffer, strlen(send_buffer), 0);
        }
    }

    return NULL;
}

int main() {
    int server_fd;
    struct sockaddr_in server_addr;
    pthread_t tid;
    int client_id = 0;

    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Initialize server address structure
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // Bind socket to address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_fd, 5) == -1) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    while (1) {
        // Accept incoming connection
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }

        // Create new client thread
        Client new_client;
        new_client.id = client_id++;
        new_client.fd = client_fd;
        strcpy(new_client.room_name, "");
        clients[num_clients++] = new_client;

        pthread_create(&tid, NULL, handle_client, (void *)&new_client);
        pthread_detach(tid);

        printf("New client connected: %d\n", client_fd);
    }

    close(server_fd);

    return 0;
}
