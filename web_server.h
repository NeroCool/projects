#include <unistd.h>    /* for getopt */
#include <fcntl.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string>
#include <errno.h>
#include <syslog.h>
#include <cstring>
#include <fstream>
#include <sstream>


#define SSTR( x ) static_cast< std::ostringstream & >( \
        ( std::ostringstream() << std::dec << x ) ).str()

#define MAX_POLL_EVENTS 10000

struct worker_params {
    int fd;
    const char *dir;
};

int setSocketNonblock (int fd);
void* proccess (void* fd_void);
