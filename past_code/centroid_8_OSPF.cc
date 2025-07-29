//centroid topology with OSPF

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

NS_LOG_COMPONENT_DEFINE("Centroid_OSPF_Topology");
void Connect(CsmaHelper& csma, Ptr<Node> a, Ptr<Node> b, NetDeviceContainer& devA, NetDeviceContainer& devB) {
    NetDeviceContainer link = csma.Install(NodeContainer(a, b));

    Ptr<NetDevice> dev0 = link.Get(0);
    Ptr<NetDevice> dev1 = link.Get(1);
    devA.Add(dev0);
    devB.Add(dev1);
}

int main(int argc, char *argv[]) {
    //Enable logging
    LogComponentEnable("OnOffApplication", LOG_LEVEL_INFO);

    // Create nodes s1-s8
    NodeContainer nodes;
    nodes.Create(8);

    // Internet stack for nodes only
    InternetStackHelper internet;
    internet.Install(nodes);

    // Create CSMA helper
    CsmaHelper csma;
    csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
    csma.SetChannelAttribute("Delay", TimeValue(MilliSeconds(2)));

    // Each node has its NetDevice container
    std::vector<NetDeviceContainer> nodeDevices(8);

    // Manually define connections between nodes
    Connect(csma, nodes.Get(0), nodes.Get(1), nodeDevices[0], nodeDevices[1]); // s1-s2
    Connect(csma, nodes.Get(0), nodes.Get(2), nodeDevices[0], nodeDevices[2]); // s1-s3
    Connect(csma, nodes.Get(1), nodes.Get(3), nodeDevices[1], nodeDevices[3]); // s2-s4
    Connect(csma, nodes.Get(2), nodes.Get(3), nodeDevices[2], nodeDevices[3]); // s3-s4
    Connect(csma, nodes.Get(3), nodes.Get(5), nodeDevices[3], nodeDevices[5]); // s4-s6
    Connect(csma, nodes.Get(4), nodes.Get(2), nodeDevices[4], nodeDevices[2]); // s5-s3
    Connect(csma, nodes.Get(4), nodes.Get(6), nodeDevices[4], nodeDevices[6]); // s5-s7
    Connect(csma, nodes.Get(2), nodes.Get(7), nodeDevices[2], nodeDevices[7]); // s3-s8

    //assign ip addresses
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    for (int i = 0; i < 8; ++i) {
        NetDeviceContainer singleDevice;
        singleDevice.Add(nodeDevices[i]);
        address.Assign(singleDevice);
        
        Ipv4InterfaceContainer interface = address.Assign(singleDevice);

        Ptr<Node> node = nodes.Get(i);
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        Ipv4Address ipAddr = ipv4->GetAddress(1, 0).GetLocal(); // (interface index, address index)

        std::cout << "Node " << i << " IP Address: " << ipAddr << std::endl;
        
    }

    //routing

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Setup UDP flow from h1 to h4
    
    uint16_t port = 9;
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address("10.1.1.6"), port)));
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer app = onoff.Install(nodes.Get(4)); // h5
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    PacketSinkHelper sink("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
    app = sink.Install(nodes.Get(5)); // h6
    
    app.Start(Seconds(0.0));
    app.Stop(Seconds(10.0));
    

    //ASCII tracing
    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("centroid.tr"));


    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();

    for (int i = 0; i < 8; ++i) {
        if (i%2==0){
            
            positionAlloc->Add(Vector(i * 30.0, 20, 0.0));   
        }
        else{
            positionAlloc->Add(Vector(i * 30.0, 0, 0.0));   
        }
        
    }

    mobility.SetPositionAllocator(positionAlloc);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);

    AnimationInterface anim("anim-centroid.xml");
    //add label

    
    for (int i = 0; i < 8; ++i) {
        std::ostringstream nodeLabel;
        nodeLabel << "h" << i+1;
        anim.UpdateNodeDescription(nodes.Get(i), nodeLabel.str()); // Label host
        anim.UpdateNodeColor(nodes.Get(i), 0, 255, 0); // Green for hosts

    }

    

    //run simulation

    Simulator::Run();
    Simulator::Destroy();
    return 0;


}