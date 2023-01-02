#include "serial.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/ioctl.h>

int serial_baud_rate_macro(long baud_rate)
{
	if (baud_rate == 50) return B50;
	else if (baud_rate == 75) return B75;
	else if (baud_rate == 110) return B110;
	else if (baud_rate == 134) return B134;
	else if (baud_rate == 150) return B150;
	else if (baud_rate == 200) return B200;
	else if (baud_rate == 300) return B300;
	else if (baud_rate == 600) return B600;
	else if (baud_rate == 1200) return B1200;
	else if (baud_rate == 1800) return B1800;
	else if (baud_rate == 2400) return B2400;
	else if (baud_rate == 4800) return B4800;
	else if (baud_rate == 9600) return B9600;
	else if (baud_rate == 19200) return B19200;
	else if (baud_rate == 38400) return B38400;
	else if (baud_rate == 57600) return B57600;
	else if (baud_rate == 115200) return B115200;
	else if (baud_rate == 230400) return B230400;
	else if (baud_rate == 460800) return B460800;
	else if (baud_rate == 500000) return B500000;
	else if (baud_rate == 576000) return B576000;
	else if (baud_rate == 921600) return B921600;
	else if (baud_rate == 1000000) return B1000000;
	else if (baud_rate == 1152000) return B1152000;
	else if (baud_rate == 1500000) return B1500000;
	else if (baud_rate == 2000000) return B2000000;
	else if (baud_rate == 2500000) return B2500000;
	else if (baud_rate == 3000000) return B3000000;
	else if (baud_rate == 3500000) return B3500000;
	else if (baud_rate == 4000000) return B4000000;
	else return -1;
}

int serial_close(int fd)
{
	return (fd >= 0) ? close(fd) : -1;
}

int serial_flush(int fd, int input, int output)
{
	if (fd < 0)
	{
		return -1;
	}

	if (input)
	{
		return tcflush(fd, output ? TCIOFLUSH : TCIFLUSH);
	}
	else
	{
		return output ? tcflush(fd, TCOFLUSH) : -1;
	}
}

int serial_drain(int fd)
{
	return tcdrain(fd);
}

int serial_has_buffer(int fd, int io)
{
	if (fd < 0)
	{
		return -1;
	}

	int count;
	if (ioctl(fd, io ? TIOCINQ : TIOCOUTQ, &count) < 0)
	{
		return -1;
	}
	else
	{
		return count;
	}
}

int serial_send(int fd, const char * buffer, size_t size)
{
	if (fd < 0)
	{
		return -1;
	}

	return write(fd, buffer, size);
}

int serial_recv(int fd, char * buffer, size_t max_size, int ms)
{
	if (fd < 0)
	{
		return -1;
	}

	if (ms > 0)
	{
		struct timeval time = {.tv_sec = 0, .tv_usec = ms * 1000};

		fd_set set;
		FD_ZERO(&set);
		FD_SET(fd, &set);

		if (select(fd + 1, &set, NULL, NULL, &time) < 0)
		{
			return -1;
		}

		if (FD_ISSET(fd, &set))
		{
			return read(fd, buffer, max_size);
		}
		else
		{
			return 0;
		}
	}

	return read(fd, buffer, max_size);
}

int serial_open(const char * path, const char * protocol)
{
	long rate;
	char bit, p, sbit;
	struct termios newtio;

	if (sscanf(protocol, "%ld,%c,%c,%c", &rate, &bit, &p, &sbit) != 4)
	{
		return -1;
	}

	/* Initialize configuration messages*/
	bzero(&newtio, sizeof (newtio));
	newtio.c_cflag |= CLOCAL | CREAD;
	newtio.c_cflag &= ~CSIZE;

	/* Close soft flow control*/
	newtio.c_iflag &= ~(ICRNL | IXON | IXOFF);

	/* Close hard flowcontrol*/
	newtio.c_cflag &= ~CRTSCTS;

	/* Set data bits */
	if (bit == '5') newtio.c_cflag |= CS5;
	else if (bit == '6') newtio.c_cflag |= CS6;
	else if (bit == '7') newtio.c_cflag |= CS7;
	else if (bit == '8') newtio.c_cflag |= CS8;
	else return -1;

	/* Set parity */
	if (p == 'n')
	{
		newtio.c_cflag &= ~PARENB;
	}
	else if (p == 'o')
	{
		newtio.c_cflag |= PARENB;
		newtio.c_cflag |= PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
	}
	else if (p == 'e')
	{
		newtio.c_cflag |= PARENB;
		newtio.c_cflag &= ~PARODD;
		newtio.c_iflag |= (INPCK | ISTRIP);
	}
	else
	{
		return -1;
	}

	/* Set stop bits */
	if (sbit == '1') newtio.c_cflag &= ~CSTOPB;
	else if (sbit == '2') newtio.c_cflag |= CSTOPB;
	else return -1;

	/* Set baudrate */
	int baudrate = serial_baud_rate_macro(rate);
	if (baudrate < 0)
	{
		return -1;
	}
	else
	{
		cfsetispeed(&newtio, baudrate);
		cfsetospeed(&newtio, baudrate);
	}

	int fd = open(path, O_RDWR);
	if (fd < 0)
	{
		return -1;
	}

	if (tcsetattr(fd, TCSANOW, &newtio) < 0)
	{
		close(fd);
		return -1;
	}

	return fd;
}
