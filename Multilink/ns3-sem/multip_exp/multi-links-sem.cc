
//./ns3 run "scratch/multi-links-sem --numLinks=2 --dataRates=2Mbps,1Mbps --delays=100ms,200ms --appRate=2Mbps"
//run with conda environement open (so that the python3 from conda is used)
//for me it is centroid-env
//delete the .pcap, .xml, .csv ... files to clean the git repo before running it with sem
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
#include <filesystem>  
#include <cstdlib>

namespace fs = std::filesystem;
using namespace ns3;
void PacketRxCallback(Ptr<const Packet> packet, Ptr<Ipv4> ipv4, uint32_t interface)
{
    Ipv4Address addr = ipv4->GetAddress(interface, 0).GetLocal();

    std::cout << "Packet received at time: " << Simulator::Now().GetSeconds()
              << "s, size: " << packet->GetSize() << " bytes"
              << ", on interface index: " << interface
              << ", IP: " << addr
              << std::endl;
}



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
    uint32_t sink_end_time =12;
    uint32_t simulation_end_time=13;

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
        //std::cout<<"rate parsed: "<<rate<<std::endl; //debug
    }

    std::vector<std::string> delays_parsed;
    std::istringstream issdelay(delays);
    std::string delay1;
    while (std::getline(issdelay, delay1, ',')) {
        delays_parsed.push_back(delay1);
        //std::cout<<"delay parsed: "<<delay1<<std::endl; //debug
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
    ApplicationContainer Senderapps = onoff.Install(nodes.Get(1)); //sending node 1
    Senderapps.Start(Seconds(app_start_time));
    Senderapps.Stop(Seconds(app_end_time));

    //Create receiving sink
    PacketSinkHelper sink(transportFactory,
                          Address(InetSocketAddress(Ipv4Address::GetAny(), port)));
 
    ApplicationContainer Sinkapps = sink.Install(nodes.Get(0)); //receiving node 0
    Sinkapps.Start(Seconds(sink_start_time));
    Sinkapps.Stop(Seconds(sink_end_time));


    
    p2p.EnablePcapAll("multi-link");

   //Set mobility
   /*
    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(nodes);  
    
    AnimationInterface anim("multi-link-anim.xml");
    */
    Ptr<FlowMonitor> flowMonitor;
    FlowMonitorHelper flowHelper;
    flowMonitor = flowHelper.InstallAll();
    
    Simulator::Stop(Seconds(simulation_end_time));
    Simulator::Run ();
    flowMonitor->SerializeToXmlFile("multi-links-flow.xml", true, true);
    
    Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(Sinkapps.Get(0));
    

   
   

    Simulator::Destroy ();

    //--------------------parsing of the pcap files
        /*
    std::string pcapDir = "/home/nourhen_dev/repos/ns-3-allinone/ns-3.41/";
    std::string pythonScript = "python3 scratch/multi-link_parse_pcap.py";

    for (const auto& entry : fs::directory_iterator(pcapDir)) {
        
        if (entry.is_regular_file() && entry.path().extension() == ".pcap") {
            if (fs::file_size(entry.path()) == 0) {
                std::cout << "SkippSing empty file: " << entry.path() << std::endl;
                continue;
            }
            std::string pcapFile = entry.path().string();

            // Construct the system command to run python parser on this file
            std::string cmd_pcap = pythonScript + " " + pcapFile + " > pcap_features.txt";

            int result = system(cmd_pcap.c_str());
            if (result == 0) {
                std::ifstream inFile("pcap_features.txt");
                std::string line;
                std::cout << "=== Parsing file: " << pcapFile << " ===" << std::endl;
                while (std::getline(inFile, line)) {
                    std::cout << line << std::endl;
                }
                std::cout << std::endl;
            } else {
                std::cerr << "Failed to run Python parser on " << pcapFile << std::endl;
            }
        }
    }
        */

    //simpler version with only one pcap file node 0, device 1 (the first device starts from count 1 and not 0 for this sim)
    /*
    int result = system("python3 scratch/multi-link_parse_pcap.py /home/nourhen_dev/repos/ns-3-allinone/ns-3.41/multi-link-0-1.pcap> pcap_features.txt");

    if (result == 0) {
        std::ifstream inFile("pcap_features.txt"); //this file will be in the ns-3 repo
        std::string line;
        while (std::getline(inFile, line)) {
            std::cout << line << std::endl;
        }
    } 
    else {
        std::cerr << "Failed to run Python parser." << std::endl;
    }
    */
   //--------------------parsing of the flowmonitor file
    std::string xml_file = "multi-links-flow.xml";
    std::string csv_file = "flow_df.csv";
    std::string cmd_flow = "/home/nourhen_dev/miniconda3/envs/centroid-env/bin/python /home/nourhen_dev/ns3-workspace/ns-3-dev/scratch/multi-link_flow_parser.py " + xml_file;
    int result = system(cmd_flow.c_str());
    if (result != 0) {
        std::cerr << "Python parser failed!" << std::endl;
        return 1;
    }

    std::ifstream inFile(csv_file);
    if (!inFile) {
        std::cerr << "Failed to open CSV file." << std::endl;
        return 1;
    }

    std::string line;
    while (std::getline(inFile, line)) {
        std::cout << line << std::endl;
    }

   

    return 0;

}