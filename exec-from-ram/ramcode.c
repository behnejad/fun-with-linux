#include <unistd.h>
int main()
{
	write(1, "hello from ram\n", 15);
	sleep(100);
	write(1, "end of ram\n", 11);
	return 0;
}
