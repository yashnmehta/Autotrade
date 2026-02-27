#ifndef SOCKET_PLATFORM_H
#define SOCKET_PLATFORM_H

#include <string>

#ifdef _WIN32
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    #pragma comment(lib, "ws2_32.lib")
    
    #ifndef __MINGW32__
        typedef int ssize_t;
    #endif

    typedef SOCKET socket_t;
    #define socket_close closesocket
    #define socket_invalid INVALID_SOCKET

    #define socket_errno WSAGetLastError()
    
    #ifndef EAGAIN
        #define EAGAIN WSAEWOULDBLOCK
    #endif
    #ifndef EWOULDBLOCK
        #define EWOULDBLOCK WSAEWOULDBLOCK
    #endif

    inline std::string socket_error_string(int err) {

        char* s = NULL;
        FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, 
                       NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, NULL);
        std::string result = s ? s : "Unknown error";
        if (s) LocalFree(s);
        return result;
    }
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <netdb.h>
    #include <cerrno>
    #include <cstring>
    typedef int socket_t;
    #define socket_close close
    #define socket_invalid -1

    #define socket_errno errno

    inline std::string socket_error_string(int err) {
        return std::string(std::strerror(err));
    }
#endif


// Helper for one-time initialization of Winsock on Windows
class WinsockLoader {
public:
    WinsockLoader() {
#ifdef _WIN32
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    }
    ~WinsockLoader() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
    
    static void init() {
        static WinsockLoader loader;
    }
};

// Cross-platform socket timeout helper
// Windows uses DWORD (milliseconds), Unix/Linux/macOS use struct timeval
inline int set_socket_timeout(socket_t sockfd, int seconds) {
#ifdef _WIN32
    DWORD timeout = seconds * 1000;  // Convert to milliseconds
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;
    return setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
}

#endif // SOCKET_PLATFORM_H
