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

void WritePacketArrivalTimesToFile(const std::vector<double>& packetArrivalTimes, const std::string& filename) {
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        std::cerr << "Error opening file " << filename << " for writing." << std::endl;
        return;
    }

    for (const auto& time : packetArrivalTimes) {
        outfile << time << "\n";
    }

    outfile.close();
    std::cout << "Successfully wrote " << packetArrivalTimes.size() << " timestamps to " << filename << std::endl;
}
std::vector<double> packetArrivalTimes;

void RxCallback(Ptr<const Packet> packet) {
    double now = Simulator::Now().GetSeconds();
    packetArrivalTimes.push_back(now);
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

int main (int argc, char* argv[]) {
    enum TrafficType trafficType = NGMN_VIDEO;
    
    std::string LinkDataRate ="3Mbps";
    std::string LinkDelay = "0.5ms";

    bool useUdp = false;
    uint32_t appStartMs = 500;//ms
    uint32_t appDurationMs = 2000000;//ms



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

    // install the packet sink at the receiver node
    uint16_t port = 4000;
    InetSocketAddress rxAddress(Ipv4Address::GetAny(), port);

    PacketSinkHelper packetSinkHelper(transportProtocol, rxAddress);
    // install the application on the rx device
    ApplicationContainer sinkApplication = packetSinkHelper.Install(nodes.Get(1));
    sinkApplication.Start(MilliSeconds(appStartMs));
    sinkApplication.Stop(MilliSeconds(appStartMs + appDurationMs));

    // install the traffic generator at the transmitter node
    TrafficGeneratorHelper trafficGeneratorHelper(
        transportProtocol,
        InetSocketAddress(interfaces.GetAddress(1, 0), port),
        GetTypeId(trafficType));

    ApplicationContainer generatorApplication = trafficGeneratorHelper.Install(nodes.Get(0));
    generatorApplication.Start(MilliSeconds(appStartMs));
    generatorApplication.Stop(MilliSeconds(appStartMs + appDurationMs));
    // Seed the ARP cache by pinging early in the simulation
    // This is a workaround until a static ARP capability is provided
    PingHelper pingHelper(interfaces.GetAddress(1, 0));
    ApplicationContainer pingApps = pingHelper.Install(nodes.Get(0));
    pingApps.Start(MilliSeconds(10));
    pingApps.Stop(MilliSeconds(500));

    Ptr<TrafficGenerator> trafficGenerator =
        generatorApplication.Get(0)->GetObject<TrafficGenerator>();
    Ptr<PacketSink> packetSink = sinkApplication.Get(0)->GetObject<PacketSink>();
    
    //uint64_t totalBytesSent = trafficGenerator->GetTotalBytes();
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        devices.Get(i)->TraceConnectWithoutContext("PhyRxEnd", MakeCallback(&RxCallback));
    }


    // Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    pointToPoint.EnablePcapAll("traffic");


    Simulator::Stop(MilliSeconds(appStartMs + appDurationMs));
    Simulator::Run ();
    monitor->SerializeToXmlFile("multi-links-flow.xml", true, true);

    Simulator::Destroy();
    WritePacketArrivalTimesToFile(packetArrivalTimes, "packet_arrival_times.txt");

    return 0;

}