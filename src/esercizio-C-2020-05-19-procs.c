#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <errno.h>
#include <semaphore.h>

#define N 10
#define COUNTDOWN  100000

#define CHECK_ERR(a,msg) {if ((a) == -1) { perror((msg)); exit(EXIT_FAILURE); } }
#define CHECK_ERR_MMAP(a,msg) {if ((a) == MAP_FAILED) { perror((msg)); exit(EXIT_FAILURE); } }

typedef struct {
	sem_t mutex; // semaforo per implementare il meccanismo di mutual exclusion (mutex)
	int countdown; // variabile condivisa
	int process_counter[N];
	int shutdown;
} shared_var;

shared_var *shared;

void child_process(int indice) {

	if (shared->shutdown != 0) {
		printf("[child] con pid = %d EXIT_SUCCESS \n", getpid());
		exit(EXIT_SUCCESS);
	}

//	printf("[child] start con pid = %d countdown = %d \n", getpid(), shared->countdown);
	if (sem_wait(&shared->mutex) == -1) {
		perror("sem_wait");
		exit(EXIT_FAILURE);
	}
	//se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]

	if (shared->countdown > 0) {
		shared->countdown--;
//		printf("[child] pid = %d countdown = %d \n", getpid(),
//				shared->countdown);
		shared->process_counter[indice]++;
	}

	if (sem_post(&shared->mutex) == -1) {
		perror("sem_post");
		exit(EXIT_FAILURE);
	}
}

int main(void) {
	int res;
	pid_t pid;

	//Preparo la memoria condivisa con le variabili e il mutex

	shared = mmap(NULL, // NULL: è il kernel a scegliere l'indirizzo
			sizeof(shared_var), // dimensione della memory map
			PROT_READ | PROT_WRITE, // memory map leggibile e scrivibile
			MAP_SHARED | MAP_ANONYMOUS, // memory map condivisibile con altri processi e senza file di appoggio
			-1, // nessun file di appoggio alla memory map
			0); // offset nel file

	CHECK_ERR_MMAP(shared, "mmap")

	res = sem_init(&shared->mutex, 1, // 1 => il semaforo è condiviso tra processi, 0 => il semaforo è condiviso tra threads del processo
			1 // valore iniziale del semaforo (se mettiamo 0 che succede?)
			);

	CHECK_ERR(res, "sem_init")

	// voglio creare N processi figli

	for (int i = 0; i < N; i++) {
		pid = fork();
		CHECK_ERR(pid, "fork")

		switch (pid) {
		case 0: // child process
			printf("[child] start with pid = %d\n", getpid());
			while (shared->countdown == 0) {
				//printf("[child] con pid = %d in ATTESA \n", getpid());
			}
			for (;;)
				child_process(i);
			break;
		case -1:
			perror("fork");
			exit(EXIT_FAILURE);
		default:
			;
		}
	}
	// Processo padre dorme 1 secondo
	sleep(1);

	shared->countdown = COUNTDOWN;
	//printf("SLEEP OUT ------->  shared->countdown = %d\n", shared->countdown);

	while (shared->shutdown == 0) {
		if (shared->countdown == 0) {
			printf("**S**H**U**T**D**O**W**N**\n");
			shared->shutdown += 1;
		}
	}
	//printf("SHUTDOWN = %d\n", shared->shutdown);

	for (int k = 0; k < N; k++) {
		res = wait(NULL); // Aspetto tutti i processi
	}


	printf("process_counter : \n");
	for (int j = 0; j < N; j++) {
		printf("process_counter [%d] = %d\n", j, shared->process_counter[j]);
	}

	res = sem_destroy(&shared->mutex);
	CHECK_ERR(res, "sem_destroy")

	printf("COUNTDOWN = %d, SHUTDOWN = %d\n", shared->countdown,shared->shutdown);
	printf("bye!\n");

	return 0;
}

/*
 * N = 10
 un processo padre crea N processi figli ( https://repl.it/@MarcoTessarotto/crea-n-processi-figli )
 shared variables: countdown, process_counter[N], shutdown
 usare mutex per regolare l'accesso concorrente a countdown
 dopo avere avviato i processi figli, il processo padre dorme 1 secondo
 poi imposta il valore di countdown al valore 100000.
 quando countdown == 0, il processo padre imposta shutdown a 1.
 aspetta che terminino tutti i processi figli e poi stampa su stdout process_counter[].
 i processi figli "monitorano" continuamente countdown:
 processo i-mo: se countdown > 0, allora decrementa countdown ed incrementa process_counter[i]
 se shutdown != 0, processo i-mo termina
 * */
