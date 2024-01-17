#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#ifndef INIT_SYS 
    #define INIT_SYS "/bin/sh"
#endif


char *const env[] = {
    "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin",
    "TERM=xterm-256color",
    NULL
};

 
int main(int argc, char** argv){
    printf("Starting /sbin/init...\n");

    /* Actual init system starts as a PID 1 */
    printf("/sbin/init: Starting %s...\n", INIT_SYS);
    execle(INIT_SYS, INIT_SYS, NULL, env);
    perror("execle() failed");

    return 1;
}