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
#include <string>
#include <errno.h>
#include <syslog.h>
#include <fstream>
#include <sstream>


#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

#define MAX_EVENTS 1000

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

static char *ip = NULL, *dir = NULL;

void* proccess(void* fd_void)
{
    int fd = *((int*)fd_void);
    static char Buffer[1024] = {};
    int RecvResult = recv(fd, Buffer, 1024, MSG_NOSIGNAL);
    if( (RecvResult == 0) && (errno != EAGAIN) )
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    } else if (RecvResult > 0) {
        string incoming_data(Buffer);
        string::size_type get_pos = incoming_data.find("GET", 0);
        //syslog(LOG_NOTICE, string("GET_POS:" + string(SSTR(get_pos).c_str())).c_str());
        if( get_pos != string::npos )
        {
            string::size_type http_pos = incoming_data.find("HTTP/1.0", 0);
            if( http_pos != string::npos )
            {
                string::size_type question_pos = incoming_data.find("?", 0);
                string request_filename;
                if( question_pos == string::npos )
                {
                    request_filename = incoming_data.substr(get_pos + 4, http_pos - get_pos - 5);
                } else {
                    request_filename = incoming_data.substr(get_pos + 4, question_pos - get_pos - 4);
                }
                std::stringstream ss;
                syslog(LOG_NOTICE, string(dir + request_filename).c_str());
                ifstream in( string(dir + request_filename).c_str(), ifstream::ate );
                if(in)
                {
                    in.seekg(0, std::ios::end);    // go to the end
                    ifstream::pos_type length = in.tellg();           // report location (this is the length)
                    in.seekg(0, std::ios::beg);    // go back to the beginning
                    char *buffer = new char[length];    // allocate memory for a buffer of appropriate dimension
                    in.read(buffer, length);       // read the whole file into the buffer
                    in.close();

                    // Create a result with "HTTP/1.0 200 OK"
                    ss << "HTTP/1.0 200 OK";
                    ss << "\r\n";
                    ss << "Content-length: ";
                    ss << length;
                    ss << "\r\n";
                    ss << "Content-Type: text/html";
                    ss << "\r\n\r\n";
                    ss << buffer;
                    delete buffer;
                } else {
                    // Create a result with "HTTP/1.0 404 NOT FOUND"
                    ss << "HTTP/1.0 404 NOT FOUND";
                    ss << "\r\n";
                    ss << "Content-length: ";
                    ss << 0;
                    ss << "\r\n";
                    ss << "Content-Type: text/html";
                    ss << "\r\n\r\n";
                }
                send(fd, ss.str().c_str(), ss.str().size(), MSG_NOSIGNAL);
                shutdown(fd, SHUT_RDWR);
                close(fd);
            } else {
                //error
            }
        } else {
                //error
        }
    }
}

int main (int argc, char **argv)
{
    int port = 0;

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
    openlog ("web_server_daemon", LOG_PID, LOG_DAEMON);

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
            if(Events[i].data.fd == MasterSocket)
            {
                int SlaveSocket = accept(MasterSocket, 0, 0);
                set_nonblock(SlaveSocket);
                struct epoll_event Event;
                Event.data.fd = SlaveSocket;
                Event.events = EPOLLIN;
                epoll_ctl(EPoll, EPOLL_CTL_ADD, SlaveSocket, &Event);
            } else {
                pthread_t thread;
                pthread_attr_t attr;
                pthread_attr_init(&attr);
                pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
                int fd = Events[i].data.fd;
                //epoll_ctl(EPoll, EPOLL_CTL_DEL, Events[i].data.fd, &Event);
                pthread_create(&thread, &attr, &proccess, &fd);
            }
        }
    }
}
