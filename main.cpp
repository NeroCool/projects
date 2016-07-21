#include <cstdlib>    /* for exit */
#include <unistd.h>    /* for getopt */
#include <iostream>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>

#define MAX_EVENTS 100

using namespace std;

int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if(-1 == (flags = fcntl(fd, F_GETFL, 0)))
        flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

int main (int argc, char **argv)
{
    int port = 0;
    char *ip = NULL, *dir = NULL;


    int opt = getopt( argc, argv, "h:p:d:" );
    while( opt != -1 )
    {
        switch( opt )
        {
            case 'h':
                ip = optarg;
                break;

            case 'p':
                port = atoi(optarg);
                break;

            case 'd':
                dir = optarg;
                break;

            case '?':
            default:
                cout << "Usage: final -h <ip> -p <port> -d <directory>" << endl;
                return EXIT_FAILURE;
        }

        opt = getopt( argc, argv, "h:p:d:" );
    }

    cout << ip << " " << port << " " << dir << endl;

    pid_t pid;
    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* On success: The child process becomes session leader */
    if (setsid() < 0)
        exit(EXIT_FAILURE);

    /* Catch, ignore and handle signals */
    //TODO: Implement a working signal handler */
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

    /* Fork off for the second time*/
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0)
        exit(EXIT_SUCCESS);

    /* Set new file permissions */
    umask(0);

    /* Change the working directory to the root directory */
    /* or another appropriated directory */
    chdir("/");

    /* Close all open file descriptors */
    int x;
    for (x = sysconf(_SC_OPEN_MAX); x>0; x--)
        close (x);

    /* Open the log file */
    openlog ("firstdaemon", LOG_PID, LOG_DAEMON);

    int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    struct sockaddr_in SockAddr;
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_addr.s_addr = inet_addr(ip);
    SockAddr.sin_port = htons(port);

    bind(MasterSocket, (struct sockaddr *)(&SockAddr), sizeof(SockAddr));
    set_nonblock(MasterSocket);
    listen(MasterSocket, SOMAXCONN);

    int EPoll = epoll_create1(0);
    struct epoll_event Event;
    Event.data.fd = MasterSocket;
    Event.events = EPOLLIN;
    epoll_ctl(EPoll, EPOLL_CTL_ADD, MasterSocket, &Event);

    for(;;)
    {
        struct epoll_event Events[MAX_EVENTS];
        int N = epoll_wait(EPoll, Events, MAX_EVENTS, -1);

        for(unsigned int i=0; i < N; i++)
        {

        }
    }
}
