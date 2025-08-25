//run an example ./ns3 run "scratch/ai-ns3-mflow-DSCP-numb-ratio-queues.cc --nVoip=2 --nGaming=0 --nVideo=0 --nFtp=0" 
//./ns3 run "scratch/ai-ns3-mflow-DSCP-numb-ratio-queues-GNN.cc --nVoip=3 --nGaming=2 --nVideo=2 --nFtp=2 --queueDiscType="PfifoFast"" 
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-module.h"
#include <numeric>
#include <vector>
#include <iostream>
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
#include <map>
#include <cmath>
#include <ns3/ipv4-header.h>
#include <ns3/tcp-header.h>
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"



using namespace ns3;
struct PacketInfo {
    double time;             // seconds
    std::string trafficType; // VOIP, Video, FTP, Gaming
    uint32_t size;           // bytes
    uint32_t f; //number of flow (per traffic flow)
    std::string dscp_tag;

};

// Utility to normalize ratios
std::vector<double> NormalizeRatios(const std::vector<double> &ratios)
{
    double total = std::accumulate(ratios.begin(), ratios.end(), 0.0);
    std::vector<double> normalized(ratios.size());

    if (total == 0) {
        // If all ratios = 0, just distribute evenly
        for (size_t i = 0; i < ratios.size(); i++)
            normalized[i] = 1.0 / ratios.size();
    } else {
        for (size_t i = 0; i < ratios.size(); i++)
            normalized[i] = ratios[i] / total;
    }
    return normalized;
}
std::vector<PacketInfo> packetArrivalData;

void RxWithTrafficType(std::string ttype,
                       std::string dscp_tag,
                       uint32_t f,
                       Ptr<const Packet> pkt,
                       const Address &addr)
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
    info.f=f;
    info.dscp_tag=dscp_tag;
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

uint8_t DscpToTos(const std::string& dscp)
{
    if (dscp == "EF")   return 46 << 2;  // Expedited Forwarding
    if (dscp == "AF41") return 34 << 2;
    if (dscp == "AF42") return 36 << 2;
    if (dscp == "AF43") return 38 << 2;
    if (dscp == "AF31") return 26 << 2;
    if (dscp == "AF32") return 28 << 2;
    if (dscp == "AF33") return 30 << 2;
    if (dscp == "AF21") return 18 << 2;
    if (dscp == "AF22") return 20 << 2;
    if (dscp == "AF23") return 22 << 2;
    if (dscp == "AF11") return 10 << 2;
    if (dscp == "AF12") return 12 << 2;
    if (dscp == "AF13") return 14 << 2;
    if (dscp == "BE")   return 0;        // Best Effort (default)

    throw std::invalid_argument("Unknown DSCP string: " + dscp);
}

void WritePacketInfoToFile(const std::vector<PacketInfo>& data, const std::string& filename)
{
    std::ofstream outfile(filename);
    if (!outfile.is_open()) {
        std::cerr << "Error opening file " << filename << "\n";
        return;
    }

    outfile << "Time(s)\tTrafficType\tSize(Bytes)\tnumb_flow\tdscp_tag\n"; //\tSrcIP\tDstIP\tSrcPort\tDstPort
    for (const auto& pkt : data) {
        outfile << pkt.time << "\t" << pkt.trafficType << "\t" 
                << pkt.size << "\t" <<pkt.f <<"\t"<<pkt.dscp_tag<< "\n";

    }
}
void
TcPacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
    std::cout << "TcPacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
DevicePacketsInQueueTrace(uint32_t oldValue, uint32_t newValue)
{
    std::cout << "DevicePacketsInQueue " << oldValue << " to " << newValue << std::endl;
}

void
SojournTimeTrace(Time sojournTime)
{
    std::cout << "Sojourn time " << sojournTime.ToDouble(Time::MS) << "ms" << std::endl;
}
void WriteStatsFile(
    uint32_t nVoip, const uint8_t &tagVoip,
    uint32_t nFtp, const uint8_t &tagFtp,
    uint32_t nGaming, const uint8_t &tagGaming,
    uint32_t nVideo, const uint8_t &tagVideo
)
{
    std::ofstream configFile("stats.txt");
    if (!configFile.is_open()) {
        NS_ABORT_MSG("Cannot open stats.txt for writing");
    }

    configFile << "VOIP:" << nVoip << ",0x" 
               << std::hex << static_cast<int>(tagVoip) << "\n";
    configFile << "FTP:" << nFtp << ",0x" 
               << std::hex << static_cast<int>(tagFtp) << "\n";
    configFile << "GAMING:" << nGaming << ",0x" 
               << std::hex << static_cast<int>(tagGaming) << "\n";
    configFile << "VIDEO:" << nVideo << ",0x" 
               << std::hex << static_cast<int>(tagVideo) << "\n";

    configFile.close();
}

void SaveGraphFeatures(
    NodeContainer &nodes,
    NetDeviceContainer &devices,
    QueueDiscContainer &queueDiscs,
    Ptr<FlowMonitor> flowMonitor,
    FlowMonitorHelper &flowHelper)
{
    // --- NODES / QUEUE FEATURES ---
    std::ofstream nodeFile( "nodes.txt");
    nodeFile << "ID\tQueueDiscType\tQueueDiscSizePackets\n";

    for (uint32_t i = 0; i < nodes.GetN(); ++i) {
        std::string qType = "None";
        uint32_t qSize = 0;
    

        // If a queue exists for this node
        if (i < queueDiscs.GetN()) {
            Ptr<QueueDisc> q = queueDiscs.Get(i);
            qType = q->GetInstanceTypeId().GetName();
            qSize = q->GetMaxSize().GetValue();              // Max size in packets
           
        }

        nodeFile << i << "\t" << qType << "\t" << qSize << "\n";
    }
    nodeFile.close();

    // --- LINKS ---
    std::ofstream linkFile("links.txt");
    linkFile << "ID\tSrcNode\tDstNode\tDataRate(bps)\tDelay(s)\n";
    
    for (uint32_t i = 0; i < devices.GetN(); ++i) {
        Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(devices.Get(i));
        uint32_t srcNode = dev->GetNode()->GetId();

        Ptr<PointToPointChannel> channel = DynamicCast<PointToPointChannel>(dev->GetChannel());
        Ptr<NetDevice> otherDev = (channel->GetDevice(0) == dev) ? channel->GetDevice(1) : channel->GetDevice(0);
        uint32_t dstNode = otherDev->GetNode()->GetId();

        DataRateValue dr;
        dev->GetAttribute("DataRate", dr);

        TimeValue delay;
        channel->GetAttribute("Delay", delay);

        linkFile << i << "\t" << srcNode << "\t" << dstNode << "\t"
                << dr.Get().GetBitRate() << "\t"
                << delay.Get().GetSeconds() << "\n";
    }
    linkFile.close();


    // --- FLOWS / PATHS ---
    flowMonitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats();

    std::ofstream flowFile("flows.txt");
    flowFile << "FlowID\tSrcIP\tDstIP\tDuration(s)\tTxPackets\tRxPackets\tMeanDelay(s)\tMeanJitter(s)\tMeanThroughput(bps)\tlost_packets\n";

    for (auto const &flow : stats) {
        FlowId fid = flow.first;
        FlowMonitor::FlowStats s = flow.second;
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(fid);

        double duration = s.timeLastRxPacket.GetSeconds() - s.timeFirstTxPacket.GetSeconds();
        double meanDelay = (s.rxPackets > 0) ? s.delaySum.GetSeconds() / s.rxPackets : 0.0;
        double meanJitter = (s.rxPackets > 0) ? s.jitterSum.GetSeconds() / s.rxPackets : 0.0;
        double meanThroughput = (duration > 0) ? s.rxBytes * 8.0 / duration : 0.0;

        flowFile << fid << "\t" << t.sourceAddress << "\t" << t.destinationAddress << "\t"
                 << duration << "\t" << s.txPackets << "\t" << s.rxPackets << "\t"
                 << meanDelay << "\t" << meanJitter << "\t" << meanThroughput <<"\t"<<s.lostPackets<< "\n";
    }

    flowFile.close();
}

void queue_occupancy(uint32_t oldValue, uint32_t newValue)
{
    double t = ns3::Simulator::Now().GetSeconds();
    //queueFile << t << "\t" << newValue << "\n";
    std::cout << "Queue size changed: " << oldValue << " -> " << newValue << " at t=" << t << std::endl;
}

std::map<uint64_t, Time> enqueueTimes;

// Trace function for enqueue
//
/*
void EnqueueTrace(Ptr<const Packet> packet)//, std::ofstream* file)
{
    enqueueTimes[packet->GetUid()] = Simulator::Now();
    // Optionally log current queue occupancy
    *file << Simulator::Now().GetSeconds() << "\tEnqueue\t" << packet->GetUid() << std::endl;
}

// Trace function for dequeue
void DequeueTrace(Ptr<const Packet> packet)//, std::ofstream* file)
{
    Time enqueueTime = enqueueTimes[packet->GetUid()];
    Time dequeueTime = Simulator::Now();
    Time queueTime = dequeueTime - enqueueTime;

    // Write to file
    *file << Simulator::Now().GetSeconds() << "\tDequeue\t" 
          << packet->GetUid() << "\tQueueTime: " 
          << queueTime.GetSeconds() << "s" << std::endl;
}
*/

int main(int argc, char *argv[])
{    //initialization
    //link
    std::string LinkDataRate ="1Mbps";
    std::string LinkDelay = "0.5ms";
    //queues
    std::string queueDiscType = "PfifoFast";//prio
    uint32_t queueDiscSize= 10;
    uint32_t netdevicesQueueSize = 10;

    bool useUdp = false;
    uint32_t appStartMs = 500;//ms
    uint32_t appDurationMs = 10000;//ms (10s)


    // Number of flows for each traffic type
    uint32_t nVoip = 2;
    uint32_t nGaming = 2;
    uint32_t nVideo = 2;
    uint32_t nFtp = 2;

    // === Ratios for VoIP ===
    double ratioVoipEF=1, ratioVoipAF41=0., ratioVoipAF42=0, ratioVoipAF43=0;
    double ratioVoipAF31=1, ratioVoipAF32=0, ratioVoipAF33=0;
    double ratioVoipAF21=1, ratioVoipAF22=1, ratioVoipAF23=0;
    double ratioVoipAF11=0, ratioVoipAF12=0, ratioVoipAF13=0;
    double ratioVoipBE=1;

    // === Ratios for Gaming ===
    double ratioGamingEF=0.0, ratioGamingAF41=0.0, ratioGamingAF42=0.3, ratioGamingAF43=0;
    double ratioGamingAF31=0, ratioGamingAF32=0, ratioGamingAF33=0;
    double ratioGamingAF21=0, ratioGamingAF22=0, ratioGamingAF23=0.3;
    double ratioGamingAF11=0.3, ratioGamingAF12=0, ratioGamingAF13=0;
    double ratioGamingBE=0;

    // === Ratios for Video ===
    double ratioVideoEF=0, ratioVideoAF41=0, ratioVideoAF42=0, ratioVideoAF43=0.3;
    double ratioVideoAF31=0.3, ratioVideoAF32=0, ratioVideoAF33=0;
    double ratioVideoAF21=0, ratioVideoAF22=0, ratioVideoAF23=0;
    double ratioVideoAF11=0, ratioVideoAF12=0.3, ratioVideoAF13=0;
    double ratioVideoBE=0;

    // === Ratios for FTP ===
    double ratioFtpEF=0.3, ratioFtpAF41=0, ratioFtpAF42=0., ratioFtpAF43=0;
    double ratioFtpAF31=0, ratioFtpAF32=0.3, ratioFtpAF33=0;
    double ratioFtpAF21=0.3, ratioFtpAF22=0, ratioFtpAF23=0;
    double ratioFtpAF11=0., ratioFtpAF12=0, ratioFtpAF13=0;
    double ratioFtpBE=0;



    // Allow overriding from command line
    CommandLine cmd;
    //link characteristics

    cmd.AddValue("LinkDataRate", "the bandwidth of the link", LinkDataRate);
    
    cmd.AddValue("LinkDelay", "the propagation delay of the link", LinkDelay);


    cmd.AddValue("nVoip", "Number of VoIP flows", nVoip);
    cmd.AddValue("nGaming", "Number of Gaming flows", nGaming);
    cmd.AddValue("nVideo", "Number of Video flows", nVideo);
    cmd.AddValue("nFtp", "Number of FTP flows", nFtp);

   // Register all ratios for VoIP
   //work in terms of DSCP to make it intuitive to the person but then convert to tos to implement in ns3

    cmd.AddValue("ratioVoipEF", "EF ratio", ratioVoipEF);
    cmd.AddValue("ratioVoipAF41", "", ratioVoipAF41);
    cmd.AddValue("ratioVoipAF42", "", ratioVoipAF42);
    cmd.AddValue("ratioVoipAF43", "", ratioVoipAF43);
    cmd.AddValue("ratioVoipAF31", "", ratioVoipAF31);
    cmd.AddValue("ratioVoipAF32", "", ratioVoipAF32);
    cmd.AddValue("ratioVoipAF33", "", ratioVoipAF33);
    cmd.AddValue("ratioVoipAF21", "", ratioVoipAF21);
    cmd.AddValue("ratioVoipAF22", "", ratioVoipAF22);
    cmd.AddValue("ratioVoipAF23", "", ratioVoipAF23);
    cmd.AddValue("ratioVoipAF11", "", ratioVoipAF11);
    cmd.AddValue("ratioVoipAF12", "", ratioVoipAF12);
    cmd.AddValue("ratioVoipAF13", "", ratioVoipAF13);
    cmd.AddValue("ratioVoipBE",  "", ratioVoipBE);

    // Gaming ratios
    cmd.AddValue("ratioGamingEF", "", ratioGamingEF);
    cmd.AddValue("ratioGamingAF41", "", ratioGamingAF41);
    cmd.AddValue("ratioGamingAF42", "", ratioGamingAF42);
    cmd.AddValue("ratioGamingAF43", "", ratioGamingAF43);
    cmd.AddValue("ratioGamingAF31", "", ratioGamingAF31);
    cmd.AddValue("ratioGamingAF32", "", ratioGamingAF32);
    cmd.AddValue("ratioGamingAF33", "", ratioGamingAF33);
    cmd.AddValue("ratioGamingAF21", "", ratioGamingAF21);
    cmd.AddValue("ratioGamingAF22", "", ratioGamingAF22);
    cmd.AddValue("ratioGamingAF23", "", ratioGamingAF23);
    cmd.AddValue("ratioGamingAF11", "", ratioGamingAF11);
    cmd.AddValue("ratioGamingAF12", "", ratioGamingAF12);
    cmd.AddValue("ratioGamingAF13", "", ratioGamingAF13);
    cmd.AddValue("ratioGamingBE",  "", ratioGamingBE);

    // Video ratios
    cmd.AddValue("ratioVideoEF", "", ratioVideoEF);
    cmd.AddValue("ratioVideoAF41", "", ratioVideoAF41);
    cmd.AddValue("ratioVideoAF42", "", ratioVideoAF42);
    cmd.AddValue("ratioVideoAF43", "", ratioVideoAF43);
    cmd.AddValue("ratioVideoAF31", "", ratioVideoAF31);
    cmd.AddValue("ratioVideoAF32", "", ratioVideoAF32);
    cmd.AddValue("ratioVideoAF33", "", ratioVideoAF33);
    cmd.AddValue("ratioVideoAF21", "", ratioVideoAF21);
    cmd.AddValue("ratioVideoAF22", "", ratioVideoAF22);
    cmd.AddValue("ratioVideoAF23", "", ratioVideoAF23);
    cmd.AddValue("ratioVideoAF11", "", ratioVideoAF11);
    cmd.AddValue("ratioVideoAF12", "", ratioVideoAF12);
    cmd.AddValue("ratioVideoAF13", "", ratioVideoAF13);
    cmd.AddValue("ratioVideoBE",  "", ratioVideoBE);

    // FTP ratios
    cmd.AddValue("ratioFtpEF", "", ratioFtpEF);
    cmd.AddValue("ratioFtpAF41", "", ratioFtpAF41);
    cmd.AddValue("ratioFtpAF42", "", ratioFtpAF42);
    cmd.AddValue("ratioFtpAF43", "", ratioFtpAF43);
    cmd.AddValue("ratioFtpAF31", "", ratioFtpAF31);
    cmd.AddValue("ratioFtpAF32", "", ratioFtpAF32);
    cmd.AddValue("ratioFtpAF33", "", ratioFtpAF33);
    cmd.AddValue("ratioFtpAF21", "", ratioFtpAF21);
    cmd.AddValue("ratioFtpAF22", "", ratioFtpAF22);
    cmd.AddValue("ratioFtpAF23", "", ratioFtpAF23);
    cmd.AddValue("ratioFtpAF11", "", ratioFtpAF11);
    cmd.AddValue("ratioFtpAF12", "", ratioFtpAF12);
    cmd.AddValue("ratioFtpAF13", "", ratioFtpAF13);
    cmd.AddValue("ratioFtpBE",  "", ratioFtpBE);

    //queues
    cmd.AddValue("queueDiscType",
                 "Bottleneck queue disc type in {PfifoFast, ARED, CoDel, FqCoDel, PIE, prio}",
                 queueDiscType);
    cmd.AddValue("queueDiscSize", "Bottleneck queue disc size in packets", queueDiscSize);
    cmd.AddValue("netdevicesQueueSize",
                 "Bottleneck netdevices queue size in packets",
                 netdevicesQueueSize);

    cmd.Parse(argc, argv);

   

    // Normalize VoIP ratios
    std::vector<double> voipRatios = NormalizeRatios({ratioVoipEF, ratioVoipAF41, ratioVoipAF42, ratioVoipAF43,
        ratioVoipAF31, ratioVoipAF32, ratioVoipAF33,
        ratioVoipAF21, ratioVoipAF22, ratioVoipAF23,
        ratioVoipAF11, ratioVoipAF12, ratioVoipAF13,
        ratioVoipBE});
    // Normalize Gaming
    std::vector<double> gamingRatios = NormalizeRatios({ratioGamingEF, ratioGamingAF41, ratioGamingAF42, ratioGamingAF43,
        ratioGamingAF31, ratioGamingAF32, ratioGamingAF33,
        ratioGamingAF21, ratioGamingAF22, ratioGamingAF23,
        ratioGamingAF11, ratioGamingAF12, ratioGamingAF13,
        ratioGamingBE});
    // Normalize Video
    std::vector<double> videoRatios = NormalizeRatios({ratioVideoEF, ratioVideoAF41, ratioVideoAF42, ratioVideoAF43,
        ratioVideoAF31, ratioVideoAF32, ratioVideoAF33,
        ratioVideoAF21, ratioVideoAF22, ratioVideoAF23,
        ratioVideoAF11, ratioVideoAF12, ratioVideoAF13,
        ratioVideoBE});
    // Normalize FTP
    std::vector<double> ftpRatios = NormalizeRatios({ratioFtpEF, ratioFtpAF41, ratioFtpAF42, ratioFtpAF43,
        ratioFtpAF31, ratioFtpAF32, ratioFtpAF33,
        ratioFtpAF21, ratioFtpAF22, ratioFtpAF23,
        ratioFtpAF11, ratioFtpAF12, ratioFtpAF13,
        ratioFtpBE});
    // Compute number of flows per DSCP in a list of ratios per traffic class
    std::vector<std::string> Dscps = {"EF", "AF41", "AF42", "AF43", "AF31", "AF32", "AF33", "AF21", "AF22", "AF23", "AF11", "AF12", "AF13","BE"};//14 DSCP
    std::cout<<"********************************Real number of flows used:*************************************"<<std::endl;
    std::map<std::string,uint32_t> voipFlowCount;
    std::cout<<"----- # of flows for voip:"<<std::endl; //debug
    uint32_t real_numb_voip=0;
    uint32_t number_flows=0;
    std::string dscp_head;
    if (nVoip>0){
        for (size_t i = 0; i < Dscps.size(); i++) {
            dscp_head=Dscps[i];
            number_flows=std::round(nVoip * voipRatios[i]);
            voipFlowCount[dscp_head] =number_flows;
            real_numb_voip+=number_flows;
            if (number_flows>0){
                std::cout<<"dscp: "<<dscp_head<<" & tos:" <<static_cast<int>(DscpToTos(dscp_head))<<" # flow: "<<number_flows<<std::endl;//debug
            }
            
        }
        std::cout<<"real number of flows used for voip is: "<<real_numb_voip<<std::endl;
        }
    else{
        std::cout<<"real number of flows used for voip is 0 "<<std::endl;

    }
    
    std::cout<<"-----------------------------"<<std::endl;


    std::cout<<"----- # of flows for gaming:"<<std::endl; //debug
    std::map<std::string,uint32_t> gamingFlowCount;
    uint32_t real_numb_gaming=0;
    number_flows=0;
    if (nGaming>0){
        for (size_t i = 0; i < Dscps.size(); i++) {
            dscp_head=Dscps[i];
            number_flows=std::round(nGaming * gamingRatios[i]);
            gamingFlowCount[dscp_head] =number_flows;
            real_numb_gaming+=number_flows;
            if (number_flows>0){
                std::cout<<"dscp: "<<dscp_head<<" & tos:" <<static_cast<int>(DscpToTos(dscp_head))<<" # flow: "<<number_flows<<std::endl;//debug
            }
            
        }
        std::cout<<"real number of flows used for gaming is: "<<real_numb_gaming<<std::endl;
        }
    else{
        std::cout<<"real number of flows used for gaming is 0 "<<std::endl;

    }

    std::cout<<"-----------------------------"<<std::endl;


    std::cout<<"----- # of flows for video:"<<std::endl; //debug
    std::map<std::string,uint32_t> videoFlowCount;
    uint32_t real_numb_video=0;
    number_flows=0;
    if (nVideo>0){
        for (size_t i = 0; i < Dscps.size(); i++) {
            dscp_head=Dscps[i];
            number_flows=std::round(nVideo * videoRatios[i]);
            videoFlowCount[dscp_head] =number_flows;
            real_numb_video+=number_flows;
            if (number_flows>0){
                std::cout<<"dscp: "<<dscp_head<<" & tos:" <<static_cast<int>(DscpToTos(dscp_head))<<" # flow: "<<number_flows<<std::endl;//debug
            }
            
        }
        std::cout<<"real number of flows used for video is: "<<real_numb_video<<std::endl;
        }
    else{
        std::cout<<"real number of flows used for video is 0 "<<std::endl;

    }
    
    std::cout<<"-----------------------------"<<std::endl;

    std::cout<<"----- # of flows for ftp:"<<std::endl; //debug
    std::map<std::string,uint32_t> ftpFlowCount;
    uint32_t real_numb_ftp=0;
    number_flows=0;
    if (nFtp>0){
        for (size_t i = 0; i < Dscps.size(); i++) {
            dscp_head=Dscps[i];
            number_flows=std::round(nFtp * ftpRatios[i]);
            ftpFlowCount[dscp_head] =number_flows;
            real_numb_ftp+=number_flows;
            if (number_flows>0){
                std::cout<<"dscp: "<<dscp_head<<" & tos:" <<static_cast<int>(DscpToTos(dscp_head))<<" # flow: "<<number_flows<<std::endl;//debug
            }
            
        }
        std::cout<<"real number of flows used for ftp is: "<<real_numb_ftp<<std::endl;
        }
    else{
        std::cout<<"real number of flows used for ftp is 0 "<<std::endl;

    }    
    std::cout<<"-----------------------------"<<std::endl;
    
    // Build nodes and link
    NodeContainer nodes;
    nodes.Create(2);
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(LinkDataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(LinkDelay));
    //pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(std::to_string (netdevicesQueueSize) + "p"));
    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper stack;
    stack.Install(nodes);

    //installing queues
    TrafficControlHelper tchBottleneck;
   

    if (queueDiscType == "PfifoFast")
    {
        tchBottleneck.SetRootQueueDisc(
            "ns3::PfifoFastQueueDisc",
            "MaxSize",
            QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    else if (queueDiscType == "ARED")
    {
        tchBottleneck.SetRootQueueDisc("ns3::RedQueueDisc");
        Config::SetDefault("ns3::RedQueueDisc::ARED", BooleanValue(true));
        Config::SetDefault("ns3::RedQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    else if (queueDiscType == "CoDel")
    {
        tchBottleneck.SetRootQueueDisc("ns3::CoDelQueueDisc");
        Config::SetDefault("ns3::CoDelQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    else if (queueDiscType == "FqCoDel")
    {
        tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
        Config::SetDefault("ns3::FqCoDelQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    else if (queueDiscType == "PIE")
    {
        tchBottleneck.SetRootQueueDisc("ns3::PieQueueDisc");
        Config::SetDefault("ns3::PieQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    }
    /*
    else if (queueDiscType == "prio")
    {   //think of automatizing this 
        
       uint16_t handle = tchBottleneck.SetRootQueueDisc ("ns3::PrioQueueDisc", "Priomap",
                                                         StringValue ("0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1"));
       TrafficControlHelper::ClassIdList cid = tchBottleneck.AddQueueDiscClasses (handle, 2, "ns3::QueueDiscClass");
       tchBottleneck.AddChildQueueDisc (handle, cid[0], "ns3::FifoQueueDisc");
       tchBottleneck.AddChildQueueDisc (handle, cid[1], "ns3::RedQueueDisc");
     
        

    }
    */
    else
    {
        NS_ABORT_MSG("--queueDiscType not valid");
    }

    QueueDiscContainer qdiscs;
    qdiscs = tchBottleneck.Install(devices);

    //internal queue in node 0
    
    Ptr<QueueDisc> q = qdiscs.Get(0);
    /*
    q->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&TcPacketsInQueueTrace));
    Config::ConnectWithoutContext(
        "/NodeList/1/$ns3::TrafficControlLayer/RootQueueDiscList/0/SojournTime",
        MakeCallback(&SojournTimeTrace));
   
    //NIC queue
    
    Ptr<NetDevice> nd = devices.Get(0);
    Ptr<PointToPointNetDevice> ptpnd = DynamicCast<PointToPointNetDevice>(nd);
    Ptr<Queue<Packet>> queue = ptpnd->GetQueue();
    queue->TraceConnectWithoutContext("PacketsInQueue", MakeCallback(&DevicePacketsInQueueTrace));
     */

   


    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = address.Assign(devices);
    //Config::SetDefault ("ns3::QueueBase::MaxSize", StringValue (std::to_string (netdevicesQueueSize) + "p"));

    std::string transportProtocol;
    if (useUdp)
    {
        transportProtocol = "ns3::UdpSocketFactory";
    }
    else
    {
        transportProtocol = "ns3::TcpSocketFactory";
    }
    
    ApplicationContainer sinkApps, generatorApps;
    std::vector<uint32_t> nFlows = {real_numb_voip,real_numb_gaming,real_numb_video,real_numb_ftp};//work with the real numbers after compiuting the rounding
    std::vector<TrafficType> trafficTypes = {NGMN_VOIP,NGMN_GAMING, NGMN_VIDEO, NGMN_FTP };
    uint16_t basePorts[] = {5060, 5000,16384,21};
    //std::map<std::string,uint32_t> count voip/gaming/viedo/ftpFlowCount
    
    std::map<std::string,uint32_t> dscp_count_per_traffic_class;
    std::cout<<"********************************Creating apps:*************************************"<<std::endl;
    //for each traffic type create flows
    for (size_t i = 0; i < trafficTypes.size(); ++i) {
        if (trafficTypes[i]==NGMN_VOIP){
            dscp_count_per_traffic_class=voipFlowCount;

        }
        else if (trafficTypes[i]==NGMN_GAMING){
            dscp_count_per_traffic_class=gamingFlowCount;
        }
        else if (trafficTypes[i]==NGMN_VIDEO){
            dscp_count_per_traffic_class=videoFlowCount;
        }
        else if (trafficTypes[i]==NGMN_FTP){
            dscp_count_per_traffic_class=ftpFlowCount;
        }


    
        uint32_t cumulative_ports=0;
        //for each dscp create the corresponding number of flows
        
        if (nFlows[i]>0){//create sockets only if #flows are not 0
            std::cout<<"***for the "<<GetName(trafficTypes[i])<<": "<<std::endl;
            for (auto [dscp_key, numbFlow_value] : dscp_count_per_traffic_class) {
                
                
                if (numbFlow_value>0){
                    std::cout <<"tos " <<static_cast<int>(DscpToTos(dscp_key)) << " /DSCP "<<dscp_key<<"  ->  " << numbFlow_value << std::endl; //debug: if dscp_key==tos_key
                    for (uint32_t ff = 0; ff < numbFlow_value; ++ff) { //f counter is used to keep track of the number (ID) of the flow per traffic class to distinguish between them
                       
                        uint16_t port = basePorts[i] + cumulative_ports;
                        
                        std::cout<<"the port is: "<<port<<std::endl; //debug to verify that the ports are incrementing in the correct order starting from the base port of the traffic class and moving onward

                        // Install sink
                        InetSocketAddress rxAddress(Ipv4Address::GetAny(), port);
                        rxAddress.SetTos(DscpToTos(dscp_key));
                        PacketSinkHelper packetSinkHelper(transportProtocol, rxAddress);
                        ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
                        sinkApp.Start(MilliSeconds(appStartMs));
                        sinkApp.Stop(MilliSeconds(appStartMs + appDurationMs));
                        sinkApps.Add(sinkApp);

                        // Install generator
                        InetSocketAddress txAddress(interfaces.GetAddress(1, 0), port);
                        txAddress.SetTos(DscpToTos(dscp_key));
                        // Assume TrafficGeneratorHelper exists and accepts TrafficType
                        TrafficGeneratorHelper trafficGeneratorHelper(transportProtocol, txAddress, GetTypeId(trafficTypes[i]));
                        ApplicationContainer genApp = trafficGeneratorHelper.Install(nodes.Get(0));
                        genApp.Start(MilliSeconds(appStartMs));
                        genApp.Stop(MilliSeconds(appStartMs + appDurationMs));
                        generatorApps.Add(genApp);

                        Ptr<Application> app = sinkApp.Get(0);
                        Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
                        uint32_t flow_id = ff + 1; 
                        sink->TraceConnectWithoutContext(
                            "Rx",
                            MakeBoundCallback(&RxWithTrafficType, GetName(trafficTypes[i]),dscp_key,cumulative_ports+1)//+1 to start from 1 and not 0 to be coherent with flowmonitor
                    );
                    std::cout<<"number of flow: "<<flow_id<<std::endl;
                    cumulative_ports+=1;
                    }
                    

                }
                
               

               
            }
        
        
        
        }
        
        std::cout<<"   # of all the flows per traffic class for "<<GetName(trafficTypes[i])<<"     is: "<<cumulative_ports<<std::endl;//debug 
      
    }
        
    
    // Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();

        // Open output file
    std::ofstream queueFile("queue_occupancy.txt");
    queueFile << "Time(s)\tQueueSize\n";

    //queue size change tracer
    //Ptr<QueueDisc> q = qdiscs.Get(0);
    //works
    //q->TraceConnectWithoutContext("PacketsInQueue", ns3::MakeCallback(&queue_occupancy));

    // Assuming you have a QueueDisc container qdiscs (does not work)
    /*
        for (uint32_t i = 0; i < qdiscs.GetN(); ++i)
        {
            Ptr<QueueDisc> q_extract = qdiscs.Get(i);
            q_extract->TraceConnectWithoutContext("Enqueue", MakeBoundCallback(&EnqueueTrace));//, &queueFile));
            q_extract->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(&DequeueTrace));//, &queueFile));
        }
    */


    Simulator::Stop(MilliSeconds(appDurationMs+1000));
    Simulator::Run();
    //Note: better to parse this in python not here
    /*
     std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (auto const &flow : stats)
    {
        FlowId flowId = flow.first;
        FlowMonitor::FlowStats st = flow.second;

        uint32_t packetsDroppedByQueueDisc = 0;
        uint64_t bytesDroppedByQueueDisc = 0;
        if (st.packetsDropped.size() > Ipv4FlowProbe::DROP_QUEUE_DISC)
        {
            packetsDroppedByQueueDisc = st.packetsDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
            bytesDroppedByQueueDisc = st.bytesDropped[Ipv4FlowProbe::DROP_QUEUE_DISC];
        }

        uint32_t packetsDroppedByNetDevice = 0;
        uint64_t bytesDroppedByNetDevice = 0;
        if (st.packetsDropped.size() > Ipv4FlowProbe::DROP_QUEUE)
        {
            packetsDroppedByNetDevice = st.packetsDropped[Ipv4FlowProbe::DROP_QUEUE];
            bytesDroppedByNetDevice = st.bytesDropped[Ipv4FlowProbe::DROP_QUEUE];
        }

        std::cout << "Flow " << flowId << ":\n";
        std::cout << "  Packets/Bytes Dropped by Queue Disc:   "
                << packetsDroppedByQueueDisc << " / " << bytesDroppedByQueueDisc << std::endl;
        std::cout << "  Packets/Bytes Dropped by NetDevice:   "
                << packetsDroppedByNetDevice << " / " << bytesDroppedByNetDevice << std::endl;
    }
    */


    monitor->SerializeToXmlFile("multi-links-flow.xml", true, true);
    
    // Save features including dynamic queue info
    SaveGraphFeatures(nodes, devices, qdiscs, monitor, flowmonHelper);


    Simulator::Destroy();
    WritePacketInfoToFile(packetArrivalData, "packet_arrival_times.txt");
    std::cout << std::endl << "*** TC Layer statistics ***" << std::endl;
    std::cout << q->GetStats() << std::endl;
    queueFile.close();

    return 0;
}
