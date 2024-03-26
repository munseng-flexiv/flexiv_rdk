/**
 * @test test_scheduler.cpp
 * A test to evaluate RDK's internal real-time scheduler.
 * @copyright Copyright (C) 2016-2023 Flexiv Ltd. All Rights Reserved.
 * @author Flexiv
 */

#include <flexiv/log.h>
#include <flexiv/scheduler.h>
#include <flexiv/utility.h>

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>

namespace {
/** Data shared between threads */
struct SharedData
{
    int64_t measured_interval = 0;
} g_data;

/** Mutex on the shared data */
std::mutex g_mutex;

/** Atomic signal to stop scheduler tasks */
std::atomic<bool> g_stop_sched = {false};
}

/** User-defined high-priority periodic task @ 1kHz */
void highPriorityTask(flexiv::Log& log)
{
    static unsigned int loop_counter = 0;

    // Scheduler loop interval start time point
    static std::chrono::high_resolution_clock::time_point tic;

    try {
        // Mark loop interval end point
        auto toc = std::chrono::high_resolution_clock::now();

        // Calculate scheduler's interrupt interval and print
        auto measured_interval
            = std::chrono::duration_cast<std::chrono::microseconds>(toc - tic).count();

        // Safely write shared data
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            g_data.measured_interval = measured_interval;
        }

        // Stop scheduler after 5 seconds
        if (++loop_counter > 5000) {
            loop_counter = 0;
            g_stop_sched = true;
        }

        // Mark loop interval start point
        tic = std::chrono::high_resolution_clock::now();

    } catch (const std::exception& e) {
        log.Error(e.what());
        g_stop_sched = true;
    }
}

/** User-defined low-priority periodic task @1Hz */
void lowPriorityTask(flexiv::Log& log)
{
    static uint64_t accumulated_time = 0;
    static uint64_t num_measures = 0;
    static float avg_interval = 0.0;
    int measured_interval = 0;

    // Safely read shared data
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        measured_interval = g_data.measured_interval;
    }

    // calculate average time interval
    accumulated_time += measured_interval;
    num_measures++;
    avg_interval = (float)accumulated_time / (float)num_measures;

    // print time interval of high-priority periodic task
    log.Info("High-priority task interval (curr | avg) = " + std::to_string(measured_interval)
             + " | " + std::to_string(avg_interval) + " us");
}

void PrintHelp()
{
    // clang-format off
    std::cout << "Required arguments: None" << std::endl;
    std::cout << "Optional arguments: None" << std::endl;
    std::cout << std::endl;
    // clang-format on
}

int main(int argc, char* argv[])
{
    // log object for printing message with timestamp and coloring
    flexiv::Log log;

    // Parse Parameters
    //==============================================================================================
    if (flexiv::utility::ProgramArgsExistAny(argc, argv, {"-h", "--help"})) {
        PrintHelp();
        return 1;
    }

    try {
        // Periodic Tasks
        //==========================================================================================
        flexiv::Scheduler scheduler;
        // Add periodic task with 1ms interval and highest applicable priority
        scheduler.AddTask(
            std::bind(highPriorityTask, std::ref(log)), "HP periodic", 1, scheduler.max_priority());
        // Add periodic task with 1s interval and lowest applicable priority
        scheduler.AddTask(std::bind(lowPriorityTask, std::ref(log)), "LP periodic", 1000, 0);
        // Start all added tasks
        scheduler.Start();

        // Block and wait for signal to stop scheduler tasks
        while (!g_stop_sched) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Received signal to stop scheduler tasks
        scheduler.Stop();

        // Restart scheduler after 2 seconds
        log.Warn("Scheduler will restart in 2 seconds");
        std::this_thread::sleep_for(std::chrono::seconds(2));
        g_stop_sched = false;
        scheduler.Start();

        // Wait for signal to stop scheduler tasks
        while (!g_stop_sched) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Received signal to stop scheduler tasks, flexiv::Scheduler's destructor can also do the
        // thread exit and resources cleanup

    } catch (const std::exception& e) {
        log.Error(e.what());
        return 1;
    }

    return 0;
}
