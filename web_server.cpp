#include "web_server.h"

int setSocketNonblock(int fd)
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

void* proccess(void* fd_void)
{
    worker_params wp = *((worker_params*)fd_void);
    int fd = wp.fd;

    static char Buffer[1024] = {};
    int RecvResult = recv(fd, Buffer, 1024, MSG_NOSIGNAL);
    if( (RecvResult == 0) && (errno != EAGAIN) )
    {
        shutdown(fd, SHUT_RDWR);
        close(fd);
    } else if (RecvResult > 0) {
        std::string incoming_data(Buffer);
        std::string::size_type get_pos = incoming_data.find("GET", 0);
        //syslog(LOG_NOTICE, string("GET_POS:" + string(SSTR(get_pos).c_str())).c_str());
        if( get_pos != std::string::npos )
        {
            std::string::size_type http_pos = incoming_data.find("HTTP/1.0", 0);
            if( http_pos != std::string::npos )
            {
                std::string::size_type question_pos = incoming_data.find("?", 0);
                std::string request_filename;
                if( question_pos == std::string::npos )
                {
                    request_filename = incoming_data.substr(get_pos + 4, http_pos - get_pos - 5);
                } else {
                    request_filename = incoming_data.substr(get_pos + 4, question_pos - get_pos - 4);
                }
                std::stringstream ss;
                syslog(LOG_NOTICE, std::string(wp.dir + request_filename).c_str());
                std::ifstream in( std::string(wp.dir + request_filename).c_str(), std::ifstream::ate );
                if(in)
                {
                    in.seekg(0, std::ios::end);    // go to the end
                    std::ifstream::pos_type length = in.tellg();           // report location (this is the length)
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
