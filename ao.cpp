#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include "ao.h"

//----------------------------------------------------------------------------------------


int set_interface_attribs (int fd, int speed, int parity)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        printf("error %d from tcgetattr\n", errno);
        return -1;
    }
    
    cfsetospeed (&tty, speed);
    cfsetispeed (&tty, speed);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;     // 8-bit chars
    // disable IGNBRK for mismatched speed tests; otherwise receive break
    // as \000 chars
    tty.c_iflag &= ~IGNBRK;         // disable break processing
    tty.c_lflag = 0;                // no signaling chars, no echo,
    // no canonical processing
    tty.c_oflag = 0;                // no remapping, no delays
    tty.c_cc[VMIN]  = 0;            // read doesn't block
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY); // shut off xon/xoff ctrl
    
    tty.c_cflag |= (CLOCAL | CREAD);// ignore modem controls,
    // enable reading
    tty.c_cflag &= ~(PARENB | PARODD);      // shut off parity
    tty.c_cflag |= parity;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    if (tcsetattr (fd, TCSANOW, &tty) != 0)
    {
        printf("error %d from tcsetattr\n", errno);
        return -1;
    }
    return 0;
}

//----------------------------------------------------------------------------------------

void set_blocking (int fd, int should_block)
{
    struct termios tty;
    memset (&tty, 0, sizeof tty);
    if (tcgetattr (fd, &tty) != 0)
    {
        printf("error %d from tggetattr\n", errno);
        return;
    }
    
    tty.c_cc[VMIN]  = should_block ? 1 : 0;
    tty.c_cc[VTIME] = 5;            // 0.5 seconds read timeout
    
    if (tcsetattr (fd, TCSANOW, &tty) != 0)
        printf("error %d setting term attributes\n", errno);
}


//----------------------------------------------------------------------------------------


const char *aoport="/dev/ttyACM1";

//----------------------------------------------------------------------------------------

    AO::AO()
{
	xpos = 0;
	ypos = 0;
}

//----------------------------------------------------------------------------------------


int AO::Init()
{

    ao_fd = open(aoport, O_RDWR | O_NOCTTY | O_SYNC);
    

    if (ao_fd < 0) {
  	printf("AO Connection not found\n");
	exit(-1); 
    }

    set_interface_attribs (ao_fd, B115200, 0);  // set speed to 115,200 bps, 8n1 (no parity)
    set_blocking (ao_fd, 0);                // set no blocking

    usleep(1000*1000*6); 
    return 0;
}

//----------------------------------------------------------------------------------------


int AO::Send(const char * s)
{

    int foo = write(ao_fd, s, strlen(s));
    usleep(10000); 
    

    return 0;
}

//----------------------------------------------------------------------------------------

void AO::Center()
{
	printf("ao reset\n");	
	Send("#r\n");
	usleep(1000*1000*6);
	printf("ao reset done\n");
}

//----------------------------------------------------------------------------------------

void AO::Set(int x, int y)
{
	char	buf[256];

	printf("AO::%d %d\n", x, y);	
	sprintf(buf, "#g%d %d\n", x, y);
	Send(buf);
	xpos = x;
	ypos = y;	
	printf("AO::done %s\n", buf);
}

//----------------------------------------------------------------------------------------


void AO::Bump(int dx, int dy)
{
	int target_x = xpos + dx;
	int target_y = ypos + dy;

	Set(target_x, target_y);
}

