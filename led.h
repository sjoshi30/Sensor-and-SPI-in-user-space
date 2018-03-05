#ifndef __GPIO_FUNC_H__


 /****************************************************************
 * Constants
 ****************************************************************/
 
#define SYSFS_GPIO_DIR "/sys/class/gpio"
#define MAX_BUF 64

#define GPIO_DIRECTION_IN 1
#define GPIO_DIRECTION_OUT 0
#define GPIO_VALUE_LOW 0
#define GPIO_VALUE_HIGH 1

/****************************************************************
 * Functions
 ****************************************************************/

int gpio_export(unsigned int gpio);
int gpio_unexport(unsigned int gpio);
int gpio_set_dir(unsigned int gpio, unsigned int out_flag);
int gpio_set_value(unsigned int gpio, unsigned int value);
int gpio_get_value(unsigned int gpio, unsigned int *value);
int gpio_set_edge(unsigned int gpio, char *edge);
int gpio_fd_open(unsigned int gpio);
int gpio_fd_close(int fd);


#endif /* __GPIO_FUNC_H__ */
