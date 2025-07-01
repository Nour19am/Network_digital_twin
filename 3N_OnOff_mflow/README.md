This is the architecture where we:
- Create a simulation (lasts 10 seconds) in the three_nodes_mflow.cc file that has 3 nodes with point-to-point (p2p) connections, each channel has its own subnetwork, and an On_Off application sending packets through multiple UDP socket flows to simulate real-time traffic with exponential time arrival of packets (different parameters for the On and Off times) and uniformly randomized data rate
The Bandwidth of the p2p channel is set to 100 Mbps and the delay is fixed to 2 ms.
- Generate output files in the Output/ directory: the flowmon_three_nodes.xml.
The three_nodes.xml is an animation xml file used to visualize packet transmission using NetAnim
- Parse information collected from  flowmonitor.xml file to prepare data for ML applications in the parse_flow.ipynb python notebook.

Note: We generate N random flows (but realistic) and we have (in general) N different flows (check the combination COMB of src port, src IP, dst port, dst IP, protocol below that we added to verify this). Indeed, the GetAny() function used to select dynamically a port in the src produces multiple (ports range (49152-65535)) enables to have generally as many different flows as the total number we want to generate (Indeed, it is not because we generate N flows that we will end up with N different flows unless the combination COMB is differet accross all the N flows (when we will complexify the scenarios the COMB would most likely change if we add DSCP header for example needed for QOS-aware modeling) )
