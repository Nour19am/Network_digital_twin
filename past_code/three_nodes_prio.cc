//-------------Version 4 prio
//Network topology 
// n0-n1: 10.1.1.0
// n0-n2: 10.1.2.0
// n1-n2: 10.1.3.0
// point-to-point
//n0-----------n1
// \           /
//  \         /
//   \       /
//    \     /
//     \   /
//      n2
//The prio queue will:Put class 0 (EF/Voice) at highest priority and Others go to lower priority queues


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-helper.h"
#include <vector>
#include <random>
#include "ns3/random-variable-stream.h"
#include "traffic-control-helper.h"
#include "csv-reader.h"

using namespace ns3;
std::ofstream csvFile;
csvFile.open("delay_output.csv");
csvFile << "time,src,dst,dscp,delay_ms\n";
void PacketDequeueCallback(Ptr<const QueueDiscItem> item)
{
    Time now = Simulator::Now();
    Ptr<Packet> pkt = item->GetPacket();
    SocketIpTosTag tag;
    pkt->PeekPacketTag(tag);

    double delay = now.GetMilliSeconds() - pkt->GetUid(); // simulate delay here (replace with timestamp tag)

    csvFile << now.GetSeconds() << ","
            << item->GetHeader().GetSource() << ","
            << item->GetHeader().GetDestination() << ","
            << uint16_t(tag.GetTos()) << ","
            << delay << "\n";
}
NS_LOG_COMPONENT_DEFINE("three_nodes_NDT");

int
main(int argc, char* argv[])
{

    //Enable logging
    LogComponentEnable("three_nodes_NDT", LOG_LEVEL_INFO);
    // Create nodes
    NodeContainer nodes;
    nodes.Create(3);



    // Create point-to-point link helper
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));


    // Create node containers for each point-to-point link
    NodeContainer n0n1 = NodeContainer(nodes.Get(0), nodes.Get(1));
    NodeContainer n0n2 = NodeContainer(nodes.Get(0), nodes.Get(2));
    NodeContainer n1n2 = NodeContainer(nodes.Get(1), nodes.Get(2));

    // Install devices and channels for each link
    NetDeviceContainer d0d1 = pointToPoint.Install(n0n1);
    NetDeviceContainer d0d2 = pointToPoint.Install(n0n2);
    NetDeviceContainer d1d2 = pointToPoint.Install(n1n2);


    //Install IP stack

    InternetStackHelper stack;
    stack.Install(nodes);

    //Different subnet for each point-to-point link
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1); //has the two interfaces

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    


    //Configure routing
    //Works only with static topologies (i.e., no mobility or topology changes during simulation)
    //Centralized; done at simulation startup
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //adding prio
    TrafficControlHelper tch;
    tch.SetRootQueueDisc ("ns3::PrioQueueDisc", "Priomap", StringValue ("0 1 1 1 1 1 1 1"));

    QueueDiscContainer qdiscs = tch.Install (d0d1); 
    QueueDiscContainer qdiscs = tch.Install (d0d2); 
    QueueDiscContainer qdiscs = tch.Install (d1d2); 

    //Generate Traffic using DSCP
    uint8_t dscp_voice = 0xb8;  // EF = DSCP 46 (EF), TOS = 184
    uint8_t dscp_best = 0x00;   // Best Effort

    // Voice traffic
    OnOffHelper onoff1 ("ns3::UdpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), 9000));
    onoff1.SetAttribute ("DataRate", StringValue ("1Mbps"));
    onoff1.SetAttribute ("PacketSize", UintegerValue (500));
    onoff1.SetAttribute ("StartTime", TimeValue (Seconds(1.0)));
    onoff1.SetAttribute ("StopTime", TimeValue (Seconds(10.0)));
    ApplicationContainer app1 = onoff1.Install (nodes.Get(0));

    // Best effort traffic
    OnOffHelper onoff2 ("ns3::UdpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), 9001));
    onoff2.SetAttribute ("DataRate", StringValue ("3Mbps"));
    onoff2.SetAttribute ("PacketSize", UintegerValue (500));
    onoff2.SetAttribute ("StartTime", TimeValue (Seconds(1.5)));
    onoff2.SetAttribute ("StopTime", TimeValue (Seconds(10.0)));
    ApplicationContainer app2 = onoff2.Install (nodes.Get(0));

    Ptr<Socket> socket1 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    socket1->SetIpTos (dscp_voice); // EF
    onoff1.SetAttribute ("Socket", PointerValue (socket1));

    Ptr<Socket> socket2 = Socket::CreateSocket(nodes.Get(0), UdpSocketFactory::GetTypeId());
    socket2->SetIpTos (dscp_best); // Best Effort
    onoff2.SetAttribute ("Socket", PointerValue (socket2));






    std::vector<Ipv4InterfaceContainer> interfaces = {i0i1, i0i2, i1i2};

    double simulationTime =180 ;
    double end_time=simulationTime+2;
    uint32_t numFlows = 100;

    // Flow monitor

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    //AnimationInterface anim("three_nodes.xml");
    Simulator::Stop(Seconds(end_time));
    Simulator::Run();
    flowMonitor->SerializeToXmlFile("flowmon_three_nodes.xml", true, true);

    Simulator::Destroy();
    return 0;
}
