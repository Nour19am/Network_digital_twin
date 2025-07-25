
// adapted from example/traffic-control/traffic-control.cc
//3 sockets per ToS value sending traffic on different ports in the sink
//source: node 1
//sink  : node 0

//CLI
//tshark -r your.pcap -Y "ip" | wc -l
//tshark -r qdisc_dscp-0-0.pcap  to count the number of packets generated
//tshark -r qdisc_dscp-0-0.pcap -Y "ip.dsfield.dscp == 10 && tcp.len > 0" | wc -l //count number of packets with DSCP=10
//echo $((0x2c5210)) //convert the size of transmitted packets to bytes

//parameters to change to have packet drop: droptail maxsize, bandwidth of the p2p, data rate of the socket/app

/* example output
*** Flow monitor statistics ***
  Tx Packets/Bytes:   1351 / 2c5210
  Offered Load: 5.83435 Mbps //The sender attempted to transmit at ~5.83 Mbps based on the data and time.
  Rx Packets/Bytes:   133f / 2c28b8 //Rx Packets: 0x133f in hex = 4927 packets were received
  Packets/Bytes Dropped by Queue Disc:   12 / 2958
  Packets/Bytes Dropped by NetDevice:   0 / 0   //No packets were dropped by the network interface (NIC) level.
  Throughput: 5.76776 Mbps  //Actual measured throughput at the receiver — slightly lower than offered load due to dropped packets or jitter
  Mean delay:   0.0268333
  Mean jitter:   0.000472322
  DSCP value:   0x0  count:   4945
  */
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"
#include <random>
using namespace ns3;
/**
 * Number of packets in TX queue trace.
 *
 * \param oldValue Old velue.
 * \param newValue New value.
 */
void
TcPacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
    std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

/**
 * Packets in the device queue trace.
 *
 * \param oldValue Old velue.
 * \param newValue New value.
 */
void
DevicePacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
    std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

/**
 * TC Soujoun time trace.
 *
 * \param sojournTime The soujourn time.
 */
void
SojournTimeTrace(Time sojournTime)
{
    std::cout << "Sojourn time " << sojournTime.ToDouble(Time::MS) << "ms" << std::endl;
}




NS_LOG_COMPONENT_DEFINE("TrafficControlExample");






int
main(int argc, char* argv[])
{
    double simulationTime = 5; // seconds
    std::string transportProt = "Tcp";
    std::string socketType;
    NS_LOG_UNCOND("App started, sending traffic");
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);


    if (transportProt == "Tcp")
    {
        socketType = "ns3::TcpSocketFactory";
    }
    else
    {
        socketType = "ns3::UdpSocketFactory";
    }

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("5p"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");  //PfifoFastQueueDisc
    QueueDiscContainer qdiscs = tch.Install(devices);

    Ptr<QueueDisc> q = qdiscs.Get(1);
    q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPacketsInQueueTrace));
    Config::ConnectWithoutContext(
        "/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",MakeCallback(&SojournTimeTrace));

    Ptr<NetDevice> nd = devices.Get(1);
    Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice>(nd);
    Ptr<Queue<Packet>> queue = ptpnd->GetQueue();
    queue->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueTrace));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Flow n1: sender  / source
          //n0: receiver/ sink

    //sink app
  
   std::vector<uint8_t> dscpValues = {0x00, 0x28, 0xb8}; // Best Effort, AF, EF
    for (size_t i = 0; i < dscpValues.size(); ++i) {
        uint16_t port = 8000 + i; // unique port
        PacketSinkHelper sinkHelper(socketType, InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sink = sinkHelper.Install(nodes.Get(0)); // node 0
        sink.Start(Seconds(0.0));
        sink.Stop(Seconds(simulationTime));
    }


    //sending app in the source node
    uint32_t payloadSize =2000 ; //for the source 1448

    uint16_t port = 8000; //for the sink
    
    //Create 3 sockets for each ToS value and generate packets per each socket
    
    std::vector<ApplicationContainer> appContainers;

    for (size_t i = 0; i < dscpValues.size(); ++i) {
        //sending app in the source node
        uint8_t tos = dscpValues[i];
        std::cout << "The tos in hex is: 0x" << std::hex << static_cast<int>(tos) << std::endl;
        // Create a new OnOff app
        OnOffHelper onoff(socketType, Ipv4Address::GetAny());
        /*
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        */
        
        
        onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.0]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));
        
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("DataRate", StringValue("80Mbps")); // bit/s

        // Create a destination socket with specific ToS
        InetSocketAddress rmt(interfaces.GetAddress(0), port + i); // Use different ports if needed
        rmt.SetTos(tos);
        onoff.SetAttribute("Remote", AddressValue(rmt));

        // Install the app
        ApplicationContainer app = onoff.Install(nodes.Get(1));  // senderNode is nodes.Get(1) in your case
        app.Start(Seconds(1.0 + i)); // stagger start times
        app.Stop(Seconds(simulationTime));

        appContainers.push_back(app);
    }


    AnimationInterface anim("qdisc-animation.xml");
     
    //tracing
    //pcap files
    pointToPoint.EnablePcapAll("qdisc_dscp");
    //flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 5)); 
    Simulator::Run();
    monitor->SerializeToXmlFile("qdisc_dscp_2_nodes.xml", true, true);
 

    
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    std::cout << std::endl << "*** Flow monitor statistics ***" << std::endl;
    std::cout << "  Tx Packets/Bytes:   " << stats[1].txPackets << " / " << stats[1].txBytes
              << std::endl;
    std::cout << "  Offered Load: "
              << stats[1].txBytes * 8.0 /
                     (stats[1].timeLastTxPacket.GetSeconds() -
                      stats[1].timeFirstTxPacket.GetSeconds()) /
                     1000000
              << " Mbps" << std::endl;
    std::cout << "  Rx Packets/Bytes:   " << stats[1].rxPackets << " / " << stats[1].rxBytes
              << std::endl;
    uint32_t packetsDroppedByQueueDisc = 0;
    uint64_t bytesDroppedByQueueDisc = 0;
    if (stats[1].packetsDropped.size() > Ipv4FlowProbe::DROP_QUEUE_DISC)
    {
        packetsDroppedByQueueDisc = stats[1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
        bytesDroppedByQueueDisc = stats[1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
    }
    std::cout << "  Packets/Bytes Dropped by Queue Disc:   " << packetsDroppedByQueueDisc << " / "
              << bytesDroppedByQueueDisc << std::endl;
    uint32_t packetsDroppedByNetDevice = 0;
    uint64_t bytesDroppedByNetDevice = 0;
    if (stats[1].packetsDropped.size() > Ipv4FlowProbe::DROP_QUEUE)
    {
        packetsDroppedByNetDevice = stats[1].packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
        bytesDroppedByNetDevice = stats[1].bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
    }
    std::cout << "  Packets/Bytes Dropped by NetDevice:   " << packetsDroppedByNetDevice << " / "
              << bytesDroppedByNetDevice << std::endl;
    std::cout << "  Throughput: "
              << stats[1].rxBytes * 8.0 /
                     (stats[1].timeLastRxPacket.GetSeconds() -
                      stats[1].timeFirstRxPacket.GetSeconds()) /
                     1000000
              << " Mbps" << std::endl;
    std::cout << "  Mean delay:   " << stats[1].delaySum.GetSeconds() / stats[1].rxPackets
              << std::endl;
    std::cout << "  Mean jitter:   " << stats[1].jitterSum.GetSeconds() / (stats[1].rxPackets - 1)
              << std::endl;

    //for each queue 
    //This function returns a map of DSCP values and their packet counts for queue number n
    auto dscpVec = classifier->GetDscpCounts(1); //What DSCP values were enqueued to queue 1
    for (auto p : dscpVec)
    {
        std::cout << "  DSCP value:   0x" << std::hex << static_cast<uint32_t>(p.first) << std::dec
                  << "  count:   " << p.second << std::endl;
    }
    auto dscpVec2 = classifier->GetDscpCounts(2);
    for (auto p : dscpVec2)
    {
        std::cout << "  DSCP value:   0x" << std::hex << static_cast<uint32_t>(p.first) << std::dec
                  << "  count:   " << p.second << std::endl;
    }
    auto dscpVec3 = classifier->GetDscpCounts(3);
    for (auto p : dscpVec3)
    {
        std::cout << "  DSCP value:   0x" << std::hex << static_cast<uint32_t>(p.first) << std::dec
                  << "  count:   " << p.second << std::endl;
    }
    
    Simulator::Destroy();
 
    return 0;
}
