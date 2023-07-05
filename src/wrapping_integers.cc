#include "wrapping_integers.hh"
#include <cmath>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return zero_point + static_cast<uint32_t>(n);
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t offset = raw_value_ - wrap(checkpoint, zero_point).raw_value_;      // compute offset
  uint64_t ans = checkpoint + offset;
  if ( offset > ( 1u << 31) && ans >= (1ul << 32)) {
    ans -= (1ul << 32);
  }
  return ans;
}