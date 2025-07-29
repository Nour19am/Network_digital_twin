//flooding problem but routing works for openflow
//centroid topology

#include <iostream>
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

void ReceivePacket(Ptr<const Packet> packet, const Address &addr) {
    std::cout << "Received packet of size " << packet->GetSize() << " bytes at " << Simulator::Now().GetSeconds() << " seconds\n";
}

void Connect(CsmaHelper& csma, Ptr<Node> a, Ptr<Node> b, NetDeviceContainer& devA, NetDeviceContainer& devB) {
    NetDeviceContainer link = csma.Install(NodeContainer(a, b));

    Ptr<NetDevice> dev0 = link.Get(0);
    Ptr<NetDevice> dev1 = link.Get(1);
    devA.Add(dev0);
    devB.Add(dev1);
    // Debug: print device type
    /*
    std::cout << "Device at node " << a->GetId() << ": "
              << dev0->GetInstanceTypeId().GetName() << std::endl;
    std::cout << "Device at node " << b->GetId() << ": "
              << dev1->GetInstanceTypeId().GetName() << std::endl;

    */
}

int main(int argc, char *argv[]) {
    //Enable logging
    LogComponentEnable("UdpClient", LOG_LEVEL_INFO);
    LogComponentEnable("UdpServer", LOG_LEVEL_INFO);
    LogComponentEnable("OpenFlowInterface", LOG_LEVEL_INFO);
    LogComponentEnable("CsmaNetDevice", LOG_LEVEL_INFO);
    LogComponentEnable("PacketSink", LOG_LEVEL_INFO);

   



    // Create switches s1-s8
    NodeContainer switches;
    switches.Create(8);

    // Create hosts h1-h8
    NodeContainer hosts;
    hosts.Create(8);

    // Internet stack for hosts only
    InternetStackHelper internet;
    internet.Install(hosts);


    // Create CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Each switch has its NetDevice container
    std::vector<NetDeviceContainer> switchDevices(8);
    NetDeviceContainer hostDevices;

    // Connect each host to its switch
    for (int i = 0; i < 8; ++i) {
        NetDeviceContainer link = csma.Install(NodeContainer(hosts.Get(i), switches.Get(i)));
        hostDevices.Add(link.Get(0));
        switchDevices[i].Add(link.Get(1));
    }

    // Manually define connections between switches
    Connect(csma, switches.Get(0), switches.Get(1), switchDevices[0], switchDevices[1]); // s1-s2
    Connect(csma, switches.Get(0), switches.Get(2), switchDevices[0], switchDevices[2]); // s1-s3
    Connect(csma, switches.Get(1), switches.Get(3), switchDevices[1], switchDevices[3]); // s2-s4
    Connect(csma, switches.Get(2), switches.Get(3), switchDevices[2], switchDevices[3]); // s3-s4
    Connect(csma, switches.Get(3), switches.Get(5), switchDevices[3], switchDevices[5]); // s4-s6
    Connect(csma, switches.Get(4), switches.Get(2), switchDevices[4], switchDevices[2]); // s5-s3
    Connect(csma, switches.Get(4), switches.Get(6), switchDevices[4], switchDevices[6]); // s5-s7
    Connect(csma, switches.Get(2), switches.Get(7), switchDevices[2], switchDevices[7]); // s3-s8

    // Install OpenFlow on each switch
    
    Ptr<ofi::LearningController> controller = CreateObject<ofi::LearningController>();
    OpenFlowSwitchHelper swtch;
    for (int i = 0; i < 8; ++i) {
        swtch.Install(switches.Get(i), switchDevices[i], controller);
    }

    

    // Assign IP addresses
    //subnet
    /*
    Ipv4AddressHelper ipv4;
    for (int i = 0; i < 8; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i+1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        NetDeviceContainer singleDevice;
        singleDevice.Add(hostDevices.Get(i));  // wrap the single device in a container
        ipv4.Assign(singleDevice);
        //ipv4.Assign(NetDeviceContainer(hostDevices.Get(i)));
        //debug
        //std::cout << "Subnet " << subnet.str() << " assigned to device " << i << std::endl;
    }
    */
   //all hosts are in the same network
   /*
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    //--------------------problem 
    for (int i = 0; i < 8; ++i) {
        NetDeviceContainer singleDevice;
        singleDevice.Add(hostDevices.Get(i));
        ipv4.Assign(singleDevice);
    }
    */

    //assign ip addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(hostDevices);



    
   


    // Setup UDP flow from h1 to h4
    
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.4"), port)));
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer app = onoff.Install(hosts.Get(0)); // h1
    app.Start(Seconds(1.0));
    app.Stop(Seconds(4.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    app = sink.Install(hosts.Get(3)); // h4
    app.Start(Seconds(0.0));
    app.Stop(Seconds(4.0));
    
    auto sinkApp = DynamicCast<PacketSink>(app.Get(0));
    sinkApp->TraceConnectWithoutContext("Rx", MakeCallback(&ReceivePacket));
    //ASCII tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("centroid.tr"));


    // Enable PCAP tracing
    //csma.EnablePcapAll("centroid");

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    for (int i = 0; i < 8; ++i) {
        positionAlloc->Add(Vector(i * 30.0, 0.0, 0.0));   // Hosts on bottom row
    }
    for (int i = 0; i < 8; ++i) {
        positionAlloc->Add(Vector(i * 30.0, 30.0, 0.0));  // Switches above
    }
    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(NodeContainer(hosts, switches));

    AnimationInterface anim("anim-centroid.xml");
    // Add labels and optional coloring for visualization

    /*
    for (int i = 0; i < 8; ++i) {
        std::ostringstream hostLabel;
        hostLabel << "h" << i+1;
        anim.UpdateNodeDescription(hosts.Get(i), hostLabel.str()); // Label host
        anim.UpdateNodeColor(hosts.Get(i), 0, 255, 0); // Green for hosts

        std::ostringstream switchLabel;
        switchLabel << "s" << i+1;
        anim.UpdateNodeDescription(switches.Get(i), switchLabel.str()); // Label switch
        anim.UpdateNodeColor(switches.Get(i), 0, 0, 255); // Blue for switches
    }

    */

    Simulator::Run();
    Simulator::Destroy();
    return 0;
}
