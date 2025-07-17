
// adapted from example/traffic-control/traffic-control.cc
//3 sockets per ToS value sending traffic on different ports in the sink
//source: node 1
//sink  : node 0
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

    // Flow n1: sender  / source
          //n0: receiver/ sink

    //sink app
  
   std::vector<uint8_t> dscpValues = {0x00, 0x28, 0xb8}; // Best Effort, AF, EF
    for (size_t i = 0; i < dscpValues.size(); ++i) {
        uint16_t port = 8000 + i; // unique port
        PacketSinkHelper sinkHelper(socketType, InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sink = sinkHelper.Install(nodes.Get(0)); // node 0
        sink.Start(Seconds(0.0));
        sink.Stop(Seconds(10.0));
    }


    //sending app in the source node
    uint32_t payloadSize = 1448; //for the source

    uint16_t port = 7; //for the sink
    
    //Create 3 sockets for each ToS value and generate packets per each socket
    
    std::vector<ApplicationContainer> appContainers;

    for (size_t i = 0; i < dscpValues.size(); ++i) {
        //sending app in the source node
        uint8_t tos = dscpValues[i];
        std::cout << "The tos in hex is: 0x" << std::hex << static_cast<int>(tos) << std::endl;
        // Create a new OnOff app
        OnOffHelper onoff(socketType, Ipv4Address::GetAny());
        onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        onoff.SetAttribute("PacketSize", UintegerValue(payloadSize));
        onoff.SetAttribute("DataRate", StringValue("50Mbps")); // bit/s

        // Create a destination socket with specific ToS
        InetSocketAddress rmt(interfaces.GetAddress(0), port + i); // Use different ports if needed
        rmt.SetTos(tos);
        onoff.SetAttribute("Remote", AddressValue(rmt));

        // Install the app
        ApplicationContainer app = onoff.Install(nodes.Get(1));  // senderNode is nodes.Get(1) in your case
        app.Start(Seconds(1.0 + i)); // stagger start times
        app.Stop(Seconds(10.0));

        appContainers.push_back(app);
    }


    AnimationInterface anim("qdisc-animation.xml");
    Simulator::Stop(Seconds(simulationTime + 5)); 
    Simulator::Run();

    

 
    return 0;
}
