- The output flow stats in the shell when running the ns3 file shows stats for the first flow (**---need to adjust this---**)
- This experiment is with those parameters:
pfifo_fast (3 bands (cannot change them))

tosValues = {0x08,0x0a,0x0c,0x0e}; //all band2
// 0x08 Max. Throughput
//0x0a MMC+MT
//0x0c MR+MT
//0x0e MMC+MR+MT


maximize througput 0x08->2, normal service: 0x00 ->1, minimize delay: 0x10 ->0

Onoff
"OnTime", ([Constant=1]);
"OffTime", ([Constant=0]);
 "DataRate", "100Mbps";
 “PacketSize", 1448
Link:
	type: p2p
	capacity: 
            "DataRate", "10Mbps";
            "Delay", "2ms";
             “DropTailQueue", "MaxSize", ("1p"); //1 packet


see document [notes](https://docs.google.com/document/d/1ESrIuSHhsO0bt7aVXoOahHjweDIED6BFgDaWa8Pwe80/edit?tab=t.0) for some notes on the implementation
