#include "elfyn/tcp/elfyn-tcp-server.hpp"
#include <cstdio>
#include <string>
#include <unistd.h>

class TelnetEcho {
public:
    TelnetEcho(unsigned int port) {
        bool ok = server.listen(port, [this](auto socket) {
            printf("Connection, echoing content...\n");
            connect(socket);
        });
        if (!ok) printf("Could not connect to port %u\n", port);
    }

    void connect(elfyn::Io &socket) {
        io = socket;

        elfyn::add(socket, [this]() {
            std::string s = io.read();
            if (s.length() == 1 && s[0] == 0) {
                printf("Disconnecting.\n");
                server.disconnect(io);
                return;
            }
            io.write("You typed: " + s);
        });

    }

    elfyn::TcpServer server;
    elfyn::Io io{-1};
};

int main(int argc, char **argv) {
    unsigned int port = 3030;
    TelnetEcho telnet(port);

    printf("Listening on port %u\n", port);

    while(elfyn::run());

    return 0;
}
