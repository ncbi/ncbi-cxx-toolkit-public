#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
main()
{
    int i;
    hrtime_t start, end;
    time_t t;
    start= gethrtime();
    for(i= 0x100000; i--; ) {
        signal (SIGPIPE, SIG_IGN);
        /*t= time(NULL);*/
        i=i;
    }
    end= gethrtime();
    printf("Execution time: %f\n", (end-start)/1000000.0);
}
