//  /$$$$$$$   /$$$$$$   /$$$$$$         /$$$$$$   /$$$$$$   /$$$$$$  /$$$$$$$ 
// | $$__  $$ /$$__  $$ /$$__  $$       /$$__  $$ /$$$_  $$ /$$__  $$| $$____/ 
// | $$  \ $$| $$  \__/| $$  \ $$      |__/  \ $$| $$$$\ $$|__/  \ $$| $$      
// | $$$$$$$/| $$      | $$  | $$        /$$$$$$/| $$ $$ $$  /$$$$$$/| $$$$$$$ 
// | $$____/ | $$      | $$  | $$       /$$____/ | $$\ $$$$ /$$____/ |_____  $$
// | $$      | $$    $$| $$  | $$      | $$      | $$ \ $$$| $$       /$$  \ $$
// | $$      |  $$$$$$/|  $$$$$$/      | $$$$$$$$|  $$$$$$/| $$$$$$$$|  $$$$$$/
// |__/       \______/  \______/       |________/ \______/ |________/ \______/ 

#include <gtest/gtest.h>
#include <atomic>
#include <vector>

#include <pcosynchro/pcothread.h>
#include <pcosynchro/pcosemaphore.h>

#include "sharedsection.h"
#include "sharedsectioninterface.h"

static void enterCritical(std::atomic<int>& nbIn) {
    int now = nbIn.fetch_add(1) + 1;
    ASSERT_EQ(now, 1) << "Deux locomotives dans la section en même temps !";
}
static void leaveCritical(std::atomic<int>& nbIn) {
    nbIn.fetch_sub(1);
}

TEST(SharedSection, TwoSameDirection_SerializesCorrectly) {
    SharedSection section;
    std::atomic<int> nbIn{0};
    Locomotive l1(1, 10, 0), l2(2, 10, 0);

    PcoThread t1([&]{
        section.access(l1, SharedSectionInterface::Direction::D1);
        enterCritical(nbIn);
        PcoThread::usleep(1000);
        leaveCritical(nbIn);
        section.leave(l1, SharedSectionInterface::Direction::D1);
        section.release(l1);
    });

    PcoThread t2([&]{
        PcoThread::usleep(500);
        section.access(l2, SharedSectionInterface::Direction::D1);
        enterCritical(nbIn);
        leaveCritical(nbIn);
        section.leave(l2, SharedSectionInterface::Direction::D1);
    });

    t1.join(); t2.join();
    ASSERT_EQ(section.nbErrors(), 0);
}

TEST(SharedSection, ConsecutiveAccess_IsError) {
    SharedSection section;
    Locomotive l1(1, 10, 0);

    section.access(l1, SharedSectionInterface::Direction::D1);
    section.access(l1, SharedSectionInterface::Direction::D1);
    section.leave(l1, SharedSectionInterface::Direction::D1);

    ASSERT_EQ(section.nbErrors(), 1);
}

TEST(SharedSection, LeaveWrongDirection_IsError) {
    SharedSection section;
    Locomotive l1(1, 10, 0);

    section.access(l1, SharedSectionInterface::Direction::D1);
    section.leave(l1, SharedSectionInterface::Direction::D2); 

    ASSERT_EQ(section.nbErrors(), 1);
}

TEST(SharedSection, OppositeDirections_ReleaseIsImmediate) {
    SharedSection section;
    Locomotive l1(1, 10, 0), l2(2, 10, 0);
    std::atomic<bool> releaseCalled{false};
    std::atomic<bool> secondEnteredBeforeRelease{false};

    PcoThread t1([&]{
        section.access(l1, SharedSectionInterface::Direction::D1);
        PcoThread::usleep(1000);
        section.leave(l1, SharedSectionInterface::Direction::D1); // should wake opposite direction
        PcoThread::usleep(1000);
        releaseCalled = true;
        section.release(l1);
    });

    PcoThread t2([&]{
        PcoThread::usleep(500);
        section.access(l2, SharedSectionInterface::Direction::D2);
        if (!releaseCalled.load()) {
            secondEnteredBeforeRelease = true;
        }
        section.leave(l2, SharedSectionInterface::Direction::D2);
        section.release(l2);
    });

    t1.join();
    t2.join();
    ASSERT_TRUE(secondEnteredBeforeRelease.load());
    ASSERT_EQ(section.nbErrors(), 0);
}

TEST(SharedSection, StopAll_UnblocksWaitingAccess) {
    SharedSection section;
    Locomotive l1(1, 10, 0), l2(2, 10, 0);
    std::atomic<bool> waitingThreadCompleted{false};
    std::atomic<bool> stopSent{false};

    PcoThread t1([&]{
        section.access(l1, SharedSectionInterface::Direction::D1);
        while (!stopSent.load()) {
            PcoThread::usleep(1000);
        }
        section.leave(l1, SharedSectionInterface::Direction::D1);
        section.release(l1);
    });

    PcoThread t2([&]{
        PcoThread::usleep(500);
        section.access(l2, SharedSectionInterface::Direction::D1);
        waitingThreadCompleted = true;
    });

    PcoThread::usleep(5000);
    section.stopAll();
    stopSent = true;

    t2.join();
    ASSERT_TRUE(waitingThreadCompleted.load());

    t1.join();
    ASSERT_EQ(section.nbErrors(), 0);
}

