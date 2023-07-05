#include "tcp_receiver.hh"
#include <cmath>
using namespace std;

void TCPReceiver::receive( TCPSenderMessage message, Reassembler& reassembler, Writer& inbound_stream )
{
  if (message.SYN && !ISN.has_value()) {   // if SYN is set, set initial sequence number
    ISN = message.seqno;
    reassembler.insert(0, message.payload, message.FIN, inbound_stream);
  }
  if (ISN.has_value() && !message.SYN) {     // if this is not the first segment
    if (message.seqno.unwrap(ISN.value(), inbound_stream.bytes_pushed() + 1) >= 1) {      // skip segments with invalid seqno
      uint64_t stream_index = message.seqno.unwrap(ISN.value(), inbound_stream.bytes_pushed() + 1) - 1;
      reassembler.insert(stream_index, message.payload, message.FIN, inbound_stream);
    }
  }
  if (ISN.has_value() && message.FIN) end_count += 1;     // 代表FIN已经到达
}

TCPReceiverMessage TCPReceiver::send( const Writer& inbound_stream ) const
{
  if (!ISN.has_value()) {           // 如果第一个包含SYN的包还没有到
    return TCPReceiverMessage(std::nullopt, inbound_stream.available_capacity() > 65535 ? 65535 : inbound_stream.available_capacity());
  }
  else {                            // 如果第一个包含SYN的包已经到达
    if (ISN.has_value() && end_count == 1 && inbound_stream.is_closed()) {
      return TCPReceiverMessage(ISN.value().wrap(inbound_stream.bytes_pushed() + 2, ISN.value()), inbound_stream.available_capacity() > 65535 ? 65535 : inbound_stream.available_capacity());
    }
    return TCPReceiverMessage(ISN.value().wrap(inbound_stream.bytes_pushed() + 1, ISN.value()), inbound_stream.available_capacity() > 65535 ? 65535 : inbound_stream.available_capacity());
  }
}
