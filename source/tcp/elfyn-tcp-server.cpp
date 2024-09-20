#include "elfyn/tcp/elfyn-tcp-server.hpp"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

namespace elfyn {

TcpServer::TcpServer()
    : Io(::socket(AF_INET, SOCK_STREAM, 0), true)
{}

bool TcpServer::listen(Port port, Connected fn) {
    if (fd_ < 0) return false;

    struct sockaddr_in name;
    name.sin_family = AF_INET;
    name.sin_addr.s_addr = INADDR_ANY;
    name.sin_port = htons(port);
    if (::bind(fd_, (const sockaddr *) &name, sizeof(name))) {
        return false;
    }

    if (::listen(fd_, 1) == -1) {
        return false;
    }

    int optval = 1;
    ::setsockopt(fd_, SOL_SOCKET, SO_REUSEADDR,
                 &optval, sizeof(optval));

    elfyn::add(*this, [this]() {
        // only allow a single connection
        if (pending() == 0 && conn_.fd() < 0) {
            struct sockaddr in_addr;
            socklen_t in_len = sizeof(in_addr);

            int fd = ::accept(fd_, &in_addr, &in_len);
            if (fd < 0) {
                return;
            }

            int optval = 1;
            ::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));
            ::fcntl(fd, F_SETFL, ::fcntl(fd, F_GETFL, 0) | O_NONBLOCK);

            conn_ = Io(fd);
            if (fn_) {
                fn_(conn_);
            }
        }
    });

    fn_ = fn;

    return true;
}

bool TcpServer::disconnect(const Event &e) {
    int fd = conn_.fd();
    if (e.fd() != fd)
        return false;

    elfyn::remove(conn_);
    ::close(fd);
    conn_ = -1;

    return true;
}

} // namespace elfyn
