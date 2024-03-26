/**
 * @test test_timeliness_monitor.cpp
 * A test to evaluate RDK's internal timeliness monitor on real-time modes. Bad communication or
 * insufficient real-time performance of the workstation PC will cause the monitor's timeliness
 * check to fail. A warning will be issued first, then if the check has failed too many times, the
 * RDK connection with the server will be closed. During this test, the robot will hold its position
 * using joint torque streaming mode.
 * @copyright Copyright (C) 2016-2023 Flexiv Ltd. All Rights Reserved.
 * @author Flexiv
 */

#include <flexiv/robot.h>
#include <flexiv/log.h>
#include <flexiv/scheduler.h>
#include <flexiv/utility.h>

#include <iostream>
#include <string>
#include <cmath>
#include <thread>
#include <atomic>

namespace {
/** Atomic signal to stop scheduler tasks */
std::atomic<bool> g_stop_sched = {false};
}

// callback function for realtime periodic task
void PeriodicTask(
    flexiv::Robot& robot, flexiv::Log& log, const std::array<double, flexiv::kJointDOF>& init_pos)
{
    // Loop counter
    static unsigned int loop_counter = 0;

    try {
        // Monitor fault on the connected robot
        if (robot.fault()) {
            throw std::runtime_error(
                "PeriodicTask: Fault occurred on the connected robot, exiting ...");
        }
        // Hold position
        std::array<double, flexiv::kJointDOF> target_vel = {};
        std::array<double, flexiv::kJointDOF> target_acc = {};
        robot.StreamJointPosition(init_pos, target_vel, target_acc);

        if (loop_counter == 5000) {
            log.Warn(">>>>> Adding simulated loop delay <<<<<");
        }
        // simulate prolonged loop time after 5 seconds
        else if (loop_counter > 5000) {
            std::this_thread::sleep_for(std::chrono::microseconds(995));
        }

        loop_counter++;

    } catch (const std::exception& e) {
        log.Error(e.what());
        g_stop_sched = true;
    }
}

void PrintHelp()
{
    // clang-format off
    std::cout << "Required arguments: [robot SN]" << std::endl;
    std::cout << "    robot SN: Serial number of the robot to connect to. "
                 "Remove any space, for example: Rizon4s-123456" << std::endl;
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
    if (argc < 2 || flexiv::utility::ProgramArgsExistAny(argc, argv, {"-h", "--help"})) {
        PrintHelp();
        return 1;
    }

    // Serial number of the robot to connect to. Remove any space, for example: Rizon4s-123456
    std::string robot_sn = argv[1];

    try {
        // RDK Initialization
        //==========================================================================================
        // Instantiate robot interface
        flexiv::Robot robot(robot_sn);

        // Clear fault on the connected robot if any
        if (robot.fault()) {
            log.Warn("Fault occurred on the connected robot, trying to clear ...");
            // Try to clear the fault
            if (!robot.ClearFault()) {
                log.Error("Fault cannot be cleared, exiting ...");
                return 1;
            }
            log.Info("Fault on the connected robot is cleared");
        }

        // enable the robot, make sure the E-stop is released before enabling
        log.Info("Enabling robot ...");
        robot.Enable();

        // Wait for the robot to become operational
        while (!robot.operational()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        log.Info("Robot is now operational");

        // set mode after robot is operational
        robot.SwitchMode(flexiv::Mode::RT_JOINT_POSITION);

        // Set initial joint positions
        auto init_pos = robot.states().q;
        log.Info("Initial joint positions set to: " + flexiv::utility::Arr2Str(init_pos));
        log.Warn(">>>>> Simulated loop delay will be added after 5 seconds <<<<<");

        // Periodic Tasks
        //==========================================================================================
        flexiv::Scheduler scheduler;
        // Add periodic task with 1ms interval and highest applicable priority
        scheduler.AddTask(
            std::bind(PeriodicTask, std::ref(robot), std::ref(log), std::ref(init_pos)),
            "HP periodic", 1, scheduler.max_priority());
        // Start all added tasks
        scheduler.Start();

        // Block and wait for signal to stop scheduler tasks
        while (!g_stop_sched) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        // Received signal to stop scheduler tasks
        scheduler.Stop();

    } catch (const std::exception& e) {
        log.Error(e.what());
        return 1;
    }

    return 0;
}
