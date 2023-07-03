.. _slcan-sample:

Serial-Line CAN
###############

Overview
********
This sample application demonstrates how to use SLCAN sub-system to send/receive
CAN bus messages throught a serial interface. SLCAN sub-system partially complies
with `slcan.c - serial line CAN interface driver (using tty line discipline) <https://github.com/torvalds/linux/blob/master/drivers/net/can/slcan/slcan-core.c>`_.

SLCAN implementes below frames:

* t => 11 bit data frame
* r => 11 bit RTR frame
* T => 29 bit data frame
* R => 29 bit RTR frame

SLCAN so far does not implements below frames:

* sb256256 : state bus-off: rx counter 256, tx counter 256
* sa057033 : state active, rx counter 57, tx counter 33
* e1a : len 1, errors: ACK error
* e3bcO: len 3, errors: Bit0 error, CRC error, Tx overrun error

SLCAN also implementes below frames which are `can-utils/slcand.c - userspace daemon for serial line CAN interface driver SLCAN <https://github.com/linux-can/can-utils/blob/master/slcand.c>`_
compliant. This allows SLCAN device t 

* O/r  : open CAN node
* C/r  : close CAN node
* SX/r : set CAN bus speed, X = CAN speed 0..8

This allows hosts to bring up an SLCAN device over serial port. For example on Linux:

.. code-block:: console

    sudo slcand -o -s8 /dev/ttyACM0
    
Building and Flashing
*********************
This sample application has been tested with `ST Nucleo F446RE <https://docs.zephyrproject.org/latest/boards/arm/nucleo_f446re/doc/index.html>`_
board. ``CONFIG_SLCAN_LOOPBACK_MODE`` is default to enabled, which makes it easy to test when only one SLCAN device is avaiable.

As ``CONFIG_BOOTLOADER_MCUBOOT=y`` by default, ``mcuboot`` image is required to be flashed together with the application image into the SLCAN device: 

.. code-block:: console

        west build \
            -b nucleo_f446re \
            -d build_mcuboot ../bootloader/mcuboot/boot/zephyr

The mcuboot artifcat is produced in ``/workdir/zephyrproject/zephyr/build_mcuboot/zephyr/zephyr.elf``. Next, building the application code:

.. code-block:: console

    west build \
        -b nucleo_f446re samples/subsys/canbus/slcan

The application artifact is produced in ``/workdir/zephyrproject/zephyr/build/zephyr/zephyr.bin``.
Since ``mcuboot`` authenticates the application artifact before booting into application code, 
the application artifact has to be signed.

.. code-block:: console

    west sign \
        -t imgtool -- \
        --key ../bootloader/mcuboot/root-rsa-2048.pem \
        --version 1.0.0

Flasing the SLCAN device in **Linux** development environment could simply be done by ``west flash`` after ``west build``
of each artifact:

.. code-block:: console

    west build \
        -b nucleo_f446re \
        -d build_mcuboot ../bootloader/mcuboot/boot/zephyr
    
    west flash

On Windows, it is recommanded to flash by `STM32CubeProgrammer Tool <https://www.st.com/en/development-tools/stm32cubeprog.html>`_.