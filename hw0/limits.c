#include <stdio.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    int resources[] = {RLIMIT_NOFILE, RLIMIT_NPROC, RLIMIT_STACK};
    getrlimit(RLIMIT_STACK, &lim);
    printf("stack size: %ld\n", lim.rlim_cur);
    getrlimit(RLIMIT_NPROC, &lim);
    printf("process limit: %ld\n", lim.rlim_cur);
    getrlimit(RLIMIT_NOFILE, &lim);
    printf("max file descriptors: %ld\n", lim.rlim_cur);
    return 0;
}
