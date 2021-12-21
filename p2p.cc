#ifndef P2P_HH
#define P2P_HH

#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <stdlib.h>

using namespace std;

class p2p
{
    private:
        int sock;
    public:
        p2p(uint16_t port, const char *addr);
        p2p();
        ~p2p();
        void send_data(const void *data, size_t len);
};

// receiver variant (rho)
p2p::p2p()
{
    
}

// sender variant (sigma)
p2p::p2p(uint16_t port, const char *addr)
{
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        cerr << "Failed to create socket" << endl;
        exit(1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        cerr << "Unable to set address" << endl;
        exit(1);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Failed to connect, error " << errno << endl;
        exit(1);
    }
}

p2p::~p2p()
{
}

void p2p::send_data(const void *data, size_t len)
{
    if (send(sock, data, len, 0) < 0) {
        cerr << "Failed to send, error " << errno << endl;
        exit(1);
    }
}

#endif

int main() {
    cout << "Hello, world" << endl;
    return 0;
}