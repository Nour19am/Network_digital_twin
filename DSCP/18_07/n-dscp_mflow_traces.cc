#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/netanim-module.h"
#include <random>
#include <iomanip>  // for hex formatting
#include <fstream>


// Converts DSCP and ECN to a full 8-bit TOS byte
uint8_t dscpToTos(uint8_t dscp, uint8_t ecn = 0) {
    if (dscp > 63) {
        std::cerr << "Error: DSCP value must be between 0 and 63.\n";
        return 0;
    }
    if (ecn > 3) {
        std::cerr << "Error: ECN value must be between 0 and 3.\n";
        return 0;
    }
    return static_cast<uint8_t>((dscp << 2) | (ecn & 0x03));
}
using namespace ns3;
std::ofstream traceFile("qdisc_dscp_trace.log", std::ios::out);

/**
 * Number of packets in TX queue trace.
 *
 * \param oldValue Old velue.
 * \param newValue New value.
 */
void
TcPacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
    //std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
    //traceFile << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
    traceFile << Simulator::Now().GetSeconds() << "s: TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;

}

/**
 * Packets in the device queue trace.
 *
 * \param oldValue Old velue.
 * \param newValue New value.
 */
void
DevicePacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
    //std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
    //traceFile << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
    traceFile << Simulator::Now().GetSeconds() << "s: DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

/**
 * TC Soujoun time trace.
 *
 * \param sojournTime The soujourn time.
 */
void
SojournTimeTrace(Time sojournTime)
{
    //std::cout << "Sojourn time " << sojournTime.ToDouble(Time::MS) << "ms" << std::endl;
    //traceFile << "SojournTime " << sojournTime.ToDouble(Time::MS) << " ms" << std::endl;
    traceFile << Simulator::Now().GetSeconds() << "s: SojournTime " << sojournTime.ToDouble(Time::MS) << " ms" << std::endl;
}



std::ofstream sinkLogFile("qdisc_dscp_sink_log.txt", std::ios::out);

// Global ToS to port mapping

std::map<uint16_t, uint16_t>port_sender_to_port_receiver;
const std::map<uint16_t, uint8_t> portToTosMap = {
    {8000, 0x08},
    {8001, 0x0a},
    {8002, 0x0c},
    {8003, 0x0e}
};


void SinkRxTrace(Ptr<const Packet> packet, const Address &address) {
    InetSocketAddress addr = InetSocketAddress::ConvertFrom(address);
    double time = Simulator::Now().GetSeconds();

    uint16_t port = addr.GetPort();
   // uint16_t receiver_port=port_sender_to_port_receiver[port];
    //uint8_t tos = portToTosMap.at(receiver_port);;  // get corresponding TOS

    sinkLogFile << "At time +" << time
                << "s sink received " << packet->GetSize()
                << " bytes from " << addr.GetIpv4()
                << " port " << port<< std::endl;
                //<< " ToS=0x" << std::hex << int(tos) << std::dec << std::endl;
}


NS_LOG_COMPONENT_DEFINE("TrafficControlExample");






int
main(int argc, char* argv[])
{
    double simulationTime = 5; // seconds
    std::string transportProt = "Tcp";
    std::string socketType;
    NS_LOG_UNCOND("App started, sending traffic");
    //LogComponentEnable("PacketSink", LOG_LEVEL_INFO);


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
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");  // RedQueueDisc
    QueueDiscContainer qdiscs = tch.Install(devices);

    Ptr<QueueDisc> q = qdiscs.Get(1);
    q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPacketsInQueueTrace));
    Config::ConnectWithoutContext(
        "/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",MakeCallback(&SojournTimeTrace));

    Ptr<NetDevice> nd = devices.Get(1);
    Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice>(nd);
    Ptr<Queue<Packet>> queue = ptpnd->GetQueue();
    queue->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueTrace));

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);

    // Flow n1: sender  / source
          //n0: receiver/ sink

    // Setting applications
   ApplicationContainer sourceApplications, sinkApplications;
   //complete tos list : tosValues_band1= {0x00,0x02,0x04,0x06,0x18,0x1a,0x1c,0x1e} //map to band1
                    //   tosValues_band2= {0x08,0x0a,0x0c,0x0e}
                    //   tosValues_band0= {0x10,0x12,0x14,0x16}
    // maximize througput 0x08->2, normal service: 0x00 ->1, minimize delay: 0x10 ->0, 0x12->0 MMC+MD  {0x10, 0x00 ,0x08}
   std::vector<uint8_t> tosValues = {0x08,0x0a,0x0c,0x0e}; 
   uint32_t portNumber = 8000;//for the sink
   uint32_t payloadSize =1448 ; //for the source 1448 bytes
   //for (uint32_t index = 0; index < tosValues.size(); ++index)
    // {
       for (uint8_t tosValue : tosValues)
         {  
            std::cout<<"portnumber -1: "<<portNumber<<std::endl;
            //sink
            auto ipv4 = nodes.Get (0)->GetObject<Ipv4> ();
            const auto address = ipv4->GetAddress (1, 0).GetLocal ();
            InetSocketAddress sinkSocket (address, portNumber);
            sinkSocket.SetTos (tosValue);
            PacketSinkHelper packetSinkHelper(socketType, sinkSocket);
            ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(0));
            sinkApplications.Add(sinkApp);
            
        
           // Map port to TOS for logging
            //portToTosMap[portNumber] = tosValue;
            //port_sender_to_port_receiver[portNumber]=address.GetPort();
            

            std::cout<<"portnumber : "<<portNumber<<" & tos value: "<<" ToS=0x" << std::hex << int(tosValue) << std::dec << std::endl;
            // Connect Rx trace to the sink
          Ptr<Application> app = sinkApp.Get(0);
          app->TraceConnectWithoutContext("Rx", MakeCallback(&SinkRxTrace));

          //source
           OnOffHelper onOffHelper (socketType, sinkSocket);
           onOffHelper.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
           onOffHelper.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
           onOffHelper.SetAttribute ("DataRate", StringValue("100Mbps"));
           onOffHelper.SetAttribute ("PacketSize", UintegerValue(payloadSize)); //bytes
           

           sourceApplications.Add (onOffHelper.Install (nodes.Get (1)));
           portNumber++;
         
         }
     //}
 
   sinkApplications.Start (Seconds (0.0));
   sinkApplications.Stop (Seconds (simulationTime + 1));
   sourceApplications.Start (Seconds (1.0));
   sourceApplications.Stop (Seconds (simulationTime + 1));

    
  
  

    

    AnimationInterface anim("qdisc-animation.xml");
     
    //tracing
    //pcap files
    pointToPoint.EnablePcapAll("qdisc_dscp");
    //flow monitor
    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simulationTime + 5)); 
    Simulator::Run();
    monitor->SerializeToXmlFile("qdisc_dscp_2_nodes.xml", true, true);
 

    
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
  

for (const auto& flowPair : stats)
{
    FlowId flowId = flowPair.first;
    auto dscpVec = classifier->GetDscpCounts(flowId);

    for (auto p : dscpVec) {
        uint8_t dscp = p.first;
        uint32_t count = p.second;
        std::ofstream out("qdisc_dscp_counts.txt", std::ios::app);
  
        // Map DSCP to band manually (pfifo_fast: 0 = high, 1 = medium, 2 = low)
        //int band = (dscp == 0xb8) ? 0 : (dscp == 0x28) ? 1 : 2;

        out << "FlowID=" << flowId
            << " DSCP=0x" << std::hex << static_cast<int>(dscp) << std::dec
            << " Count=" << count
           
            << std::endl;
    }
  }

    Simulator::Destroy();
    traceFile.close();
    sinkLogFile.close();


    return 0;
}
