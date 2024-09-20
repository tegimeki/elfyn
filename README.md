# ✨ elfyn
Event-loop framework for C++ applications.

## 🎯 Goals
* Simple, ergonomic API
* Cross-platform (epoll, kqueue [TODO], freertos [TODO])
* Heapless runtime-capable (based on std::function optimizations)
* Minimal dependencies, only requiring libstdc++
* Trivial build integration

## 🧝 Usage
The `elfyn` namespace declares a few types:

* `Event`, which is a generic wrapper on a file-descriptor or
other OS-specific identifier for I/O operations.  There are
specialized subclasses for:
  * `Io`, which is useful on sockets, files, etc.
  * `Timer`, providing adjustable intervals for periodic actions
  * `Notifier`, which can be used for signaling between threads

The application can use `elfyn::add()` for each event to be
handled, and other methods such as `elfyn::every()` and `elfyn::after()`
allow periodic actions to be initiated without declaring timer
events.

The `run()` method transfers control to the event-loop for the
specified amount of time (or indefinitely, if not specified).
When `run()` returns `false`, the event-loop has stopped and
any cleanup can be done prior to exiting the application or
thread.  Multi-threaded applications should call `quit()` to
signal that event-loops outside of the main thread should
be shut-down.

## 🛠️  Building
A Makefile is provided which uses the `maketh` set of macros, and
you can do the same in your project - or, simply compile the handful
of sources required for your platform.  See `Makefile` for details;
the basic list is `inc/elfyn/elfyn.hpp` and the `.cpp` file for the
desired event back-end (e.g. `src/epoll/elfyn-epoll.cpp`).

## ⌛️ Examples / Tests
See the `test/` subdirectory (more to be added)

## License
Creative Commons (CC BY) Michael Fairman <mfairman@tegimeki.com>
