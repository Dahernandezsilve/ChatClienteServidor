#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_MESSAGE_SIZE 1024

int serverSocket;
char username[50];

void* receiveMessages(void* arg) {
    char buffer[MAX_MESSAGE_SIZE];
    ssize_t bytesReceived;

    while ((bytesReceived = recv(serverSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesReceived] = '\0';
        printf("%s\n", buffer);
        if (strstr(buffer, "Error: Nombre de usuario ya en uso") != NULL) {
            close(serverSocket);
            exit(1);
        }
    }

    return NULL;
}

void sendMessage(const char* message) {
    send(serverSocket, message, strlen(message), 0);
}

void handleExit() {
    sendMessage(username); // Enviar el nombre de usuario para desconectar
    close(serverSocket);
    printf("Desconectado del servidor\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <nombredeusuario> <IPdelservidor> <puertodelservidor>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    strcpy(username, argv[1]);
    char* serverIp = argv[2];
    int serverPort = atoi(argv[3]);

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error al crear el socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort);

    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
        perror("Dirección del servidor inválida");
        exit(EXIT_FAILURE);
    }

    if (connect(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error al conectar con el servidor");
        exit(EXIT_FAILURE);
    }

    sendMessage(username); // Enviar el nombre de usuario para registrar

    pthread_t receiveThread;
    pthread_create(&receiveThread, NULL, receiveMessages, NULL);

    char message[MAX_MESSAGE_SIZE];
    while (1) {
        fgets(message, sizeof(message), stdin);
        message[strcspn(message, "\n")] = 0;

        if (strcmp(message, "/exit") == 0) {
            handleExit();
        } else {
            sendMessage(message);
        }
    }

    return 0;
}
