#include "ns3/core-module.h"

#include "ns3/network-module.h"

#include "ns3/internet-module.h"

#include "ns3/point-to-point-module.h"

#include "ns3/applications-module.h"

#include "ns3/flow-monitor-module.h"
#include <ns3/traffic-generator-ftp-single.h>
#include <ns3/traffic-generator-helper.h>
#include <ns3/traffic-generator-ngmn-ftp-multi.h>
#include <ns3/traffic-generator-ngmn-gaming.h>
#include <ns3/traffic-generator-ngmn-video.h>
#include <ns3/traffic-generator-ngmn-voip.h>
#include <ns3/packet-sink-helper.h>
#include <ns3/packet-sink.h>
#include <ns3/ping-helper.h>
#include <ns3/ipv4-global-routing-helper.h>

#include <fstream>

using namespace ns3;
#include <vector>
#include <fstream>
#include <iostream>



//std::vector<double> packetArrivalTimes;





struct PacketInfo {
    double time;             // seconds
    std::string trafficType; // VOIP, Video, FTP, Gaming
    uint32_t size;           // bytes
    /*
    std::string srcIp; //ns3::Ipv4Address
    std::string dstIp;
    uint16_t srcPort;
    uint16_t dstPort;
    */
};
std::vector<PacketInfo> packetArrivalData;

std::string currentTrafficType; // set this before simulation
void WritePacketInfoToFile(const std::vector<PacketInfo>& data, const std::string& filename)
{
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        std::cerr << "Error opening file " << filename << "\n";
        return;
    }

    outfile << "Time(s)\tTrafficType\tSize(Bytes)\n"; //\tSrcIP\tDstIP\tSrcPort\tDstPort
    for (const auto& pkt : data) {
        outfile << pkt.time << "\t" << pkt.trafficType << "\t" 
                << pkt.size << "\t" << "\n";
                /*
                << pkt.srcIp << "\t" << pkt.dstIp << "\t" 
                << pkt.srcPort << "\t" << pkt.dstPort << "\n";
                */
    }
}

void RxWithTrafficType(std::string ttype, Ptr<const Packet> pkt, const Address &addr)
{
    double now = Simulator::Now().GetSeconds();
    uint32_t pktSize = pkt->GetSize();

    Ipv4Header ipv4;
    pkt->PeekHeader(ipv4);

    TcpHeader tcp;
    pkt->PeekHeader(tcp);

    PacketInfo info;
    info.time = now;
    info.trafficType = ttype;
    info.size = pktSize; 
    /*
    info.srcIp = ipv4.GetSource().GetAddress();
    info.dstIp =ipv4.GetDestination().GetAddress();

    info.srcPort = tcp.GetSourcePort();
    info.dstPort = tcp.GetDestinationPort();
    */
    packetArrivalData.push_back(info);
}



enum TrafficType
{
    NGMN_FTP,
    NGMN_VIDEO,
    NGMN_GAMING,
    NGMN_VOIP
};

static inline std::istream&
operator>>(std::istream& is, TrafficType& item)
{
    uint32_t inputValue;
    is >> inputValue;
    item = (TrafficType)inputValue;
    return is;
}

TypeId
GetTypeId(const TrafficType& item)
{
    switch (item)
    {
    case NGMN_FTP:
        return TrafficGeneratorNgmnFtpMulti::GetTypeId();
    case NGMN_VIDEO:
        return TrafficGeneratorNgmnVideo::GetTypeId();
    case NGMN_GAMING:
        return TrafficGeneratorNgmnGaming::GetTypeId();
    case NGMN_VOIP:
        return TrafficGeneratorNgmnVoip::GetTypeId();
    default:
        NS_ABORT_MSG("Unknown traffic type");
    };
}

std::string
GetName(const TrafficType& item)
{
    switch (item)
    {
    case NGMN_FTP:
        return "ftp";
    case NGMN_VIDEO:
        return "video";
    case NGMN_GAMING:
        return "gaming";
    case NGMN_VOIP:
        return "voip";
    default:
        NS_ABORT_MSG("Unknown traffic type");
    };
}
Ptr<Socket> CreateTaggedSocket(Ptr<Node> node)
{
    Ptr<Socket> sock = Socket::CreateSocket(node, TcpSocketFactory::GetTypeId());
    sock->SetIpTos(0x08); // Set DSCP/TOS at IP level
    return sock;
}

int main (int argc, char* argv[]) {
    //enum TrafficType trafficType = NGMN_FTP;
    
    std::string LinkDataRate ="1Mbps";
    std::string LinkDelay = "0.5ms";

    bool useUdp = false;
    uint32_t appStartMs = 500;//ms
    uint32_t appDurationMs = 200000;//ms (200s)



    CommandLine cmd(__FILE__);
    //link characteristics

    cmd.AddValue("LinkDataRate", "the bandwidth of the link", LinkDataRate);
    
    cmd.AddValue("LinkDelay", "the propagation delay of the link", LinkDelay);




    cmd.Parse(argc, argv);  

    std::cout<<"LinkDataRate:  "<<LinkDataRate<<std::endl; //debug
    // configure the transport protocol to be used
    std::string transportProtocol;
    if (useUdp)
    {
        transportProtocol = "ns3::UdpSocketFactory";
    }
    else
    {
        transportProtocol = "ns3::TcpSocketFactory";
    }

    NodeContainer nodes;

    nodes.Create(2);

    PointToPointHelper pointToPoint;

    pointToPoint.SetDeviceAttribute("DataRate", StringValue(LinkDataRate));

    pointToPoint.SetChannelAttribute("Delay", StringValue(LinkDelay));

    NetDeviceContainer devices;

    devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;

    stack.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interfaces = address.Assign(devices);



    // Create a custom socket for tagging
    //Ptr<Socket> sock = CreateTaggedSocket(nodes.Get(0));


    std::vector<TrafficType> trafficTypes = {NGMN_FTP, NGMN_VIDEO, NGMN_GAMING, NGMN_VOIP};
    std::vector<uint8_t> tosValues = {0x28, 0x68, 0x88, 0xb8}; // Example tos for each traffic type AF11,AF31,AF41,EF
    //uint16_t basePort = 4000;

    ApplicationContainer sinkApps;
    ApplicationContainer generatorApps;

    for (size_t i = 0; i < trafficTypes.size(); ++i) {
        //uint16_t port = basePort + i;
        uint16_t port = 0;

    switch (trafficTypes[i]) {
        case NGMN_VOIP:
            port = 5060;
            break;
        case NGMN_FTP:
            port = 21;
            break;
        case NGMN_GAMING:
            port = 5000;
            break;
        case NGMN_VIDEO:
            port = 16384;
            break;
        default:
            NS_ABORT_MSG("Unknown traffic type");
    }

        // Install packet sink on receiver node for this traffic type
        InetSocketAddress rxAddress(Ipv4Address::GetAny(), port);
        rxAddress.SetTos (tosValues[i]);
        PacketSinkHelper packetSinkHelper(transportProtocol, rxAddress);
        ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));

        sinkApp.Start(MilliSeconds(appStartMs));
        sinkApp.Stop(MilliSeconds(appStartMs + appDurationMs));
        sinkApps.Add(sinkApp);
        
        // Connect callback *per sink*
        Ptr<Application> app = sinkApp.Get(0);
        Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
        sink->TraceConnectWithoutContext(
            "Rx",
            MakeBoundCallback(&RxWithTrafficType, GetName(trafficTypes[i]))
        );


        // Install traffic generator on sender node
        InetSocketAddress txAddress(interfaces.GetAddress(1, 0), port);
        txAddress.SetTos (tosValues[i]);
        TrafficGeneratorHelper trafficGeneratorHelper(
            transportProtocol,
            txAddress,
            GetTypeId(trafficTypes[i])
        );

   
       

        ApplicationContainer genApp = trafficGeneratorHelper.Install(nodes.Get(0));
  
        genApp.Start(MilliSeconds(appStartMs));
        genApp.Stop(MilliSeconds(appStartMs + appDurationMs));
        generatorApps.Add(genApp);

    }
 




    // Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    //pointToPoint.EnablePcapAll("traffic");


    Simulator::Stop(MilliSeconds(appStartMs + appDurationMs));
    Simulator::Run ();
    monitor->SerializeToXmlFile("multi-links-flow.xml", true, true);

    Simulator::Destroy();
    WritePacketInfoToFile(packetArrivalData, "packet_arrival_times.txt");

    return 0;

}