#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"
int main(int argc, char *argv[])
{


    char buf[100];
    char *arg[MAXARG] = {0}, *q = buf; 
    int arg_num = argc - 1 <= MAXARG ? argc - 1 : MAXARG, i;
    for (i = 0; i < argc - 1 && i < MAXARG; i++) {
        arg[i] = argv[i + 1];
    }
    int arg_num_base = arg_num;
    if (read(0, q, 1) > 0) {
        char *st = q;
        if (*q != ' ') {
            q++;
        }
        while (read(0, q, 1) > 0) {
            if (*q == ' ') {
                if (q != st) {
                    *q = 0;
                    q++;
                    arg[arg_num++] = st;
                    st = q;
                }
            }
            else if (*q == '\n') {
                if (q != st) {
                    *q = 0;
                    ++q;
                    arg[arg_num++] = st;
                    st = q;
                }
                if (fork() == 0) {
                    exec(arg[0], arg);
                }
                else {
                    wait(0);
                    for (int i = arg_num_base; i < arg_num; i++) {
                        arg[i] = 0;
                    }
                    arg_num = arg_num_base;
                }
            }
            else {
                q++;
            }
        }
    }
    exit(0);
}