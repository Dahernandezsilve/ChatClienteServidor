# ChatClienteServidor
Proyecto 1 Sistemas Operativos

Este proyecto es un chat hecho a base de multithreading, broadcasting, uso de sockets y de un protocolo definido entre todo el curso para poder realizar comunicación entre chats. Fue programado con el lenguaje C.

Para ejecutar este proyecto, es necesario tener instalados la librería de build-essential de Linux, así como también la librería de protobuf-c.

Para poder instalar estas librerías en tu terminal Linux, estos son los comandos:

* sudo apt-get install -y protobuf-c-compiler protobuf-compiler libprotobuf-c-dev
* sudo apt-get install build-essential

Luego de esto y al tener tu proyecto en tu máquina virtual, procede a ejecutar el archivo Makefile que se encuentra en el proyecto con "make".

Con esto hecho, necesitas mínimo dos terminales para ejecutar y probar el proyecto. Una es para levantar el servidor y otra es para el cliente.

Para levantar el servidor, debes ejecutar el comando "./servidor 'número de puerto en el que deseas levantarlo' ".
Mientras que, para ejecutar el cliente y conectarte al servidor, debes ejecutar el comando "./cliente 'nombre de usuario' 'ip' 'puerto' ".

Una vez hecho esto y si no hubo inconvenientes, habrás ingresado a tu propio chat como el cliente a tu servidor! En este se encuentran 7 funcionalidades clave.

![image](https://github.com/Dahernandezsilve/ChatClienteServidor/assets/88167635/9752cb46-2132-4548-807c-ecc3b7beabfc)
![image](https://github.com/Dahernandezsilve/ChatClienteServidor/assets/88167635/b60f1275-deb1-404d-90eb-b2de5c010c8e)

Estas son: send, general, list, info, status, help, exit.

### Send

* Envía mensajes privados a algún otro usuario conectado. Solo colocas el receptor y el mensaje a enviar.

### General

* Chat general, ingresas a otra parte del chat en la que puedes usar send para enviar un mensaje a todos o exit para regresar al lobby.

### List

* Lista de usuarios conectados en el servidor, como también su username y status.

### Info

* Muestra la información de un usuario conectado, basta con colocar su username para recibir su información.

### Status

* Te permite cambiar de status de cara a los demás usuarios y a ti. 0 es online (por defecto), 1 es busy o ocupado, 2 es offline.

### Help

* El comando de ayuda para ver más información de los comandos disponibles.

### Exit

* El unregister o liberación del usuario, es para desloguearte del chat.

Con todo esto, ya podrás utilizar el chat de buena manera y poder hablar con tus amigos a través de él. Disfruta del proyecto! :D
