#include <errno.h>
#include <unistd.h>
#include <sys/mman.h>

/*
objcopy --input binary --output elf64-x86-64  busybox-x86_64 busybox-x86_64.o
objdump -t x86_64.o
*/

/*
cmake:
cmake_minimum_required(VERSION 3.16)
set (CMAKE_C_FLAGS "-fPIC") 
set (CMAKE_CXX_FLAGS "-fPIC")
project(attach-bin)
SET(OBJS busybox-x86_64.o)
set_source_files_properties(
        ${OBJS}
        PROPERTIES
        EXTERNAL_OBJECT true
        GENERATED true
)
add_executable(attach-bin ${OBJS} attach-bin.c)
target_link_libraries(attach-bin dl m c)
*/

extern const char _binary_busybox_x86_64_start[];
extern const char _binary_busybox_x86_64_end[];

void call(char *const args_list[])
{
    if (fork() != 0)
    {
        return;
    }

    const char *ptr = _binary_busybox_x86_64_start;
    int fd = memfd_create("_", MFD_CLOEXEC);
    if (fd > 0)
    {
        while (ptr != _binary_busybox_x86_64_end)
        {
            write(fd, ptr, 1);
            ++ptr;
        }


        char *const env_list[] = {NULL};

        printf("return code: %d\n", fexecve(fd, args_list, env_list));
        printf("errno: %d\n", errno);

        close(fd);
    }
}


int main(int argc, const char * argv[], char **envp)
{
    char *const args_list[] = {"uname", "-a", NULL};
    int x = 0;
    while (1)
    {
        printf("\rAlive: %d", x++);
        fflush(stdout);
        if (x % 5 == 0)
            call(args_list);
        sleep(1);
    }
}
