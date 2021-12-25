#ifndef P2P_HH
#define P2P_HH

#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <iostream>
#include <iomanip>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <stdio.h>

using namespace std;

#define STUN_PORT 19302
#define STUN_ADDR "74.125.197.127"
#define MAP_ADDR_ATTR 0x0001
#define XOR_MAP_ADDR_ATTR 0x0020
#define MAGIC_COOKIE 0x2112A442

struct STUNRequestHeader {
    const unsigned short message_type;
    const unsigned short message_length;
    const unsigned int tid_0;
    const unsigned int tid_1;
    const unsigned int tid_2;
    const unsigned int tid_3;

    STUNRequestHeader(const unsigned short msg_length=0): 
                                            message_type(htons(0x0001)),
                                            message_length(htons(msg_length)),
                                            tid_0(htonl(MAGIC_COOKIE)), 
                                            tid_1(htonl(rand() % UINT_MAX)),
                                            tid_2(htonl(rand() % UINT_MAX)),
                                            tid_3(htonl(rand() % UINT_MAX)) {}
};

struct MappedAddressAttribute {
    unsigned char pad;
    unsigned char fam;
    unsigned short port;
    unsigned int addr;
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

    STUNRequestHeader req = STUNRequestHeader();
    if (sizeof(req) != 20) {
        cerr << "Incorrect header size: " << sizeof(req) << endl;
        exit(1);
    }

    // cout << "Connected!" << endl;
    send(stun_sock , &req, sizeof(req), 0);

    const size_t buf_size = 128;
    unsigned char buf[buf_size];
    size_t n_recvd;
    if ((n_recvd = recv(stun_sock, &buf, buf_size, 0)) < 0) {
        cerr << "Error in receiving from socket: " << errno << endl;
        exit(1);
    }

    // cout << "DUMP:" << endl;
    // for (size_t i = 0; i < buf_size && i < n_recvd; i++) {
    //     printf("%02x ", buf[i]);
    // }
    // cout << endl << endl;

    STUNRequestHeader *res = (STUNRequestHeader*)buf;
    if (ntohs(res->message_type) != 0x0101) {
        cerr << "Did not receive a binding response" << endl;
        exit(1);
    }
    cout << "STUN response length: " << ntohs(res->message_length) << " bytes" << endl;

    // could check recvd tid against sent tid for completeness

    unsigned short attr_type = ntohs(*(short *)(buf + 20));
    bool is_xor = false;
    if (attr_type == MAP_ADDR_ATTR) {
        cout << "Received mapped address" << endl;
        is_xor = false;
    } else if (attr_type == XOR_MAP_ADDR_ATTR) {
        cout << "Received XOR mapped address" << endl;
        is_xor = true;
    } else {
        cerr << "Received unexpected attribute: " << std::hex << attr_type << endl;
        exit(1);
    }

    unsigned short val_length = ntohs(*(short *)(buf + 22));
    if (val_length != 0x0008) {
        cerr << "Received unexpected val length: " << std::hex << val_length << endl;
        exit(1);
    }

    MappedAddressAttribute *attr = (MappedAddressAttribute*)(buf + 24);
    if (attr->fam != 0x01) {
        cerr << "Not IPv4: " << std::hex << attr->fam << endl;
        exit(1);
    }

    unsigned int mask = is_xor ? MAGIC_COOKIE : 0;
    cout << dec << "Port: " << (ntohs(attr->port) ^ (mask >> 16)) << " Address: " << (ntohl(attr->addr) ^ mask) << endl;
}

#endif

int main() {
    p2p receiver;
    receiver.send_stun();
    return 0;
}