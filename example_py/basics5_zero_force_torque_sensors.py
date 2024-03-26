#!/usr/bin/env python

"""basics5_zero_force_torque_sensors.py

This tutorial zeros the robot's force and torque sensors, which is a recommended (but not
mandatory) step before any operations that require accurate force/torque measurement.
"""

__copyright__ = "Copyright (C) 2016-2023 Flexiv Ltd. All Rights Reserved."
__author__ = "Flexiv"

import time
import argparse
from utility import list2str

# Import Flexiv RDK Python library
# fmt: off
import sys
sys.path.insert(0, "../lib_py")
import flexivrdk
# fmt: on


def print_description():
    """
    Print tutorial description.

    """
    print(
        "This tutorial zeros the robot's force and torque sensors, which is a recommended "
        "(but not mandatory) step before any operations that require accurate "
        "force/torque measurement."
    )
    print()


def main():
    # Program Setup
    # ==============================================================================================
    # Parse arguments
    argparser = argparse.ArgumentParser()
    argparser.add_argument(
        "robot_sn",
        help="Serial number of the robot to connect to. Remove any space, for example: Rizon4s-123456",
    )
    args = argparser.parse_args()

    # Define alias
    log = flexivrdk.Log()
    mode = flexivrdk.Mode

    # Print description
    log.Info("Tutorial description:")
    print_description()

    try:
        # RDK Initialization
        # ==========================================================================================
        # Instantiate robot interface
        robot = flexivrdk.Robot(args.robot_sn)

        # Clear fault on the connected robot if any
        if robot.fault():
            log.Warn("Fault occurred on the connected robot, trying to clear ...")
            # Try to clear the fault
            if not robot.ClearFault():
                log.Error("Fault cannot be cleared, exiting ...")
                return 1
            log.Info("Fault on the connected robot is cleared")

        # Enable the robot, make sure the E-stop is released before enabling
        log.Info("Enabling robot ...")
        robot.Enable()

        # Wait for the robot to become operational
        while not robot.operational():
            time.sleep(1)

        log.Info("Robot is now operational")

        # Zero Sensors
        # ==========================================================================================
        # Get and print the current TCP force/moment readings
        log.Info(
            "TCP force and moment reading in base frame BEFORE sensor zeroing: "
            + list2str(robot.states().ext_wrench_in_world)
            + "[N][Nm]"
        )

        # Run the "ZeroFTSensor" primitive to automatically zero force and torque sensors
        robot.SwitchMode(mode.NRT_PRIMITIVE_EXECUTION)
        robot.ExecutePrimitive("ZeroFTSensor()")

        # WARNING: during the process, the robot must not contact anything, otherwise the result
        # will be inaccurate and affect following operations
        log.Warn(
            "Zeroing force/torque sensors, make sure nothing is in contact with the robot"
        )

        # Wait for the primitive completion
        while robot.busy():
            time.sleep(1)
        log.Info("Sensor zeroing complete")

        # Get and print the current TCP force/moment readings
        log.Info(
            "TCP force and moment reading in base frame AFTER sensor zeroing: "
            + list2str(robot.states().ext_wrench_in_world)
            + "[N][Nm]"
        )

    except Exception as e:
        # Print exception error message
        log.Error(str(e))


if __name__ == "__main__":
    main()
