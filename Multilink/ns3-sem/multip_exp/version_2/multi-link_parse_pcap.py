#usage
#python3 scratch/multi-link_parse_pcap.py /home/nourhen_dev/repos/ns-3-allinone/ns-3.41/multi-link-0-1.pcap

import pyshark
import sys

def parse_pcap(file_path):
    # Open the pcap file with pyshark (offline capture)
    capture = pyshark.FileCapture(file_path, keep_packets=False)

    for pkt in capture:
        ts = float(pkt.sniff_timestamp)  # timestamp
        length = int(pkt.length)
        protocols = ",".join(pkt.layers[i].layer_name for i in range(len(pkt.layers)))

        # Default values
        src_ip = dst_ip = src_port = dst_port = protocol_name = "N/A"

        # Try to get IP info if available
        if 'ip' in pkt:
            src_ip = pkt.ip.src
            dst_ip = pkt.ip.dst
            protocol_name = pkt.ip.proto  # protocol number

        # Try to get TCP or UDP ports
        if 'tcp' in pkt:
            src_port = pkt.tcp.srcport
            dst_port = pkt.tcp.dstport
            protocol_name = 'TCP'
        elif 'udp' in pkt:
            src_port = pkt.udp.srcport
            dst_port = pkt.udp.dstport
            protocol_name = 'UDP'

        print(f"Timestamp: {ts:.6f} s, Length: {length} bytes, Protocols: {protocols}, "
            f"SrcIP: {src_ip}, DstIP: {dst_ip}, SrcPort: {src_port}, DstPort: {dst_port}, Proto: {protocol_name}")

    capture.close()


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python parse_pcap.py <pcap_file>")
        sys.exit(1)

    pcap_file = sys.argv[1]
    parse_pcap(pcap_file)
