#include <stdio.h>
#include <sys/time.h>
#include <sys/resource.h>

int main() {
    struct rlimit lim;
    getrlimit(RLIMIT_STACK, &lim);
    printf("stack size: %lld\n",  (long long) lim.rlim_cur);
    getrlimit(RLIMIT_NPROC, &lim);
    printf("process limit: %lld\n", (long long) lim.rlim_cur);
    getrlimit(RLIMIT_NOFILE, &lim);
    printf("max file descriptors: %lld\n", (long long) lim.rlim_cur);

    return 0;
}
