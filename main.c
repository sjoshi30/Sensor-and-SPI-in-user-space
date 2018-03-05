
/* Course : CSE 438 - Embedded Systems Programming
 * Assignment 3 : SPI Device Programming and Pulse Width Measurement
 * Team Member1 : Samruddhi Joshi  ASUID : 1213364722
 * Team Member2 : Varun Joshi 	   ASUID : 1212953337 
*/
 
/**
*	Include Library Headers 
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include <assert.h>
#include <linux/input.h> /* required for mouse events*/
#include <math.h>
#include <pthread.h>     /* required for pthreads */
#include <semaphore.h>   /* required for semaphores */
#include <sched.h> 

#include "led.h"

/**
 * Define constants using the macro
 */ 
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define POLL_TIMEOUT 1000 /* 3 seconds */

/*********************************/
#define GP_IO2 13  //GPIO13 corresponds to IO2
#define GP_IO3 14  //GPIO14 corresponds to IO3
#define GP_IO2_MUX 77 //GPIO31 corresponds to MUX controlling IO2
#define GP_IO3_MUX1 76  //GPIO76 corresponds to MUX controlling IO3
#define GP_IO3_MUX2 64  //GPIO64 corresponds to MUX controlling IO3
/*********************************/

#define CPU_CLOCK_SPEED 400000000 //400 MHz
#define SPI_DEVICE_NAME "/dev/spidev1.0"


#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
static uint8_t mode = 0;
static uint8_t bits = 8;
static uint32_t speed = 500000;
static uint16_t delay;

double distance = 0;
pthread_mutex_t lock;

 /**
 * Thread Arguments
 */
pthread_t thread_ID[2];    /* 1 SPI_transmit thread, 1 sensor_detect */
pthread_attr_t thread_attr[2];
int thread_priority[]={50,50};  
struct sched_param param[2];
int rerror[2]; /* to check for error in pthread creation */

uint8_t array [] = {
		0x01, 0x06,
		0x02, 0x06,	
		0x03, 0x06,
		0x04, 0x03,
		0x05, 0x7F,
		0x06, 0xA2,
		0x07, 0x32,
		0x08, 0x16,
};

void init_sequence(void);

/***********************************************************************
 * rdtsc() function is used to calulcate the number of clock ticks
 * and measure the time. TSC(time stamp counter) is incremented 
 * every cpu tick (1/CPU_HZ).
 **********************************************************************/
static __inline__ unsigned long long my_rdtsc(void)
 {
     unsigned long lo, hi;
     __asm__ __volatile__ ( "rdtsc" : "=a" (lo), "=d" (hi) ); 
     return( (unsigned long long)lo | ((unsigned long long)hi << 32) );
 }


/***********************************************************************
* transfer - Function to create a transfer structure and pass it to 
* 		IOCTL to display on LED.
* @address: Address
* @data: data
*
* Returns 0 on success.
* 
* Description: Function to create a transfer structure and pass it to 
* 		IOCTL to display on LED. Various parameters for the transfer are
* 		set like, bits per word, speed etc.
***********************************************************************/
void transfer(int fd, uint8_t address, uint8_t data)
{
	int retValue;

	//printf("transfer start fd %d\n",fd);

	uint8_t tx[2];
	
	uint8_t rx[ARRAY_SIZE(tx)] = {0,};
	//printf("1\n");

	tx[0] = address;
	tx[1] = data;
	
	struct spi_ioc_transfer tr = {
		.tx_buf = (unsigned long)tx,
		.rx_buf = (unsigned long)rx,
		.len = 2,
		.delay_usecs = 1,
		.speed_hz = 10000000, //10000000
		.cs_change = 1,
		.bits_per_word = 8,
	};
	
	
	retValue = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

	//printf("Return val of ioctl %d \n",retValue);
	//printf("Inside transfer\n");
	if (retValue < 1)
		printf("can't send spi message\n");
}


void* Func_UltrasonicDetect(void *ptr)
{
	struct pollfd Echo_Pin;
	int retPoll, retValue;
	unsigned long long Rise_time;
	unsigned long long Fall_time;
	char *buf[MAX_BUF];
	long double dummy1, dummy2;
	int fd, fd_val, res, fd_edge, fd13, fd11;
	unsigned char Readvalue[2];

	
	fd_val = open("/sys/class/gpio/gpio14/value",O_RDONLY); //echo
	//fd13 = open("/sys/class/gpio/gpio13/value", O_WRONLY); //trig
	fd11 = open("/sys/class/gpio/gpio11/value", O_WRONLY);
	//printf(" fd13 %d\n",fd13 );
	fd_edge = open("/sys/class/gpio/gpio14/edge", O_WRONLY);
	Echo_Pin.fd = fd_val;
	Echo_Pin.events = POLLPRI;
	Echo_Pin.revents = 0;


	while(1)
	{
		lseek(Echo_Pin.fd, 0, SEEK_SET);
		Echo_Pin.revents = 0;
		
		write(fd_edge,"rising",6);
		
		write(fd11,"1",1);
		//printf(" res for fd13 %d\n", res);
		usleep(20);
		write(fd11,"0",1);
		//usleep(2);
		

		retPoll = poll(&Echo_Pin, 1, 3000);
		
		if(retPoll < 0)
		{
			printf("Poll Error Ocurred\n");	
		}
		else
		{
			if(Echo_Pin.revents & POLLPRI)
			{
				Rise_time = my_rdtsc();
				//timeRising = timeRising/400;
				//printf("Rising edge %llu \n",timeRising);
				
				read(Echo_Pin.fd,Readvalue,1);
				Echo_Pin.revents = 0;

				//FLush buffer
				lseek(Echo_Pin.fd, 0, SEEK_SET);
				//Echo_Pin.revents = 0;
				write(fd_edge,"falling",7);
				
				retPoll = poll(&Echo_Pin, 1, 3000);
				
				if(retPoll < 0)
				{
					printf("poll Error Ocurred\n");	
				}
				else if(retPoll > 0)
				{
					if(Echo_Pin.revents & POLLPRI)
					{
						Fall_time = my_rdtsc();
						//timeFalling = (long double)timeFalling/400;
						//printf("Falling edge %llu \n",timeFalling);
						
						read(Echo_Pin.fd,Readvalue,1);
						//Echo_Pin.revents = 0;
					}
				}
			}
		}
			
		usleep(600000);

		pthread_mutex_lock(&lock);
		distance = (double)(((Fall_time - Rise_time) / 400) * 0.017);
		pthread_mutex_unlock(&lock);
		printf("Distance is %0.2f \n",distance);
	}
		close(fd_edge);
		close(fd_val);
		close(fd11);
		pthread_exit(0);
}


/***********************************************************************
* thread_transmit_spi - Thread Function to send data to the LED display.
* @fd: file descriptor
*
* Returns 0 on success.
* 
* Description: Thread Function to send data to the LED display. We 
* 	continuosly monitor the distance obatined from the sensor, using 
* 	this distance, we can find if the person/obstance is approaching or 
* 	going away from sensor. Based on this the dog id made to run slow 
* 	and fast and turn its direction.
***********************************************************************/
void* Func_SPITransmit(void *ptr)
{
	int i,fd,j,delay;
	int retValue;
	double distance_previous = 0, distance_current = 0, distance_diff = 0, distance_threshhold=0;
	char new_direction = 'L', old_direction = 'L';
	
	init_sequence();

	fd = open(SPI_DEVICE_NAME, O_RDWR);

	if(fd < 0)
	{
		printf("Can not open device file fd_spi.\n");
		return 0;
	}
	else
	{
		printf("fd_spi device opened succcessfully.\n");
	}
	
	/*********************/
	
	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, 0x0F, 0x01);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	usleep(100000);

	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, 0x0F, 0x00);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	usleep(100000);

	// Enable mode B
	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, 0x09, 0x00);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	usleep(100000);
	// Define Intensity
	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, 0x0A, 0x00);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	usleep(100000);
	// Only scan 7 digit
	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, 0x0B, 0x07);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	usleep(100000);
	// Turn on chip
	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, 0x0C, 0x01);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	usleep(100000);

	for(i=1; i < 9; i++)
	{
	gpio_set_value(15,GPIO_VALUE_LOW);
	transfer(fd, i, 0x00);
	gpio_set_value(15,GPIO_VALUE_HIGH);
	}

	while(1)
	{	
		pthread_mutex_lock(&lock);
		distance_current = distance;
		pthread_mutex_unlock(&lock);
		distance_diff = distance_current - distance_previous;
		distance_threshhold = distance_current / 10.0;
		//printf("Distance = %0.2f\n",distance_current);
		if(distance>35)
		{
			delay=600000;
		}
		else
		{
			delay=60000;
		}
		if((distance_diff > -distance_threshhold) && (distance_diff < distance_threshhold))
		{
			new_direction = old_direction;
		}
		else if(distance_diff > distance_threshhold)
		{
			new_direction = 'R';
		}
		else if(distance_diff < -distance_threshhold)
		{
			new_direction = 'L';
		}
		
		if(new_direction == 'R')
		{
			//printf("Moving Away... Move Right\n");
			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x01, 0x08);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x02, 0x90);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x03, 0xf0);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x04, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x05, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x06, 0x37);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x07, 0xdf);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x08, 0x98);
			gpio_set_value(15,GPIO_VALUE_HIGH);
			
			usleep(delay);
			
			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x01, 0x20);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x02, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x03, 0x70);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x04, 0xd0);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x05, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x06, 0x97);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x07, 0xff);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x08, 0x18);
			gpio_set_value(15,GPIO_VALUE_HIGH);
			
			usleep(delay);
			
		}
		else if(new_direction == 'L')
		{
			//printf("Moving Closer... Move Left\n");
			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x01, 0x98);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x02, 0xdf);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x03, 0x37);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x04, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x05, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x06, 0xf0);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x07, 0x90);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x08, 0x08);
			gpio_set_value(15,GPIO_VALUE_HIGH);
			
			
			usleep(delay);
			
			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x01, 0x18);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x02, 0xff);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x03, 0x97);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x04, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x05, 0xd0);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x06, 0x70);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x07, 0x10);
			gpio_set_value(15,GPIO_VALUE_HIGH);

			gpio_set_value(15,GPIO_VALUE_LOW);
			transfer(fd, 0x08, 0x20);
			gpio_set_value(15,GPIO_VALUE_HIGH);
			
			
			usleep(delay);
		 
		}
		
		distance_previous = distance_current;
		old_direction = new_direction;
}
		
	
pthread_exit(0);
}

/***********************************************************************
* main - Main Thread Function which creates two threads, one to read the
* 		sensor and other to display data onto LED
* @data: Thread Parameters
*
* Returns NULL
* 
* Description:  Main Thread Function which creates two threads, one to 
* 	read the sensor and other to display data onto LED
***********************************************************************/
int main(void)
{
	int i;

	pthread_mutex_init(&lock, NULL);

	for(i=0; i<2; i++)
	{
	pthread_attr_init(&thread_attr[i]);
	pthread_attr_getschedparam(&thread_attr[i], &param[i]);
	param[i].sched_priority = thread_priority[i];  /* set thread priority */
	pthread_attr_setschedparam(&thread_attr[i], &param[i]);
	pthread_setschedparam(pthread_self(), SCHED_FIFO, &param[i]);
	}
	
	rerror[1] = pthread_create(&thread_ID[1], &thread_attr[1], &Func_UltrasonicDetect, NULL);
	rerror[0] = pthread_create(&thread_ID[0], &thread_attr[0], &Func_SPITransmit, NULL);		
	if(rerror[0] != 0)
		{
		printf("Error while creating thread \n");
		}


	for(i=0; i<2; i++)
	{
	pthread_join(thread_ID[i],NULL); /* wait for all threads to terminate */
	}
	
	return 0;
}

void init_sequence(void)
{
	int fd, fd34, fd14, fd77, fd76, fd64, fd11;

	/* Export all GPIO Pins */
	gpio_export(11);
	gpio_export(13);
	gpio_export(14);
	gpio_export(34);
	gpio_export(16);
	gpio_export(77);
	gpio_export(76);
	gpio_export(64);
	gpio_export(32);
	//gpio_export(17);
	
	/* Set Directions for all GPIO Pins */ 
	fd14 = open("/sys/class/gpio/gpio14/direction", O_WRONLY);
	write(fd14,"in",2);
	gpio_set_dir(13,0);
	gpio_set_dir(11,0);
	gpio_set_dir(32,0);
	//gpio_set_dir(14,1);
	gpio_set_dir(34,0);
	gpio_set_dir(16,0);
	// gpio_set_dir(77,0);
	// gpio_set_dir(76,0);
	// gpio_set_dir(64,0);

	// fd34 = open("/sys/class/gpio/gpio34/direction", O_WRONLY);
	// write(fd34,"out",3);
	
	/* Set values for all GPIO Pins */
	// 0 - output
	// fd11 = open("/sys/class/gpio/gpio11/value", O_WRONLY);
	// write(fd11,"1",1);
	gpio_set_value(32,0);
	gpio_set_value(13,0);
	//gpio_set_value(14,0);
	gpio_set_value(16,1);
	gpio_set_value(34,0);
	gpio_set_value(77,0);
	gpio_set_value(76,0);
	gpio_set_value(64,0);
	//gpio_set_value(17,1);

	/******************************/
	gpio_export(72);
	gpio_export(44);
	gpio_export(46);
	gpio_export(15);

	gpio_export(24);
	gpio_export(42);
	gpio_export(30);

	gpio_export(25);
	gpio_export(43);
	gpio_export(31);

	gpio_set_dir(72, GPIO_DIRECTION_OUT);
	gpio_set_dir(44, GPIO_DIRECTION_OUT);
	gpio_set_dir(46, GPIO_DIRECTION_OUT);
	gpio_set_dir(15, GPIO_DIRECTION_OUT);

	gpio_set_dir(24, GPIO_DIRECTION_OUT);
	gpio_set_dir(42, GPIO_DIRECTION_OUT);
	gpio_set_dir(30, GPIO_DIRECTION_OUT);

	gpio_set_dir(25,GPIO_DIRECTION_OUT);
	gpio_set_dir(43,GPIO_DIRECTION_OUT);
	gpio_set_dir(31,GPIO_DIRECTION_OUT);

	gpio_set_value(25,1);
	gpio_set_value(43,1);
	gpio_set_value(31,1);

	gpio_set_value(24,GPIO_VALUE_LOW);
	gpio_set_value(42,GPIO_VALUE_LOW);
	gpio_set_value(30,GPIO_VALUE_LOW);

	gpio_set_value(72,GPIO_VALUE_LOW);
	gpio_set_value(44,GPIO_VALUE_HIGH);
	gpio_set_value(46,GPIO_VALUE_HIGH);
	/******************************/
}
