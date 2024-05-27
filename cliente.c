#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <protobuf-c/protobuf-c.h>
#include "chat.pb-c.h"

#define BUFFER_SIZE 1024

char username[50];
int exit_requested = 0;

struct ThreadArgs {
    int sock;
    const char *username;
};

void send_request(int sock, Chat__Request *request) {
    uint8_t buffer[BUFFER_SIZE];
    unsigned len = chat__request__pack(request, buffer);

    printf("Sending request, operation: %d, length: %u\n", request->operation, len); // Depuración

    if (send(sock, buffer, len, 0) < 0) {
        perror("Send failed");
    } else {
        printf("Request sent successfully\n"); // Depuración
    }
}

Chat__Response* receive_response(int sock) {
    uint8_t buffer[BUFFER_SIZE];
    int n = recv(sock, buffer, BUFFER_SIZE, 0);
    if (n < 0) {
        perror("Receive failed");
        return NULL;
    }
    printf("Received response, length: %d\n", n); // Depuración
    return chat__response__unpack(NULL, n, buffer);
}

void update_user_status(int sock, const char *username, Chat__UserStatus new_status) {
    Chat__UpdateStatusRequest update_status_req = CHAT__UPDATE_STATUS_REQUEST__INIT;
    update_status_req.username = (char *)username;
    update_status_req.new_status = new_status;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__UPDATE_STATUS;
    request.payload_case = CHAT__REQUEST__PAYLOAD_UPDATE_STATUS;
    request.update_status = &update_status_req;

    printf("Updating status for user: %s\n", username); // Depuración
    send_request(sock, &request);

    Chat__Response *response = receive_response(sock);
    if (response) {
        printf("Server response: %s\n", response->message);
        chat__response__free_unpacked(response, NULL);
    }
}

void register_user(int sock, const char *username) {
    Chat__NewUserRequest new_user_req = CHAT__NEW_USER_REQUEST__INIT;
    new_user_req.username = (char *)username;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__REGISTER_USER;
    request.payload_case = CHAT__REQUEST__PAYLOAD_REGISTER_USER;
    request.register_user = &new_user_req;

    printf("Registering user: %s\n", username); // Depuración
    send_request(sock, &request);

    Chat__Response *response = receive_response(sock);
    if (response) {
        printf("Server response: %s\n", response->message);
        chat__response__free_unpacked(response, NULL);
    }
}

void send_message(int sock, const char *recipient, const char *content) {
    Chat__SendMessageRequest send_msg_req = CHAT__SEND_MESSAGE_REQUEST__INIT;
    send_msg_req.recipient = (char *)recipient;
    send_msg_req.content = (char *)content;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__SEND_MESSAGE;
    request.payload_case = CHAT__REQUEST__PAYLOAD_SEND_MESSAGE;
    request.send_message = &send_msg_req;

    printf("Sending message to: %s, content: %s\n", recipient, content); // Depuración
    send_request(sock, &request);
}


void list_connected_users(int sock) {
    Chat__UserListRequest user_list_req = CHAT__USER_LIST_REQUEST__INIT;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__GET_USERS;
    request.payload_case = CHAT__REQUEST__PAYLOAD_GET_USERS;
    request.get_users = &user_list_req;

    printf("Requesting list of connected users\n"); // Depuración
    send_request(sock, &request);

    // Espera un tiempo limitado para recibir la respuesta del servidor
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    struct timeval timeout;
    timeout.tv_sec = 5; // Espera 5 segundos
    timeout.tv_usec = 0;

    int activity = select(sock + 1, &readfds, NULL, NULL, &timeout);
    if (activity == -1) {
        perror("Select error");
    } else if (activity == 0) {
        printf("No response from server.\n");
    } else {
        Chat__Response *response = receive_response(sock);
        if (response && response->result_case == CHAT__RESPONSE__RESULT_USER_LIST) {
            printf("Connected users:\n"); // Depuración
            for (size_t i = 0; i < response->user_list->n_users; ++i) {
                printf("User: %s, Status: %d\n", response->user_list->users[i]->username, response->user_list->users[i]->status);
            }
            chat__response__free_unpacked(response, NULL);
        } else {
            printf("Failed to get user list or incorrect response format\n"); // Depuración
        }
    }
}

void *message_receiver(void *arg) {
    int sock = *((int *)arg);
    printf("hola\n");
    while (!exit_requested) { // Hilo sigue en ejecución hasta que se solicite la salida
        struct timeval tv;
        tv.tv_sec = 1; // Espera 1 segundo
        tv.tv_usec = 0;

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);

        int activity = select(sock + 1, &readfds, NULL, NULL, &tv);
        if (activity < 0) {
            perror("Select error");
            break;
        } else if (activity == 0) {
            continue; // No hay actividad, intenta de nuevo
        } else {
            Chat__Response *response = receive_response(sock);
            printf("hola\n");
            if (response) {
                if (response->result_case != CHAT__RESPONSE__RESULT_INCOMING_MESSAGE) {
                    break; // Salir del bucle si no es un mensaje entrante
                }
                switch (response->result_case) {
                    case CHAT__RESPONSE__RESULT_INCOMING_MESSAGE:
                        printf("Message received from %s: %s\n", response->incoming_message->sender, response->incoming_message->content);
                        break;
                    default:
                        printf("Unknown response received. Response type: %d\n", response->result_case);
                        // Imprimir el contenido de la respuesta
                        uint8_t *response_buffer = NULL;
                        unsigned response_len = chat__response__get_packed_size(response);
                        response_buffer = malloc(response_len);
                        if (response_buffer != NULL) {
                            chat__response__pack(response, response_buffer);
                            printf("Response content (hex): ");
                            for (unsigned i = 0; i < response_len; ++i) {
                                printf("%02x ", response_buffer[i]);
                            }
                            printf("\n");
                            free(response_buffer);
                        }
                        break;
                }
                chat__response__free_unpacked(response, NULL);

            } else {
                printf("Failed to receive response\n");
                break;
            }
        }
    }
    return NULL;
}


void unregister_user(int sock, const char *username_to_unregister) {
    Chat__User unregister_user_req = CHAT__USER__INIT;
    unregister_user_req.username = (char *)username_to_unregister;

    Chat__Request request = CHAT__REQUEST__INIT;
    request.operation = CHAT__OPERATION__UNREGISTER_USER;
    request.payload_case = CHAT__REQUEST__PAYLOAD_UNREGISTER_USER;
    request.unregister_user = &unregister_user_req;

    printf("Unregistering user: %s\n", username_to_unregister); // Depuración
    send_request(sock, &request);

    Chat__Response *response = receive_response(sock);
    if (response) {
        printf("Server response: %s\n", response->message);
        chat__response__free_unpacked(response, NULL);
    }

    close(sock);
    exit_requested = 1; // Establecer la variable de salida
    exit(0);
}

void *user_input(void *args_ptr) {
    struct ThreadArgs *args = (struct ThreadArgs *)args_ptr;
    int sock = args->sock;
    const char *username = args->username;
    char command[BUFFER_SIZE];
    while (1) {
        printf("Enter command (send/list/info/status/help/exit): ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = 0;

        printf("Command entered: %s\n", command); // Agregamos una impresión para verificar el comando ingresado

        if (strcmp(command, "send") == 0) {
            char recipient[BUFFER_SIZE];
            char message[BUFFER_SIZE];
            printf("Enter recipient: ");
            fgets(recipient, BUFFER_SIZE, stdin);
            recipient[strcspn(recipient, "\n")] = 0;
            printf("Enter message: ");
            fgets(message, BUFFER_SIZE, stdin);
            message[strcspn(message, "\n")] = 0;
            send_message(sock, recipient, message);
        } else if (strcmp(command, "list") == 0) {
            list_connected_users(sock);
        } else if (strcmp(command, "info") == 0) {
            printf("Under construction.\n");
        } else if (strcmp(command, "status") == 0) {
            int new_status;
            printf("Enter new status (0 for online, 1 for busy, 2 for offline): ");
            scanf("%d", &new_status);
            getchar(); // Consumir el carácter de nueva línea residual en el búfer de entrada
            if (new_status >= 0 && new_status <= 2) {
                update_user_status(sock, username, new_status);
            } else {
                printf("Invalid status.\n");
            }
        } else if (strcmp(command, "help") == 0) {
            printf("Help incoming!\n");
            printf("Available commands:\nsend - Send public or private messages. If you choose to send a private message\nYou must declare the receiver and the content of your message.\nlist - The list of users using the chat.\nstatus - Change your current status.\ninfo - Look for some user's info.\nexit - Unregister/Log out.\n");
        } else if (strcmp(command, "exit") == 0) {
            unregister_user(sock, username);
            close(sock);
            //exit_requested = 1; // Establece el indicador de salida
            break;
        } else {
            printf("Unknown command.\n");
        }

        // Limpiar el búfer de entrada para evitar problemas de lectura
        fflush(stdin);
    }
    return NULL;
}





int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <username> <server_ip> <server_port>\n", argv[0]);
        return 1;
    }

    const char *username = argv[1];
    const char *server_ip = argv[2];
    int server_port = atoi(argv[3]);

    int sock;
    struct sockaddr_in server_addr;

    

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to server %s:%d\n", server_ip, server_port); // Depuración

    register_user(sock, username);
    struct ThreadArgs args = {sock, username};

    pthread_t receiver_thread, input_thread;
    if (pthread_create(&receiver_thread, NULL, message_receiver, (void *)&sock) != 0 ||
        pthread_create(&input_thread, NULL, user_input, &args) != 0) {
        perror("Failed to create threads");
        return 1;
    }

    pthread_join(receiver_thread, NULL);  
    pthread_join(input_thread, NULL);  

    close(sock);
    return 0;
}
