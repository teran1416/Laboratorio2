// lab2_shm_v2.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <time.h>

#define MAX_PEDIDOS 5
#define PEDIDO_LEN 64

typedef struct {
    int cliente_id;
    char pedido[PEDIDO_LEN];
    int estado; // 0: libre, 1: recibido, 2: preparado
} Pedido;

typedef struct {
    Pedido cola[MAX_PEDIDOS];
    int head;
    int tail;
} BufferCompartido;

#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

#define SEM_MUTEX 0
#define SEM_LIBRES 1
#define SEM_OCUPADOS 2

int sem_id;

// ============================
// Utilidades
// ============================

void hora_actual(char *dest, size_t size) {
    time_t t = time(NULL);
    strftime(dest, size, "%H:%M:%S", localtime(&t));
}

void sem_wait(int sem_num) {
    struct sembuf op = {sem_num, -1, 0};
    semop(sem_id, &op, 1);
}

void sem_signal(int sem_num) {
    struct sembuf op = {sem_num, +1, 0};
    semop(sem_id, &op, 1);
}

void init_semaforos() {
    sem_id = semget(SEM_KEY, 3, IPC_CREAT | 0666);
    semctl(sem_id, SEM_MUTEX, SETVAL, 1);
    semctl(sem_id, SEM_LIBRES, SETVAL, MAX_PEDIDOS);
    semctl(sem_id, SEM_OCUPADOS, SETVAL, 0);
}

// ============================
// Cliente
// ============================

void cliente(int id) {
    sem_id = semget(SEM_KEY, 3, 0666);
    int shm_id = shmget(SHM_KEY, sizeof(BufferCompartido), 0666);
    BufferCompartido *buffer = (BufferCompartido *) shmat(shm_id, NULL, 0);

    char comida[PEDIDO_LEN];
    printf("Cliente %d - ¿Qué desea pedir? ", id);
    fgets(comida, PEDIDO_LEN, stdin);
    comida[strcspn(comida, "\n")] = '\0';

    sem_wait(SEM_LIBRES);
    sem_wait(SEM_MUTEX);

    int pos = buffer->tail;

    buffer->cola[pos].cliente_id = id;
    strncpy(buffer->cola[pos].pedido, comida, PEDIDO_LEN);
    buffer->cola[pos].estado = 1; // recibido

    char hora[9];
    hora_actual(hora, sizeof(hora));
    printf("Cliente %d - Pedido '%s' enviado a cocina [%s]\n", id, comida, hora);

    buffer->tail = (buffer->tail + 1) % MAX_PEDIDOS;

    sem_signal(SEM_MUTEX);
    sem_signal(SEM_OCUPADOS);

    // Esperar hasta que el pedido esté listo
    int encontrado = 0;
    while (!encontrado) {
        sem_wait(SEM_MUTEX);
        for (int i = 0; i < MAX_PEDIDOS; i++) {
            if (buffer->cola[i].cliente_id == id && buffer->cola[i].estado == 2) {
                encontrado = 1;
                hora_actual(hora, sizeof(hora));
                printf("Cliente %d - Pedido '%s' está listo [%s]\n", id, buffer->cola[i].pedido, hora);
                buffer->cola[i].estado = 0; // liberar slot
                break;
            }
        }
        sem_signal(SEM_MUTEX);
        if (!encontrado) sleep(1);
    }

    shmdt(buffer);
}

// ============================
// Cocina
// ============================

void cocina() {
    int shm_id = shmget(SHM_KEY, sizeof(BufferCompartido), IPC_CREAT | 0666);
    BufferCompartido *buffer = (BufferCompartido *) shmat(shm_id, NULL, 0);

    init_semaforos();

    buffer->head = 0;
    buffer->tail = 0;

    printf("🍳 Cocina activa. Esperando pedidos...\n");

    while (1) {
        sem_wait(SEM_OCUPADOS);
        sem_wait(SEM_MUTEX);

        int pos = buffer->head;
        Pedido *p = &buffer->cola[pos];

        if (p->estado == 1) {
            char hora[9];
            hora_actual(hora, sizeof(hora));
            printf("🍽️  Preparando pedido de Cliente %d: %s [%s]\n", p->cliente_id, p->pedido, hora);

            p->estado = 1; // aún en preparación
            sem_signal(SEM_MUTEX);

            sleep(3); // Simula tiempo de preparación

            sem_wait(SEM_MUTEX);
            p->estado = 2; // preparado
            hora_actual(hora, sizeof(hora));
            printf("✅ Pedido de Cliente %d listo [%s]\n", p->cliente_id, hora);
        }

        buffer->head = (buffer->head + 1) % MAX_PEDIDOS;

        sem_signal(SEM_MUTEX);
        sem_signal(SEM_LIBRES);
    }

    shmdt(buffer);
}

// ============================
// Main
// ============================

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Uso: %s [cliente ID | cocina]\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "cocina") == 0) {
        cocina();
    } else {
        int id = atoi(argv[1]);
        if (id <= 0) {
            printf("ID de cliente inválido.\n");
            return 1;
        }
        cliente(id);
    }

    return 0;
}

