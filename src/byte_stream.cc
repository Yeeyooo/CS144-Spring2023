#include "byte_stream.hh"
#include <cmath>
#include <stdexcept>

using namespace std;

ByteStream::ByteStream( uint64_t capacity ) : capacity_( capacity ) {}

void Writer::push( string data )
{
  if ( input_end_flag ) { // if the stream has been closed or something wrong has happened
    return;
  }
  const uint64_t push_size = min( capacity_ - buffer.size(), data.length() );
  buffer.append( data.substr( 0, push_size ) );
  write_count += push_size;
}

void Writer::close()
{
  input_end_flag = true; // close the byte stream
}

void Writer::set_error()
{
  error_flag = true; // set error flag
}

bool Writer::is_closed() const
{
  return input_end_flag;
}

uint64_t Writer::available_capacity() const
{
  return capacity_ - buffer.size();
}

uint64_t Writer::bytes_pushed() const
{
  return write_count;
}

string_view Reader::peek() const
{
  return string_view( buffer );
}

bool Reader::is_finished() const
{
  return input_end_flag && buffer.empty(); // when buffer is empty
}

bool Reader::has_error() const
{
  return error_flag;
}

void Reader::pop( uint64_t len ) // remove 'len' bytes from the buffer
{
  const uint64_t pop_size = min( len, buffer.length() );
  buffer.erase( 0, pop_size );
  read_count += pop_size;
}
uint64_t Reader::bytes_buffered() const
{
  // Your code here.
  return buffer.size();
}

uint64_t Reader::bytes_popped() const
{
  // Your code here.
  return read_count;
}