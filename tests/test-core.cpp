#include <cstdio>
#include <thread>

#include "elfyn/elfyn.hpp"

using namespace std::chrono_literals;

int main(int argc, char **argv) {
    printf("Starting...\n");

    // a thread which runs for a few iterations then stops
    std::thread second([]() {
        int count = 0;
        while (elfyn::run(500ms)) {
            printf("Second thread... %d\n", count);
            count++;
            if (count > 5) {
                elfyn::stop();
            }
        }
    });

    // an event used to signal between threads
    elfyn::Notifier note;

    // a thread which signals to the notifier that it is ready
    std::thread notifier([&note]() {
        while (elfyn::run(500ms)) {
            printf("Notifying...\n");
            note.notify();
        }
    });

    // when the thread above notifies us, this handler runs
    elfyn::add(note, []() {
        printf("Notified!\n");
    });

    // this is run every 1.5 seconds
    elfyn::every(1500ms, []() { printf("tick 1.5s!\n"); });

    // main event loop, idling every second and stopping after 10s
    int count = 10;
    while (elfyn::run(1s)) {
        printf("Main thread idle @ %lums\n", elfyn::Time::running() / 1000000l);
        if (--count <= 0)
            elfyn::stop();
    }

    printf("Quitting...\n");
    elfyn::quit();

    second.join();
    notifier.join();

    return 0;
}
