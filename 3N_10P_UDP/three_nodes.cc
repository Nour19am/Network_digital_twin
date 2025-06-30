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
//put this file in the scratch/ directory under the ns-3.version you use (I use ns-3.41 version)
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/netanim-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("three_nodes_NDT");

int
main(int argc, char* argv[])
{

    uint32_t nPackets = 10;

    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);

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
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    


    //Configure routing
    //Works only with static topologies (i.e., no mobility or topology changes during simulation)
    //Centralized; done at simulation startup
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    //example de génération de traffic (à modifier)

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));//server is n1

    serverApps.Start(Seconds(1.0));
    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(i0i1.GetAddress(1), 9);//This ensures the client (n0) sends packets to the correct IP address of the server (n1) on the n0–n1 link (10.1.1.1)

    echoClient.SetAttribute("MaxPackets", UintegerValue(nPackets));
    echoClient.SetAttribute("Interval", TimeValue(Seconds(0.5)));
    echoClient.SetAttribute("PacketSize", UintegerValue(1024));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0)); //client is n0
    clientApps.Start(Seconds(2.0));
    clientApps.Stop(Seconds(10.0));

    AsciiTraceHelper ascii;
    pointToPoint.EnableAsciiAll(ascii.CreateFileStream("three_nodes_NDT.tr"));

    //These .pcap files can be analyzed with tools like Wireshark, tcpdump, or parsed using Python with libraries like Scapy or dpkt.
    pointToPoint.EnablePcapAll("three_nodes");

    // Flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    AnimationInterface anim("three_nodes.xml");
    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    flowMonitor->SerializeToXmlFile("flowmon_three_nodes.xml", true, true);

    Simulator::Destroy();
    return 0;
}

