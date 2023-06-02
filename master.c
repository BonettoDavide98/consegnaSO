#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <sys/sem.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <sys/resource.h>
#include "merce.h"

#define MAX_DAYS 30
#define NAVE "nave"
#define PORTO "porto"
#define TIMER "timer"

int main (int argc, char ** argv) {
	/*read parameters from file*/
	FILE *inputfile;
	struct parameters parameters;
	struct report reports[MAX_DAYS + 1];
	int i, j;
	int day = 0;
	struct mesg_buffer message;
	int sem_id, status;
	pid_t child_pid, *kid_pids;
	struct sembuf sops;
    char *args[15];
	char *argss[12];
	char *argst[8];
	int master_msgq;
	int *ports_shm_id_aval;
	struct merce **ports_shm_ptr_aval;
	int *ports_shm_id_req;
	int **ports_shm_ptr_req;
	struct position *ports_positions;
	int *msgqueue_porto;
	int *msgqueue_nave;
	int a,b,tot,temp;
	int * totalgenerated;
	int *temparray;
	char idin[10];
	char posx_str[20];
	char posy_str[20];
	char merce[20];
	int idfind;
	char x[20];
	char y[20];
	char dayr[3];
	char tempstr[20];
	char tempstr2[20];
	char portid[20];
	int * spoilednave;
	int * spoiledporto;
	int num_kid_pids_navi;
	int num_kid_pids_porti;
	int * totalsent;
	int * totaldelivered;
	int * totalport;
	int * mostavailable;
	int * mostrequested;

	int allinport = 0;
	int allinship = 0;
	int alldelivered = 0;
	int allspoiledport = 0;
	int allspoiledship = 0;

	int flag = 1;
	int timeended = 0;
	int allrequestfulfilled = 0;
	int allmerciempty = 0;

	srand(time(NULL));

	if(argc == 2) {
		if((inputfile = fopen(argv[1], "r")) == NULL) {
			printf("Error in opening file\n");
			return(1);
		}
		if(read_parameters_from_file(inputfile, &parameters) == 0) {
			printf("Invalid parameters");
		}
	} else {
		printf("Usage: master <filepath>\n");
		return(1);
	}

	for(i = 0; i < MAX_DAYS + 1; i++) {
		reports[i].seawithcargo = 0;
		reports[i].seawithoutcargo = 0;
		reports[i].docked = 0;
		reports[i].ports = malloc(parameters.SO_PORTI * sizeof(struct portstatus));
		for(j = 0; j < parameters.SO_PORTI; j++) {
			reports[i].ports[j].mercepresent = 0;
			reports[i].ports[j].mercesent = 0;
			reports[i].ports[j].mercereceived = 0;
			reports[i].ports[j].docksocc = 0;
			reports[i].ports[j].dockstot = 0;
		}
	}

	ports_shm_id_aval = malloc(parameters.SO_PORTI * sizeof(int));
	ports_shm_ptr_aval = malloc(parameters.SO_PORTI * sizeof(ports_shm_ptr_aval));
	ports_shm_id_req = malloc(parameters.SO_PORTI * sizeof(int));
	ports_shm_ptr_req = malloc(parameters.SO_PORTI * sizeof(ports_shm_ptr_req));
	ports_positions = malloc(parameters.SO_PORTI * sizeof(struct position));
	msgqueue_porto = malloc(parameters.SO_PORTI * sizeof(int));
	msgqueue_nave = malloc(parameters.SO_NAVI * sizeof(int));

	for(i = 0; i < 15; i++) {
		args[i] = malloc(20);
	}

	for(i = 0; i < 12; i++) {
		argss[i] = malloc(20);
	}

	for(i = 0; i < 8; i++) {
		argst[i] = malloc(20);
	}

	sem_id = semget(IPC_PRIVATE, 1, 0600);
	semctl(sem_id, 0, SETVAL, 0);

	kid_pids = malloc((parameters.SO_PORTI + parameters.SO_NAVI + 1) * sizeof(*kid_pids));

	if((master_msgq = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
		printf("*** master msgqueue error ***\n");
		exit(1);
	}

	ports_positions[0].x = 0;
	ports_positions[0].y = 0;

	ports_positions[1].x = parameters.SO_LATO;
	ports_positions[1].y = 0;

	ports_positions[2].x = 0;
	ports_positions[2].y = parameters.SO_LATO;

	ports_positions[3].x = parameters.SO_LATO;
	ports_positions[3].y = parameters.SO_LATO;

	totalgenerated = malloc((1 + parameters.SO_MERCI) * sizeof(int));
	mostavailable = malloc((1 + parameters.SO_MERCI * 2) * sizeof(int));
	mostrequested = malloc((1 + parameters.SO_MERCI * 2) * sizeof(int));
	for(i = 0; i < parameters.SO_MERCI + 1; i++) {
		totalgenerated[i] = 0;
	}
	for(i = 0; i < parameters.SO_MERCI * 2 + 1; i++) {
		mostavailable[i] = 0;
		mostrequested[i] = 0;
	}

	/*create ports*/
	for(i = 0; i < parameters.SO_PORTI; i++) {
		if((int) (ports_shm_id_aval[i]  = shmget(IPC_PRIVATE, sizeof(int), 0600)) < 0) {
			printf("*** shmget aval error ***\n");
			exit(1);
		}

		ports_shm_ptr_aval[i] = malloc((parameters.SO_MERCI) * sizeof(struct merce));
		if((struct merce *) (ports_shm_ptr_aval[i] = (struct merce *) shmat(ports_shm_id_aval[i], NULL, 0)) == -1) {
			printf("*** shmat aval error ***\n");
			exit(1);
		}

		if((int) (ports_shm_id_req[i]  = shmget(IPC_PRIVATE, sizeof(int), 0600)) < 0) {
			printf("*** shmget req error ***\n");
			exit(1);
		}

		ports_shm_ptr_req[i] = malloc((parameters.SO_MERCI * 3 + 1) * sizeof(int));
		if((int *) (ports_shm_ptr_req[i] = (int *) shmat(ports_shm_id_req[i], NULL, 0)) == -1) {
			printf("*** shmat req error ***\n");
			exit(1);
		}

		if((msgqueue_porto[i] = msgget(IPC_PRIVATE, 0600)) == -1) {
			printf("*** msgqueue_porto error ***\n");
			exit(1);
		}
		
		if(i > 3) {
			ports_positions[i].x = (rand() / (double)RAND_MAX) * parameters.SO_LATO;
			ports_positions[i].y = (rand() / (double)RAND_MAX) * parameters.SO_LATO;
		}
		
		for(j = 0; j < parameters.SO_MERCI; j++) {
			ports_shm_ptr_aval[i][j].type = 0;
			ports_shm_ptr_aval[i][j].qty = 0;
		}

		for(j = 0; j < parameters.SO_MERCI * 2 + 1; j++) {
			ports_shm_ptr_req[i][j] = 0;
		}
		
		temparray = malloc((parameters.SO_MERCI + 1) * sizeof(int));
		for(j = 0; j < parameters.SO_MERCI + 1; j++) {
			temparray[j] = 0;
		}

		tot = 0;
		
		while(tot + parameters.SO_SIZE <= parameters.SO_FILL/parameters.SO_PORTI) {
			temp = 1 + (rand() % parameters.SO_SIZE);
			temparray[1 + (rand() % parameters.SO_MERCI)] += temp;
			tot += temp;
		}

		a = 0;
		for(j = 1; j < parameters.SO_MERCI + 1; j++) {
			switch(rand() % 2) {
				case 0:
					ports_shm_ptr_aval[i][a].type = j;
					ports_shm_ptr_aval[i][a].qty = temparray[j];
					if(temparray[j] > mostavailable[j + parameters.SO_MERCI]) {
						mostavailable[j + parameters.SO_MERCI] = temparray[j];
						mostavailable[j] = i;
					}
					totalgenerated[j] += temparray[j];
					ports_shm_ptr_aval[i][a].spoildate += parameters.SO_MIN_VITA + (rand() % (parameters.SO_MAX_VITA - parameters.SO_MIN_VITA + 1) - 1);
					a += 1;
					break;
				case 1:
					ports_shm_ptr_req[i][j] = temparray[j];
					if(temparray[j] > mostrequested[j + parameters.SO_MERCI]) {
						mostrequested[j + parameters.SO_MERCI] = temparray[j];
						mostrequested[j] = i;
					}
					break;
			}
		}
		ports_shm_ptr_req[i][0] = a;
		free(temparray);
	}

	/*create ships*/
	for(j = 0; j < parameters.SO_NAVI; j++) {
		if((msgqueue_nave[j] = msgget(IPC_PRIVATE, IPC_CREAT | 0600)) == -1) {
			printf("*** msgqueue error ***\n");
			exit(1);
		}
	}

	/*start port processes*/
	for(i = 0; i < parameters.SO_PORTI; i++) {
		strcpy(args[0], PORTO);
		sprintf(args[1], "%d", ports_shm_id_aval[i]);
		sprintf(args[2], "%d", sem_id);
		sprintf(args[3], "%d", msgqueue_porto[i]);
		sprintf(args[4], "%d", i);
		sprintf(args[5], "%f", ports_positions[i].x);
		sprintf(args[6], "%f", ports_positions[i].y);
		sprintf(args[7], "%d", 1 + (rand() % parameters.SO_BANCHINE));
		sprintf(args[8], "%d", ports_shm_id_req[i]);
		sprintf(args[9], "%d", parameters.SO_FILL);
		sprintf(args[10], "%d", parameters.SO_LOADSPEED);
		sprintf(args[11], "%d", parameters.SO_MERCI);
		sprintf(args[12], "%d", master_msgq);
		sprintf(args[13], "%d", parameters.SO_SWELL_DURATION);
    	args[14] = NULL;

		switch(kid_pids[i] = fork()) {
			case -1:
				break;
			case 0:
    			execve(PORTO, args, NULL);
				break;
			default:
				break;
		}
	}
	
	/*start ship processes*/
	for(j = 0; j < parameters.SO_NAVI; j++) {
		strcpy(argss[0], NAVE);
		sprintf(argss[1], "%d", msgqueue_nave[j]);
		sprintf(argss[2], "%d", j);
		sprintf(argss[3], "%f", (rand() / (double)RAND_MAX) * parameters.SO_LATO);
		sprintf(argss[4], "%f", (rand() / (double)RAND_MAX) * parameters.SO_LATO);
		sprintf(argss[5], "%d", parameters.SO_SPEED);
		sprintf(argss[6], "%d", master_msgq);
		sprintf(argss[7], "%d", parameters.SO_CAPACITY);
		sprintf(argss[8], "%d", parameters.SO_STORM_DURATION);
		sprintf(argss[9], "%d", parameters.SO_MERCI);
		sprintf(argss[10], "%d", sem_id);
		argss[11] = NULL;
		
		switch(kid_pids[parameters.SO_PORTI + j] = fork()) {
			case -1:
				break;
			case 0:
    			execve(NAVE, argss, NULL);
				break;
			default:
				break;
		}
	}

	/*start timer process*/
	strcpy(argst[0], TIMER);
	sprintf(argst[1], "%d", MAX_DAYS);
	sprintf(argst[2], "%d", parameters.SO_NAVI);
	sprintf(argst[3], "%d", parameters.SO_PORTI);
	sprintf(argst[4], "%d", master_msgq);
	sprintf(argst[5], "%d", sem_id);
	sprintf(argst[6], "%d", parameters.SO_MAELSTORM);
	argst[7] = NULL;

	switch(kid_pids[parameters.SO_PORTI + parameters.SO_NAVI] = fork()) {
		case -1:
			break;
		case 0:
    		execve(TIMER, argst, NULL);
			break;
		default:
			break;
	}

	/*start every child simultaneously*/
	sleep(1);
	sops.sem_num = 0;
	sops.sem_flg = 0;
	sops.sem_op = parameters.SO_PORTI + parameters.SO_NAVI + 2;
	semop(sem_id, &sops, 1);
	
	spoilednave = malloc((parameters.SO_MERCI + 1) * sizeof(int));
	spoiledporto = malloc((parameters.SO_MERCI + 1) * sizeof(int));
	for(i = 0; i < parameters.SO_MERCI + 1; i++) {
		spoiledporto[i] = 0;
		spoilednave[i] = 0;
	}
	num_kid_pids_navi = parameters.SO_NAVI;
	num_kid_pids_porti = parameters.SO_PORTI;

	/*handle messages*/
	while(flag) {
		msgrcv(master_msgq, &message, (sizeof(long) + sizeof(char) * 100), 1, 0);
		switch(message.mesg_text[0]) {
			case 's':
				/*register daily report from ship*/
				strtok(message.mesg_text, ":");
				strcpy(dayr, strtok(NULL, ":"));
				strcpy(tempstr, strtok(NULL, ":"));
				switch(atoi(tempstr)) {
					case 0:
						reports[atoi(dayr)].seawithcargo++;
						break;
					case 1:
						reports[atoi(dayr)].seawithoutcargo++;
						break;
					case 2:
						reports[atoi(dayr)].docked++;
						break;
					default:
						break;
				}
				break;
			case 'p':
				/*register daily report from port*/
				if(timeended == 0) {
					strtok(message.mesg_text, ":");
					strcpy(portid, strtok(NULL, ":"));
					strcpy(dayr, strtok(NULL, ":"));
					strcpy(tempstr, strtok(NULL, ":"));
					reports[atoi(dayr)].ports[atoi(portid)].mercesent = atoi(tempstr);
					strcpy(tempstr, strtok(NULL, ":"));
					reports[atoi(dayr)].ports[atoi(portid)].mercereceived = atoi(tempstr);
					strcpy(tempstr, strtok(NULL, ":"));
					reports[atoi(dayr)].ports[atoi(portid)].dockstot = atoi(tempstr);
					strcpy(tempstr, strtok(NULL, ":"));
					reports[atoi(dayr)].ports[atoi(portid)].docksocc = atoi(tempstr);
				}
				break;
			case 'd':
				for(i = 0; i < parameters.SO_NAVI + parameters.SO_PORTI; i++) {
					kill(kid_pids[i], SIGUSR2);
				}
				
				/*count total avaiable merci*/
				for(i = 0; i < parameters.SO_PORTI; i++) {
					for(j = 0; j < parameters.SO_MERCI * (day + 1); j++) {
						if(ports_shm_ptr_aval[i][j].type == 0) {
							j = parameters.SO_MERCI * (day + 1);
						} else if(ports_shm_ptr_aval[i][j].type > 0 && ports_shm_ptr_aval[i][j].qty > 0) {
							reports[day].ports[i].mercepresent += ports_shm_ptr_aval[i][j].qty;
						}
					}
				}

				/*check if there is at least one merce available*/
				allmerciempty = 1;
				for(i = 0; i < parameters.SO_PORTI; i++) {
					if(reports[day].ports[i].mercepresent > 0) {
						allmerciempty = 0;
						i = parameters.SO_PORTI;
					}
				}

				/*check if every request has been fulfilled*/
				allrequestfulfilled = 1;
				for(i = 0; i < parameters.SO_PORTI; i++) {
					for(j = 1; j <= parameters.SO_MERCI; j++) {
						if(ports_shm_ptr_req[i][j] > 0) {
							allrequestfulfilled = 0;
							j = parameters.SO_MERCI + 1;
							i = parameters.SO_PORTI;
						}
					}
				}

				if(timeended == 0 && (allrequestfulfilled == 1 || allmerciempty == 1)) {
					timeended = 1;
					strcpy(message.mesg_text, "t");
					msgsnd(master_msgq, &message, (sizeof(long) + sizeof(char) * 2), 0);
					kill(kid_pids[parameters.SO_PORTI + parameters.SO_NAVI], SIGINT);
					printf("SIMULATION INTERRUPTED\n");
				}

				/*print report*/
				printf("-----------------------------------\n");
				if(day < MAX_DAYS && timeended == 0) {
					printf("DAY %d REPORT\n", day);
				} else {
					printf("FINAL REPORT\n");
				}
				printf("SHIP BY SEA WITHOUT CARGO: %d\n", reports[day].seawithoutcargo);
				printf("SHIP BY SEA WITH CARGO: %d\n", reports[day].seawithcargo);
				printf("SHIP DOCKED: %d\n", reports[day].docked);
				for(i = 0; i < parameters.SO_PORTI; i++) {
					printf("PORT %d: %d TONS OF MERCE AVAILABLE | ", i, reports[day].ports[i].mercepresent);
					printf("SENT %d TONS OF MERCE | ", reports[day].ports[i].mercesent);
					printf("RECEIVED %d TONS OF MERCE | ", reports[day].ports[i].mercereceived);
					printf("%d/%d OCCUPIED DOCKS\n", reports[day].ports[i].docksocc, reports[day].ports[i].dockstot);
				}
				printf("-----------------------------------\n");
				day++;
				break;
			case 't':
				/*send termination signal to every port and ship*/
				for(i = 0; i < parameters.SO_NAVI + parameters.SO_PORTI + 1; i++) {
					kill(kid_pids[i], SIGINT);
				}
				timeended = 1;
				break;
			case 'P':
				/*get spoiled merci for end report*/
				strtok(message.mesg_text, ":");
				strcpy(tempstr, strtok(NULL, ":"));
				if(strcmp(tempstr, "end") == 0) {
					num_kid_pids_porti--;
				} else {
					strcpy(tempstr2, strtok(NULL, ":"));
					spoiledporto[atoi(tempstr)] += atoi(tempstr2);
				}
				break;
			case 'S' :
				/*get spoiled merci for end report*/
				strtok(message.mesg_text, ":");
				strcpy(tempstr, strtok(NULL, ":"));
				if(strcmp(tempstr, "end") == 0) {
					num_kid_pids_navi--;
				} else {
					strcpy(tempstr2, strtok(NULL, ":"));
					spoilednave[atoi(tempstr)] += atoi(tempstr2);
				}
				break;
			default :
				if(timeended == 0) {
					strcpy(idin, strtok(message.mesg_text, ":"));
					strcpy(posx_str, strtok(NULL, ":"));
					strcpy(posy_str, strtok(NULL, ":"));
					strcpy(merce, strtok(NULL, ":"));
					idfind = getRequesting(posx_str, posy_str, ports_positions, ports_shm_ptr_req, atoi(merce), parameters.SO_PORTI, parameters.SO_MERCI);

					message.mesg_type = 1;
					sprintf(x, "%f", ports_positions[idfind].x);
					sprintf(y, "%f", ports_positions[idfind].y);
					sprintf(message.mesg_text, "%d", msgqueue_porto[idfind]);
					strcat(message.mesg_text, ":");
					strcat(message.mesg_text, x);
					strcat(message.mesg_text, ":");
					strcat(message.mesg_text, y);
					msgsnd(msgqueue_nave[atoi(idin)], &message, (sizeof(long) + sizeof(char) * 100), 0);
				}
				break;
		}

		if(num_kid_pids_navi <= 0 && num_kid_pids_porti <= 0) {
			flag = 0;
		}
	}

	totalsent = malloc((parameters.SO_MERCI + 1) * sizeof(int));
	totaldelivered = malloc((parameters.SO_MERCI + 1) * sizeof(int));
	totalport = malloc((parameters.SO_MERCI + 1) * sizeof(int));
	for(i = 0; i < parameters.SO_MERCI + 1; i++) {
		totalsent[i] = 0;
		totaldelivered[i] = 0;
		totalport[i] = 0;
	}

	for(i = 0; i < parameters.SO_PORTI; i++) {
		/*add merce still in port*/
		if(allmerciempty != 1) {
			for(j = 0; j < parameters.SO_MERCI; j++) {
				if(ports_shm_ptr_aval[i][j].type > 0 && ports_shm_ptr_aval[i][j].qty > 0) {
					totalport[ports_shm_ptr_aval[i][j].type] += ports_shm_ptr_aval[i][j].qty;
				}
			}
		}

		/*add merce received*/
		for(j = 1; j <= parameters.SO_MERCI; j++) {
			if(ports_shm_ptr_req[i][j + parameters.SO_MERCI] > 0) {
				totaldelivered[j] += ports_shm_ptr_req[i][j + parameters.SO_MERCI];
			}
		}

		/*add merce sent*/
		for(j = 1; j <= parameters.SO_MERCI; j++) {
			if(ports_shm_ptr_req[i][j + (parameters.SO_MERCI * 2)] > 0) {
				totalsent[j] += ports_shm_ptr_req[i][j + (parameters.SO_MERCI * 2)];
			}
		}
	}

	/*print merci report*/
	for(i = 1; i <= parameters.SO_MERCI; i++) {
		allinport += totalport[i];
		allinship += totalsent[i] - spoilednave[i] - totaldelivered[i];
		alldelivered += totaldelivered[i];
		allspoiledport += spoiledporto[i];
		allspoiledship += spoilednave[i];
	}
	printf("TOTAL: AVAILABLE %d | IN SHIP %d | DELIVERED %d | SPOILED IN PORT %d | SPOILED IN SHIP %d |\n", allinport, allinship, alldelivered, allspoiledport, allspoiledship);

	printf("-----------------------------------\n");
	for(i = 1; i < parameters.SO_MERCI + 1; i++) {
		printf("MERCE %d:\n| GENERATED %d | AVAILABLE %d | SENT %d | DELIVERED %d |\n| SPOILED IN PORT %d | SPOILED IN SHIP %d |\n| MOST AVAILABLE IN PORT %d | MOST REQUESTED IN PORT %d |\n", i, totalgenerated[i], totalport[i], totalsent[i], totaldelivered[i], spoiledporto[i], spoilednave[i], mostavailable[i], mostrequested[i]);
	}

	/*close messagequeues*/
	for(i = 0; i < parameters.SO_PORTI; i++) {
		msgctl(msgqueue_porto[i], IPC_RMID, NULL);
	}
	for(i = 0; i < parameters.SO_NAVI; i++) {
		msgctl(msgqueue_nave[i], IPC_RMID, NULL);
	}

	msgctl(master_msgq, IPC_RMID, NULL);

	/*close semaphore and shared memories*/
	semctl(sem_id, 0, IPC_RMID);
	for(i = 0; i < parameters.SO_PORTI; i++) {
		shmctl(ports_shm_id_aval[i], IPC_RMID, NULL);
		shmctl(ports_shm_id_req[i], IPC_RMID, NULL);
	}

	exit(0);
}

/*gets the closed port that has a request for the specified merce; if the merce is not specified or no port has a request for it returns a random port*/
int getRequesting(char *posx_s, char *posy_s, struct position * portpositions, int ** portsrequests, int merce, int nporti, int nmerci) {
	int i;
	struct position currpos;
	struct position minpos;
	int imin = 0;

	if(merce == 0) {
		return rand() % nporti;
	}
	minpos.x = 1000000;
	minpos.y = 1000000;
	sscanf(posx_s, "%lf", &currpos.x);
	sscanf(posy_s, "%lf", &currpos.y);
	for(i = 0; i < nporti; i++) {
		if(portsrequests[i][merce] > 0) {
			if(sqrt(pow((portpositions[i].x - currpos.x),2) + pow((portpositions[i].y - currpos.y),2)) < sqrt(pow((minpos.x - currpos.x),2) + pow((minpos.y - currpos.y),2))) {
				imin = i;
				minpos.x = portpositions[i].x;
				minpos.y = portpositions[i].y;
			}
		}
	}
	if(imin >= 0) {
		return imin;
	}
	return rand() % nporti;
}

/*load parameters from an input file; parameters must be separated by comma*/
int read_parameters_from_file(FILE *inputfile, struct parameters * parameters) {
	char * string = malloc(256);
	char data[10];
	fgets(string, 256, inputfile);

	strcpy(data, strtok(string, ","));
	parameters->SO_NAVI = atoi(data);
	if(parameters->SO_NAVI < 1) {
		printf("SO_NAVI must be >= 1\n");
		return 0;
	}

	strcpy(data, strtok(NULL, ","));
	parameters->SO_PORTI = atoi(data);
	if(parameters->SO_PORTI < 4) {
		printf("SO_PORTI must be >= 4\n");
		return 0;
	}

	strcpy(data, strtok(NULL, ","));
	parameters->SO_MERCI = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_SIZE = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_MIN_VITA = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_MAX_VITA = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_LATO = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_SPEED = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_CAPACITY = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_BANCHINE = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_FILL = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_LOADSPEED = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_DAYS = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_STORM_DURATION = atoi(data);

	strcpy(data, strtok(NULL, ","));
	parameters->SO_SWELL_DURATION = atoi(data);

	strcpy(data , strtok(NULL, ","));
	parameters->SO_MAELSTORM = atoi(data);

	free(string);

	return 1;
}