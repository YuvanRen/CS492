/*
	Yuvan Rengifo
	I pledge my honor that I have abided by the Stevens Honor System
*/

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "scull.h"
#include <sys/wait.h>
#include <pthread.h>

#define CDEV_NAME "/dev/scull"

/* Quantum command line option */
static int g_quantum;

static void usage(const char *cmd)
{
	printf("Usage: %s <command>\n"
	       "Commands:\n"
	       "  R          Reset quantum\n"
	       "  S <int>    Set quantum\n"
	       "  T <int>    Tell quantum\n"
	       "  G          Get quantum\n"
	       "  Q          Query quantum\n"
	       "  X <int>    Exchange quantum\n"
	       "  H <int>    Shift quantum\n"
		   "  i 		 Task info\n"	/* 3.1 Information usage*/
		   "  p 		 Processes Test Log\n"   /* 4.2 Processes*/
		   "  t			 Threads Test log"
		   "  h          Print this message\n",
	       cmd);
}

typedef int cmd_t;

static cmd_t parse_arguments(int argc, const char **argv)
{
	cmd_t cmd;

	if (argc < 2) {
		fprintf(stderr, "%s: Invalid number of arguments\n", argv[0]);
		cmd = -1;
		goto ret;
	}

	/* Parse command and optional int argument */
	cmd = argv[1][0];
	switch (cmd) {
	case 'S':
	case 'T':
	case 'H':
	case 'X':
		if (argc < 3) {
			fprintf(stderr, "%s: Missing quantum\n", argv[0]);
			cmd = -1;
			break;
		}
		g_quantum = atoi(argv[2]);
		break;
	case 'R':
	case 'G':
	case 'Q':
	case 'h':
	case 'i':
	case 'p':
	case 't':
		break;
	default:
		fprintf(stderr, "%s: Invalid command\n", argv[0]);
		cmd = -1;
	}

ret:
	if (cmd < 0 || cmd == 'h') {
		usage(argv[0]);
		exit((cmd == 'h')? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return cmd;
}

//////////////////////////////////////////////////////////////////
/* Create threads that returns void  */
void *helper(void *arg){

    int fd = *(int *)arg; /* Fill descriptor */

    struct task_info kr;

    for (int i = 0; i < 2; i++) { 
        int ret = ioctl(fd, SCULL_IOCIQUANTUM, &kr); 
        if (ret == 0) {
            // Print the task information received from the driver
            printf("state: %ld, cpu: %d, priority: %d, pid: %d, tgi: %d, nv: %lu, niv: %lu\n",
            	   kr.state,
			    	kr.cpu,
				 	kr.prio, 
				  	kr.pid, 
				   	kr.tgid,
				    kr.nvcsw,
					kr.nivcsw);
		}
    }
    return NULL;
}

static int do_op(int fd, cmd_t cmd)
{
	int ret, q;
	
	struct task_info kr;	/* Initialize task_info */

	pthread_t threads[4];	/* Initialize threads arr */
	switch (cmd) {
	case 'R':
		ret = ioctl(fd, SCULL_IOCRESET);
		if (ret == 0)
			printf("Quantum reset\n");
		break;
	case 'Q':
		q = ioctl(fd, SCULL_IOCQQUANTUM);
		printf("Quantum: %d\n", q);
		ret = 0;
		break;
	case 'G':
		ret = ioctl(fd, SCULL_IOCGQUANTUM, &q);
		if (ret == 0)
			printf("Quantum: %d\n", q);
		break;
	case 'T':
		ret = ioctl(fd, SCULL_IOCTQUANTUM, g_quantum);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'S':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCSQUANTUM, &q);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'X':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCXQUANTUM, &q);
		if (ret == 0)
			printf("Quantum exchanged, old quantum: %d\n", q);
		break;
	case 'H':
		q = ioctl(fd, SCULL_IOCHQUANTUM, g_quantum);
		printf("Quantum shifted, old quantum: %d\n", q);
		ret = 0;
		break;
    case 'i': /* 3.1 'i'nfo case*/

  	  ret = ioctl(fd, SCULL_IOCIQUANTUM, &kr); /* 3.2 Get all needed info*/
    	if (ret == 0) {
        	printf("state: %ld, cpu: %d, priority: %d, pid: %d, tgi: %d, nv: %lu, niv: %lu\n",
            	   kr.state,
			    	kr.cpu,
				 	kr.prio, 
				  	kr.pid, 
				   	kr.tgid,
				    kr.nvcsw,
					kr.nivcsw);
    }

		break;
	case 'p': /* 4.2 'p' processes test log case*/

		/* Create 4 processes*/
		for(int i = 0; i < 4; i++){
			pid_t pid = fork(); /* Create child process*/

			if(pid==0){ /* Only child processes*/
				for(int j=0; j < 2; j++){ /* Each process calls SCULL_IOCIQUANTUM twice*/
					ret = ioctl(fd, SCULL_IOCIQUANTUM, &kr);
			    	if (ret == 0) {
						printf("state: %ld, cpu: %d, priority: %d, pid: %d, tgi: %d, nv: %lu, niv: %lu\n",
            				   kr.state,
			    				kr.cpu,
								kr.prio,
				 			  	kr.pid,
				 			   	kr.tgid,
				 			    kr.nvcsw,
								kr.nivcsw);
   						 }
				}
				exit(EXIT_SUCCESS);
			}
		}
		/* Parent Process */
	
		for (int i= 0; i< 4; i++){
			wait(NULL); /* Dont do anything until children are done*/
		}
		ret = 0;
    	break;
	
	case 't': /* 4.3 't' threads test log case */

		/* Create 4 concurrent threads*/
		for(int i = 0; i< 4; i++){
			pthread_create(&threads[i], NULL, helper, &fd);
		}
		/* Wait for all to be created*/
		for (int j = 0; j < 4; j++) { 
                pthread_join(threads[j], NULL);
            }
        ret = 0;
        break;

	default:
		/* Should never occur */
		abort();
		ret = -1; /* Keep the compiler happy */
	}

	if (ret != 0)
		perror("ioctl");
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, ret;
	cmd_t cmd;

	cmd = parse_arguments(argc, argv);

	fd = open(CDEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("cdev open");
		return EXIT_FAILURE;
	}

	printf("Device (%s) opened\n", CDEV_NAME);

	ret = do_op(fd, cmd);

	if (close(fd) != 0) {
		perror("cdev close");
		return EXIT_FAILURE;
	}

	printf("Device (%s) closed\n", CDEV_NAME);

	return (ret != 0)? EXIT_FAILURE : EXIT_SUCCESS;
}
