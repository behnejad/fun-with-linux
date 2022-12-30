#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>

int main()
{
	char temp_name[] = "tempXXXXXX";
	int pid = getpid();
	printf("pid: %d\n", pid);
	//int temp = mkstemp(temp_name);
	int temp = memfd_create(temp_name, MFD_CLOEXEC);
	if (temp < 0)
		return -1;
	printf("temp: %s %d\n", temp_name, temp);
	//printf("unlink: %d %d\n", unlink(temp_name), errno);
	int fd = open("a.out", O_RDONLY);
	if (fd < 0) 
	{
		printf("a.out open failed\n");
		close(temp);
		return -1;
	}
	char buffer[100];
	while (1)
	{
		int r = read(fd, buffer, 100);
		if (r > 0)
			write(temp, buffer, r);
		else if (r == 0)
			break;
		else
		{
			printf("read error\n");
			break;
		}

	}
	close(fd);
	fchmod(temp, 0700);
	char path[200];
	sprintf(path, "/proc/self/fd/%d", temp);
	printf("start\n");
	printf("exec: %d\n", execve(path, NULL, NULL));	
	printf("end\n");
	sleep(100);
	close(temp);
	return 0;
}
