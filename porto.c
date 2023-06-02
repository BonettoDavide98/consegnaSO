#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include "merce.h"

int port_id;
int master_msgq;
int num_merci;
int *shm_ptr_req;
struct merce *shm_ptr_aval;
int docks;
int port_sem_id;
int * spoiled;
int day = 0;

void removeSpoiled(struct merce *);
void reporthandler();
void endreporthandler();


int main (int argc, char * argv[]) {
	struct mesg_buffer message; 
	struct sembuf sops;
	struct merce *available;
	struct merce *requested;
	struct position pos;
	int shm_id_aval, shm_id_req;
	key_t mem_key;
	char ship_id[30];
	char operation[20];
	char text[20];
	int i;
	int master_sem_id = atoi(argv[2]);
	int msgq_porto = atoi(argv[3]);
	int fill = atoi(argv[9]);
	int loadtime = atoi(argv[10]);
	port_id = atoi(argv[4]);
	docks = atoi(argv[7]);
	num_merci = atoi(argv[11]);
	master_msgq = atoi(argv[12]);
	spoiled = malloc((1 +num_merci) * sizeof(int));


	/*setup signal handlers*/
	signal(SIGUSR2, reporthandler);
	signal(SIGINT, endreporthandler);

	/*setup shared memory access*/
	if((int) (shm_id_aval = atoi(argv[1])) < 0) {
		printf("*** shmget error porto aval ***\n");
		exit(1);
	}
	if((struct merce *) (shm_ptr_aval = (struct merce *) shmat(shm_id_aval, NULL, 0)) == -1) {
		printf("*** shmat error porto aval ***\n");
		exit(1);
	}
	if((int) (shm_id_req = atoi(argv[8])) < 0) {
		printf("*** shmget error porto req ***\n");
		exit(1);
	}
	if((int *) (shm_ptr_req = (int *) shmat(shm_id_req, NULL, 0)) == -1) {
		printf("*** shmat error porto req ***\n");
		exit(1);
	}

	/*initialize spoiled*/
	for(i = 0; i < num_merci + 1; i++) {
		spoiled[i] = 0;
	}

	/*create semaphore to oversee shared memory writing*/
	port_sem_id = semget(IPC_PRIVATE, 1, 0600);
	semctl(port_sem_id, 0, SETVAL, 0);
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = docks;
	semop(port_sem_id, &sops, 1);

	/*wait until parent unlocks semaphore*/
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = -1;
	semop(master_sem_id, &sops, 1);

	/*start handling ships*/

	while(1) {
		while(msgrcv(msgq_porto, &message, (sizeof(long) + sizeof(char) * 30), 1, 0) == -1) {
			/*loop until message is received*/
		}
		strcpy(operation, strtok(message.mesg_text, ":"));
		strcpy(ship_id, strtok(NULL, ":"));
		removeSpoiled(shm_ptr_aval);

		if(strcmp(operation, "dockrq") == 0) {
			strcpy(message.mesg_text, "accept");
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", shm_id_req);
			strcat(message.mesg_text, text);
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", shm_id_aval);
			strcat(message.mesg_text, text);
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", loadtime);
			strcat(message.mesg_text, text);
			strcat(message.mesg_text, ":");
			sprintf(text, "%d", port_sem_id);
			strcat(message.mesg_text, text);
			msgsnd(atoi(ship_id), &message, (sizeof(long) + sizeof(char) * 50), 0);
			removeSpoiled(shm_ptr_aval);
		}
	}

	exit(0);
}

void removeSpoiled(struct merce *available) {
	int i;
	for(i = 0; i < num_merci; i++) {
		if(available[i].type > 0 && available[i].qty > 0) {
			if(available[i].spoildate < day) {
				spoiled[available[i].type] += available[i].qty;
				available[i].type = -1;
				available[i].qty = 0;
			}
		}
	}
}

void reporthandler() {
	struct mesg_buffer message;
	char temp[20];
	int tot = 0;
	int i;

	message.mesg_type = 1;
	day++;

	removeSpoiled(shm_ptr_aval);
	strcpy(message.mesg_text, "p");
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", port_id);		/*port id*/
	strcat(message.mesg_text, temp);
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", day);			/*current day*/
	strcat(message.mesg_text, temp);
	strcat(message.mesg_text, ":");
	for(i = (num_merci * 2) + 1; i <= (num_merci * 3); i++) {
		tot += shm_ptr_req[i];
	}
	sprintf(temp, "%d", tot);
	strcat(message.mesg_text, temp);
	tot = 0;
	strcat(message.mesg_text, ":");		/*merce sent*/
	for(i = num_merci + 1; i <= (num_merci * 2); i++) {
		tot += shm_ptr_req[i];
	}
	sprintf(temp, "%d", tot);
	strcat(message.mesg_text, temp);	/*merce received*/
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", docks);
	strcat(message.mesg_text, temp);	/*total docks*/
	strcat(message.mesg_text, ":");
	sprintf(temp, "%d", docks - semctl(port_sem_id, 0, GETVAL));
	strcat(message.mesg_text, temp);	/*occupied docks*/

	msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 50), 0);
}

void endreporthandler() {
	struct mesg_buffer message;
	char temp[20];
	struct timespec sleep;
	int i;
	sleep.tv_sec = 0;
	sleep.tv_nsec = 100;
	message.mesg_type = 1;

	for(i = 1; i < num_merci + 1; i++) {
		if(spoiled[i] > 0) {
			strcpy(message.mesg_text, "P");
			strcat(message.mesg_text, ":");
			sprintf(temp, "%d", i);
			strcat(message.mesg_text, temp);
			strcat(message.mesg_text, ":");
			sprintf(temp, "%d", spoiled[i]);
			strcat(message.mesg_text, temp);
			msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 15), 0);
			nanosleep(&sleep, &sleep);
		}
	}
	strcpy(message.mesg_text, "P:end");
	msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 10), 0);

	exit(0);
}