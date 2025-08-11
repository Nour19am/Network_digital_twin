#include "ns3/core-module.h"

#include "ns3/network-module.h"

#include "ns3/internet-module.h"

#include "ns3/point-to-point-module.h"

#include "ns3/applications-module.h"

#include "ns3/flow-monitor-module.h"

#include <fstream>

using namespace ns3;

int main (int argc, char* argv[]) {
    std::string LinkDataRate ="5Mbps";
    std::string LinkDelay = "2ms";
    uint32_t    AppPacketSize = 1024;//bytes
    uint32_t    MaxPackets   = 10;
    float     Interval =0.1;



    CommandLine cmd(__FILE__);

    //link characteristics

    cmd.AddValue("LinkDataRate", "the bandwidth of the link", LinkDataRate);
    
    cmd.AddValue("LinkDelay", "the propagation delay of the link", LinkDelay);


    //app characteristics
    cmd.AddValue("AppPacketSize","app packet size",AppPacketSize);
    cmd.AddValue("MaxPackets","max packets sent",MaxPackets);
    cmd.AddValue("Interval","time interval between two packets sent",Interval);
    cmd.Parse(argc, argv);  

    std::cout<<"LinkDataRate:  "<<LinkDataRate<<std::endl; //debug

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

    UdpEchoServerHelper echoServer(9);

    ApplicationContainer serverApps = echoServer.Install(nodes.Get(1));

    serverApps.Start(Seconds(1.0));

    serverApps.Stop(Seconds(10.0));

    UdpEchoClientHelper echoClient(interfaces.GetAddress(1), 9);

    echoClient.SetAttribute("MaxPackets", UintegerValue(MaxPackets));

    echoClient.SetAttribute("Interval", TimeValue(Seconds(Interval)));

    echoClient.SetAttribute("PacketSize", UintegerValue(AppPacketSize));

    ApplicationContainer clientApps = echoClient.Install(nodes.Get(0));

    clientApps.Start(Seconds(2.0));

    clientApps.Stop(Seconds(10.0));

    // Enable FlowMonitor
    FlowMonitorHelper flowmonHelper;
    Ptr<FlowMonitor> monitor = flowmonHelper.InstallAll();


    Simulator::Stop(Seconds(11));
    Simulator::Run ();
 // Collect Metrics
    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmonHelper.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    // Open CSV file (overwrite mode)
    std::ofstream csvFile("network_data.csv");
    csvFile << "FlowID,SourceIP,DestinationIP,SourcePort,DestinationPort,Protocol,"
            << "TxPackets,RxPackets,TxBytes,RxBytes,"
            << "LostPackets,TimesForwarded,"
            << "DelaySum_s,JitterSum_s,"
            << "TimeFirstTx_s,TimeLastTx_s,TimeFirstRx_s,TimeLastRx_s,"
            << "MeanDelay(s),MeanJitter(s),MeanTxPktSize(bytes),MeanRxPktSize(bytes),"
            << "txDuration(s),rxDuration(s),"
            << "MeanTxBitrate(bps),MeanRxBitrate(bps),MeanHopCount,PacketLossRatio\n";


    for (auto const& flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);



        double rxPackets = flow.second.rxPackets;
        double txPackets = flow.second.txPackets;
        double meanDelay = (rxPackets > 0) ? (flow.second.delaySum.GetSeconds() / rxPackets) : 0;
        double meanJitter = (rxPackets > 1) ? (flow.second.jitterSum.GetSeconds() / (rxPackets - 1)) : 0;
        double meanTxPktSize = (txPackets > 0) ? ((double)flow.second.txBytes / txPackets) : 0;
        double meanRxPktSize = (rxPackets > 0) ? ((double)flow.second.rxBytes / rxPackets) : 0;

        double txDuration = (flow.second.timeLastTxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds());
        double rxDuration = (flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstRxPacket.GetSeconds());
        double meanTxBitrate = (txDuration > 0) ? ((8.0 * flow.second.txBytes) / txDuration) : 0;
        double meanRxBitrate = (rxDuration > 0) ? ((8.0 * flow.second.rxBytes) / rxDuration) : 0;

        double meanHopCount = (rxPackets > 0) ? (1.0 + (double)flow.second.timesForwarded / rxPackets) : 0;
        double packetLossRatio = (rxPackets + flow.second.lostPackets > 0) ?
                                 ((double)flow.second.lostPackets / (rxPackets + flow.second.lostPackets)) : 0;

    csvFile << flow.first << ","
            << t.sourceAddress << ","
            << t.destinationAddress << ","
            << t.sourcePort << ","
            << t.destinationPort << ","
            << (t.protocol == 6 ? "TCP" : (t.protocol == 17 ? "UDP" : "Other")) << ","
            << flow.second.txPackets << ","
            << flow.second.rxPackets << ","
            << flow.second.txBytes << ","
            << flow.second.rxBytes << ","
            << flow.second.lostPackets << ","
            << flow.second.timesForwarded << ","
            << flow.second.delaySum.GetSeconds() << ","
            << flow.second.jitterSum.GetSeconds() << ","
            << flow.second.timeFirstTxPacket.GetSeconds() << ","
            << flow.second.timeLastTxPacket.GetSeconds() << ","
            << flow.second.timeFirstRxPacket.GetSeconds() << ","
            << flow.second.timeLastRxPacket.GetSeconds()<< ","
            << meanDelay<<","
            << meanJitter<<","
            << meanTxPktSize <<","
            << meanRxPktSize << ","
            << txDuration <<","
            << rxDuration <<","
            << meanTxBitrate << ","
            << meanRxBitrate <<","
            << meanHopCount <<","
            << packetLossRatio <<","

            << "\n";

    }

    csvFile.close();
    Simulator::Destroy();
    return 0;

}