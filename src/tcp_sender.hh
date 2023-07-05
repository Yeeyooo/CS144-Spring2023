#pragma once

#include "byte_stream.hh"
#include "tcp_receiver_message.hh"
#include "tcp_sender_message.hh"
#include <cmath>
#include <map>
#include <queue>
class TCPSender
{
  Wrap32 isn_;                  // initial sequence number aka.sequence number for SYN

  uint64_t timer{0};   
  bool timer_working = false;  // if the timer is working?

  bool syn_set = false;
  bool fin_set = false;

  uint64_t next_absolute_seq{0}; // absolute sequence number of the next segment's first byte
  uint64_t inflight_number{0};   // sequence numbers in flight

  uint64_t initial_RTO_ms_;     // initial retransmission timeout
  uint64_t current_RTO_ms_;     // current retransmission timeout, change as time goes by
  
  uint64_t last_ack{0};         // last acknowledged sequence number
  uint16_t window_size{1};        // window size is one by default
  uint64_t consecutive_retrans{0};    // keep track of consecutive retransmissions
  // index in the map is **absolute** sequence number
  std::map<uint64_t, TCPSenderMessage> outstandings{};   // keep track of outstanding segments
  std::deque<TCPSenderMessage> to_be_sent{};
public:
  /* Construct TCP sender with given default Retransmission Timeout and possible ISN */
  TCPSender( uint64_t initial_RTO_ms, std::optional<Wrap32> fixed_isn );

  /* Push bytes from the outbound stream */
  void push( Reader& outbound_stream );

  /* Send a TCPSenderMessage if needed (or empty optional otherwise) */
  std::optional<TCPSenderMessage> maybe_send();

  /* Generate an empty TCPSenderMessage */
  TCPSenderMessage send_empty_message() const;

  /* Receive an act on a TCPReceiverMessage from the peer's receiver */
  void receive( const TCPReceiverMessage& msg );

  /* Time has passed by the given # of milliseconds since the last time the tick() method was called. */
  void tick( uint64_t ms_since_last_tick );

  /* Accessors for use in testing */
  uint64_t sequence_numbers_in_flight() const;  // How many sequence numbers are outstanding?
  uint64_t consecutive_retransmissions() const; // How many consecutive *re*transmissions have happened?
};