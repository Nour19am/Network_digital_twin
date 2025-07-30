#to test after running the simulation
#python3 scratch/multi-link_flow_parser.py multi-links-flow.xml

import xml.etree.ElementTree as ET
import pandas as pd
import json
import sys

def parse_flowmonitor(xml_file):
    # Load the XML file
    flowmon_path=xml_file
    tree = ET.parse(flowmon_path)  # Replace with your file path
    root = tree.getroot()

    # Prepare a list to collect parsed flow data
    flow_data = []

    # Iterate over all <Flow> elements inside <FlowStats>
    #---------------nano seconds to second & bytes to Megabytes
    for flow in root.find('FlowStats').findall('Flow'):
        flow_id = int(flow.attrib['flowId'])
        time_first_tx = float(flow.attrib['timeFirstTxPacket'].replace('ns', '').replace('+', ''))*1E-9
        time_first_rx = float(flow.attrib['timeFirstRxPacket'].replace('ns', '').replace('+', ''))*1E-9
        time_last_tx = float(flow.attrib['timeLastTxPacket'].replace('ns', '').replace('+', ''))*1E-9
        time_last_rx = float(flow.attrib['timeLastRxPacket'].replace('ns', '').replace('+', ''))*1E-9
        delay_sum = float(flow.attrib['delaySum'].replace('ns', '').replace('+', ''))*1E-9
        jitter_sum = float(flow.attrib['jitterSum'].replace('ns', '').replace('+', ''))*1E-9
        last_delay = float(flow.attrib['lastDelay'].replace('ns', '').replace('+', ''))*1E-9
        tx_bytes = int(flow.attrib['txBytes'])
        rx_bytes = int(flow.attrib['rxBytes'])
        tx_packets = int(flow.attrib['txPackets'])
        rx_packets = int(flow.attrib['rxPackets'])
        lost_packets = int(flow.attrib['lostPackets'])
        times_forwarded = int(flow.attrib['timesForwarded'])
        duration = time_last_rx - time_first_tx
        throughput_Mbps = (int(flow.attrib['rxBytes']) * 8) / (duration * 1e6)  # bits/sec → Mbps


        flow_data.append({
            'flow_id': flow_id,
            'time_first_tx_s': time_first_tx,
            'time_first_rx_s': time_first_rx,
            'time_last_tx_s': time_last_tx,
            'time_last_rx_s': time_last_rx,
            'delay_sum_s': delay_sum,
            'jitter_sum_s': jitter_sum,
            'last_delay_s': last_delay,
            'tx_bytes': tx_bytes,
            'rx_bytes': rx_bytes,
            'tx_packets': tx_packets,
            'rx_packets': rx_packets,
            'lost_packets': lost_packets,
            'times_forwarded': times_forwarded,
            'duration':duration,
            'throughput_Mbps':throughput_Mbps
        })

    classifier_info = {}
    for flow in root.find('Ipv4FlowClassifier').findall('Flow'):
        flow_id = int(flow.attrib['flowId'])
        classifier_info[flow_id] = {
            'src': flow.attrib['sourceAddress'],
            'dst': flow.attrib['destinationAddress'],
            'src_port': flow.attrib['sourcePort'],
            'dst_port': flow.attrib['destinationPort'],
            'protocol': flow.attrib['protocol']
        }

    # Merge with previous stats if needed
    for row in flow_data:
        fid = row['flow_id']
        if fid in classifier_info:
            row.update(classifier_info[fid])
    df_flowmon = pd.DataFrame(flow_data)
    df_flowmon['mean_delay']=  df_flowmon['delay_sum_s']/df_flowmon['rx_packets']
    df_flowmon['mean-jitter']=df_flowmon['jitter_sum_s']/df_flowmon['rx_packets']

    df_flowmon['flow_5tuple'] = (df_flowmon['src'].astype(str) + '-' +
                            df_flowmon['dst'].astype(str) + '-' +
                            df_flowmon['src_port'].astype(str) + '-' + 
                            df_flowmon['dst_port'].astype(str) + '-' +
                            df_flowmon['protocol'].astype(str))

    return df_flowmon

if __name__ == "__main__":
    xml_file = sys.argv[1]
    output_csv = "flow_df.csv"
    flows = parse_flowmonitor(xml_file)
    flows.to_csv(output_csv, index=False)

