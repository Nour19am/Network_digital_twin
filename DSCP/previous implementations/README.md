Those are files used to generate testing code:

- n-dscp_test_unique.cc generates traffic through one socket attached to a TOS between 2 nodes (one flow)
- n-dscp_test_mflow.cc creates 3 sockets attached to 3 different TOS between 2 nodes to use prioirty queues (3 different flows)
- n-dscp-test_mflow_traces provides pcap files to parse with python using tshark for packets characteristics and flowmonitor file for flow characteristics. Also outputs metrics for the queues (overall dropped packets in queues or in NIC, throughput, sent packets per DSCP class)