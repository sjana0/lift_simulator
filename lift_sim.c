#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define NFLOORS 9
#define NTURNS 3                    //number of turns per person
#define UP 1
#define DOWN 0
#define P(s) semop(s, pop, 1)
#define V(s) semop(s, vop, 1)
#define val(s) semctl(s, 0, GETVAL)
#define Pi(s, i) semop(s, &pop[i], 1)
#define Vi(s, i) semop(s, &vop[i], 1)
#define vali(s, i) semctl(s, i, GETVAL)

#define NLIFTS 2
#define NPERSON 5

typedef struct
{
	int waitingtogoup;  /* the number of people waiting to go up */
	int waitingtogodown;  /* the number of people waiting to go down */
	int up_arrow;  /* people going up wait on this */
	int down_arrow; /* people going down wait on this */
	int finished;    /* people who have finished all turns */
} Floor_info;
typedef struct
{
	int position;   /* which floor it is on */
	int direction;   /* going UP or DOWN */
	int peopleinlift;  /* number of people in the lift */
	int stops;    /* for each floor how many people are waiting to get off (variable is used)*/
	int stopsem;  /* people in the lift wait on one of these (list is used)*/
} Lift_info;

void printfun(Floor_info floor[NFLOORS], Lift_info lift[NLIFTS])
{
	int j, k, l;
	printf("Floor area|");
	for(j=0; j<NLIFTS; j++) printf("   Lift %d   |", j);
	printf("\n---------------------------------------------------\n");
	for(j=NFLOORS-1; j>=0; j--)
	{
		for(k=0; k<val(floor[j].waitingtogoup); k++) printf("*");
		for(; k<10; k++) printf(" ");
		printf("|");
		for(k=0; k<NLIFTS; k++)
		{
			if(val(lift[k].position) == j) printf("------------");
			else printf("            ");
			printf("|");
		}
		printf("\n");
		for(k=0; k<val(floor[j].finished); k++) printf("*");
		for(; k<10; k++) printf(" ");
		printf("|");
		for(k=0; k<NLIFTS; k++)
		{
			if(val(lift[k].position) == j)
			{
				printf("|");
				for(l=0; l<val(lift[k].peopleinlift); l++) printf("*");
				for(; l<10; l++) printf(" ");
				printf("|");
			}
			else for(l=0; l<12; l++) printf(" ");
			printf("|");
		}
		printf("\n");
		for(k=0; k<val(floor[j].waitingtogodown); k++) printf("*");
		for(; k<10; k++) printf(" ");
		printf("|");
		for(k=0; k<NLIFTS; k++)
		{
			if(val(lift[k].position) == j) printf("------------");
			else printf("            ");
			printf("|");
		}
		printf("\n---------------------------------------------------\n");
	}
}
int main()
{
	struct sembuf pop[NFLOORS], vop[NFLOORS];
	int i, j, k, l, person, print, exchanges;
	print = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
	semctl(print, 0, SETVAL, 1);
	for(i=0; i<NFLOORS; i++)
	{
		pop[i].sem_num = vop[i].sem_num = i;
		pop[i].sem_op= -1; vop[i].sem_op= 1;
	}
	
	Floor_info floor[NFLOORS];
	//create and initialize all floors' semaphores to 0
	for(i=0; i<NFLOORS; i++)
	{
		floor[i].waitingtogoup = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		floor[i].waitingtogodown = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		floor[i].up_arrow = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		floor[i].down_arrow = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		floor[i].finished = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		semctl(floor[i].waitingtogoup, 0, SETVAL, 0);
		semctl(floor[i].waitingtogodown, 0, SETVAL, 0);
		semctl(floor[i].up_arrow, 0, SETVAL, 0);
		semctl(floor[i].down_arrow, 0, SETVAL, 0);
		semctl(floor[i].finished, 0, SETVAL, 0);
	}
	Lift_info lift[NLIFTS];
	//create and initialize all lifts' semaphores
	for(i=0; i<NLIFTS; i++)
	{
		lift[i].position = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		lift[i].direction = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		lift[i].peopleinlift = semget(IPC_PRIVATE, 1, 0777|IPC_CREAT);
		lift[i].stops = semget(IPC_PRIVATE, NFLOORS, 0777|IPC_CREAT);
		lift[i].stopsem = semget(IPC_PRIVATE, NFLOORS, 0777|IPC_CREAT);
		semctl(lift[i].position, 0, SETVAL, 0);
		semctl(lift[i].direction, 0, SETVAL, UP);
		semctl(lift[i].peopleinlift, 0, SETVAL, 0);
		for(j=0; j<NFLOORS; j++) semctl(lift[i].stops, j, SETVAL, 0);
		for(j=0; j<NFLOORS; j++) semctl(lift[i].stopsem, j, SETVAL, 0);
	}
	
	for(person=0; person<NPERSON; person++)
		if(fork() == 0)
		{	/* person */
			srand(getpid());
			int isin=0, wanttogo;
			for(i=0; i<NTURNS; i++)
			{
				while((wanttogo=rand()%NFLOORS) == isin);
				printf("Person %d wants to go to floor %d\n", person, wanttogo);
				if(wanttogo > isin)
				{
					V(floor[isin].waitingtogoup);       //increment waiting to go up
					P(floor[isin].up_arrow);            //wait in floor's semaphore
					P(floor[isin].waitingtogoup);       //decrement waiting to go up
					for(j=0; j<NLIFTS && !(val(lift[j].position)==isin && val(lift[j].direction)==UP); j++);    //find the lift
				}
				else
				{
					V(floor[isin].waitingtogodown);     //increment waiting to go down
					P(floor[isin].down_arrow);          //wait on floor's semaphore
					P(floor[isin].waitingtogodown);    //decrement waiting to go down
					for(j=0; j<NLIFTS && !(val(lift[j].position)==isin && val(lift[j].direction)==DOWN); j++);  //find the lift
				}
				V(lift[j].peopleinlift);                //get in lift
				Vi(lift[j].stops, wanttogo);            //push button of floor
				Pi(lift[j].stopsem, wanttogo);          //wait in lift's semaphore
				isin = wanttogo;
				sleep(2);
			}
			V(floor[isin].finished);
			exit(0);
		}
	
	for(i=0; i<NLIFTS; i++)
		if(fork() == 0)
		{	/* lift i */
			int turn=0, infloor=0;
			while(turn < (i==0?3*NTURNS/2:NTURNS))
			{
				P(print);
				printfun(floor, lift);
				V(print);
				exchanges = 0;
				
				while(vali(lift[i].stops, infloor))  
				{
					Pi(lift[i].stops, infloor);      //drop off passengers
					Vi(lift[i].stopsem, infloor);    //release passenger processes
					P(lift[i].peopleinlift);         //decrement people in lift
					exchanges++;
				}
				sleep(1);                            //wait for passengers to get in
				if(exchanges)
				{
					P(print);
					printfun(floor, lift);
					V(print);
				}
				exchanges=0;
							
				if(val(lift[i].direction) == UP)
				{
					if(infloor == NFLOORS-1)  //highest floor
					{
						P(lift[i].direction);        //change direction to DOWN
						while(val(floor[infloor].waitingtogodown))
						{
							V(floor[infloor].down_arrow);    //release person process waiting in floor
							exchanges++;
						}
						turn++;
					}
					else
					{
						while(val(floor[infloor].waitingtogoup))
						{
							V(floor[infloor].up_arrow);    //release person process waiting in floor
							exchanges++;
						}
					}
				}
				else if(infloor == 0)  //ground floor
				{
					V(lift[i].direction);            //change direction to UP
					while(val(floor[infloor].waitingtogoup))
					{
						V(floor[infloor].up_arrow);  //release person process waiting in floor
						exchanges++;
					}
					turn++;
				}
				else
				{
					while(val(floor[infloor].waitingtogodown))
					{
						V(floor[infloor].down_arrow);    //release person process waiting in floor
						exchanges++;
					}
				}
				
				if(exchanges)
				{
					P(print);
					printfun(floor, lift);
					V(print);
				}
				
				if(val(lift[i].direction) == UP)
				{
					V(lift[i].position);             //move UP one floor
					infloor++;
				}
				else
				{
					P(lift[i].position);             //move DOWN one floor
					infloor--;
				}
				sleep(0.5*(i+1));                          //duration to move between floors for lift i
			}
			exit(0);
		}
	while(wait(NULL) > 0);                 //wait for all child processes to end
	/* remove semaphores */
	for(i=0; i<NFLOORS; i++)
	{
		semctl(floor[i].waitingtogoup, 0, IPC_RMID);
		semctl(floor[i].waitingtogodown, 0, IPC_RMID);
		semctl(floor[i].up_arrow, 0, IPC_RMID);
		semctl(floor[i].down_arrow, 0, IPC_RMID);
	}
	for(i=0; i<NLIFTS; i++)
	{
		semctl(lift[i].position, 0, IPC_RMID);
		semctl(lift[i].direction, 0, IPC_RMID);
		semctl(lift[i].peopleinlift, 0, IPC_RMID);
		semctl(lift[i].stops, 0, IPC_RMID);
		semctl(lift[i].stopsem, 0, IPC_RMID);
	}
}
/*  #semaphores = 5*NFLOORS +2*NFLOORS*NLIFTS + 3*NLIFTS
	o/p format:
	floor area                      |      lift 0       |...
	--------------------------------------------------------
	-people who want to go up-      |-------------------|     )
	-people who finished all turns- || -inside lift 0- ||     }--floor 1   
	-people who want to go down-    |-------------------|     )
	---------------------------------------------------------
	-                               |                   |     )
	-                               |   no lift here    |     }--floor 0   
	-                               |                   |     )
	---------------------------------------------------------
*/
