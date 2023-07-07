#include "network_interface.hh"
#include <queue>
#include "arp_message.hh"
#include "ethernet_frame.hh"
#include <map>
using namespace std;

// ethernet_address: Ethernet (what ARP calls "hardware") address of the interface
// ip_address: IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface( const EthernetAddress& ethernet_address, const Address& ip_address )
  : ethernet_address_( ethernet_address ), ip_address_( ip_address )
{
  cerr << "DEBUG: Network interface has Ethernet address " << to_string( ethernet_address_ ) << " and IP address "
       << ip_address.ip() << "\n";
}

// dgram: the IPv4 datagram to be sent
// next_hop: the IP address of the interface to send it to (typically a router or default gateway, but
// may also be another host if directly connected to the same network as the destination)

// Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) by using the
// Address::ipv4_numeric() method.
void NetworkInterface::send_datagram( const InternetDatagram& dgram, const Address& next_hop )
{
  uint32_t next_hop_ip = next_hop.ipv4_numeric();     // IP address of the next-hop router
  // if the destination Ethernet address is already known, send it right away
  if (arp_cache.find(next_hop_ip) != arp_cache.end()) {
    EthernetAddress corresponding = arp_cache[next_hop_ip].first;   // corresponding Ethernet address of next-hop router
    EthernetFrame next;
    next.header.type = EthernetHeader::TYPE_IPv4;
    next.header.dst = corresponding;        // destination Ethernet address
    next.header.src = ethernet_address_;    // source Ethernet address
    // set the payload to be the serialized datagram
    next.payload = serialize(dgram);
    frames_out.emplace(next);
  }
  else if (arp_cache.find(next_hop_ip) == arp_cache.end()) {   
    if (timing.count(next_hop_ip) == 0 || timing[next_hop_ip] + 5000 < time) {
      // Construct ARP request message
      ARPMessage arp;
      arp.opcode = ARPMessage::OPCODE_REQUEST;    // request ARP message
      arp.sender_ethernet_address = ethernet_address_;
      arp.sender_ip_address = ip_address_.ipv4_numeric();
      arp.target_ip_address = next_hop_ip;
      // Construct Ethernet frame
      EthernetFrame arp_request;
      arp_request.header.src = ethernet_address_;
      arp_request.header.dst = ETHERNET_BROADCAST;
      arp_request.header.type = EthernetHeader::TYPE_ARP;
      arp_request.payload = serialize(arp);
      frames_out.emplace(arp_request);     // arp request message
      timing[next_hop_ip] = time;   
      // destination MAC address is temporarily empty
      EthernetFrame next;
      next.header.type = EthernetHeader::TYPE_IPv4;
      next.header.src = ethernet_address_;
      next.payload = serialize(dgram);
      waiting_mac.emplace(make_pair(next_hop_ip, next));
    }
  }
}

// frame: the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame( const EthernetFrame& frame )
{
  // Ignore any frames not destined for the network interface
  if (frame.header.dst != ETHERNET_BROADCAST && frame.header.dst != ethernet_address_) {
    return std::nullopt;
  }
  
  // if the inbound frame is IPv4, parse the payload as an InternetDatagram
  if (frame.header.type == EthernetHeader::TYPE_IPv4) {
    InternetDatagram internet_message;
    if (parse(internet_message, frame.payload)) {    // On success, return the resulting InternetDatagram to the caller
      return internet_message;
    }
    else {
      return std::nullopt;
    }
  }

  if (frame.header.type == EthernetHeader::TYPE_ARP) {
    ARPMessage arp_message;
    bool flag = parse(arp_message, frame.payload);    // parse received ARP message and act accordingly
    uint32_t my_ip = ip_address_.ipv4_numeric();
    if (flag) {   
      // Learn mappings from both requests and replies
      arp_cache[arp_message.sender_ip_address] = make_pair(arp_message.sender_ethernet_address, time);
      
      if (arp_message.opcode == ARPMessage::OPCODE_REPLY && arp_message.target_ip_address == my_ip) {
        while (!waiting_mac.empty() && waiting_mac.front().first == arp_message.sender_ip_address) {
        EthernetFrame curr = waiting_mac.front().second;
        curr.header.dst = arp_message.sender_ethernet_address;    // fill the empty destination MAC address
        frames_out.emplace(curr);
        waiting_mac.pop();
       }
       return std::nullopt;
      }
      else if (arp_message.opcode == ARPMessage::OPCODE_REQUEST && arp_message.target_ip_address == my_ip) {
        ARPMessage reply;       // Construct an appropriate ARP reply message
        reply.opcode = ARPMessage::OPCODE_REPLY;
        reply.sender_ethernet_address = ethernet_address_;
        reply.sender_ip_address = my_ip;
        reply.target_ethernet_address = arp_message.sender_ethernet_address;
        reply.target_ip_address = arp_message.sender_ip_address;
        EthernetFrame carry;
        carry.header.dst = arp_message.sender_ethernet_address;
        carry.header.src = ethernet_address_;
        carry.header.type = EthernetHeader::TYPE_ARP;
        carry.payload = serialize(reply);
        frames_out.emplace(carry);     // push current Ethernet frame into the waiting queue
        return std::nullopt;
      }
    }
  }
  return std::nullopt;
}

// ms_since_last_tick: the number of milliseconds since the last call to this method
void NetworkInterface::tick( const size_t ms_since_last_tick )
{
  time += ms_since_last_tick;    // update time
  // Expire any IP-to-Ethernet mappings that have expired
  std::map<uint32_t, pair<EthernetAddress, size_t>>::iterator itr = arp_cache.begin();
  while (itr != arp_cache.end()) {
    if (itr -> second.second + 30000 <= time) {
      itr = arp_cache.erase(itr);
    }
    else {
      ++itr;
    }
  }
}

optional<EthernetFrame> NetworkInterface::maybe_send()
{
  if (!frames_out.empty()) {      // if the queue is not empty
    EthernetFrame next = frames_out.front();
    frames_out.pop();
    return next;
  }
  else {
    return std::nullopt;
  }
}
