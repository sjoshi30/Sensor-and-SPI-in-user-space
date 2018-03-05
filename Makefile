

APP = output


HOME=/opt/iot-devkit/1.7.2/sysroots
PATH := $(PATH):$(HOME)/x86_64-pokysdk-linux/usr/bin/i586-poky-linux
CC=i586-poky-linux-gcc
ARCH=x86
SROOT=$(HOME)/i586-poky-linux/

all :
	$(CC) -o $(APP) --sysroot=$(SROOT) main.c gpio.c -pthread -Wall
clean:
	
	rm -f *.o	
	rm -f $(APP) 
