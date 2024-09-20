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
    int fd() const;

protected:
    Event(int fd, bool owned = false);
    virtual ~Event();

    int fd_;
    bool owned_;
};

// a readable/writable event source, e.g. socket or file
class Io : public Event {
public:
    Io(int fd, bool owned = false);

    size_t pending() const;
    int read(void *, size_t) const;

    bool write(const void *, size_t) const;

#ifdef ELFYN_STRING
    std::string read() const;
    bool write(const std::string &) const;
#endif
};

// a waitable event for cross-thread notifications
class Notifier : public Event {
public:
    Notifier();

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
bool add(Io &, Handler);
bool add(Notifier &, Handler);
bool add(Timer &, Handler);

// periodic events
bool every(Time::Interval, Handler);

// deferred (single-shot) events
// after(Time::Interval, Handler);

bool remove(Event const&);

// run the event-loop (indefinitely, or for some time)
bool run(Time::Interval timeout = Time::Interval::max());

// stop this event loop
bool stop();

// stop all event loops
void quit();

} // namespace elfyn
