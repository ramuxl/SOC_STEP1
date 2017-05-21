/*
* Simple app to read/write into custom IP in PL via /dev/uoi0 interface
* To compile for arm: make ARCH=arm CROSS_COMPILE=arm-xilinx-linux-gnueabi-
* Author: Tsotnep, Kjans
*/
  
//C
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>
#include "udpclient.h"
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <string.h>

#define BYTES_TO_RECEIVE 256
#define BYTES_TO_READ 4
#define TEMP_FIFO "/temp.fifo"
#define BROADCAST_ADDRESS "10.255.255.255"
#define BROADCAST_PORT 7891
#define BUFFER_SIZE 256
  
#define SLV_REG_0   ((unsigned *)(ptr + 0))
//function prototyeps
void *ReceiveAudio(void *);
void *ReadAudio(void *);

typedef struct thread_arg{
	const char * fifo_file;
	unsigned*  mem_pointer;
} my_thread_arg;
  
int main(int argc, char *argv[])
{
    /*if (*argv[1] == 'p') {
        printf("::::START_USAGE::::\n");
        printf("EXAMPLE : %s 2 4 \n", argv[0]);
        printf("::::END_USAGE::::\n");
    }
    else {*/
	
        //take inputs from user
       // unsigned multiplierInput1 = atoi(argv[1]);
       // unsigned multiplierInput2 = atoi(argv[2]);
     
  
        //open dev/uio0
        int fd = open ("/dev/uio0", O_RDWR);
        if (fd < 1) { perror(argv[0]); return -1; }
 
  
        //Redirect stdout/printf into /dev/kmsg file (so it will be printed using printk)
        freopen ("/dev/kmsg","w",stdout);
  
        //get architecture specific page size
        unsigned pageSize = sysconf(_SC_PAGESIZE);
  
        //Map virtual memory to physical memory          
        void *ptr;
          ptr = mmap(NULL, pageSize, (PROT_READ |PROT_WRITE), MAP_SHARED, fd, pageSize*0);
          
        //setup socket
        if (udp_client_setup (BROADCAST_ADDRESS, BROADCAST_PORT) == 1){
			perror("Error with UDP setup");
			exit(-1);
		}
          
         //Create FIFO
         const char * my_fifo = TEMP_FIFO;
	 my_fifo = strcat (getenv("HOME"), TEMP_FIFO);
	if (access(my_fifo, F_OK) != -1){
	
		remove(my_fifo);
	}
         if(mkfifo(my_fifo, S_IRUSR| S_IWUSR) != 0){
			 perror("mkfifo() error\n");
			 exit (-1);
		}
	//mkfifo(my_fifo, 0666);
		 //create read thread argument structure
		 my_thread_arg read_thread_arg;
		 read_thread_arg.fifo_file = my_fifo;
		 read_thread_arg.mem_pointer = SLV_REG_0;
          
        //create threads
		 pthread_t ReceiveThread;
		 pthread_t ReadThread;
		 int rc;
		 rc = pthread_create(&ReceiveThread, NULL, ReceiveAudio,(void *)my_fifo );
		 if (rc){
			 printf("Error: Failed to create thread 1 with retrun code %d\n", rc);
			 exit(-1);
		 }
		 rc = pthread_create(&ReadThread, NULL, ReadAudio,(void *)&read_thread_arg);
		 if (rc){
			 printf("Error: Failed to create thread 2 with retrun code %d\n", rc);
			 exit(-1);
		 }
  
        
		pthread_join(ReceiveThread, NULL);
		pthread_join(ReadThread, NULL);
		
        //unmap
        munmap(ptr, pageSize);
  
        //close
        fclose(stdout);
        
        //exit thread
        pthread_exit(NULL);
    //}
    return 0;
}

void *ReceiveAudio (void *tempfifo)
{
	const char *fifo;
	fifo = (char *) tempfifo;
	int wfd, bytes_written;
	unsigned buffer[BUFFER_SIZE];
	wfd = open (fifo, O_WRONLY);
	while (true){
		
		if (wfd < 0){
			perror("open() error for write end");
			return;
		}
		if (udp_client_recv (buffer, BYTES_TO_RECEIVE) == 1){;//receive 256 bytes from UDP to buffer
			perror("Error receiving from UDP");
			close(wfd);
			return;
		}
		if ((bytes_written = write (wfd, buffer, BYTES_TO_RECEIVE) )== -1){ //Write 256 bytes from buffer to FIFO
			perror("write () error");
			close(wfd);
			return;
		}
		/*else{
			printf("Written %d bytes to FIFO\n", bytes_written);
		}*/
		
	}
	close(wfd);
}

void *ReadAudio(void *thread_arg)
{
	my_thread_arg *func_data;
	func_data = (my_thread_arg *) thread_arg;
	const char *fifo = func_data -> fifo_file;
	int rfd, bytes_read;
	rfd = open(fifo, O_RDONLY);
	while (true){
		
		if(rfd < 0){
			perror("open() error for read end");
			return;			
		} 
		if((bytes_read =read(rfd, func_data -> mem_pointer, BYTES_TO_READ)) == -1){//Write 4  bytes to SLV_REG_0
			perror("read() error");
			close(rfd);
			return;
		}
		/*else{
			printf("%d bytes have been read from FIFO into AXI_AUDIO\n", bytes_read);
		}*/
	}
	
	close(rfd);
}
