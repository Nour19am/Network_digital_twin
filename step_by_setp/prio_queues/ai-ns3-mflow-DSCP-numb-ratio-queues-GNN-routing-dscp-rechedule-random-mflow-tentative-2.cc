//run an example ./ns3 run "scratch/ai-ns3-mflow-DSCP-numb-ratio-queues.cc --nVoip=2 --nGaming=0 --nVideo=0 --nFtp=0" 
//./ns3 run "scratch/ai-ns3-mflow-DSCP-numb-ratio-queues-GNN.cc --nVoip=3 --nGaming=2 --nVideo=2 --nFtp=2 --queueDiscType="PfifoFast""
//./ns3 run "scratch/ai-ns3-mflow-DSCP-numb-ratio-queues-GNN-routing-dscp-rechedule-random-mflow-tentative-2.cc --queueDiscType="RedQueueDisc""


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
#include <typeinfo>
#include <unordered_map>
#include "ns3/ipv4-static-routing-helper.h"
#include <cstdlib>   // for rand() and srand()
#include <ctime>     // for time()


using namespace ns3;


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
    if (dscp == "BF")   return 0;        // Best Effort (default)

    throw std::invalid_argument("Unknown DSCP string: " + dscp);
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
    std::vector<NetDeviceContainer*> device_containers,
    std::vector<QueueDiscContainer> allQdiscs
)
{
// --- NODES / QUEUE FEATURES ---
std::ofstream nodeFile("nodes.txt");
nodeFile << "ID\tIP_Addresses\n";

for (uint32_t i = 0; i < nodes.GetN(); ++i) {
    
    // --- Collect all IPv4 addresses of the node ---
    std::ostringstream ipList;
    Ptr<Ipv4> ipv4 = nodes.Get(i)->GetObject<Ipv4>();
    if (ipv4) {
        int32_t nInterfaces = ipv4->GetNInterfaces();
        for (int32_t iface = 0; iface < nInterfaces; ++iface) {
            for (uint32_t addrIndex = 0; addrIndex < ipv4->GetNAddresses(iface); ++addrIndex) {
                Ipv4Address addr = ipv4->GetAddress(iface, addrIndex).GetLocal();
                if (addr != Ipv4Address("0.0.0.0") and addr != Ipv4Address("127.0.0.1") ) { // skip unspecified or localhost
                    ipList << addr << " ";
                }
            }
        }
    }

    nodeFile << i << "\t" << ipList.str() << "\n";
}

nodeFile.close();

// --- LINKS WITH QUEUES ---
std::ofstream linkFile("links.txt");
linkFile << "ID\tSrcNode\tDstNode\tSrcIP\tDstIP\tDataRate(bps)\tDelay(s)\tQueueDiscType\tQueueDiscSizePackets\tChild_name\tChild_size\n";

uint32_t linkId = 0;
uint32_t links_number = 1; // identifies which physical link

// Loop over all NetDeviceContainers (parallel links)
for (auto devices : device_containers) {
    if (!devices) continue;

    for (uint32_t i = 0; i < devices->GetN(); ++i) {
        Ptr<PointToPointNetDevice> dev = DynamicCast<PointToPointNetDevice>(devices->Get(i));
        if (!dev) continue;

        uint32_t srcNode = dev->GetNode()->GetId();
        Ptr<PointToPointChannel> channel = DynamicCast<PointToPointChannel>(dev->GetChannel());
        if (!channel) continue;

        // Identify the other device on the channel
        Ptr<NetDevice> otherDev = (channel->GetDevice(0) == dev) ? channel->GetDevice(1) : channel->GetDevice(0);
        uint32_t dstNode = otherDev->GetNode()->GetId();

        // Retrieve IP addresses for the first P2P interface
        Ptr<Ipv4> ipv4Src = dev->GetNode()->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4Dst = otherDev->GetNode()->GetObject<Ipv4>();

        Ipv4Address srcIP = ipv4Src->GetAddress(links_number, 0).GetLocal();
        Ipv4Address dstIP = ipv4Dst->GetAddress(links_number, 0).GetLocal();

        // Retrieve DataRate and Delay
        DataRateValue dr;
        dev->GetAttribute("DataRate", dr); 
        TimeValue delay;
        channel->GetAttribute("Delay", delay);

        // --- Retrieve the queue disc for this NetDevice ---
        std::string qType = "None";
        uint32_t qSize = 0;
        const QueueDiscContainer& qdc = allQdiscs[i];
        
        //works well for 1 queue
       
        ns3::Ptr<ns3::QueueDisc> qdisc = qdc.Get(0);
        qType=qdisc->GetInstanceTypeId().GetName();
        //added
        
        Ptr<QueueDiscClass> qClass = qdisc->GetQueueDiscClass(0);
        Ptr<QueueDisc> child = qClass->GetQueueDisc();
        std::string child_name=child->GetInstanceTypeId().GetName();
        ns3::QueueSize child_size=child->GetMaxSize();

        

   

        // Write link info
        /*
        linkFile << linkId++ << "\t"
                 << srcNode << "\t" << dstNode << "\t"
                 << srcIP << "\t" << dstIP << "\t"
                 << dr.Get().GetBitRate() << "\t"
                 << delay.Get().GetSeconds() << "\t"
                 << qType << "\t" << qSize << "\n";
                 */

        linkFile << linkId++ << "\t"
                 << srcNode << "\t" << dstNode << "\t"
                 << srcIP << "\t" << dstIP << "\t"
                 << dr.Get().GetBitRate() << "\t"
                 << delay.Get().GetSeconds() << "\t"
                 << qType << "\t" << qSize << "\t"
                 << child_name << "\t" << child_size <<"\n";
                 ;
    }
    links_number += 1;
}

linkFile.close();
}


std::map<uint64_t, Time> enqueueTimes;


void generateConfigFile(const std::string& filename,
                        int real_numb_voip,
                        int real_numb_ftp,
                        int real_numb_gaming,
                        int real_numb_video) {
    std::ofstream configFile(filename);

    if (!configFile.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return;
    }

    // Write each line (no DSCP tag)
    configFile << "VOIP:" << real_numb_voip << "\n";
    configFile << "FTP:" << real_numb_ftp << "\n";
    configFile << "GAMING:" << real_numb_gaming << "\n";
    configFile << "VIDEO:" << real_numb_video << "\n";

    configFile.close();
    std::cout << "Configuration file generated: " << filename << std::endl;
}



void CreateApps(
     std::vector<TrafficType>& trafficTypes,
     std::vector<uint32_t>& nFlows,
     std::map<std::string, uint32_t>& voipFlowCount,
     std::map<std::string, uint32_t>& gamingFlowCount,
     std::map<std::string, uint32_t>& videoFlowCount,
     std::map<std::string, uint32_t>& ftpFlowCount,
    std::map<std::string, int>& dscpToLink,
     std::vector<Ipv4InterfaceContainer>& interfaces,
     NodeContainer& nodes,
     std::string& transportProtocol,
     std::array<uint16_t, 4>& basePorts,
    uint32_t appStartMs,
     uint32_t duration)
     
{   
    ApplicationContainer sinkApps, generatorApps;
    std::map<std::string,uint32_t> dscp_count_per_traffic_class;
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
        
        if (nFlows[i]>0){ //create sockets only if #flows are not 0
            std::cout<<"***for the "<<GetName(trafficTypes[i])<<": "<<std::endl;
            for (auto [dscp_key, numbFlow_value] : dscp_count_per_traffic_class) {
                
                
                if (numbFlow_value>0){
                    std::cout <<"tos " <<static_cast<int>(DscpToTos(dscp_key)) << " /DSCP "<<dscp_key<<"  ->  " << numbFlow_value << std::endl; //debug: if dscp_key==tos_key
                    for (uint32_t ff = 0; ff < numbFlow_value; ++ff) { //f counter is used to keep track of the number (ID) of the flow per traffic class to distinguish between them
                        //int linkIndex = ff % 3; // Round-robin over 3 links
                        uint16_t port = basePorts[i] + cumulative_ports;
                        
                        std::cout<<"the port is: "<<port<<std::endl; //debug to verify that the ports are incrementing in the correct order starting from the base port of the traffic class and moving onward

                        // Install sink
                        InetSocketAddress rxAddress(Ipv4Address::GetAny(), port);
                        rxAddress.SetTos(DscpToTos(dscp_key));
                        PacketSinkHelper packetSinkHelper(transportProtocol, rxAddress);
                        ApplicationContainer sinkApp = packetSinkHelper.Install(nodes.Get(1));
                        sinkApp.Start(MilliSeconds(appStartMs));
                        
                        sinkApp.Stop(MilliSeconds(appStartMs +duration));
                        sinkApps.Add(sinkApp);
                        std::cout<<"app starting  time: "<<appStartMs           <<std::endl;
                        std::cout<<"app finishing time: "<<appStartMs +duration<<std::endl;

                        // Install generator
                        //InetSocketAddress txAddress(interfaces.GetAddress(1, 0), port);
                        int linkIndex = dscpToLink[dscp_key]; // Use your DSCP key

                        InetSocketAddress txAddress(interfaces[linkIndex].GetAddress(1), port);
                        
                        txAddress.SetTos(DscpToTos(dscp_key));
                        // Assume TrafficGeneratorHelper exists and accepts TrafficType
                        TrafficGeneratorHelper trafficGeneratorHelper(transportProtocol, txAddress, GetTypeId(trafficTypes[i]));
                        ApplicationContainer genApp = trafficGeneratorHelper.Install(nodes.Get(0));
                        genApp.Start(MilliSeconds(appStartMs));
                        genApp.Stop(MilliSeconds(appStartMs + duration));
                        generatorApps.Add(genApp);

                        //Ptr<Application> app = sinkApp.Get(0);
                        //Ptr<PacketSink> sink = DynamicCast<PacketSink>(app);
                        uint32_t flow_id = ff + 1; 
                   
                    std::cout<<"number of flow: "<<flow_id<<std::endl;
                    cumulative_ports+=1;
                    }
                    

                }
                
            }
          
        }
        
        std::cout<<"   # of all the flows per traffic class for "<<GetName(trafficTypes[i])<<"     is: "<<cumulative_ports<<std::endl;//debug 
      
    }
// Cleanup after they stop (drop refs so memory is freed)
    Simulator::Schedule(MilliSeconds(appStartMs + duration + 1), [sinkApps, generatorApps]() mutable {
        sinkApps = ApplicationContainer();
        generatorApps = ApplicationContainer();
    });


    
}
// class packetfilter
#include "ns3/packet-filter.h"
#include "ns3/ipv4-header.h"
#include "ns3/log.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("MyDscpPacketFilter");

class MyDscpPacketFilter : public PacketFilter
{
public:
  static TypeId GetTypeId (void)
  {
    static TypeId tid = TypeId ("ns3::MyDscpPacketFilter")
      .SetParent<PacketFilter> ()
      .SetGroupName("TrafficControl");
    return tid;
  }

  MyDscpPacketFilter () {}

  void SetDscpMapping (const std::map<uint8_t, uint32_t>& mapping)
  {
    m_dscpToBand = mapping;
  }

protected:
  virtual int32_t DoClassify (Ptr<QueueDiscItem> item) const
  {
    Ipv4Header ipHeader;
    Ptr<Packet> pkt = item->GetPacket ()->Copy ();

    if (pkt->PeekHeader (ipHeader))
    {
      uint8_t dscp = ipHeader.GetDscp ();
      auto it = m_dscpToBand.find (dscp);
      if (it != m_dscpToBand.end ())
      {
        NS_LOG_DEBUG("DSCP " << unsigned(dscp) << " mapped to band " << it->second);
        return it->second; // Band index
      }
      return 0; // Default band
    }
    return -1; // Not classified
  }

  virtual bool CheckProtocol (Ptr<QueueDiscItem> item) const
  {
    // Only classify IPv4 packets
    return (item->GetProtocol () == 0x0800);
  }

private:
  std::map<uint8_t, uint32_t> m_dscpToBand;
};

int main(int argc, char *argv[])
{    //initialization
    //link
    std::string LinkDataRate ="0.5Mbps";
    std::string LinkDelay = "5ms";
    //queues
    std::string queueDiscType = "PfifoFast";//prio
    uint32_t queueDiscSize= 10;
    uint32_t netdevicesQueueSize = 10;
    std::string prio_type_band="ns3::PieQueueDisc" ; //"ns3::FifoQueueDisc","ns3::RedQueueDisc",
    // "ns3::PfifoFastQueueDisc","ns3::CoDelQueueDisc","ns3::TbfQueueDisc","ns3::PieQueueDisc"

    bool useUdp = false;
    uint32_t appStartMs = 500;//ms
    uint32_t appDurationMs = 20000;//ms (10s)
    uint32_t nBands = 4; // default






    // Number of flows for each traffic type
    uint32_t nVoip = 1;
    uint32_t nGaming = 1;
    uint32_t nVideo = 1;
    uint32_t nFtp = 1;

    // === Ratios for VoIP ===
    double ratioVoipEF=1, ratioVoipAF41=0., ratioVoipAF42=0, ratioVoipAF43=0;
    double ratioVoipAF31=0, ratioVoipAF32=0, ratioVoipAF33=0;
    double ratioVoipAF21=0, ratioVoipAF22=0, ratioVoipAF23=0;
    double ratioVoipAF11=0, ratioVoipAF12=0, ratioVoipAF13=0;
    double ratioVoipBE=0;

    // === Ratios for Gaming ===
    double ratioGamingEF=0.0, ratioGamingAF41=1.0, ratioGamingAF42=0.0, ratioGamingAF43=0;
    double ratioGamingAF31=0, ratioGamingAF32=0, ratioGamingAF33=0;
    double ratioGamingAF21=0, ratioGamingAF22=0, ratioGamingAF23=0.0;
    double ratioGamingAF11=0.0, ratioGamingAF12=0, ratioGamingAF13=0;
    double ratioGamingBE=0;

    // === Ratios for Video ===
    double ratioVideoEF=0, ratioVideoAF41=0, ratioVideoAF42=0, ratioVideoAF43=0.0;
    double ratioVideoAF31=1.0, ratioVideoAF32=0, ratioVideoAF33=0;
    double ratioVideoAF21=0, ratioVideoAF22=0, ratioVideoAF23=0;
    double ratioVideoAF11=0, ratioVideoAF12=0.0, ratioVideoAF13=0;
    double ratioVideoBE=0;

    // === Ratios for FTP ===
    double ratioFtpEF=0.0, ratioFtpAF41=0, ratioFtpAF42=0., ratioFtpAF43=0;
    double ratioFtpAF31=0, ratioFtpAF32=0.0, ratioFtpAF33=0;
    double ratioFtpAF21=0, ratioFtpAF22=0, ratioFtpAF23=0;
    double ratioFtpAF11=0, ratioFtpAF12=0, ratioFtpAF13=0;
    double ratioFtpBE=1;



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
    cmd.AddValue("nBands", "Number of bands in PrioQueueDisc", nBands);
    cmd.AddValue("prio_type_band", "Number of bands in PrioQueueDisc", prio_type_band);

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
    std::vector<std::string> Dscps = {"EF", "AF41", "AF42", "AF43", "AF31", "AF32", "AF33", "AF21", "AF22", "AF23", "AF11", "AF12", "AF13","BF"};//14 DSCP
    std::cout<<"********************************Real number of flows used:*************************************"<<std::endl;
    std::vector<std::string> used_dscps={};
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
                // Check if newItem is already in myList
                if (std::find(used_dscps.begin(), used_dscps.end(), dscp_head) == used_dscps.end()) {
                    used_dscps.push_back(dscp_head); // only add if not found
                }
                
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
                if (std::find(used_dscps.begin(), used_dscps.end(), dscp_head) == used_dscps.end()) {
                        used_dscps.push_back(dscp_head); // only add if not found
                    }
                    
            
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
                if (std::find(used_dscps.begin(), used_dscps.end(), dscp_head) == used_dscps.end()) {
                            used_dscps.push_back(dscp_head); // only add if not found
                        }
            
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
                if (std::find(used_dscps.begin(), used_dscps.end(), dscp_head) == used_dscps.end()) {
                        used_dscps.push_back(dscp_head); // only add if not found
                    }
            
            }
            
        }
        std::cout<<"real number of flows used for ftp is: "<<real_numb_ftp<<std::endl;
        }
    else{
        std::cout<<"real number of flows used for ftp is 0 "<<std::endl;

    }   
    
     std::cout<<"-----------------------------"<<std::endl;
    std::cout<<"*******************************Used DSCPS***************************"<<std::endl;
    for (const auto& item : used_dscps) {
                    std::cout << item << " ";
                }
    
     std::cout<<"-----------------------------"<<std::endl;            
    generateConfigFile("config.txt", real_numb_voip, real_numb_ftp, real_numb_gaming, real_numb_video);
    
    // Build nodes and link
    NodeContainer nodes;
    nodes.Create(2);
    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue(LinkDataRate));
    pointToPoint.SetChannelAttribute("Delay", StringValue(LinkDelay));
    //pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("100p"));
    pointToPoint.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(std::to_string (netdevicesQueueSize) + "p"));
    
    InternetStackHelper stack;
    stack.Install(nodes);
    
    //QueueDiscContainer qdiscs;
    TrafficControlHelper tchBottleneck;
    /*
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
        //Config::SetDefault("ns3::RedQueueDisc::ARED", BooleanValue(true),"UseEcn", BooleanValue(true));
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
 
    else
    {
        NS_ABORT_MSG("--queueDiscType not valid");
    }
    
    tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    Config::SetDefault("ns3::FqCoDelQueueDisc::MaxSize",
                           QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueDiscSize)));
    */
    std::vector<NetDeviceContainer> links;
    std::vector<QueueDiscContainer> allQdiscs;

    //-------------------start of create prio queue disc on each device
   
   
    /*
    std::ostringstream priomap;
    for (uint32_t i = 0; i < 16; i++) {
        priomap << (i % nBands);
        if (i != 15) priomap << " ";  // add space except after last element
    }

    uint16_t handle =
        tchBottleneck.SetRootQueueDisc("ns3::PrioQueueDisc",
                                    "Priomap",
                                    StringValue(priomap.str()));

    

    // Add child queues (mixing RED and FIFO for demo)
    
        */
     std::ostringstream priomap;
    for (uint32_t i = 0; i < 16; i++) {
        priomap << (i % nBands);
        if (i != 15) priomap << " ";  // add space except after last element
    }
   
    
                                             
    uint16_t handle =
            tchBottleneck.SetRootQueueDisc("ns3::PrioQueueDisc",
                                           "Priomap",StringValue(priomap.str()));
                                           //StringValue("0 1 0 1 0 1 0 1 0 1 0 1 0 1 0 1"));//StringValue(priomap.str())
    // Add queue disc classes = "bands"
    TrafficControlHelper::ClassIdList cid =
        tchBottleneck.AddQueueDiscClasses(handle, nBands, "ns3::QueueDiscClass");
    for (uint32_t i = 0; i < nBands; i++) {
        
        tchBottleneck.AddChildQueueDisc(handle, cid[i], prio_type_band, "MaxSize", StringValue("10p"));
        
        /*
         } else {
            tchBottleneck.AddChildQueueDisc(handle, cid[i], "ns3::RedQueueDisc", "MaxSize", StringValue("10p"));
        }
        */
    }
    
    
    Ptr<MyDscpPacketFilter> filter = CreateObject<MyDscpPacketFilter>();

    // Example: EF → band 0, AF41 → band 1, Best Effort → band 3
    std::map<uint8_t, uint32_t> mapping;
    mapping[Ipv4Header::DSCP_EF] = 0;
    mapping[Ipv4Header::DSCP_AF41] = 1;
    mapping[Ipv4Header::DSCP_AF31] = 2;
    mapping[0] = 3;  // Best Effort (DSCP 0)

    filter->SetDscpMapping(mapping);
    
    
   /*
    TrafficControlHelper::ClassIdList cid =
            tchBottleneck.AddQueueDiscClasses(handle, 2, "ns3::QueueDiscClass");
    tchBottleneck.AddChildQueueDisc(handle, cid[0], "ns3::FifoQueueDisc","MaxSize", QueueSizeValue(QueueSize("50p")));
    tchBottleneck.AddChildQueueDisc(handle, cid[1], "ns3::RedQueueDisc","MaxSize", QueueSizeValue(QueueSize("50p")));
    */

    //-------------------end of create prio queue disc on each device

    for (int i = 0; i < 3; ++i) {
        NetDeviceContainer link = pointToPoint.Install(nodes);
        links.push_back(link);

        // Install a queue disc on this link
        QueueDiscContainer qdiscs = tchBottleneck.Install(link);
        // Get the first QueueDisc installed
        Ptr<QueueDisc> qdisc = qdiscs.Get(0);
        qdisc->AddPacketFilter(filter);
        
        allQdiscs.push_back(qdiscs);
    }

   
    //print type of each queue
    std::cout<<"*************Queues installed on links***************"<<std::endl;
    // Example: EF → band 0, AF41 → band 1, BF → band 3

    //for other queues than Prio
    /*
    for (size_t i = 0; i < allQdiscs.size(); ++i) {
        QueueDiscContainer qdc = allQdiscs[i];

        for (uint32_t j = 0; j < qdc.GetN(); ++j) {
            Ptr<QueueDisc> qdisc = qdc.Get(j);
            std::cout << "Link " << i
                    << " QueueDisc[" << j << "] = "
                    << qdisc->GetInstanceTypeId().GetName()
                    << std::endl;
        }
    }
        */

    //for prio queues
    for (size_t i = 0; i < allQdiscs.size(); ++i) {
        QueueDiscContainer qdc = allQdiscs[i];
        for (uint32_t j = 0; j < qdc.GetN(); ++j) {
            Ptr<QueueDisc> qdisc = qdc.Get(j);
            std::cout << "Link " << i
                    << " Root QueueDisc[" << j << "] = "
                    << qdisc->GetInstanceTypeId().GetName()
                    //<<" with size: "<<qdisc->GetMaxSize()
                    << std::endl;
            // Iterate over classes (bands)
            for (uint32_t i = 0; i < qdisc->GetNQueueDiscClasses(); i++) {
                Ptr<QueueDiscClass> qClass = qdisc->GetQueueDiscClass(i);
                Ptr<QueueDisc> child = qClass->GetQueueDisc();

                std::cout << "  Band " << i 
                        << " -> Child QueueDisc: " 
                        << child->GetInstanceTypeId().GetName()
                        << ", MaxSize=" << child->GetMaxSize() 
                         << std::endl;
            }
        }
    }
    
    std::cout<<"------------------------------------------------------------"<<std::endl;



    //installing queues
    /*
    TrafficControlHelper tchBottleneck;
   

    
        */

    //QueueDiscContainer qdiscs;
    //qdiscs = tchBottleneck.Install(devices);
    

    // 4. Assign IP addresses
    Ipv4AddressHelper ipv4;
    std::vector<Ipv4InterfaceContainer> interfaces;
    for (int i = 0; i < 3; ++i) {
        std::ostringstream subnet;
        subnet << "10.1." << i + 1 << ".0";
        ipv4.SetBase(subnet.str().c_str(), "255.255.255.0");
        interfaces.push_back(ipv4.Assign(links[i]));
        ipv4.NewNetwork();
    }

    
    
    std::vector<NetDeviceContainer*> device_containers;
    for (auto& link : links) {
        device_containers.push_back(&link);
    }


    std::vector<QueueDiscContainer*> allqdiscs_containers;
    for (auto& qdc : allQdiscs) {
        allqdiscs_containers.push_back(&qdc);
    }

    std::string transportProtocol;
    if (useUdp)
    {
        transportProtocol = "ns3::UdpSocketFactory";
    }
    else
    {
        transportProtocol = "ns3::TcpSocketFactory";
    }
    
  


    
    std::vector<uint32_t> nFlows = {real_numb_voip,real_numb_gaming,real_numb_video,real_numb_ftp};//work with the real numbers after compiuting the rounding
    std::vector<TrafficType> trafficTypes = {NGMN_VOIP,NGMN_GAMING, NGMN_VIDEO, NGMN_FTP };
    std::array<uint16_t, 4> basePorts = {5060, 5000, 16384, 21};
    //std::map<std::string,uint32_t> count voip/gaming/viedo/ftpFlowCount
    
    
    //map dscps to links
    /*
    std::map<std::string, int> dscpToLink = {
    {"EF", 0},
    {"AF41", 1},
    {"AF42", 1},
    {"AF43", 1},
    {"AF31", 2},
    {"AF32", 2},
    {"AF33", 2},
    {"AF21", 1},
    {"AF22", 1},
    {"AF23", 1},
    {"AF11", 2},
    {"AF12", 2},
    {"AF13", 2},
    {"BF", 2}
};
*/
std::srand(static_cast<unsigned int>(std::time(nullptr)));
std::map<std::string, int> dscpToLink ={};
    for (const auto& dscp : used_dscps) {
            dscpToLink[dscp] = std::rand() % 3;  // initialize value to 0
            std::cout << "DSCP " << dscp << " -> Link " << dscpToLink[dscp] << std::endl;
            
        }


//Simulator::Schedule(Seconds(5.0), &RescheduleDscpLinks, std::ref(dscpToLink), 3);
   // std::map<std::string, int> dscpToLink;
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();
    uint32_t duration=19000; //ms less then the rescheduling

    //Simulator::Schedule(Seconds(5.0), &RescheduleDscpLinks, std::ref(dscpToLink), 3,std::ref(flowmonHelper),std::ref(monitor),std::ref(appStartMs),duration);
    //create mapping dscps to links
    

    std::cout<<"********************************Creating apps:*************************************"<<std::endl;
    //for each traffic type create flows
   
    CreateApps(trafficTypes,nFlows,voipFlowCount,gamingFlowCount,videoFlowCount,ftpFlowCount,
                dscpToLink,interfaces,nodes,transportProtocol,basePorts,appStartMs,duration);


    // Enable FlowMonitor
    

    //pointToPoint.EnablePcapAll("dynamic-dscp-routing");

    //pointToPoint.EnablePcapAll("pcap");
    // Save features including dynamic queue info
    SaveGraphFeatures(nodes, device_containers,allQdiscs);
    Simulator::Stop(MilliSeconds(appDurationMs));
    Simulator::Run();

    monitor->SerializeToXmlFile("multi-links-flow.xml", true, true);
    
    


    Simulator::Destroy();
    //WritePacketInfoToFile(packetArrivalData, "packet_arrival_times.txt");
    //flowMonitor->SerializeToXmlFile("flowmonitor-final.xml", true, true);


    return 0;
}
