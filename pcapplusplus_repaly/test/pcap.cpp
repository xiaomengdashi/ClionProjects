#include <pcapplusplus/Packet.h>
#include <pcapplusplus/EthLayer.h>
#include <pcapplusplus/IPv4Layer.h>
#include <pcapplusplus/UdpLayer.h>
#include <pcapplusplus/DnsLayer.h>
#include <pcapplusplus/IcmpLayer.h>
#include <pcapplusplus/PcapLiveDeviceList.h>
#include <pcapplusplus/PcapLiveDevice.h>
#include <pcapplusplus/PayloadLayer.h>

using namespace pcpp;

int main()
{
    // Open a live device for sending packets
    PcapLiveDevice* dev = PcapLiveDeviceList::getInstance().getPcapLiveDeviceByName("ap1");
    if (dev == NULL)
    {
        printf("Couldn't find interface with name ap1\n");
        return -1;
    }

    // Open the device
    if (!dev->open())
    {
        printf("Couldn't open the device\n");
        return -1;
    }

    // Create an Ethernet packet
    MacAddress srcMac("00:11:22:33:44:55");
    MacAddress dstMac("ff:ff:ff:ff:ff:ff");
    EthLayer ethLayer(srcMac, dstMac, PCPP_ETHERTYPE_IP);

    // Create an IPv4 packet
    IPv4Address srcIP("192.168.1.2");
    IPv4Address dstIP("8.8.8.8");  // DNS server
    IPv4Layer ipLayer(srcIP, dstIP);
    ipLayer.getIPv4Header()->ipId = htons(1);

    // Create a UDP packet for DNS
    UdpLayer udpLayer(12345, 53);  // DNS port 53

    // Create a DNS query packet
    DnsLayer dnsLayer;
    dnsLayer.getDnsHeader()->transactionID = htons(0x1234);
    dnsLayer.getDnsHeader()->recursionDesired = true;
    dnsLayer.addQuery("example.com", DNS_TYPE_A, DNS_CLASS_IN);

    // Create a payload
    uint8_t payloadData[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    
    PayloadLayer payloadLayer(payloadData, sizeof(payloadData));

    // Create a packet
    Packet packet;
    packet.addLayer(&ethLayer);
    packet.addLayer(&ipLayer);
    packet.addLayer(&udpLayer);
    packet.addLayer(&dnsLayer);
    packet.computeCalculateFields();

    // Send the DNS packet
    if (!dev->sendPacket(&packet))
    {
        printf("Couldn't send the DNS packet\n");
        return -1;
    }
    printf("DNS packet sent successfully\n");

    // Create and send an ICMP packet
    // Create new Ethernet layer for ICMP
    EthLayer ethLayer2(srcMac, dstMac, PCPP_ETHERTYPE_IP);
    
    // Create new IPv4 layer for ICMP
    IPv4Address icmpDstIP("192.168.1.3");
    IPv4Layer ipLayer2(srcIP, icmpDstIP);
    ipLayer2.getIPv4Header()->ipId = htons(2);
    ipLayer2.getIPv4Header()->protocol = PACKETPP_IPPROTO_ICMP;
    
    // Create ICMP layer
    IcmpLayer icmpLayer;
    
    // Create payload for ICMP
    uint8_t icmpPayloadData[] = {0x00, 0x01, 0x02, 0x03, 0x04};
    PayloadLayer icmpPayloadLayer(icmpPayloadData, sizeof(icmpPayloadData));
    
    // Create ICMP packet
    Packet icmpPacket;
    icmpPacket.addLayer(&ethLayer2);
    icmpPacket.addLayer(&ipLayer2);
    icmpPacket.addLayer(&icmpLayer);
    icmpPacket.addLayer(&icmpPayloadLayer);
    icmpPacket.computeCalculateFields();
    
    // Send the ICMP packet
    if (!dev->sendPacket(&icmpPacket))
    {
        printf("Couldn't send the ICMP packet\n");
        return -1;
    }
    printf("ICMP packet sent successfully\n");

    return 0;
}