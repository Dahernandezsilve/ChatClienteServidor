#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <protobuf-c/protobuf-c.h>
#include "chat.pb-c.h"

#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_QUEUE_SIZE 100 // Tamaño máximo de la cola

typedef struct {
    int socket;
    char username[50];
    char ip_address[INET_ADDRSTRLEN];
    Chat__UserStatus status;
} client_t;

client_t *clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Estructura para una respuesta en la cola
typedef struct {
    Chat__Response *response;
} ResponseQueueItem;


typedef struct {
    ResponseQueueItem items[MAX_QUEUE_SIZE];
    int front, rear;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} ResponseQueue;

// Definición de la cola global
ResponseQueue response_queue;


// Inicializar la cola
void response_queue_init(ResponseQueue *queue) {
    queue->front = 0;
    queue->rear = 0;
    pthread_mutex_init(&queue->mutex, NULL);
    pthread_cond_init(&queue->not_empty, NULL);
    pthread_cond_init(&queue->not_full, NULL);
}

// Insertar una respuesta en la cola
void response_queue_push(ResponseQueue *queue, Chat__Response *response) {
    pthread_mutex_lock(&queue->mutex);
    while ((queue->rear + 1) % MAX_QUEUE_SIZE == queue->front) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    queue->items[queue->rear].response = response;
    queue->rear = (queue->rear + 1) % MAX_QUEUE_SIZE;
    pthread_cond_signal(&queue->not_empty);
    pthread_mutex_unlock(&queue->mutex);
}

// Sacar una respuesta de la cola
Chat__Response *response_queue_pop(ResponseQueue *queue) {
    pthread_mutex_lock(&queue->mutex);
    while (queue->front == queue->rear) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    Chat__Response *response = queue->items[queue->front].response;
    queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    pthread_cond_signal(&queue->not_full);
    pthread_mutex_unlock(&queue->mutex);
    return response;
}

// Función para liberar memoria de una respuesta de la cola
void response_queue_item_free(ResponseQueueItem *item) {
    chat__response__free_unpacked(item->response, NULL);
}

// Función para liberar memoria de todos los elementos de la cola
void response_queue_destroy(ResponseQueue *queue) {
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    while (queue->front != queue->rear) {
        response_queue_item_free(&queue->items[queue->front]);
        queue->front = (queue->front + 1) % MAX_QUEUE_SIZE;
    }
}


void add_client(client_t *client) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i]) {
            clients[i] = client;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void remove_client(client_t *client) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] == client) {
            clients[i] = NULL;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void update_user_status(Chat__UpdateStatusRequest *update_request) {
    const char *username = update_request->username;
    Chat__UserStatus new_status = update_request->new_status;

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->username, username) == 0) {
            clients[i]->status = new_status;
            printf("Updated status for user %s to %d\n", username, new_status);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void list_connected_users(int client_socket) {
    printf("Sending user list to client...\n"); // Mensaje de depuración

    Chat__UserListResponse user_list = CHAT__USER_LIST_RESPONSE__INIT;
    user_list.n_users = 0;
    user_list.users = malloc(MAX_CLIENTS * sizeof(Chat__User*));

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            Chat__User *user = malloc(sizeof(Chat__User));
            chat__user__init(user);
            user->username = strdup(clients[i]->username); // Duplicar la cadena de caracteres
            user->status = clients[i]->status; // Utiliza el estado real del usuario
            user_list.users[user_list.n_users++] = user;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    Chat__Response response = CHAT__RESPONSE__INIT;
    response.operation = CHAT__OPERATION__GET_USERS;
    response.status_code = CHAT__STATUS_CODE__OK;
    response.result_case = CHAT__RESPONSE__RESULT_USER_LIST;
    response.user_list = &user_list;

    uint8_t buffer[BUFFER_SIZE];
    unsigned len = chat__response__pack(&response, buffer);
    printf("Packed user list, length: %u\n", len);
    if (send(client_socket, buffer, len, 0) < 0) {
        perror("Send failed");
    } else {
        printf("User list sent successfully\n");
    }

    for (int i = 0; i < user_list.n_users; i++) {
        free(user_list.users[i]->username); // Liberar la memoria de la cadena de caracteres duplicada
        free(user_list.users[i]);
    }
    free(user_list.users);
}

void send_private_message(client_t *sender, Chat__SendMessageRequest *msg_req) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->username, msg_req->recipient) == 0) {
            // Crear un mensaje de respuesta
            Chat__Response response = CHAT__RESPONSE__INIT;
            response.operation = CHAT__OPERATION__INCOMING_MESSAGE;
            response.status_code = CHAT__STATUS_CODE__OK;
            response.result_case = CHAT__RESPONSE__RESULT_INCOMING_MESSAGE;
            
            // Crear y llenar el mensaje de respuesta
            Chat__IncomingMessageResponse *incoming_msg = malloc(sizeof(Chat__IncomingMessageResponse));
            chat__incoming_message_response__init(incoming_msg);
            incoming_msg->sender = strdup(sender->username); // Establecer el remitente
            incoming_msg->content = strdup(msg_req->content);
            incoming_msg->type = CHAT__MESSAGE_TYPE__DIRECT; // Indicar que es un mensaje directo
            
            // Establecer el mensaje de respuesta en el campo correspondiente
            response.incoming_message = incoming_msg;

            // Empaquetar y enviar el mensaje de respuesta
            uint8_t buffer[BUFFER_SIZE];
            unsigned len = chat__response__pack(&response, buffer);
            if (send(clients[i]->socket, buffer, len, 0) < 0) {
                perror("Send failed");
            } else {
                printf("Message sent to %s from %s\n", msg_req->recipient, sender->username);
            }
            break;
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->username, sender->username) == 0) {
            // Crear un mensaje de respuesta
            Chat__Response response = CHAT__RESPONSE__INIT;
            response.operation = CHAT__OPERATION__SEND_MESSAGE;
            response.status_code = CHAT__STATUS_CODE__OK;
            response.message = "Message delivered successfully";

            // Empaquetar y enviar el mensaje de respuesta
            uint8_t buffer[BUFFER_SIZE];
            unsigned len = chat__response__pack(&response, buffer);
            if (send(clients[i]->socket, buffer, len, 0) < 0) {
                perror("Send failed");
            } else {
                printf("Confirmation sent to %s\n", sender->username);
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void send_general_message(client_t *sender, Chat__SendMessageRequest *msg_req) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i]) {
            // Crear un mensaje de respuesta
            Chat__Response response = CHAT__RESPONSE__INIT;
            response.operation = CHAT__OPERATION__INCOMING_MESSAGE;
            response.status_code = CHAT__STATUS_CODE__OK;
            response.result_case = CHAT__RESPONSE__RESULT_INCOMING_MESSAGE;

            // Crear y llenar el mensaje de respuesta
            Chat__IncomingMessageResponse *incoming_msg = malloc(sizeof(Chat__IncomingMessageResponse));
            chat__incoming_message_response__init(incoming_msg);
            incoming_msg->sender = strdup(sender->username); // Establecer el remitente
            incoming_msg->content = strdup(msg_req->content);
            incoming_msg->type = CHAT__MESSAGE_TYPE__BROADCAST; // Indicar que es un mensaje general

            // Establecer el mensaje de respuesta en el campo correspondiente
            response.incoming_message = incoming_msg;

            // Empaquetar y enviar el mensaje de respuesta
            uint8_t buffer[BUFFER_SIZE];
            unsigned len = chat__response__pack(&response, buffer);
            if (send(clients[i]->socket, buffer, len, 0) < 0) {
                perror("Send failed");
            } else {
                printf("General message sent from %s\n", sender->username);
            }

            // Liberar la memoria utilizada
            free(incoming_msg->sender);
            free(incoming_msg->content);
            free(incoming_msg);
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->username, sender->username) == 0) {
            // Crear un mensaje de respuesta
            Chat__Response response = CHAT__RESPONSE__INIT;
            response.operation = CHAT__OPERATION__SEND_MESSAGE;
            response.status_code = CHAT__STATUS_CODE__OK;
            response.message = "Message delivered successfully";

            // Empaquetar y enviar el mensaje de respuesta
            uint8_t buffer[BUFFER_SIZE];
            unsigned len = chat__response__pack(&response, buffer);
            if (send(clients[i]->socket, buffer, len, 0) < 0) {
                perror("Send failed");
            } else {
                printf("Confirmation sent to %s\n", sender->username);
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}



void *handle_client(void *arg) {
    client_t *cli = (client_t *)arg;
    uint8_t buffer[BUFFER_SIZE];
    int n;

    while ((n = recv(cli->socket, buffer, BUFFER_SIZE, 0)) > 0) {
        printf("Received message from client %s: %.*s\n", cli->username, n, buffer); // Depuración

        Chat__Request *request = chat__request__unpack(NULL, n, buffer);
        if (!request) {
            printf("Failed to unpack request from client %s\n", cli->username); // Depuración
            continue;
        }

        printf("Request unpacked, operation: %d\n", request->operation); // Depuración

        switch (request->operation) {
            case CHAT__OPERATION__REGISTER_USER:
                // Este caso ya está manejado en la función main.
                break;
            case CHAT__OPERATION__GET_USERS:
                /**printf("Handling GET_USERS request\n"); // Depuración
                list_connected_users(cli->socket);
                break;*/
                printf("Handling GET_USERS request\n"); // Depuración

                const char *username = request->get_users->username;
                if (username && strlen(username) > 0) {
                    // Solicitud para obtener información de un usuario específico
                    printf("Handling GET_USER_INFO request for user: %s\n", username);

                    Chat__Response response = CHAT__RESPONSE__INIT;
                    response.operation = CHAT__OPERATION__GET_USERS;

                    // Buscar la información del usuario solicitado
                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i] && strcmp(clients[i]->username, username) == 0) {
                            // Usuario encontrado, devolver su información
                            response.status_code = CHAT__STATUS_CODE__OK;
                            response.message = "User information retrieved successfully";

                            Chat__UserListResponse user_info = CHAT__USER_LIST_RESPONSE__INIT;
                            user_info.type = CHAT__USER_LIST_TYPE__SINGLE;
                            user_info.n_users = 1;
                            user_info.users = malloc(sizeof(Chat__User*));

                            Chat__User *user = malloc(sizeof(Chat__User));
                            chat__user__init(user);
                            user->username = strdup(clients[i]->username);
                            user->status = clients[i]->status;
                            user_info.users[0] = user;

                            response.result_case = CHAT__RESPONSE__RESULT_USER_LIST;
                            response.user_list = &user_info;

                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);

                    if (response.status_code == CHAT__STATUS_CODE__OK) {
                        unsigned len = chat__response__pack(&response, buffer);
                        send(cli->socket, buffer, len, 0);
                    } else {
                        response.status_code = CHAT__STATUS_CODE__BAD_REQUEST;
                        response.message = "User not found";
                        unsigned len = chat__response__pack(&response, buffer);
                        send(cli->socket, buffer, len, 0);
                    }
                } else {
                    // Solicitud para obtener la lista de todos los usuarios conectados
                    list_connected_users(cli->socket);
                }
                break;
            case CHAT__OPERATION__SEND_MESSAGE:
                if (request->send_message->recipient != NULL && strlen(request->send_message->recipient) > 0) {
                    printf("Handling SEND_MESSAGE request\n");
                    send_private_message(cli, request->send_message);
                } else {
                    printf("Handling SEND_GENERAL_MESSAGE request\n");
                    send_general_message(cli, request->send_message);
                }
                break;
            case CHAT__OPERATION__UPDATE_STATUS:
                printf("Handling UPDATE_STATUS request\n"); // Depuración
                update_user_status(request->update_status);
                {
                    Chat__Response response = CHAT__RESPONSE__INIT;
                    response.operation = CHAT__OPERATION__UPDATE_STATUS;
                    response.status_code = CHAT__STATUS_CODE__OK;
                    response.message = "User status updated successfully";
                    unsigned len = chat__response__pack(&response, buffer);
                    send(cli->socket, buffer, len, 0);
                }
                // ...
                break;
            case CHAT__OPERATION__UNREGISTER_USER:
                printf("Handling UNREGISTER_USER request for client %s\n", cli->username);
                
                Chat__Response response = CHAT__RESPONSE__INIT;
                response.operation = CHAT__OPERATION__UNREGISTER_USER;
                response.status_code = CHAT__STATUS_CODE__OK;
                response.message = "User unregistered successfully";
                unsigned len = chat__response__pack(&response, buffer);

                if (send(cli->socket, buffer, len, 0) < 0) {
                    perror("Send failed");
                } else {
                    printf("Unregister response sent successfully\n");
                }

                close(cli->socket);
                remove_client(cli);
                free(cli);
                pthread_exit(NULL);
                break;
            default:
                printf("Unknown operation: %d\n", request->operation); // Depuración
                // Operación no válida, enviar respuesta de error al cliente
                break;
        }

        chat__request__free_unpacked(request, NULL);
    }

    close(cli->socket);
    remove_client(cli);
    free(cli);
    return NULL;
}

int username_exists(const char *username) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i] && strcmp(clients[i]->username, username) == 0) {
            pthread_mutex_unlock(&clients_mutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int server_port = atoi(argv[1]);
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len;
    //pthread_t tid;

    // Create socket
    if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    // Bind socket
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return 1;
    }

    // Listen on the socket
    if (listen(server_socket, 5) < 0) {
        perror("Listen failed");
        return 1;
    }

    printf("Server listening on port %d\n", server_port);

    while (1) {
        client_len = sizeof(client_addr);
        if ((client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len)) < 0) {
            perror("Accept failed");
            continue;
        }
        // Crear una nueva instancia de client_t para almacenar la información del cliente
        client_t *new_client = (client_t *)malloc(sizeof(client_t));
        new_client->socket = client_socket;

        // Obtener la dirección IP del cliente
        inet_ntop(AF_INET, &(client_addr.sin_addr), new_client->ip_address, INET_ADDRSTRLEN);

        printf("New client connected with IP: %s\n", new_client->ip_address); // Mensaje de depuración

        uint8_t buffer[BUFFER_SIZE];
        int n = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (n <= 0) {
            perror("Initial receive failed"); // Depuración
            close(client_socket);
            continue;
        }

        printf("Received initial request from client, length: %d\n", n); // Depuración

        Chat__Request *request = chat__request__unpack(NULL, n, buffer);
        if (!request) {
            printf("Failed to unpack initial request\n"); // Depuración
            close(client_socket);
            continue;
        }

        switch (request->operation) {
            case CHAT__OPERATION__REGISTER_USER:
                {
                    const char *username = request->register_user->username;
                    printf("Registering user: %s\n", username); // Depuración
                    if (username_exists(username)) {
                        Chat__Response response = CHAT__RESPONSE__INIT;
                        response.operation = CHAT__OPERATION__REGISTER_USER;
                        response.status_code = CHAT__STATUS_CODE__BAD_REQUEST;
                        response.message = "Username already exists";
                        unsigned len = chat__response__pack(&response, buffer);
                        send(client_socket, buffer, len, 0);
                        close(client_socket);
                    } else {
                        pthread_t tid;
                        client_t *cli = (client_t *)malloc(sizeof(client_t));
                        cli->socket = client_socket;
                        strncpy(cli->username, username, sizeof(cli->username));
                        add_client(cli);

                        Chat__Response response = CHAT__RESPONSE__INIT;
                        response.operation = CHAT__OPERATION__REGISTER_USER;
                        response.status_code = CHAT__STATUS_CODE__OK;
                        response.message = "User registered successfully";
                        unsigned len = chat__response__pack(&response, buffer);
                        send(client_socket, buffer, len, 0);

                        if (pthread_create(&tid, NULL, &handle_client, (void*)cli) != 0) {
                            perror("Thread creation failed");
                            free(cli);
                        }
                        pthread_detach(tid);
                    }
                }
                break;
            case CHAT__OPERATION__GET_USERS:
                printf("Handling GET_USERS request\n"); // Depuración
                list_connected_users(client_socket);
                break;
            case CHAT__OPERATION__SEND_MESSAGE:
                {
                    const char *recipient = request->send_message->recipient;
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i] && strcmp(clients[i]->username, recipient) == 0) {
                            send_private_message(clients[i], request->send_message);
                            break;
                        }
                    }
                }
                break;
            case CHAT__OPERATION__UPDATE_STATUS:
                // Manejar solicitud de actualizar estado de usuario
                // ...
                break;
            case CHAT__OPERATION__UNREGISTER_USER:
                {
                    const char *username = request->unregister_user->username;
                    printf("Handling UNREGISTER_USER request for client %s\n", username);

                    pthread_mutex_lock(&clients_mutex);
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i] && strcmp(clients[i]->username, username) == 0) {
                            // Cerrar el socket y eliminar al cliente
                            close(clients[i]->socket);
                            free(clients[i]);
                            clients[i] = NULL;
                            break;
                        }
                    }
                    pthread_mutex_unlock(&clients_mutex);

                    Chat__Response response = CHAT__RESPONSE__INIT;
                    response.operation = CHAT__OPERATION__UNREGISTER_USER;
                    response.status_code = CHAT__STATUS_CODE__OK;
                    response.message = "User unregistered successfully";
                    unsigned len = chat__response__pack(&response, buffer);
                    send(client_socket, buffer, len, 0);
                }
                break;
            default:
                printf("Unknown initial operation: %d\n", request->operation); // Depuración
                // Operación no válida, enviar respuesta de error al cliente
                break;
        }

        chat__request__free_unpacked(request, NULL);
    }

    close(server_socket);
    return 0;
}
