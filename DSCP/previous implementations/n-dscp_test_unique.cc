
// adapted from example/traffic-control/traffic-control.cc
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TrafficControlExample");



int
main(int argc, char* argv[])
{
    double simulationTime = 10; // seconds
    std::string transportProt = "Tcp";
    std::string socketType;


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
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1p"));

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::RedQueueDisc");
    QueueDiscContainer qdiscs = tch.Install(devices);





    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Flow
    //sink app in the receiving node
    uint16_t port = 7;
    Address localAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper packetSinkHelper(socketType, localAddress);
    ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));

    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(Seconds(simulationTime + 0.1));

    //source app in the sending node
    uint32_t payloadSize = 1448;
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    OnOffHelper onoff(socketType, Ipv4Address::GetAny());
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
    onoff.SetAttribute("DataRate", StringValue("50Mbps")); // bit/s
    ApplicationContainer apps;

    InetSocketAddress rmt(interfaces.GetAddress(0), port);
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    std::vector<uint8_t> dscpValues = {0x00, 0x28, 0x48, 0x68, 0xb8}; // Default, AF, EF
    uint8_t tos = dscpValues[dist(gen) % dscpValues.size()]; //tos should be of type uint8_t and not uint32_t for example
    

    //Hardcoded tos
    /*
    uint8_t tos= 0xb8;
    rmt.SetTos(0xb8);
    */
    
    std::cout << "The tos in hex is: 0x" << std::hex << static_cast<int>(tos) << std::endl;
    rmt.SetTos(tos);
    
    AddressValue remoteAddress(rmt);
    onoff.SetAttribute("Remote", remoteAddress);
    apps.Add(onoff.Install(nodes.Get(1)));
    apps.Start(Seconds(1.0));
    apps.Stop(Seconds(simulationTime + 0.1));



    Simulator::Stop(Seconds(simulationTime + 5)); 
    Simulator::Run();

    

 
    return 0;
}
