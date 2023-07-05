#include "reassembler.hh"
using namespace std;

void Reassembler::insert( uint64_t first_index, string data, bool is_last_substring, Writer& output )
{
  if ( output.is_closed() )
    return;
  if ( is_last_substring ) {
    eof_has_come = true;
    last_index = first_index + data.length();
    if ( eof_has_come && next_expected >= last_index ) { // empty byte_stream----corner case
      output.close();
    }
  }

  if ( first_index <= next_expected && first_index + data.length() > next_expected ) { // this is exactly the byte string that we want
    const uint64_t full_len = first_index + data.length() - next_expected;
    const uint64_t write_len = min( output.available_capacity(), full_len );
    output.push( data.substr( next_expected - first_index, write_len ) );
    next_expected += write_len;
    if ( write_len != full_len ) { // 如果当前byte_stream的容量已满，需要清空缓存的所有字符串，并且设置cached_number为0
      cached_number = 0;
      cache.clear();
    } else if ( cached_number != 0 ) { // 如果当前的字符串完全写入byte_stream, 处理缓存中过期的部分(前提是有缓存的部分)
      auto q = cache.begin();
      bool should_plus = true;
      for ( ; q != cache.end(); q++ ) {
        if ( q->first <= next_expected && ( q->first + q->second.length() ) > next_expected ) {
          break; // 在此时跳出循环
        }
        if ( q->first + q->second.length() <= next_expected ) {
          cached_number -= q->second.length();
        }
        if ( q->first >= next_expected ) {
          should_plus = false;
          break;
        }
      }

      if ( q == cache.end() ) {
        cache.clear();
      } else {
        if ( should_plus ) {      // 找到一个一半重合的
          const string tmp = q->second; // 保存一下这个一部分重合的字符串的信息
          const uint64_t tmp_index = q->first;
          q++; // increment the pointer
          cache.erase( cache.begin(), q );
          cached_number -= ( next_expected - tmp_index );
          cache[next_expected] = tmp.substr( next_expected - tmp_index );
        } else {
          cache.erase( cache.begin(), q );
        }
      }
    }

    if ( eof_has_come && next_expected >= last_index ) { // if the last byte has been pushed
      output.close();
    }
    // 处理缓存中缓存部分的字符串
    if ( !output.is_closed() && write_len == full_len && cached_number != 0 ) { // 当前截断后的字符串都已经被成功写入byte_stream,继续处理缓冲区中缓冲的字符串
      auto p = cache.begin(); // iterator pointer
      bool flag = false;      // if this flag is true, we need to clear all cached string and set cached_number zero
      for ( ; p != cache.end(); p++ ) {
        if ( p->first > next_expected )
          break; // 到达不合格的地方,但是此时并不是byte_stream耗尽了容量，跳出循环

        const uint64_t curr_write_len = min( output.available_capacity(), p->second.length() ); // 取当前Index存储的字符串长度和可用容量之间的最小值
        if ( curr_write_len != p->second.length() ) { // 如果当前byte_stream的capacity已经为0，需要清空所有缓存的字符串
          flag = true;
          break;
        }
        output.push( p->second.substr( 0, curr_write_len ) );
        next_expected += curr_write_len;
        cached_number -= curr_write_len;
      }

      if ( flag ) {
        cached_number = 0;
        cache.clear();
      } else {
        cache.erase( cache.begin(), p );
      }
      if ( eof_has_come && next_expected == last_index ) {
        output.close();
      }
    }
  }

  if ( first_index > next_expected ) {                // current byte string needs to be cached
    bool discard = false;                             // if current byte string needs to be discarded
    auto pos_iter = cache.upper_bound( first_index ); // 获取当前cache中第一个大于Index的指针
    if ( pos_iter != cache.begin() ) {                // 尝试获取小于等于index的指针
      pos_iter--;
    }
    // 处理前面的部分
    uint64_t new_first_index = first_index; // 表示当前字符串截断后的新起始位置
    if ( pos_iter != cache.end() && pos_iter->first < first_index ) {   // 如果前面存在子串
      const uint64_t up_first_index = pos_iter->first;                  // 前面第一个字串的第一个序号
      if ( first_index < up_first_index + pos_iter->second.length() ) { // 如果存在重叠
        // 进一步检测是部分重叠还是被完全包含
        if ( first_index + data.length() <= up_first_index + pos_iter->second.length() ) { // 如果是被完全包含
          discard = true;                                               // 当前的字符串需要被直接丢弃
        } else {                                                        // 如果只是部分重叠
          new_first_index = up_first_index + pos_iter->second.length(); // 部分截断后新的起始序号
          data = data.substr( new_first_index - first_index );          // 截断前面一部分的字符串
        }
      }
    }

    // 如果当前的字符串在处理完前面的部分之后不需要直接被丢弃，则处理后面的部分
    if ( !discard ) {                                  // 如果处理完前面的部分字符串不需要被丢弃
      pos_iter = cache.lower_bound( new_first_index ); // 指向cache中可能存在的大于等于first_index的指针
      while ( pos_iter != cache.end() && new_first_index <= pos_iter->first ) {
        if ( new_first_index == pos_iter->first
             && ( new_first_index + data.length() <= pos_iter->first + pos_iter->second.length() ) ) {
          discard = true; // 表示当前字符串需要被直接丢弃, 并且跳出循环
          break;
        } else if ( pos_iter->first < new_first_index + data.length() ) { // 如果与后面可能存在的字符串存在重叠
          if ( new_first_index + data.length() < pos_iter->first + pos_iter->second.length() ) { // 如果只是部分重叠
            data = data.substr( 0, pos_iter->first - new_first_index ); // 去掉重叠的一部分
          }
          else {                                        // 如果是完全重叠
            cached_number -= pos_iter->second.length(); // 更新cached_number
            pos_iter = cache.erase( pos_iter ); // 在cache中去除掉被完全重叠的字符串，让指针指向下一个
            continue;
          }
        } else { // 不存在重叠的情况
          break;
        }
      }
    }

    // after handling overlapping part
    if ( !discard ) {
      const uint64_t gap = new_first_index - next_expected;
      const uint64_t final_write_len = min( output.available_capacity() - gap, data.length() );
      cache[new_first_index] = data.substr( 0, final_write_len );
      cached_number += final_write_len; // update variable
    }
  }
}

uint64_t Reassembler::bytes_pending() const
{
  return cached_number;
}