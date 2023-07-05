#pragma once

#include "byte_stream.hh"
#include <cmath>
#include <map>
#include <vector>
#include <string>

class Reassembler
{
private:
  uint64_t next_expected = 0; // the index of the next byte we want, bytes before next_expected have all been
                              // successfully received and pushed
  uint64_t last_index = 0;    // the index of the last index in the whole byte stream
  uint64_t cached_number = 0; // used for bytes_pending function
  bool eof_has_come = false;  // flag used to indicate that last byte string has become
  std::map<uint64_t, std::string> cache {}; // the first one is the index, the second one is the corresponding string
public:
  /*
   * Insert a new substring to be reassembled into a ByteStream.
   *   `first_index`: the index of the first byte of the substring
   *   `data`: the substring itself
   *   `is_last_substring`: this substring represents the end of the stream
   *   `output`: a mutable reference to the Writer
   *
   * The Reassembler's job is to reassemble the indexed substrings (possibly out-of-order
   * and possibly overlapping) back into the original ByteStream. As soon as the Reassembler
   * learns the next byte in the stream, it should write it to the output.
   *
   * If the Reassembler learns about bytes that fit within the stream's available capacity
   * but can't yet be written (because earlier bytes remain unknown), it should store them
   * internally until the gaps are filled in.
   *
   * The Reassembler should discard any bytes that lie beyond the stream's available capacity
   * (i.e., bytes that couldn't be written even if earlier gaps get filled in).
   *
   * The Reassembler should close the stream after writing the last byte.
   */
  void insert( uint64_t first_index, std::string data, bool is_last_substring, Writer& output );

  // How many bytes are stored in the Reassembler itself?
  uint64_t bytes_pending() const;
};
