This experiment is the continuation of the one on the 17-07 folder.


**Simualtion file: **

- n-dscp_mflow_traces.cc

  
**Output files:**
- flow stats             (qdisc_dscp_2_nodes.xml)
- packet stats           (qdisc_dscp-0-0.pcap and qdisc_dscp-0-1.pcap files)
- queue logs             (qdisc_dscp_trace.log)
- dscp count per flow     (qdisc_dscp_counts.txt)  (**--redundant--**)
- application-level logs (qdisc_dscp_sink_log.txt)
- animation               (qdisc-animation.xml)

n-qdisc_dscp_parse.ipynb **parses the files**

**Next-steps:**

- verify the implementation of the tos (because there are tos values that were never sent)
- join the dataframe and perform ML tasks
- Try to use a simpler simulator OMNET++ for faster implementation
