#include <cstdlib>    /* for exit */
#include <iostream>
#include <sys/epoll.h>
#include "web_server.h"

int main (int argc, char **argv)
{
    char *ip = NULL, *dir = NULL;
    int port = 0;


    int opt = getopt( argc, argv, "h:p:d:" );
    if( opt == -1 )
    {
        std::cout << "Usage:" << argv[0] << " -h <ip> -p <port> -d <server_directory>" << std::endl;
        return EXIT_SUCCESS;
    }
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
                std::cout << "Usage:" << argv[0] << " -h <ip> -p <port> -d <server_directory>" << std::endl;
                return EXIT_FAILURE;
        }

        opt = getopt( argc, argv, "h:p:d:" );
    }

    if( (ip == NULL) || (dir == NULL) || (port <= 0) )
    {
        std::cout << "Usage:" << argv[0] << " -h <ip> -p <port> -d <server_directory>" << std::endl;
        return EXIT_FAILURE;
    }

    openlog("web_server_daemon", LOG_PID, LOG_DAEMON);
    if( daemon(0,0) == -1 )
    {
        syslog(LOG_CRIT, std::string("Create daemon error:" + std::string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    int masterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( masterSocket == 1 )
    {
        syslog(LOG_CRIT, std::string("Create socket error:" + std::string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(ip);
    sockAddr.sin_port = htons(port);

    if( bind(masterSocket, (struct sockaddr *)(&sockAddr), sizeof(sockAddr)) == -1 )
    {
        syslog(LOG_CRIT, std::string("Bind error:" + std::string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }
    setSocketNonblock(masterSocket);
    if( listen(masterSocket, SOMAXCONN) == -1 )
    {
        syslog(LOG_CRIT, std::string("Set listen socket state error:" + std::string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    int e_poll = epoll_create1(0);
    if( e_poll == -1 )
    {
        syslog(LOG_CRIT, std::string("Epoll create error:" + std::string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }
    struct epoll_event poll_event;
    poll_event.data.fd = masterSocket;
    poll_event.events = EPOLLIN;
    if( epoll_ctl(e_poll, EPOLL_CTL_ADD, masterSocket, &poll_event) == -1 )
    {
        syslog(LOG_CRIT, std::string("Epoll control error:" + std::string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    for(;;)
    {
        struct epoll_event poll_events[MAX_POLL_EVENTS];
        int N = epoll_wait(e_poll, poll_events, MAX_POLL_EVENTS, -1);

        for(unsigned int i=0; i < N; i++)
        {
            if(poll_events[i].data.fd == masterSocket)
            {
                int SlaveSocket = accept(masterSocket, 0, 0);
                setSocketNonblock(SlaveSocket);
                struct epoll_event poll_event;
                poll_event.data.fd = SlaveSocket;
                poll_event.events = EPOLLIN;
                epoll_ctl(e_poll, EPOLL_CTL_ADD, SlaveSocket, &poll_event);
            } else {
                pthread_t thread;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                //e_poll_ctl(e_poll, e_poll_CTL_DEL, poll_events[i].data.fd, &poll_event);
                worker_params wp;
                wp.fd = poll_events[i].data.fd;
                wp.dir = dir;
                pthread_create(&thread, &attr, &proccess, &wp);
            }
        }
    }
}
