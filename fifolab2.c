#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>

#define FIFO_PEDIDOS "fifo_pedidos"
#define FIFO_RESPUESTAS "fifo_respuestas"
#define MAX_MENSAJE 128

typedef struct {
    int cliente_id;
    char mensaje[MAX_MENSAJE];
} Pedido;

typedef struct {
    int cliente_id;
    int estado; // 1: recibido, 2: listo
    char respuesta[MAX_MENSAJE];
} Confirmacion;

void crear_fifos() {
    if (access(FIFO_PEDIDOS, F_OK) == -1) {
        if (mkfifo(FIFO_PEDIDOS, 0666) == -1) {
            perror("No se pudo crear FIFO_PEDIDOS");
            exit(EXIT_FAILURE);
        }
    }

    if (access(FIFO_RESPUESTAS, F_OK) == -1) {
        if (mkfifo(FIFO_RESPUESTAS, 0666) == -1) {
            perror("No se pudo crear FIFO_RESPUESTAS");
            exit(EXIT_FAILURE);
        }
    }
}

void eliminar_fifos(int sig) {
    unlink(FIFO_PEDIDOS);
    unlink(FIFO_RESPUESTAS);
    printf("\nFIFOs eliminados correctamente. Saliendo...\n");
    exit(0);
}

void cliente(int id) {
    Pedido p;
    Confirmacion c;
    int fd_envio, fd_recep;

    p.cliente_id = id;

    printf("Cliente %d - Escriba su pedido: ", id);
    fgets(p.mensaje, MAX_MENSAJE, stdin);
    p.mensaje[strcspn(p.mensaje, "\n")] = '\0';  // eliminar salto de línea

    fd_envio = open(FIFO_PEDIDOS, O_WRONLY);
    if (fd_envio == -1) {
        perror("No se pudo abrir FIFO_PEDIDOS");
        exit(EXIT_FAILURE);
    }

    write(fd_envio, &p, sizeof(Pedido));
    close(fd_envio);

    fd_recep = open(FIFO_RESPUESTAS, O_RDONLY);
    if (fd_recep == -1) {
        perror("No se pudo abrir FIFO_RESPUESTAS");
        exit(EXIT_FAILURE);
    }

    while (read(fd_recep, &c, sizeof(Confirmacion)) > 0) {
        if (c.cliente_id == id && c.estado == 1) {
            printf("Cliente %d - Confirmación de recepción: %s\n", id, c.respuesta);
            break;
        }
    }
    close(fd_recep);

    // Esperar segundo mensaje (preparado)
    fd_recep = open(FIFO_RESPUESTAS, O_RDONLY);
    if (fd_recep == -1) {
        perror("No se pudo abrir FIFO_RESPUESTAS");
        exit(EXIT_FAILURE);
    }

    while (read(fd_recep, &c, sizeof(Confirmacion)) > 0) {
        if (c.cliente_id == id && c.estado == 2) {
            printf("Cliente %d - Pedido listo: %s\n", id, c.respuesta);
            break;
        }
    }
    close(fd_recep);
}

void cocina() {
    Pedido p;
    Confirmacion c;
    int fd_entrada, fd_salida;

    crear_fifos();
    signal(SIGINT, eliminar_fifos);

    printf("📦 Cocina activa. Esperando pedidos...\n");

    while (1) {
        fd_entrada = open(FIFO_PEDIDOS, O_RDONLY);
        if (fd_entrada == -1) {
            perror("Error al abrir FIFO_PEDIDOS");
            sleep(1);
            continue;
        }

        if (read(fd_entrada, &p, sizeof(Pedido)) == sizeof(Pedido)) {
            printf("🍳 Pedido recibido de Cliente %d: %s\n", p.cliente_id, p.mensaje);
            close(fd_entrada);

            // Confirmación de recepción
            c.cliente_id = p.cliente_id;
            c.estado = 1;
            snprintf(c.respuesta, MAX_MENSAJE, "Tu pedido de '%.90s' fue recibido.", p.mensaje);

            fd_salida = open(FIFO_RESPUESTAS, O_WRONLY);
            if (fd_salida != -1) {
                write(fd_salida, &c, sizeof(Confirmacion));
                close(fd_salida);
            }

            // Simular preparación
            sleep(3);

            // Confirmación de preparación
            c.estado = 2;
            snprintf(c.respuesta, MAX_MENSAJE, "¡'%.90s' está listo para recoger!", p.mensaje);

            fd_salida = open(FIFO_RESPUESTAS, O_WRONLY);
            if (fd_salida != -1) {
                write(fd_salida, &c, sizeof(Confirmacion));
                close(fd_salida);
            }

            printf("✅ Pedido listo: Cliente %d fue notificado.\n", p.cliente_id);
        } else {
            close(fd_entrada);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Uso: %s [cocina | ID_cliente]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (strcmp(argv[1], "cocina") == 0) {
        cocina();
    } else {
        int id = atoi(argv[1]);
        if (id <= 0) {
            fprintf(stderr, "ID de cliente inválido.\n");
            return EXIT_FAILURE;
        }
        cliente(id);
    }

    return EXIT_SUCCESS;
}

