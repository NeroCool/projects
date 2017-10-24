#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <sstream>
#include <iostream>
#include <arpa/inet.h>
#include <sys/epoll.h>


#define MAX_POLL_EVENTS    100     /// Масимальное число получаемых за раз событий epoll
#define RECV_BUFFER_LENGTH 1024    /// Размер буфера для считывания данных из сокета

using std::string;
using std::ostringstream;
using std::stringstream;
using std::cout;
using std::endl;
using std::ios;
using std::ifstream;

/**
 * @brief The worker_params struct Параметры для обрабатывающего запрос потока
 */
struct worker_params {
    struct epoll_event poll_event; /// Структура с событием
    int epoll_sd;                  /// Дескриптор epoll
    const char *dir;               /// Корневая директория сервера
};

/**
 * @brief setSocketNonblock Установка сокета в неблокирующий режим
 * @param sd Дескриптор сокета
 * @return Код возврата функции установки сокета в неблокирующий режим
 */
int setSocketNonblock (int sd);

/**
 * @brief workerProccess Поток, обрабатывающий клиентский запрос
 * @param params
 * @return NULL в случае ошибки
 */
void* workerProccess (void* params);

/**
 * Шаблонная функция для конвертации числа в строку
 * @param number Число
 * @return Число в строковом представлении
 */
template <typename T> static string numberToString(T number)
{
    ostringstream ss;
    ss << number;
    return ss.str();
}
