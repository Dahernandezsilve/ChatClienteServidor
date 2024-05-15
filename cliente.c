#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>


#define MAX_SERVER_RESPONSE_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <nombre_de_usuario> <ip_del_servidor> <puerto_del_servidor>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *username = argv[1];
    const char *serverIp = argv[2];
    int port = atoi(argv[3]);
    if (port <= 0) {
        fprintf(stderr, "Puerto inválido\n");
        exit(EXIT_FAILURE);
    }

    int clientSocket;
    struct sockaddr_in serverAddr;

    // Crear socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Error al crear el socket del cliente");
        exit(EXIT_FAILURE);
    }

    // Configurar dirección del servidor
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
        perror("Dirección del servidor inválida");
        exit(EXIT_FAILURE);
    }

    // Conectar al servidor
    if (connect(clientSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Error al conectar al servidor");
        exit(EXIT_FAILURE);
    }

    printf("Conectado al servidor\n");

    // Enviar nombre de usuario al servidor
    if (send(clientSocket, username, strlen(username), 0) < 0) {
        perror("Error al enviar nombre de usuario al servidor");
        exit(EXIT_FAILURE);
    }

    printf("Nombre de usuario enviado al servidor: %s\n", username);

    // Enviar mensaje al servidor (aquí puedes agregar la lógica para enviar mensajes adicionales)

    // Leer respuesta del servidor
    char buffer[MAX_SERVER_RESPONSE_SIZE];
    ssize_t bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesReceived < 0) {
        perror("Error al recibir respuesta del servidor");
        exit(EXIT_FAILURE);
    }

    printf("Respuesta del servidor: %s\n", buffer);

    // Cerrar el socket del cliente
    close(clientSocket);

    return 0;
}