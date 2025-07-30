
#include "ns3/traffic-control-module.h"
#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <string>


using namespace ns3;


//Utility functions

void PrintVector(const std::vector<std::string>& vec) {
    std::cout<<"print vector"<<std::endl;
    for (const auto& s : vec) {
        std::cout << s << std::endl;
    }
}

std::vector<std::string> splitInput(const std::string& input){
    std::vector<std::string> result;
    std::stringstream ss(input);
    std::string item;

    while (std::getline(ss,item,',')){
        result.push_back(item);
    }
    return result;

}

NS_LOG_COMPONENT_DEFINE("MultiLinks");

int
main(int argc, char* argv[])

{     
    Config::SetDefault ("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

    //Link characteristics
    uint32_t numLinks= 2;
    std::string dataRates="1Mbps,500Kbps";
    std::string delays="100ms,200ms";
    std::string queueType = "ns3::FifoQueueDisc";
    std::string queueMaxSize="100p";


    //app characteristics

    //fixed for the moments
    uint32_t app_start_time=1;
    uint32_t app_end_time=10;
    uint32_t sink_start_time=1;
    uint32_t sink_end_time =25;
    uint32_t simulation_end_time=30;

    //entered as arguments
    std::string transportType = "udp"; // or "tcp"
    std::string onTimeType = "Constant";
    std::string onTimeParam = "1.0";
    std::string offTimeType = "Constant";
    std::string offTimeParam = "0.5";
    std::string appRate = "1Mbps";
    uint32_t    packetSize = 100;



    CommandLine cmd(__FILE__);

    cmd.AddValue("numLinks", "Number of point-to-point links", numLinks);
    cmd.AddValue("dataRates", "Data rate of each link", dataRates);
    cmd.AddValue("delays", "Propagation delay of each link", delays);
    cmd.AddValue("queueType", "Type of root queue discipline", queueType);
    cmd.AddValue("queueMaxSize","Max size of packets per queue", queueMaxSize);

    cmd.AddValue("transportType", "Transport type: udp or tcp", transportType);
    cmd.AddValue("onTimeType", "Type of OnTime distribution (Constant, Exponential)", onTimeType);
    cmd.AddValue("onTimeParam", "Parameter for OnTime distribution", onTimeParam);
    cmd.AddValue("offTimeType", "Type of OffTime distribution (Constant, Exponential)", offTimeType);
    cmd.AddValue("offTimeParam", "Parameter for OffTime distribution", offTimeParam);


    cmd.AddValue("appRate", "Application data rate", appRate);
    cmd.AddValue("packetSize", "Packet size in bytes", packetSize);
    cmd.Parse(argc, argv);  


    std::string onTime = "ns3::" + onTimeType + "RandomVariable[" +
                     (onTimeType == "Constant" ? "Constant=" : "Mean=") + onTimeParam + "]";

    std::string offTime = "ns3::" + offTimeType + "RandomVariable[" +
                      (offTimeType == "Constant" ? "Constant=" : "Mean=") + offTimeParam + "]";



    // Create nodes
    NodeContainer nodes;
    nodes.Create(2);
    //Install Internet Stack 
    InternetStackHelper internet;
    internet.Install(nodes);

    //Create devices and install queues
    TrafficControlHelper tch;
    tch.SetRootQueueDisc(queueType, "MaxSize", StringValue(queueMaxSize)); // "1000p"
    
    PointToPointHelper p2p;
    std::vector<NetDeviceContainer> devices;
    std::vector<Ipv4InterfaceContainer> interfaces;
    std::vector<QueueDiscContainer> queues;


    std::vector<std::string> dataRates_parsed;
    std::istringstream iss(dataRates);
    std::string rate;
    while (std::getline(iss, rate, ',')) {
        dataRates_parsed.push_back(rate);
        std::cout<<"rate parsed: "<<rate<<std::endl; //debug
    }

    std::vector<std::string> delays_parsed;
    std::istringstream issdelay(delays);
    std::string delay1;
    while (std::getline(issdelay, delay1, ',')) {
        delays_parsed.push_back(delay1);
        std::cout<<"delay parsed: "<<delay1<<std::endl; //debug
    }
    NS_ASSERT_MSG(dataRates_parsed.size() == numLinks, "Mismatch in number of links and dataRates"); //debug
    NS_ASSERT_MSG(delays_parsed.size() == numLinks, "Mismatch in number of links and delays"); //debug


    for (uint32_t i = 0; i < numLinks; ++i) {

        p2p.SetDeviceAttribute("DataRate", StringValue(dataRates_parsed[i]));
        p2p.SetChannelAttribute("Delay", StringValue(delays_parsed[i]));   
        NetDeviceContainer dev = p2p.Install(nodes.Get(0), nodes.Get(1));
        devices.push_back(dev);
        QueueDiscContainer qdisc=tch.Install(dev);
        queues.push_back(qdisc);
	    

         //Assign IP addresses
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        Ipv4AddressHelper address;
        address.SetBase(subnet.str().c_str(), "255.255.255.0");
        Ipv4InterfaceContainer iface = address.Assign(dev);
        interfaces.push_back(iface);
    }
   

    //Enable routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //Create sending app
    std::string transportFactory;
    if (transportType=="udp"){
        transportFactory="ns3::UdpSocketFactory";
    }
    else{
        transportFactory ="ns3::TcpSocketFactory";
    }
    uint16_t port = 9; 
    Ptr<Node> dstNode = nodes.Get(0);
    Ptr<NetDevice> dstDev = devices[0].Get(0); //take from the first device
    Ptr<Ipv4> ipv4 = dstNode->GetObject<Ipv4>();
    uint32_t interfaceIndex = ipv4->GetInterfaceForDevice(dstDev);
    Ipv4Address dstAddr = ipv4->GetAddress(interfaceIndex, 0).GetLocal();
    //std::cout<<"dst adress is "<<dstAddr<<std::endl; //debug
    OnOffHelper onoff(transportFactory, InetSocketAddress(dstAddr, port));
    onoff.SetConstantRate(DataRate(appRate)); 
    onoff.SetAttribute("PacketSize", UintegerValue(packetSize)); 
    onoff.SetAttribute("OnTime", StringValue(onTime));
    onoff.SetAttribute("OffTime", StringValue(offTime));
    ApplicationContainer apps = onoff.Install(nodes.Get(1)); //sending node 1
    apps.Start(Seconds(app_start_time));
    apps.Stop(Seconds(app_end_time));

    //Create receiving sink
    PacketSinkHelper sink(transportFactory,
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
 
    apps = sink.Install(nodes.Get(0)); //receiving node 0
    apps.Start(Seconds(sink_start_time));
    apps.Stop(Seconds(sink_end_time));



    //p2p.EnablePcapAll("multi-link");

   //Set mobility
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);  

    //AnimationInterface anim("multi-link-anim.xml");
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();

    Simulator::Stop(Seconds(simulation_end_time));
    Simulator::Run ();
    //flowMonitor->SerializeToXmlFile("multi-links-flow.xml", true, true);

    Simulator::Destroy ();

    return 0;

}