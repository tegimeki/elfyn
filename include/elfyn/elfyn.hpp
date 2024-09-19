/*
 * elfyn event-loop framework
 */
#pragma once

#include <functional>
#include <chrono>

#ifdef ELFYN_STRING
#include <string>
#endif

namespace elfyn {

// event handler (NOTE: on heapless systems, maximum of 1-2 captures)
using Handler = std::function<void()>;

class Time {
public:
    using Interval = std::chrono::nanoseconds;
    using Instant = std::chrono::time_point<Interval>;

    static Time::Instant now();   // since epoch
    static Interval running(); // since start of application
};

// base event class
class Event {
public:
    int fd(); // TODO: abstract identifier for porting

protected:
    Event(int fd, bool owned = false);
    virtual ~Event();

    int fd_;
    bool owned_;
};

// a readable event source, e.g. socket
class Readable : public Event {
public:
    Readable(int fd, bool owned = false);

    size_t pending();
    int read(void *, size_t);

#ifdef ELFYN_STRING
    std::string read();
#endif
};

// a waitable event for cross-thread notifications
class Waitable : public Event {
public:
    Waitable();

    bool notify();
};

// timer with updateable interval
class Timer : public Event {
public:
    Timer(Time::Interval);

    void interval(Time::Interval);
    Time::Interval interval();

private:
    Time::Interval interval_;
};

//
// primary event-loop interface
//

// watch for events
bool add(Readable &, Handler);
bool add(Waitable &, Handler);
bool add(Timer &, Handler);

// periodic events
bool every(Time::Interval, Handler);

// deferred (single-shot) events
// after(Time::Interval, Handler);

bool remove(Event&);

// run the event-loop (indefinitely, or for some time)
bool run(Time::Interval timeout = Time::Interval::max());

// stop this event loop
bool stop();

// stop all event loops
void quit();

} // namespace elfyn
