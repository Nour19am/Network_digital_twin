//Switch Connections:
//    s3
//    |
//s1-s2
//    |
//    s4
//on-off app generate flooding

//Host to Switch:
//h1-s1, h2-s2,h3-s3,h4-s4


#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("OpenFlowCustomTopology");

void Connect(CsmaHelper& csma, Ptr<Node> a, Ptr<Node> b, NetDeviceContainer& devA, NetDeviceContainer& devB) {
    NetDeviceContainer link = csma.Install(NodeContainer(a, b));

    Ptr<NetDevice> dev0 = link.Get(0);
    Ptr<NetDevice> dev1 = link.Get(1);
    devA.Add(dev0);
    devB.Add(dev1);
}

int main(int argc, char *argv[]) {

    LogComponentEnable("OpenFlowInterface", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

     // Create switches s1-s8
    NodeContainer switches;
    switches.Create(4);

    // Create hosts h1-h8
    NodeContainer hosts;
    hosts.Create(4);

    // Internet stack for hosts only
    InternetStackHelper internet;
    internet.Install(hosts);

    // Create CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Each switch has its NetDevice container
    std::vector<NetDeviceContainer> switchDevices(4);
    NetDeviceContainer hostDevices;

    // Connect each host to its switch
    for (int i = 0; i < 4; ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(hosts.Get(i), switches.Get(i)));
        hostDevices.Add(link.Get(0));
        switchDevices[i].Add(link.Get(1));
    }
   // Manually define connections between switches
    Connect(csma, switches.Get(0), switches.Get(1), switchDevices[0], switchDevices[1]); // s1-s2
    Connect(csma, switches.Get(1), switches.Get(2), switchDevices[1], switchDevices[2]); // s2-s3
    Connect(csma, switches.Get(1), switches.Get(3), switchDevices[1], switchDevices[3]); // s2-s4
    
    // Install OpenFlow on each switch
    Ptr<ofi::LearningController> controller = CreateObject<ofi::LearningController>();
    OpenFlowSwitchHelper swtch;
    for (int i = 0; i < 4; ++i) {
        swtch.Install(switches.Get(i), switchDevices[i], controller);
    }

    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0","255.255.255.0");
    ipv4.Assign(hostDevices);

  // Setup UDP flow from h1 to h4
    
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.3"), port)));
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer app = onoff.Install(hosts.Get(0)); // h1
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    app = sink.Install(hosts.Get(3)); // h4
    app.Start(Seconds(0.0));
    app.Stop(Seconds(10.0));

    //ASCII tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("centroid.tr"));


    //animation
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    //MobilityHelper mobility;
    //hosts
    AnimationInterface::SetConstantPosition (hosts.Get (0), -30, 0);//h0
    AnimationInterface::SetConstantPosition (hosts.Get (1), 10, 0);//h1
    AnimationInterface::SetConstantPosition (hosts.Get (2), 0, 30);//h2
    AnimationInterface::SetConstantPosition (hosts.Get (3), 0, -30);//h3
    //switches

    AnimationInterface::SetConstantPosition (switches.Get (0), -15, 0);//s0
    AnimationInterface::SetConstantPosition (switches.Get (1), 0,  0);//s1
    AnimationInterface::SetConstantPosition (switches.Get (2), 0, 15);//s2
    AnimationInterface::SetConstantPosition (switches.Get (3), 0, -15);//s3

    AnimationInterface anim("anim-centroid.xml");


    Simulator::Stop(Seconds(11.0));
    Simulator::Run();
    Simulator::Destroy();
    return 0;
}   