#ifndef P2P_HH
#define P2P_HH

#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>

using namespace std;

#define STUN_PORT 19302
#define STUN_ADDR "74.125.197.127"

struct STUNRequestHeader {
    const unsigned short message_type;
    const unsigned short message_length;
    const unsigned int tid_0;
    const unsigned int tid_1;
    const unsigned int tid_2;
    const unsigned int tid_3;

    STUNRequestHeader(bool is_new=false, const unsigned short msg_length=0): 
                                            message_type(htons(0x0001)),
                                            message_length(is_new ? htons(msg_length) : htons(msg_length + 20)),
                                            tid_0(is_new ? htons(0x2112A442) : htons(rand() % UINT_MAX)), 
                                            tid_1(htons(rand() % UINT_MAX)),
                                            tid_2(htons(rand() % UINT_MAX)),
                                            tid_3(htons(rand() % UINT_MAX)) {}
};

class p2p
{
    private:
        int sock;
    public:
        p2p(uint16_t port, const char *addr);
        p2p();
        ~p2p();
        void send_stun();
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

void p2p::send_stun()
{
    int stun_sock;
    if ((stun_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        cerr << "Unable to create STUN socket" << endl;
        exit(1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(STUN_PORT);
    if (inet_pton(AF_INET, STUN_ADDR, &serv_addr.sin_addr) <= 0) {
        cerr << "Unable to set STUN address" << endl;
        exit(1);
    }

    if (connect(stun_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Failed to connect to STUN server, error " << errno << endl;
        exit(1);
    }

    STUNRequestHeader req = STUNRequestHeader(true);
    if (sizeof(req) != 20) {
        cerr << "Incorrect header size: " << sizeof(req) << endl;
        exit(1);
    }

    cout << "Connected!" << endl;
    send(stun_sock , &req, sizeof(req), 0);

    char buffer[1024];
    while (true) {
        size_t num_read = read(stun_sock, buffer, 1024);
        cout << num_read << endl;
        for (size_t i = 0; i < num_read; i++) {
            cout << buffer[i];
        }
    }
}
#endif

int main() {
    cout << "Hello, world" << endl;
    // STUNRequestHeader req = STUNRequestHeader(true);
    // for (size_t i = 0; i < 20; i++) {
    //     printf("%02x ",((unsigned char*)&req)[i]);
    // }
    printf("\n");
    p2p receiver;
    receiver.send_stun();
    return 0;
}