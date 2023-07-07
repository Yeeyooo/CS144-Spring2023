#include "router.hh"

#include <iostream>
#include <limits>

using namespace std;

// route_prefix: The "up-to-32-bit" IPv4 address prefix to match the datagram's destination address against
// prefix_length: For this route to be applicable, how many high-order (most-significant) bits of
//    the route_prefix will need to match the corresponding bits of the datagram's destination address?
// next_hop: The IP address of the next hop. Will be empty if the network is directly attached to the router (in
//    which case, the next hop address should be the datagram's final destination).
// interface_num: The index of the interface to send the datagram out on.
void Router::add_route( const uint32_t route_prefix,
                        const uint8_t prefix_length,
                        const optional<Address> next_hop,
                        const size_t interface_num )
{
  cerr << "DEBUG: adding route " << Address::from_ipv4_numeric( route_prefix ).ip() << "/"
       << static_cast<int>( prefix_length ) << " => " << ( next_hop.has_value() ? next_hop->ip() : "(direct)" )
       << " on interface " << interface_num << "\n";
  // RouterTableEntry new_item = {route_prefix, prefix_length, next_hop, interface_num};  // add a new routing rule
  // routing_table.emplace_back(new_item);
  routing_table.push_back(RouterTableEntry{route_prefix, prefix_length, next_hop, interface_num});
}


void Router::route() {
  for (auto & inter : interfaces_) {
    while (1) {
      std::optional<InternetDatagram> data = inter.maybe_receive();
      if (!data.has_value()) {  // if current queue runs out of items
        break;
      }
      InternetDatagram dgram = data.value();
      if (dgram.header.ttl <= 1) {    // early exit
        continue;
      }
      size_t index = 0;
      bool flag = false;
      uint8_t longest = 0;
      for (size_t i = 0; i < routing_table.size(); i++) {
        if (routing_table[i].prefix_length >= longest) {
          uint32_t mask = routing_table[i].prefix_length == 0 ? 0 : (0xFFFFFFFF << (32 - routing_table[i].prefix_length));
          if ((dgram.header.dst & mask) == (routing_table[i].route_prefix & mask)) {
            flag = true;     // find one item at least
            index = i;
            longest = routing_table[i].prefix_length;
          }
        }
      }
      if (!flag) {
        continue;
      }
      dgram.header.ttl -= 1;
      AsyncNetworkInterface& forward = interface(routing_table[index].interface_num);
      if (routing_table[index].next_hop.has_value()) {
        forward.send_datagram(dgram, routing_table[index].next_hop.value());
      }
      else {
        forward.send_datagram(dgram, Address::from_ipv4_numeric(dgram.header.dst));
      }
    }
  }
}



// void Router::route() {
//   // for each network interface
//   for (AsyncNetworkInterface& inter : interfaces_) {
//     while (1) {
//       auto datagram = inter.maybe_receive();
//       if (!datagram.has_value()) {
//         break;
//       }
//       // current query IP address
//       InternetDatagram curr_datagram = datagram.value();  // corresponding InternetDatagram
//       const uint32_t dest_ip = curr_datagram.header.dst;  // extract destination IP address

//       std::vector<Router::RouterTableEntry>::iterator max_match = routing_table.end();    // iter points to the longest prefix match entry

//       for (auto iter = routing_table.begin(); iter != routing_table.end(); iter++) {
//         // if (iter -> prefix_length == 0 || (dest_ip & (0xFFFFFFFF << (32 - iter -> prefix_length))) == iter -> route_prefix) {
//         //   if (max_match == routing_table.end() || ((max_match -> prefix_length) <= (iter -> prefix_length))) {
//         //     max_match = iter;
//         //   }
//         // }
//         if (iter -> prefix_length == 0 || ((iter -> route_prefix ^ dest_ip) >> (32 - iter -> prefix_length)) == 0) {
//           if (max_match == routing_table.end() || max_match -> prefix_length < iter -> prefix_length) {
//             max_match = iter;
//           }
//         }
//       }

//       // if the find a entry that satisfies certain conditions, then we forward this InternetDatagram
//       // otherwise, we drop this datagram
//       if (max_match != routing_table.end() && curr_datagram.header.ttl-- > 1) { 
//         // curr_datagram.header.ttl -= 1;
//         AsyncNetworkInterface& forward_interface = interfaces_[max_match -> interface_num];
//         const optional<Address> hop = max_match -> next_hop;
//         if (hop.has_value()) {   // The router is directly attached to the network in question
//           forward_interface.send_datagram(curr_datagram, hop.value());
//         }
//         else {   // The router is connected to the network in question through some other router
//           // Address next_ip = Address::from_ipv4_numeric(curr_datagram.header.dst);
//           forward_interface.send_datagram(curr_datagram, Address::from_ipv4_numeric(curr_datagram.header.dst));
//         }
//       }

//     }

//   }
// }
