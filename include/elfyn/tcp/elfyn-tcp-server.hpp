/*
 * elfyn event-loop framework
 */
#pragma once

#include "elfyn/elfyn.hpp"

namespace elfyn {

class TcpServer : public Io {
public:
    TcpServer();

    using Port = unsigned short;
    using Connected = std::function<void(Io)>;

    bool listen(Port, Connected);

    bool disconnect(const Event&);

private:
    Connected fn_;
    Io conn_{-1};
};

} // namespace elfyn
