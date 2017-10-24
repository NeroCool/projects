#include <cstdlib>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include "web_server.h"


int main (int argc, char **argv)
{
    char *ip = NULL, *dir = NULL;
    int port = 0;


    /// Обработка входных параметров программы
    int opt = getopt( argc, argv, "h:p:d:" );
    if( opt == -1 )
    {
        cout << "Usage:" << argv[0] << " -h <ip> -p <port> -d <server_directory>" << endl;
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
                cout << "Usage:" << argv[0] << " -h <ip> -p <port> -d <server_directory>" << endl;
                return EXIT_FAILURE;
        }

        opt = getopt( argc, argv, "h:p:d:" );
    }

    if( (ip == NULL) || (dir == NULL) || (port <= 0) )
    {
        cout << "Usage:" << argv[0] << " -h <ip> -p <port> -d <server_directory>" << endl;
        return EXIT_FAILURE;
    }
    /// Превращение программы в службу
    openlog("web_server_daemon", LOG_PID, LOG_DAEMON);
    /// Превращение программы в службу
    if( daemon(0,0) == -1 )
    {
        syslog(LOG_CRIT, string("Create daemon error:" + string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    /// Создание принимающего запросы сокета
    int master_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if( master_socket == 1 )
    {
        syslog(LOG_CRIT, string("Create mastersocket error:" + string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    struct sockaddr_in sockAddr;
    sockAddr.sin_family = AF_INET;
    sockAddr.sin_addr.s_addr = inet_addr(ip);
    sockAddr.sin_port = htons(port);

    if( bind(master_socket, (struct sockaddr *)(&sockAddr), sizeof(sockAddr)) == -1 )
    {
        syslog(LOG_CRIT, string("Bind error:" + string(strerror(errno))).c_str());
        return EXIT_FAILURE;
    }

    /// Установка сокета в неблокирующий режим
    if( setSocketNonblock(master_socket) )
    {
        syslog(LOG_CRIT, string("Set nonblock socket state error:" + string(strerror(errno))).c_str());
        shutdown(master_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }

    if( listen(master_socket, SOMAXCONN) == -1 )
    {
        syslog(LOG_CRIT, string("Set listen socket state error:" + string(strerror(errno))).c_str());
        shutdown(master_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }

    /// Создание epoll сокета для использования мультиплексирования
    int e_poll = epoll_create1(0);
    if( e_poll == -1 )
    {
        syslog(LOG_CRIT, string("Epoll create error:" + string(strerror(errno))).c_str());
        shutdown(master_socket, SHUT_RDWR);
        return EXIT_FAILURE;
    }
    struct epoll_event poll_event;
    poll_event.data.fd = master_socket;
    poll_event.events = EPOLLIN;
    if( epoll_ctl(e_poll, EPOLL_CTL_ADD, master_socket, &poll_event) == -1 )
    {
        syslog(LOG_CRIT, string("Epoll control error:" + string(strerror(errno))).c_str());
        shutdown(master_socket, SHUT_RDWR);
        close(e_poll);
        return EXIT_FAILURE;
    }

    syslog(LOG_INFO, "Web server start listening...");
    for(;;)
    {
        struct epoll_event poll_events[MAX_POLL_EVENTS];
        /// Ожидание событий
        int poll_events_count = epoll_wait(e_poll, poll_events, MAX_POLL_EVENTS, -1);
        if( poll_events_count == -1 )
        {
            syslog(LOG_ERR, string("Epoll wait error:" + string(strerror(errno))).c_str());
            shutdown(master_socket, SHUT_RDWR);
            close(e_poll);
            return EXIT_FAILURE;
        }

        for( unsigned int i=0; i < poll_events_count; i++ )
        {
            if( poll_events[i].data.fd == master_socket )
            {
                int slave_socket = accept(master_socket, 0, 0);
                if( setSocketNonblock(slave_socket) == -1 )
                {
                    syslog(LOG_ERR, string("Set nonblock socket state error:" + string(strerror(errno))).c_str());
                    shutdown(master_socket, SHUT_RDWR);
                    close(e_poll);
                    return EXIT_FAILURE;
                }

                struct epoll_event poll_event;
                poll_event.data.fd = slave_socket;
                poll_event.events = EPOLLIN;
                if( epoll_ctl(e_poll, EPOLL_CTL_ADD, slave_socket, &poll_event) == -1 )
                {
                    syslog(LOG_ERR, string("Epoll control error:" + string(strerror(errno))).c_str());
                    shutdown(master_socket, SHUT_RDWR);
                    close(e_poll);
                    return EXIT_FAILURE;
                }
            } else {
                pthread_t thread;
                pthread_attr_t attr;
                int ret_code = pthread_attr_init(&attr);
                if( ret_code != 0 )
                {
                    syslog(LOG_ERR, string("Init thread attr errorcode:" + numberToString(ret_code)).c_str());
                    shutdown(master_socket, SHUT_RDWR);
                    close(e_poll);
                    return EXIT_FAILURE;
                }
                /// Установка типа потока как отсоединённого
                ret_code = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                if( ret_code != 0 )
                {
                    syslog(LOG_ERR, string("Set thread detach state errorcode:" +numberToString(ret_code)).c_str());
                    shutdown(master_socket, SHUT_RDWR);
                    close(e_poll);
                    return EXIT_FAILURE;
                }
                worker_params wp;
                wp.poll_event = poll_events[i];
                wp.dir = dir;
                wp.epoll_sd = e_poll;
                /// Создание отсоединённого потока для обработки клиентского запроса
                ret_code = pthread_create(&thread, &attr, &workerProccess, &wp);
                if( ret_code == EAGAIN )
                {
                    syslog(LOG_ERR, "Not enough resources to create thread");
                    shutdown(master_socket, SHUT_RDWR);
                    close(e_poll);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    return EXIT_SUCCESS;
}
