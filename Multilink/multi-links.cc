/*
two point-to-point links between the same pair of nodes

per-device TrafficControl with FifoQueueDisc

link failure simulation using Ipv4::SetDown/SetUp

OnOff UDP traffic generation

packet capture (pcap) animation via NetAnim and FlowMonitor logging
*/


#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/olsr-helper.h"
#include "ns3/mobility-module.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>
#include "ns3/netanim-module.h"


using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Multi-links");

int
main(int argc, char* argv[])

{       
    // The below value configures the default behavior of global routing.
    // By default, it is disabled.  To respond to interface events, set to true
    Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));
    

    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);
    //Create Multiple NetDevices per Node
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    // First link (e.g., point-to-point)
    NS_LOG_INFO("Create channels.");
    PointToPointHelper ptop;
    ptop.SetDeviceAttribute ("DataRate", StringValue ("1Mbps"));//5Mbps, 500Kbps
    ptop.SetChannelAttribute ("Delay", StringValue ("200ms"));
    //ptop.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(100));

    NetDeviceContainer dev1 = ptop.Install (nodes.Get(0), nodes.Get(1));

    // Second link
    
   
    ptop.SetDeviceAttribute ("DataRate", StringValue ("1Mbps")); //500Kbps
    ptop.SetChannelAttribute ("Delay", StringValue ("200ms")); //1s
    NetDeviceContainer dev2 = ptop.Install (nodes.Get(0), nodes.Get(1));
    

     

    //Install Internet Stack 
    NS_LOG_INFO("Installing Internet stack on nodes.");
    InternetStackHelper internet;
    internet.Install(nodes);



    //Assign Different IP Addresses
    NS_LOG_INFO("Assign IP Addresses.");

    TrafficControlHelper tch;
    //uint16_t handle = tch.SetRootQueueDisc("ns3::FifoQueueDisc");
    tch.SetRootQueueDisc("ns3::FifoQueueDisc", "MaxSize", StringValue("1000p")); // 100 packets

   
    QueueDiscContainer qdiscs = tch.Install(dev1);
    QueueDiscContainer qdiscs2 = tch.Install(dev2);

    Ipv4AddressHelper address1;
    address1.SetBase ("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface1 = address1.Assign (dev1);
    
    Ipv4AddressHelper address2;
    address2.SetBase ("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface2 = address2.Assign (dev2);
    

    // Create router nodes, initialize routing database and set up the routing
    // tables in the nodes.
    
    //TrafficControlHelper tch;
    //tch.Uninstall(dev2);
    


    //dev1.DisableFlowControl();
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    
       // Create a second OnOff application to send UDP datagrams of size
    // 210 bytes at a rate of 448 Kb/s
    NS_LOG_INFO("Create Applications.");

  
    uint16_t port = 9; // Discard port (RFC 863)
    //OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(iface1.GetAddress(0), port));
    
    //OnOffHelper onoff("ns3::UdpSocketFactory", (InetSocketAddress(nodes.Get(0)->GetObject<Ipv4>()->GetAddress(1,0).GetLocal(), port)));
    Ptr<Node> dstNode = nodes.Get(0);
    Ptr<NetDevice> dstDev = dev1.Get(0); // or another device connected to dstNode

    Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
    uint32_t interfaceIndex = ipv4->GetInterfaceForDevice(dstDev);
    Ipv4Address dstAddr = ipv4->GetAddress(interfaceIndex, 0).GetLocal();
    std::cout<<"dst adress is "<<dstAddr<<std::endl;
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dstAddr, port));

    //OnOffHelper onoff("ns3::UdpSocketFactory", (InetSocketAddress(iface1.GetAddress(0), port)));
    //onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=5]"));
    //onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetConstantRate(DataRate("1Mbps")); //
    onoff.SetAttribute("PacketSize", UintegerValue(1000)); // application-layer payload size

    ApplicationContainer apps = onoff.Install(nodes.Get(1));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(10.0));

    // Create an optional packet sink to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory",
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
        //UDP listener that waits for packets on the specified port, 
                          //from any IP (if 2 links, 2 possible sending IP in case of one link failure)
    apps = sink.Install(nodes.Get(0));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(25.0));

    
    AsciiTraceHelper ascii;
    ptop.EnableAsciiAll(ascii.CreateFileStream("multi-link.tr"));
    //mutli-link-<nodeId>-<deviceId>.pcap
    //mutli-link-0-1.pcap
    ptop.EnablePcapAll("multi-link"); //will capture packets on all PointToPoint devices in your simulation. 
    //ptop.EnablePcap("link1", dev1);
    
    //link failure scenario
    
    Ptr<Node> n1 = nodes.Get(1);
    Ptr<Ipv4> ipv41 = n1->GetObject<Ipv4>();
    // The first ifIndex is 0 for loopback, then the first p2p is numbered 1,
    // then the next p2p is numbered 2
    
    uint32_t ipv4ifIndex1 = 1;
    
    Simulator::Schedule(Seconds(4), &Ipv4::SetDown, ipv41, ipv4ifIndex1);
    std::cout << "Node has " << ipv41->GetNInterfaces() << " interfaces." << std::endl;

    std::cout<<"interface "<< ipv41->GetAddress(1, 0).GetLocal()<<" is down"<<std::endl;
    Simulator::Schedule(Seconds(6), &Ipv4::SetUp, ipv41, ipv4ifIndex1);
    
    // Trace routing tables
    Ptr<OutputStreamWrapper> routingStream =
        Create<OutputStreamWrapper>("multi-link.routes", std::ios::out);
    Ipv4RoutingHelper::PrintRoutingTableAllAt(Seconds(12), routingStream);


    //Set mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);  

    AnimationInterface anim("multi-link-anim.xml");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll(); //Install FlowMonitor on all nodes or on selected nodes (flowMonitor = flowHelper.Install(NodeContainer(n0, n1));)




    NS_LOG_INFO("Run Simulation.");
    //Simulator::Stop (Seconds (17));
    Simulator::Stop(Seconds(30));
    Simulator::Run ();
    flowMonitor->SerializeToXmlFile("multi-links-flow.xml", true, true);

    Simulator::Destroy ();
    NS_LOG_INFO("Done.");
    return 0;


}