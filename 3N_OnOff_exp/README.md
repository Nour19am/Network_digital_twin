This is the first architecture where we:
- Create a simulation in the three_nodes_exp.cc file that has 3 nodes with point-to-point (p2p) connections, each channel has its own subnetwork, and an On_Off application sending packets from n0-->n1 and from n1-->n2 with exponential time arrival of packets (different parameters for the On and Off times) and uniformly randomized data rate
The Bandwidth of the p2p channel is set to 100 Mbps and the delay is fixed to 2 ms.
- Generate output files in the Output/ directory: the flowmon_three_nodes.xml. 
The three_nodes.xml is an animation xml file used to visualize packet transmission using NetAnim
- Parse information collected from  flowmonitor.xml file to prepare data for ML applications in the parse_flow.ipynb python notebook.

