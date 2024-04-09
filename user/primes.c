#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"



int safe_fork(int pp1[2], int pp2[2]) {
    int pid = fork();
    if (pid < 0) {
        fprintf(2, "fork() error!\n");
        close(pp1[0]);
        close(pp1[1]);
        if (pp2) {
            close(pp2[0]);
            close(pp2[1]);
        }
        exit(1);
    }
    return pid;
}


int primes(int pp[2]) {
    close(pp[1]);
    int pc[2], first;
    read(pp[0], &first, 4);
    if (!first) {
        printf("final:%d exit!\n", first);
        exit(0);
    }
    else {
        printf("prime %d\n", first);
    }
    pipe(pc);
    if (safe_fork(pp, pc) == 0) {
        close(pp[0]);
        return primes(pc);
    }
    else {
        close(pc[0]);
        int rcv, rcv_num = read(pp[0], &rcv, 4);
        while (rcv_num == 4) {
            if (rcv % first != 0) {
                write(pc[1], &rcv, 4);
            }
            if (rcv == 0) {
                write(pc[1], &rcv, 4);
                close(pp[0]);
                close(pc[1]);
                wait(0);
                return first;
            }
            rcv_num = read(pp[0], &rcv, 4);
        }
        printf("read error\n");
        exit(1);
    }
}


int main(int argc, char *argv[])
{
    if (argc != 1) {
        fprintf(2, "!usage: primes\n");
        exit(1);
    }

    int pp[2], first = 2, i = 0, final = 0;
    printf("prime %d\n", first);
    pipe(pp);

    if (safe_fork(pp, 0) == 0) {
        first = primes(pp);
    }
    else {
        close(pp[0]);
        for (i = first + 1; i < 36; i++) {
            if (i % 2 != 0) {
                write(pp[1], &i, 4);
            }
        }
        write(pp[1], &final, 4);
        close(pp[1]);
        wait(0);
    }
    printf("first:%d exit!\n", first);
    exit(0);
}