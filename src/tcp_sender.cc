#include "tcp_sender.hh"
#include "tcp_config.hh"

#include <random>

using namespace std;

/* TCPSender constructor (uses a random ISN if none given) */
TCPSender::TCPSender( uint64_t initial_RTO_ms, optional<Wrap32> fixed_isn )
  : isn_( fixed_isn.value_or( Wrap32 { random_device()() } ) ), initial_RTO_ms_( initial_RTO_ms ), current_RTO_ms_( initial_RTO_ms )
{}

uint64_t TCPSender::sequence_numbers_in_flight() const
{
  return inflight_number;     // remember we should maintain this variable explicitly
}

uint64_t TCPSender::consecutive_retransmissions() const
{
  return consecutive_retrans;
}

optional<TCPSenderMessage> TCPSender::maybe_send()
{
  if (to_be_sent.size() == 0) {
    return std::nullopt;
  }
  TCPSenderMessage next_message = to_be_sent.front();
  to_be_sent.pop_front();
  // when a segment containing data is sent, if the timer is not working,
  // START it running 
  if (!timer_working) {
    timer_working = true;
    timer = 0;
  }
  return next_message;
}

void TCPSender::push( Reader& outbound_stream )
{
  uint16_t curr_window_size = window_size == 0 ? 1 : window_size; // if the window size is zero, we pretend like it is one
  while (curr_window_size >= inflight_number) {
    TCPSenderMessage next;    // construct next segment waiting to be sent
    if (!syn_set) {           // should we set SYN value in this segment?
      next.SYN = true;
      syn_set = true;
    }
    next.seqno = isn_.wrap(next_absolute_seq, isn_);      // wrap current sequence number
    // make individual message as big as possible

    uint64_t payload_size_one = min(TCPConfig::MAX_PAYLOAD_SIZE, curr_window_size - inflight_number);   // the number can be sent
    uint64_t payload_size = min(payload_size_one, outbound_stream.bytes_buffered());                    // the number can be read
    std::string next_payload;
    read(outbound_stream, payload_size, next_payload);
    next.payload = Buffer(next_payload);
    // should we set FIN flag???
    
    if (!fin_set && outbound_stream.is_finished() && payload_size + inflight_number < curr_window_size) {
      next.FIN = true;             // set FIN flag in current segment
      fin_set = true;
    }


    if (next.sequence_length() == 0) {
      break;
    }

    to_be_sent.push_back(next);      // "send" this segment by pushing it into the queue
    outstandings[next_absolute_seq] = next;    // keep track of sent yet not knowledged segments

    // maintain relevant variables
    next_absolute_seq += next.sequence_length();
    inflight_number += next.sequence_length();  
    if (next.FIN) {
      break;
    }
  }
}

TCPSenderMessage TCPSender::send_empty_message() const
{
  return TCPSenderMessage(isn_.wrap(next_absolute_seq, isn_), false, Buffer(), false);
}

void TCPSender::receive( const TCPReceiverMessage& msg )
{
  window_size = msg.window_size;   // new window size
  // compare criterion is **absolute** sequence number
  uint64_t new_absolute_ackno = msg.ackno -> unwrap(isn_, next_absolute_seq);
  if (new_absolute_ackno <= last_ack || new_absolute_ackno > next_absolute_seq) {    // received invalid ackno
    return;
  }
   
  auto iter = outstandings.begin();
  for (; iter != outstandings.end(); iter++) {
    if (new_absolute_ackno < (iter -> first + iter -> second.sequence_length())) {
      break;
    }
  }
  outstandings.erase(outstandings.begin(), iter);       // remove those acknowledged parts
  inflight_number -= (new_absolute_ackno - last_ack);   // maintain inflight_number variable
  if (!outstandings.empty()) {
    timer_working = true;
    timer = 0;
  }
  else {     // if all outstanding segments have been acknowledged, stop the timer
    timer_working = false;
    timer = 0;
  }
  last_ack = new_absolute_ackno;       // update last_ack, this variable is non-decreasing
  current_RTO_ms_ = initial_RTO_ms_;   // set RTO back to its initial value
  consecutive_retrans = 0;             // set consecutive retransmissions back to zero
}


void TCPSender::tick( const size_t ms_since_last_tick )
{
  if ( !timer_working ) {           // if the timer is not working, return immediately
    return;
  }
  timer += ms_since_last_tick;    // update time to represent current time
  auto iter = outstandings.begin();
  if (timer >= current_RTO_ms_ && iter != outstandings.end()) {    // timer has expired
    to_be_sent.push_front(outstandings.begin() -> second);         // retransmit earliest segment...use what??? (possibly solved)
    consecutive_retrans += 1;   // increment this variable
    if (window_size > 0) {      // if window size is nonzero, then network is congested
      current_RTO_ms_ *= 2;     // double the value of RTO
    }
    timer = 0;                  // reset the timer and start it
    timer_working = true;       // restart the timer
  }
}