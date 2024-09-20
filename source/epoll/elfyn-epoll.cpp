#include "elfyn/elfyn.hpp"
#include <cstdio>
#include <mutex>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/timerfd.h>
#include <unistd.h>
#include <unordered_map>

namespace elfyn {

struct epoll_impl;
static std::vector<epoll_impl*> thread_list;
static std::mutex thread_mutex;

// helpers for eventfd operations
static bool notify(int fd, uint64_t value = 1) {
    return (size_t) ::write(fd, &value, sizeof(value)) == sizeof(value);
}

static bool ack(int fd) {
    uint64_t value;
    return (size_t) ::read(fd, &value, sizeof(value)) == sizeof(value);
}

static thread_local class epoll_impl {
public:
    epoll_impl() {
        // main epoll file descriptor
        epollfd_ = epoll_create1(EPOLL_CLOEXEC);
        if (epollfd_ < 0) return;

        // event file descriptor for stopping the loop
        stopfd_ = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
        if (stopfd_ < 0) {
            ::close(epollfd_);
        }

        // always listen for stop event
        epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = stopfd_;
        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, stopfd_, &event) < 0) {
            ::close(stopfd_);
            ::close(epollfd_);
        }

        std::lock_guard<std::mutex> guard(thread_mutex);
        thread_list.push_back(this);

        running_ = true;
    }

    ~epoll_impl() {
        {
            std::lock_guard<std::mutex> guard(thread_mutex);
            for (auto it = thread_list.begin(); it != thread_list.end(); it++) {
                if (*it == this) {
                    thread_list.erase(it);
                    break;
                }
            }
        }
        while (!dispatch_.empty()) {
            remove(dispatch_.begin()->second.fd);
        }
        close(stopfd_);
        close(epollfd_);
    }

    bool add(Event &e, Handler fn, bool ack) {
        epoll_event event;
        event.events = EPOLLIN | EPOLLPRI;
        event.data.fd = e.fd();

        if (epoll_ctl(epollfd_, EPOLL_CTL_ADD, e.fd(), &event) != 0) {
            printf("Failed to add FD %d\n", e.fd());
            return false;
        }

        Dispatch d = {
            .fd = e.fd(),
            .ack = ack,
            .fn = fn
        };
        dispatch_[e.fd()] = d;
        
        return true;
    }

    bool every(Time::Interval interval, Handler fn) {
        auto timer = new Timer(interval);
        if (timer == nullptr) return false;
        timers_[timer->fd()] = timer;
        return add(*timer, fn, /*ack=*/true);
    }

    bool remove(const Event &e) {
        return remove(e.fd());
    }

    bool remove(int fd) {
        auto timer = timers_.find(fd);
        if (timer != timers_.end()) {
            timers_.erase(timer);
        }

        auto dispatch = dispatch_.find(fd);
        if (dispatch != dispatch_.end()) {
            dispatch_.erase(dispatch);
            return true;
        }

        return false;
    }

    void close(int &fd) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    bool run(Time::Interval interval) {
        const size_t max_events = 64;
        struct epoll_event events[max_events];
        bool forever = interval == Time::Interval::max();
        bool immediate = interval == Time::Interval{};

        while (forever || interval.count() >= 0) {
            Time::Instant now = Time::now();
            int ms = (forever) ? -1 :
                ((immediate) ? 0 :
                 std::chrono::duration_cast<std::chrono::milliseconds>(interval).count());

            int fd_count = epoll_wait(epollfd_, events, max_events, ms);

            for (int index = 0; index < fd_count; index++) {
                epoll_event *e = &events[index];
                int fd = e->data.fd;

                if (e->events & EPOLLHUP) {
                    printf("HUP on %d\n", fd);
                }

                // TODO: handle HUP and other exceptions

                if (fd == stopfd_) {
                    ack(fd);
                    running_ = false;
                    return false; // TODO: flush all events here?
                }

                auto found = dispatch_.find(fd);
                if (found != dispatch_.end()) {
                    auto d = found->second;
                    if (d.ack)
                        ack(d.fd);
                    d.fn();
                }
            }

            auto elapsed = Time::now() - now;
            interval -= elapsed;
        }
        return running_;
    }

    int stopfd() {
        return stopfd_;
    }

    bool stop() {
        return notify(stopfd_);
    }

private:
    int epollfd_{-1};
    int stopfd_{-1};
    bool running_{};

    struct Dispatch {
        int fd;
        bool ack;
        Handler fn;
    };
    std::unordered_map<int,Dispatch> dispatch_;

    std::unordered_map<int,Timer*> timers_;
} epoll;


/*
 * Event
 */
Event::Event(int fd, bool owned)
    : fd_(fd), owned_(owned)
{
}

int Event::fd() const {
    return fd_;
}

Event::~Event() {
    if (owned_ && fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}


/*
 * Io
 */
Io::Io(int fd, bool owned)
    : Event(fd, owned)
{
}

size_t Io::pending() const {
    size_t count = 0;
    ::ioctl(fd_, FIONREAD, &count);
    return count;
}

int Io::read(void *data, size_t sz) const {
    return ::read(fd_, data, sz);
}

bool Io::write(const void *data, size_t length) const {
    ssize_t wrote = ::write(fd_, data, length);
    return (size_t) wrote == length;
}

#ifdef ELFYN_STRING
std::string Io::read() const {
  std::string str;
  const size_t max_len = 65536; // TODO: make this an option

  size_t sz = pending();
  if (sz > max_len - 1) {
    sz = max_len - 1;
  }
  str.resize(sz + 1);

  int count = ::read(fd_, &str[0], sz);
  if (count < 0) {
    return str;
  } else if ((size_t) count < sz) {
    sz = count;
  }
  str[sz] = '\0';

  return str;
}

bool Io::write(const std::string &s) const {
    return write(s.data(), s.length());
}

#endif

/*
 * Notifier
 */
Notifier::Notifier()
    : Event(::eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE), true)
{
}

bool Notifier::notify() {
    return elfyn::notify(fd_);
}

/*
 * Timer
 */
Timer::Timer(Time::Interval i = Time::Interval{})
    : Event(timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK), true) {
    interval(i);
}

void Timer::interval(Time::Interval d) {
  interval_ = d;

  auto secs = std::chrono::duration_cast<std::chrono::seconds>(d);
  d -= secs;

  struct itimerspec its = {
      .it_interval {
          .tv_sec = secs.count(),
          .tv_nsec = d.count(),
      },
      .it_value {
          .tv_sec = secs.count(),
          .tv_nsec = d.count(),
      },
  };
  timerfd_settime(fd(), 0, &its, NULL);
}

Time::Interval Timer::interval() { return interval_; }

/*
 * Primary Event-Loop Interface
 */
bool add(Io &r, Handler fn) {
    return epoll.add(r, fn, false);
}

bool add(Timer &t, Handler fn) {
    return epoll.add(t, fn, true);
}

bool add(Notifier &w, Handler fn) {
    return epoll.add(w, fn, true);
}

bool every(Time::Interval interval, Handler fn) {
    return epoll.every(interval, fn);
}

bool run(Time::Interval timeout) {
    return epoll.run(timeout);
}

bool remove(const Event &e) {
    return epoll.remove(e);
}

bool stop() {
    return epoll.stop();
}

void quit() {
    std::vector<int> fds;
    {
        std::lock_guard<std::mutex> guard(thread_mutex);
        for (auto thread : thread_list) {
            fds.push_back(thread->stopfd());
        }
    }

    for (auto fd : fds) {
        (void) notify(fd); // ignore any closed FDs in case thread exited already
    }
}

/*
 * Time
 */

Time::Instant Time::now() {
    auto epoch = std::chrono::steady_clock::now().time_since_epoch();
    return Time::Instant(std::chrono::duration_cast<Interval>
                         (epoch));
}

static auto startup_time = Time::now();

Time::Interval Time::running() {
    auto now = Time::now();
    return Interval(std::chrono::duration_cast<Interval>
                         (now - startup_time));
}

} // namespace elfyn
