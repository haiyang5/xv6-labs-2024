#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    if (argc != 1)
    {
        fprintf(2, "!usage: pingpong\n");
        exit(1);
    }

    int pp[2], pc[2];
    pipe(pp);
    pipe(pc);
    char r;

    if (fork() == 0) {
        int pid = getpid();
        close(0);
        dup(pp[0]);
        close(pp[0]);
        close(pp[1]);
        close(pc[0]);
        read(0, &r, 1);
        fprintf(1, "%d: received ping\n", pid);

        write(pc[1],&r, 1);    }
    else {
        int pid = getpid();
        close(0);
        dup(pc[0]);
        close(pc[0]);
        close(pc[1]);
        close(pp[0]);
        write(pp[1],"x", 1);
        read(0, &r, 1);
        fprintf(1, "%d: received pong\n", pid);
    }

    exit(0);
}