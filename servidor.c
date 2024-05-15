#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> // Incluye inet_pton
#include <unistd.h>

#define DEFAULT_IP "127.0.0.1"  // Dirección IP por defecto
#define MAX_PENDING_CONNECTIONS 5
#define MAX_CLIENT_MESSAGE_SIZE 1024

void handleClient(int clientSocket) {
    char buffer[MAX_CLIENT_MESSAGE_SIZE];
    ssize_t bytesReceived;

    // Leer datos del cliente
    bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived < 0) {
        perror("Error al recibir datos del cliente");
        close(clientSocket);
        return;
    }

    printf("Mensaje recibido del cliente: %s\n", buffer);

    // Enviar respuesta al cliente
    const char* responseMessage = "Mensaje recibido por el servidor";
    if (send(clientSocket, responseMessage, strlen(responseMessage), 0) < 0) {
        perror("Error al enviar datos al cliente");
    }

    // Cerrar el socket del cliente
    close(clientSocket);
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

        printf("Cliente conectado\n");

        // Manejar la conexión con el cliente
        handleClient(clientSocket);
    }

    // Cerrar el socket del servidor
    close(serverSocket);

    return 0;
}
