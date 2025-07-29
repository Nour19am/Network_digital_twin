//openflow SDN for centroid
//h0-s0-h1 (first)


//include necessary modules
#include <iostream>
#include <fstream>
#include "ns3/core-config.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/openflow-module.h"
#include "ns3/log.h"
#include "ns3/csma-module.h"
#include "ns3/netanim-module.h"

//simulation script

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Centroid_simple");
int main(int argc, char *argv[])

{
    //Enable logging

    //LogComponentEnable("UdpEchoClientApplication",LOG_LEVEL_INFO);
    //LogComponentEnable("UdpEchoClientApplication",LOG_LEVEL_INFO);
    LogComponentEnable("OpenFlowInterface", LOG_LEVEL_INFO);


    //Create nodes
    NodeContainer nodes;
    nodes.Create(2);
    NodeContainer switches;
    switches.Create (1);
    NodeContainer allNodes= NodeContainer(nodes,switches);


    //Install Internet stack
    InternetStackHelper internet;
    internet.Install(nodes);


    //Create csma links
    CsmaHelper csma;
    csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
    csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (2)));
    NetDeviceContainer link;
    NetDeviceContainer nodeDevices;
    NetDeviceContainer switchDevices;
    for (int i = 0; i < 2; i++)
    {
        NetDeviceContainer link = csma.Install (NodeContainer (nodes.Get (i), switches));
        nodeDevices.Add (link.Get (0));
        switchDevices.Add (link.Get (1));
    }
    //Install OpenFlow Switch on the switch node
    OpenFlowSwitchHelper swtch;
    Ptr<Node> switchNode = switches.Get(0);
    Ptr<ns3::ofi::LearningController> controller = CreateObject<ns3::ofi::LearningController> ();
    NetDeviceContainer validSwitchDevices;
    for (uint32_t i = 0; i < switchNode->GetNDevices(); ++i) {
        Ptr<NetDevice> dev = switchNode->GetDevice(i);
        std::cout << "Device " << i << " type: " << dev->GetInstanceTypeId().GetName() << std::endl;
        
        validSwitchDevices.Add(dev);
        
    }

    swtch.Install(switchNode, validSwitchDevices, controller);
    //swtch.Install (switchNode, switchDevices, controller);

    // We've got the "hardware" in place.  Now we need to add IP addresses.
    Ipv4AddressHelper ipv4;
    ipv4.SetBase ("10.1.1.0","255.255.255.0");
    ipv4.Assign(nodeDevices);

    //OnOff app to send UDP from h0 to h1
    uint16_t port =9;
    OnOffHelper onoff ("ns3::UdpSocketFactory", Address(InetSocketAddress(Ipv4Address ("10.1.1.2"),port)));
    onoff.SetConstantRate(DataRate("500kb/s"));
    ApplicationContainer app= onoff.Install(nodes.Get(0));
    //Start the application
    app.Start(Seconds(1.0));
    app.Stop(Seconds(10.0));

    //Create an optional packet sink to receive these packets
    PacketSinkHelper sink("ns3::UdpSocketFactory",Address(InetSocketAddress(Ipv4Address::GetAny (),port)));
    app=sink.Install(nodes.Get(1));
    app.Start(Seconds(0.0));

    AsciiTraceHelper ascii;
    csma.EnableAsciiAll(ascii.CreateFileStream("openflow-centroid.tr"));

    //tcdump traces for each interface
    csma.EnablePcapAll("openflow-centroid",false);

    //Start actual simulation
    AnimationInterface anim("openflow-centroid.xml");
    Simulator::Run();
    Simulator::Stop(Seconds(11.0));
    Simulator::Destroy();
    
    return 0;









}