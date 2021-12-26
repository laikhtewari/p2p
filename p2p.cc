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
#include <cstring>

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
        int tx_sock;
        int rx_sock;
        int outgoing_socket(int socket_type, uint16_t port, const char *addr, int flags);
        int incoming_socket(uint16_t port, unsigned int addr, int flags);
    public:
        ~p2p();
        void establish_outgoing(uint16_t port, const char *addr);
        void establish_incoming(uint16_t port, unsigned int addr);
        void send_stun(bool verbose);
        void send_data(const void *data, size_t len);
        size_t read_data(void *buf, size_t len);
        int get_tx_sock() { return tx_sock; }
};

/*
// receiver variant (rho)
p2p::p2p()
{
    send_stun(false);

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == 0)
    {
        cerr << "Setting up socket failed" << endl; 
        exit(1);
    }

    struct sockaddr_in serv;
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = INADDR_ANY;
    serv.sin_port = 0;

    if (bind(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0)
    {
        cerr << "Failed to bind socket" << endl;
        exit(1);
    }

    if (listen(sock, 1) < 0) {
        cerr << "Failed to set socket listening" << endl;
        exit(1);
    }

    int recv_sock = accept(sock, NULL, NULL);

    char buf[1024];
    while (true) {
        for (size_t i = 0; i < recv(recv_sock, &buf, 1024, 0); i++) {
            cout << buf[i];
        }
    }
    
}
*/

/*
// sender variant (sigma)
p2p::p2p(const char *addr, uint16_t port)
{
    sock = setup_socket(SOCK_STREAM, port, addr);
}
*/

p2p::~p2p()
{
    close(tx_sock);
    close(rx_sock);
}

void p2p::send_data(const void *data, size_t len)
{
    if (send(tx_sock, data, len, 0) < 0) {
        cerr << "Failed to send, error " << errno << endl;
        exit(1);
    }
}

size_t p2p::read_data(void *buf, size_t len) {
    return recv(rx_sock, buf, len, 0);
}

void p2p::send_stun(bool verbose)
{
    int stun_sock = outgoing_socket(SOCK_DGRAM, STUN_PORT, STUN_ADDR, 0);

    STUNRequestHeader req = STUNRequestHeader();
    if (sizeof(req) != 20) {
        cerr << "Incorrect header size: " << sizeof(req) << endl;
        exit(1);
    }
    send(stun_sock , &req, sizeof(req), 0);

    const size_t buf_size = 128;
    unsigned char buf[buf_size];
    size_t n_recvd;
    if ((n_recvd = recv(stun_sock, &buf, buf_size, 0)) < 0) {
        cerr << "Error in receiving from socket: " << errno << endl;
        exit(1);
    }

    if (verbose) {
        cout << "DUMP:" << endl;
        for (size_t i = 0; i < buf_size && i < n_recvd; i++) {
            printf("%02x ", buf[i]);
        }
        cout << endl << endl;
    }

    STUNRequestHeader *res = (STUNRequestHeader*)buf;
    if (ntohs(res->message_type) != 0x0101) {
        cerr << "Did not receive a binding response" << endl;
        exit(1);
    }

    if (verbose) {
        cout << "STUN response length: " << ntohs(res->message_length) << " bytes" << endl;
    }

    // could check recvd tid against sent tid for completeness

    unsigned short attr_type = ntohs(*(short *)(buf + 20));
    bool is_xor = false;
    if (attr_type == MAP_ADDR_ATTR) {
        if (verbose) cout << "Received mapped address" << endl;
        is_xor = false;
    } else if (attr_type == XOR_MAP_ADDR_ATTR) {
        if (verbose) cout << "Received XOR mapped address" << endl;
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
    unsigned short port = ntohs(attr->port) ^ (mask >> 16);
    in_addr addr;
    addr.s_addr = htonl(ntohl(attr->addr) ^ mask);
    char addr_str[INET_ADDRSTRLEN];

    cout << dec << "Port: " << port << " Address: " << inet_ntop(AF_INET, &addr, addr_str, INET_ADDRSTRLEN) << endl;
    close(stun_sock);
}

int p2p::outgoing_socket(int socket_type, uint16_t port, const char *addr, int flags) {
    int _sock;
    if ((_sock = socket(AF_INET, socket_type, flags)) < 0) {
        cerr << "Unable to create socket" << endl;
        exit(1);
    }

    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &serv_addr.sin_addr) <= 0) {
        cerr << "Unable to set address" << endl;
        exit(1);
    }

    if (connect(_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        cerr << "Failed to connect to server, error " << errno << endl;
        exit(1);
    }
    return _sock;
}

int p2p::incoming_socket(uint16_t port, unsigned int addr, int flags) {
    int _sock;
    if ((_sock = socket(AF_INET, SOCK_STREAM, flags)) == 0)
    {
        cerr << "Setting up socket failed" << endl; 
        exit(1);
    }

    struct sockaddr_in cli_addr;
    cli_addr.sin_family = AF_INET;
    cli_addr.sin_addr.s_addr = addr;
    cli_addr.sin_port = port;

    if (bind(_sock, (struct sockaddr *)&cli_addr, sizeof(cli_addr)) < 0)
    {
        cerr << "Failed to bind socket" << endl;
        exit(1);
    }

    if (listen(_sock, 1) < 0) {
        cerr << "Failed to set socket listening" << endl;
        exit(1);
    }
    return _sock;
}

void p2p::establish_outgoing(uint16_t port, const char *addr) {
    tx_sock = outgoing_socket(SOCK_STREAM, port, addr, SO_REUSEADDR);
}

void p2p::establish_incoming(uint16_t port, unsigned int addr) {
    rx_sock = incoming_socket(port, addr, SO_REUSEADDR);
}

#endif

int main(int argc, char *argv[]) {
    p2p _p2p;
    _p2p.send_stun(false);

    string line;
    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    
    cout << "Enter peer address: ";
    if (!getline(cin, line) || inet_pton(AF_INET, line.c_str(), &peer_addr.sin_addr) <= 0) {
        cerr << "Unable to read address" << endl;
        exit(1);
    }
    const char *peer_addr_str = line.c_str();

    cout << "Enter peer port: ";
    if (!getline(cin, line) || !(peer_addr.sin_port = htons(strtol(line.c_str(), NULL, 10)))) {
        cerr << "Unable to read port" << endl;
        exit(1);
    }

    _p2p.establish_outgoing(peer_addr.sin_port, peer_addr_str);
    cout << "Successfully connected outbound socket!" << endl;

    sockaddr_in priv;
    socklen_t priv_size = sizeof(priv);
    getsockname(_p2p.get_tx_sock(), (sockaddr *)&priv, &priv_size);

    _p2p.establish_incoming(priv.sin_port, priv.sin_addr.s_addr);
    cout << "Successfully connected inbound socket!" << endl;

    char buf[1024];
    string to_tx;
    while (true) {
        getline(cin, to_tx);
        for (size_t i = 0; i < _p2p.read_data(buf, 1024); i++) {
            cout << buf[i];
        }
        _p2p.send_data(to_tx.c_str(), to_tx.length());
    }

    cout << "Address: " << ntohl(peer_addr.sin_addr.s_addr) << " Port: " << ntohs(peer_addr.sin_port) << endl;

    // if (argc < 2 || !(strcmp(argv[1], "receive") == 0 || strcmp(argv[1], "send") == 0)) {
    //     cout << "Please specify either receive or send" << endl;
    //     exit(1);
    // }

    // if (strcmp(argv[1], "receive") == 0) {
    //     p2p _p2p;
    // } else {
    //     if (argc < 4) {
    //         cout << "Please specify address and port" << endl;
    //         exit(1);
    //     }
    //     p2p _p2p(argv[2], strtol(argv[3], NULL, 10));
    //     while (true) {
    //         char *data = "Hello, world";
    //         _p2p.send_data(data, strlen(data));
    //     }
    // }
    return 0;
}