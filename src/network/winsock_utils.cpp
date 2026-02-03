#include "network/tcp_client.h"
#include <stdexcept>
#include <cstring>

#ifdef _WIN32
static bool g_winsock_init = false;
#endif

bool init_winsock() {
#ifdef _WIN32
    if (g_winsock_init) return true;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    g_winsock_init = true;
    return true;
#else
    return true;
#endif
}

void cleanup_winsock() {
#ifdef _WIN32
    if (g_winsock_init) {
        WSACleanup();
        g_winsock_init = false;
    }
#endif
}
