# Network_digital_twin

This repository contains multiple directories, each of which encompasses a C++ file for the ns-3 simulation, a python notebook for parsing flow data where paths to the output files from where to read the data must be changed to be able to run it, and an Output folder where results are gathered (flowmon.xml file generetad by the flowmonitor add-ons of ns-3 that give information per flow like time of transmission, arrival time to destination, IP and ports of the source and the destination, delay, jitter, protocol...and other information depending on the flow generated, and an .xml file to visualize animation of the packet transmission using NetAnim.
Each folder has a README.md file explaining the setup of the experiment. Comments in the beginning of each C++ simulation file give more insight into topology, subnetwork IP addresses, traffic generated and other features for a more understandable code.
The simulations in each folder enable to trace the progression of the experiment in order to establish realistic traffic scenarios for data acquisition. The main point is to be able to confirm a setup and build upon it by adding a necessary complexity.


To be done/redone:
- add mobility model (stationary) for the static simulations to avoid printing warning issued by the animation add-ons
- adjust the code to avoid the "Max packets per trace file exceeded" (disable the animation tracing for now to avoid this problem as it works fine without it)
- add logs
- enable command line variable input for more flexibility
- fix the seed for reporductibility of some experiments
