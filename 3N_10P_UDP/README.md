This is the first architecture where we:
- Create a simulation in the three_nodes.cc file that has 3 nodes with point-to-point connections, each channel has its own subnetwork, and a UDP application sending 10
packets from node 0 to node 1 is used to simulate a simple constant bit rate traffic (CBR) without lik failures nor packet losts.
The data rate is set to 100 Mbps and the delay is fixed to 2 ms.
- Generate output files in the Output/ directory: the .tr file (ASCII tracing), .pcap files and the flowmon_three_nodes.xml. We will be using the .xml file for parsing later.
The three_nodes.xml is an animation xml file used to visualize packet transmission using NetAnim
- Parse information collected from .tr, .pcap and the flowmonitor.xml file to prepare data for ML applications in the parse.ipynb python notebook. You can recreate the conda environment wth the installed dependencies using the environment.yml file
