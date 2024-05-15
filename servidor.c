#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define DEFAULT_IP "127.0.0.1"
#define MAX_PENDING_CONNECTIONS 5
#define MAX_CLIENT_MESSAGE_SIZE 1024
#define MAX_USERS 100

typedef struct {
    char username[50];
    char ip[INET_ADDRSTRLEN];
} User;

User users[MAX_USERS];
int userCount = 0;
pthread_mutex_t userLock = PTHREAD_MUTEX_INITIALIZER;

void sendMessage(int socket, const char* message) {
    send(socket, message, strlen(message), 0);
}

void removeUser(const char* username) {
    pthread_mutex_lock(&userLock);
    for (int i = 0; i < userCount; ++i) {
        if (strcmp(users[i].username, username) == 0) {
            users[i] = users[userCount - 1];
            userCount--;
            break;
        }
    }
    pthread_mutex_unlock(&userLock);
}

void getUserList(int clientSocket) {
    pthread_mutex_lock(&userLock);
    char userList[MAX_CLIENT_MESSAGE_SIZE] = "Usuarios conectados:\n";
    for (int i = 0; i < userCount; ++i) {
        strcat(userList, users[i].username);
        strcat(userList, "\n");
    }
    pthread_mutex_unlock(&userLock);
    sendMessage(clientSocket, userList);
}

void* handleClient(void* arg) {
    int clientSocket = *(int*)arg;
    free(arg);

    char buffer[MAX_CLIENT_MESSAGE_SIZE];
    ssize_t bytesReceived;

    // Leer el nombre de usuario del cliente
    bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
    if (bytesReceived < 0) {
        perror("Error al recibir datos del cliente");
        close(clientSocket);
        return NULL;
    }
    buffer[bytesReceived] = '\0';

    char username[50];
    strncpy(username, buffer, sizeof(username));
    username[sizeof(username) - 1] = '\0';

    char clientIp[INET_ADDRSTRLEN];
    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    getpeername(clientSocket, (struct sockaddr*)&addr, &addr_size);
    inet_ntop(AF_INET, &addr.sin_addr, clientIp, INET_ADDRSTRLEN);

    // Verificar si el usuario ya está registrado
    pthread_mutex_lock(&userLock);
    for (int i = 0; i < userCount; ++i) {
        if (strcmp(users[i].username, username) == 0) {
            sendMessage(clientSocket, "Error: Nombre de usuario ya en uso");
            pthread_mutex_unlock(&userLock);
            close(clientSocket);
            return NULL;
        }
    }

    // Registrar al nuevo usuario
    strcpy(users[userCount].username, username);
    strcpy(users[userCount].ip, clientIp);
    userCount++;
    pthread_mutex_unlock(&userLock);

    sendMessage(clientSocket, "Registro exitoso");

    printf("Usuario registrado: %s desde %s\n", username, clientIp);

    // Leer más mensajes del cliente
    while ((bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';

        // Verificar si el cliente solicita el listado de usuarios
        if (strcmp(buffer, "/list") == 0) {
            getUserList(clientSocket);
        } else {
            printf("Mensaje recibido de %s: %s\n", username, buffer);
        }
    }

    // Eliminar usuario de la lista al desconectarse
    removeUser(username);

    printf("Usuario desconectado: %s\n", username);

    close(clientSocket);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <puerto>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[1]);
    if (port <= 0) {
        fprintf(stderr, "Puerto inválido\n");
        exit(EXIT_FAILURE);
    }

    int serverSocket, clientSocket;
    struct sockaddr_in serverAddr, clientAddr;
    socklen_t clientAddrSize = sizeof(clientAddr);

    // Crear socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error al crear el socket del servidor");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, DEFAULT_IP, &serverAddr.sin_addr) <= 0) {
        perror("Dirección del servidor inválida");
        exit(EXIT_FAILURE);
    }

    // Enlazar socket a la dirección del servidor
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error al enlazar el socket del servidor");
        exit(EXIT_FAILURE);
    }

    // Escuchar por conexiones entrantes
    if (listen(serverSocket, MAX_PENDING_CONNECTIONS) < 0) {
        perror("Error al escuchar por conexiones entrantes");
        exit(EXIT_FAILURE);
    }

    printf("Servidor iniciado en %s:%d. Esperando por conexiones...\n", DEFAULT_IP, port);

    // Aceptar conexiones entrantes
    while (1) {
        clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientAddrSize);
        if (clientSocket < 0) {
            perror("Error al aceptar la conexión entrante");
            continue;
        }

        printf("Intento de conexión\n");

        // Crear un hilo para manejar la conexión con el cliente
        pthread_t thread;
        int* clientSocketPtr = malloc(sizeof(int));
        *clientSocketPtr = clientSocket;
        if (pthread_create(&thread, NULL, handleClient, clientSocketPtr) != 0) {
            perror("Error al crear el hilo");
            close(clientSocket);
            free(clientSocketPtr);
        } else {
            pthread_detach(thread); // Desvincular el hilo para que se limpie solo al finalizar
        }
    }

    // Cerrar el socket del servidor
    close(serverSocket);

    return 0;
}
