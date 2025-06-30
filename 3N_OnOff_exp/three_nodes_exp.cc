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
//Randomized OnTime / OffTime using exponential distribution
//Randomized data rate with uniform distribution [1, 10] Mbps.
//n0-------n1 On_Off
//n1-------n2 On_Off
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


    //Traffic generation with exponential distribution for the on_off application

    //n0---------n1
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(i0i1.GetAddress(1), 9));;//This ensures the client (n0) sends packets to the correct IP address of the server (n1) on the n0–n1 link (10.1.1.1)
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    // Use exponential on/off durations
    onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));

    // Use uniform random data rate between 1Mbps and 10Mbps
    Ptr<UniformRandomVariable> rateVar = CreateObject<UniformRandomVariable>();
    rateVar->SetAttribute("Min", DoubleValue(1.0));
    rateVar->SetAttribute("Max", DoubleValue(10.0));
    double rate = rateVar->GetValue();
    onoff.SetAttribute("DataRate", StringValue(std::to_string(rate) + "Mbps"));

    // Install on source node
    ApplicationContainer app = onoff.Install(nodes.Get(0));// node 0 sends
    app.Start(Seconds(1.0));
    app.Stop(Seconds(20.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApp = sink.Install(nodes.Get(1)); //destination node
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(21.0));

    //n1----------n2
    OnOffHelper onoff2("ns3::UdpSocketFactory", InetSocketAddress(i1i2.GetAddress(1), 10)); //port 10
    onoff2.SetAttribute("PacketSize", UintegerValue(1024));
    onoff2.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.5]"));
    onoff2.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.2]"));
    onoff2.SetAttribute("DataRate", StringValue(std::to_string(rate) + "Mbps"));  
    ApplicationContainer app2 = onoff2.Install(nodes.Get(1)); // Node 1 sends
    // Install on source node
    app2.Start(Seconds(1.0));
    app2.Stop(Seconds(20.0));

    PacketSinkHelper sink2("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 10)); //port 10
    ApplicationContainer sinkApp2=sink2.Install(nodes.Get(2)); //destination node
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(21.0));


    // Flow monitor
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    AnimationInterface anim("three_nodes.xml");
    Simulator::Stop(Seconds(22.0));
    Simulator::Run();
    flowMonitor->SerializeToXmlFile("flowmon_three_nodes.xml", true, true);

    Simulator::Destroy();
    return 0;
}

