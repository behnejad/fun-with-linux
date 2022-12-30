#include <unistd.h>
#include <stdio.h>

int main()
{
	printf("hello from ram %ld\n", getpid());
	sleep(100);
	write(1, "end of ram\n", 11);
	return 0;
}
