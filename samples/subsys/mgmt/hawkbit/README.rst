.. _hawkbit-api-sample:

Hawkbit Direct Device Integration API sample
############################################

Overview
********
This branch of ``Hawkbit`` sample application leverages the forked origin one
and aims to bring up `CC3220SF LaunchXL <https://docs.zephyrproject.org/latest/boards/arm/cc3220sf_launchxl/doc/index.html>`_ board to run the application. See
`READMDE <https://github.com/jonathanyhliang/zephyr/tree/fork/samples/subsys/mgmt/hawkbit>`_
of the forked origin version for more detail about ``Hawkbit``.

This branch of sample application makes below changes from the fored orgin version:

* Add device driver of CC3220 flash controller
* CC3220SF LaunchXL board configuration
* Disabled DHCPv4 start-up function call as DHCPv4 is offloaded to CC3220
* Remove DNS resolver dependency as it is not supported by CC3220

Building and Running
********************

Building, flashing and running ``Hawkbit`` application on ``CC3220SF LaunchXL`` board
remains the sample as the forked orign version. Refer to the original
`READMDE <https://github.com/jonathanyhliang/zephyr/tree/fork/samples/subsys/mgmt/hawkbit>`_
for more detail.
