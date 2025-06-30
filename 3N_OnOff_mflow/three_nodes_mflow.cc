//-------------Version 4 multiple flows
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

using namespace ns3;


void CreateRandomFlow(Ptr<Node> src, Ptr<Node> dst, Ipv4Address dstAddress, double startTime, double stopTime, uint16_t port = 9)
{
    // Set up UDP OnOffApplication
    OnOffHelper onoff("ns3::UdpSocketFactory", InetSocketAddress(dstAddress, port));
    onoff.SetAttribute("PacketSize", UintegerValue(1024));

    // Randomize data rate: 1–10 Mbps
    Ptr<UniformRandomVariable> rateVar = CreateObject<UniformRandomVariable>();
    rateVar->SetAttribute("Min", DoubleValue(1.0));
    rateVar->SetAttribute("Max", DoubleValue(10.0));
    double rate = rateVar->GetValue();
    onoff.SetAttribute("DataRate", StringValue(std::to_string(rate) + "Mbps"));

    // Exponential On/Off durations
    onoff.SetAttribute("OnTime", StringValue("ns3::ExponentialRandomVariable[Mean=1.0]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=0.5]"));

    // Install app on the source node
    ApplicationContainer app = onoff.Install(src);
    app.Start(Seconds(startTime));
    app.Stop(Seconds(stopTime));


}


void GenerateRandomTraffic(NodeContainer& nodes, 
                           std::vector<Ipv4InterfaceContainer>& interfaces,
                           uint32_t nFlows,
                           double simDuration)
{    


    Ptr<UniformRandomVariable> nodePicker = CreateObject<UniformRandomVariable>();
    //Ptr<UniformRandomVariable> dscpPicker = CreateObject<UniformRandomVariable>();
    Ptr<UniformRandomVariable> startTimePicker = CreateObject<UniformRandomVariable>();
  


    uint32_t numNodes = nodes.GetN();



    for (uint32_t i = 0; i < nFlows; ++i)
    {
        // Pick random src and dst (src ≠ dst)
        uint32_t srcIndex = nodePicker->GetInteger(0, numNodes - 1);
        uint32_t dstIndex;
        do {
            dstIndex = nodePicker->GetInteger(0, numNodes - 1);
        } while (dstIndex == srcIndex);

        Ptr<Node> src = nodes.Get(srcIndex);
        Ptr<Node> dst = nodes.Get(dstIndex);

        // Pick IP address for destination
        Ipv4Address dstIp = Ipv4Address::GetAny();
        for (auto& iface : interfaces)
        {   //To get the Node pointer owning the interface
            //Ipv4InterfaceContainer::Get(index) returns a std::pair<Ptr<Ipv4>, uint32_t>
            if (iface.Get(0).first->GetObject<Node>() == dst)
            {
                dstIp = iface.GetAddress(0); // Get IP of destination
                break;
            }
            if (iface.Get(1).first->GetObject<Node>() == dst)
            {
                dstIp = iface.GetAddress(1);
                break;
            }
        }

        // Skip if IP not found (topology mismatch)
        if (dstIp == Ipv4Address::GetAny()) continue;

      ;
   

        // Pick start time in [1.0, simDuration - 5]
        double startTime = startTimePicker->GetValue(1.0, simDuration - 5.0);

        // Create flow
        CreateRandomFlow(src, dst, dstIp, startTime, simDuration - 1.0);
    }
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
    Ipv4InterfaceContainer i0i1 = address.Assign(d0d1);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i0i2 = address.Assign(d0d2);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i1i2 = address.Assign(d1d2);
    


    //Configure routing
    //Works only with static topologies (i.e., no mobility or topology changes during simulation)
    //Centralized; done at simulation startup
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();


    //Generate Traffic using DSCP

    std::vector<Ipv4InterfaceContainer> interfaces = {i0i1, i0i2, i1i2};

    double simulationTime = 10.0;
    uint32_t numFlows = 20;
    //numFlows flows = numFlows sockets created over the simulation period, each sending packets according to the OnOff traffic pattern.
    GenerateRandomTraffic(nodes, interfaces, numFlows, simulationTime);
    // Flow monitor

    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    AnimationInterface anim("three_nodes.xml");
    Simulator::Stop(Seconds(31.0));
    Simulator::Run();
    flowMonitor->SerializeToXmlFile("flowmon_three_nodes.xml", true, true);

    Simulator::Destroy();
    return 0;
}

