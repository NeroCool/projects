#include <sstream>
#include <fstream>
#include "web_server.h"

int setSocketNonblock(int sd)
{
    int flags = -1;
#if defined(O_NONBLOCK)
    if( (flags = fcntl(sd, F_GETFL, 0)) == -1 )
        flags = 0;
    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

void* workerProccess(void* params)
{
    worker_params wp = *((worker_params*)params);
    int sd = wp.poll_event.data.fd;

    char receive_buffer[RECV_BUFFER_LENGTH] = {};
    std::string incoming_data;

    int recv_count = -1;
    /// Считывание имеющихся в сокете данных
    while( recv_count != 0 )
    {
        memset(receive_buffer, '\0', RECV_BUFFER_LENGTH);
        recv_count = recv(sd, receive_buffer, RECV_BUFFER_LENGTH, MSG_NOSIGNAL);
        if( ((recv_count == -1) && (errno == EAGAIN)) || (recv_count == 0) )
            break;

        if( (recv_count == -1) && (errno != EAGAIN) )
        {
            syslog(LOG_ERR, std::string("Recv error:" + std::string(strerror(errno))).c_str());
            shutdown(sd, SHUT_RDWR);
            epoll_ctl(wp.epoll_sd, EPOLL_CTL_DEL, sd, &wp.poll_event);
            return NULL;
        }
        incoming_data += (receive_buffer);
    }

    if( incoming_data.empty() )
    {
        return NULL;
    } else {
        std::stringstream ss;
        /// Обработка заголовка
        std::string::size_type get_pos = incoming_data.find("GET", 0);
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
                /// Считывание содержимого запрошенного файла, если таковой существует
                std::ifstream in( std::string(wp.dir + request_filename).c_str(), std::ifstream::ate );
                if(in)
                {
                    in.seekg(0, std::ios::end);
                    std::ifstream::pos_type length = in.tellg();
                    in.seekg(0, std::ios::beg);
                    char *buffer = new char[length];
                    in.read(buffer, length);
                    in.close();

                    /// Создание ответа "HTTP/1.0 200 OK"
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
                    /// Создание ответа "HTTP/1.0 404 NOT FOUND"
                    ss << "HTTP/1.0 404 NOT FOUND";
                    ss << "\r\n";
                    ss << "Content-length: ";
                    ss << 0;
                    ss << "\r\n";
                    ss << "Content-Type: text/html";
                    ss << "\r\n\r\n";
                }
                send(sd, ss.str().c_str(), ss.str().size(), MSG_NOSIGNAL);
                shutdown(sd, SHUT_RDWR);
                epoll_ctl(wp.epoll_sd, EPOLL_CTL_DEL, sd, &wp.poll_event);
            } else {
                /// Создание ответа "HTTP/1.0 400 BAD REQUEST"
                ss << "HTTP/1.0 400 BAD REQUEST";
                ss << "\r\n";
                ss << "Content-length: ";
                ss << 0;
                ss << "\r\n";
                ss << "Content-Type: text/html";
                ss << "\r\n\r\n";
                send(sd, ss.str().c_str(), ss.str().size(), MSG_NOSIGNAL);
                shutdown(sd, SHUT_RDWR);
                epoll_ctl(wp.epoll_sd, EPOLL_CTL_DEL, sd, &wp.poll_event);
            }
        } else {
            /// Создание ответа "HTTP/1.0 400 BAD REQUEST"
            ss << "HTTP/1.0 400 BAD REQUEST";
            ss << "\r\n";
            ss << "Content-length: ";
            ss << 0;
            ss << "\r\n";
            ss << "Content-Type: text/html";
            ss << "\r\n\r\n";
            send(sd, ss.str().c_str(), ss.str().size(), MSG_NOSIGNAL);
            shutdown(sd, SHUT_RDWR);
            epoll_ctl(wp.epoll_sd, EPOLL_CTL_DEL, sd, &wp.poll_event);
        }
    }
}
