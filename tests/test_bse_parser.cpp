#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <map>
#include <iomanip>
#include <cstring>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #pragma comment(lib, "ws2_32.lib")
    typedef int socklen_t;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <signal.h>
    #define SOCKET int
    #define INVALID_SOCKET -1
    #define SOCKET_ERROR -1
    #define closesocket close
#endif

std::atomic<bool> g_running(true);

#ifdef _WIN32
// Set Ctrl+C handler for Windows
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) {
        g_running = false;
        std::cout << "\nStopping..." << std::endl;
        return TRUE;
    }
    return FALSE;
}
#else
// Set Ctrl+C handler for Unix
void SignalHandler(int signal) {
    if (signal == SIGINT) {
        g_running = false;
        std::cout << "\nStopping..." << std::endl;
    }
}
#endif

void run_sniffer(const std::string& ip, int port) {
#ifdef _WIN32
    // 1. Init Winsock
    WSADATA wsaData;
    int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (res != 0) {
        std::cerr << "WSAStartup failed: " << res << std::endl;
        return;
    }
#endif

    // 2. Create Socket
    SOCKET sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed" << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    // 3. Reuse Address
    int reuse = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));

    // 4. Bind
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Bind failed" << std::endl;
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    // 5. Join Multicast
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(ip.c_str());
    mreq.imr_interface.s_addr = htonl(INADDR_ANY); // Default interface

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&mreq, sizeof(mreq)) < 0) {
        std::cerr << "Failed to join multicast group " << ip << std::endl;
        closesocket(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return;
    }

    // 6. Set Timeout (optional, so we can check g_running)
#ifdef _WIN32
    DWORD timeout = 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
#endif

    std::cout << "Listening for BSE Packets on " << ip << ":" << port << "..." << std::endl;
    std::cout << "Printing Message Codes ONLY (Raw Sniffer)" << std::endl;

    char buffer[2048];
    std::map<uint16_t, uint64_t> counts;

    while (g_running) {
        int n = recv(sock, buffer, sizeof(buffer), 0);
        
        if (n > 0) {
            if (n < 10) continue; // Too small for header

            // Offset 8-9 is MsgType (Little Endian)
            uint16_t msgType = *(uint16_t*)(buffer + 8);

            // Print it
            bool isNew = counts[msgType] == 0;
            counts[msgType]++;
            if (msgType != 2020 || isNew) {
                std::cout << "RX Code: " << msgType << std::endl;
            }
        }
    }

    closesocket(sock);
#ifdef _WIN32
    WSACleanup();
#endif
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
#else
    signal(SIGINT, SignalHandler);
#endif

    std::string ip = "239.1.2.5";
    int port = 26002;

    if (argc >= 3) {
        ip = argv[1];
        port = std::stoi(argv[2]);
    }

    run_sniffer(ip, port);
    return 0;
}
