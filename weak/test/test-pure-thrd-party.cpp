#include "h.hpp"

#include <ethash/keccak.hpp>

#include <string>
using std::string;
#include <string_view>
using std::string_view;
#include <evmc/evmc.hpp>
#include <optional>
using std::optional;
#include <algorithm>



BOOST_AUTO_TEST_CASE(test_ethash){
  string s = "baz(uint32,bool)";
  ethash::hash256 h = ethash::keccak256(reinterpret_cast<uint8_t*>(s.data()), s.size());

  string_view v{reinterpret_cast<const char*>(h.bytes),4};
  BOOST_CHECK_EQUAL(v,"\xcd\xcd\x77\xc0");

  // Got from --------------------------------------------------
  // from eth_hash.auto import keccak
  // s='baz(uint32,bool)'
  // b1 = bytes(s,'utf-8')
  // b = keccak(b1)
  // h = b.hex()
  // h[:8] # 'cdcd77c0' the first 4 bytes used as function selector
}

string addressToString(evmc::address a) noexcept{
  auto bs = evmc::bytes_view(a);
  // in evmc.hpp ln67, evmch gives a convertion to bytes_view from address
  return evmc::hex(bs);
  // In fact, we can just use:
  //    string s  = evmc::hex(this->from);
  // But this one is more readable and less C++
}

namespace std{
  // 🦜 Define this to help ostream to display evmc::address
  ostream& operator<<(ostream& os, const evmc::bytes& h){
    // os << intx::be::load<intx::uint<256>,evmc::address>(h);
    os << evmc::hex(h);
    return os;
  };

  // 🦜 Define this to help ostream to display evmc::address
  ostream& operator<<(ostream& os, const evmc::address& h){
    // os << intx::be::load<intx::uint<256>,evmc::address>(h);
    os << addressToString(h);
    return os;
  };
}

// 🦜 : Teach c++ how to compare two address
bool operator==(const evmc::address && l, const evmc::address && r){
  return std::equal(std::cbegin(l.bytes), std::cend(l.bytes) , std::cbegin(r.bytes));
};
bool operator!=(const evmc::address && l, const evmc::address && r){
  return not (l == r);
};

/*
  🦜 : It looks like boost have trouble about ambiguous overloading?
 */
// bool operator==(const evmc::address &l, const evmc::address r){
//   return std::equal(std::cbegin(l.bytes), std::cend(l.bytes) , std::cbegin(r.bytes));
// };
// bool operator==(const evmc::address &l, const evmc::address &r){
//   return std::equal(std::cbegin(l.bytes), std::cend(l.bytes) , std::cbegin(r.bytes));
// };
// bool operator==(const evmc::address &l, const evmc::address &&r){
//   return std::equal(std::cbegin(l.bytes), std::cend(l.bytes) , std::cbegin(r.bytes));
// };
// bool operator!=(const evmc::address &l, const evmc::address &r){
//   return not (l == r);
// };

BOOST_AUTO_TEST_SUITE(test_evmc);

BOOST_AUTO_TEST_CASE(test_from_hex_to_construct_addr){
  optional<evmc::address> r = evmc::from_hex<evmc::address>("00000000000000000000000000000000000000ff");
  optional<evmc::address> r_too_long = evmc::from_hex<evmc::address>("00000000000000000000000000000000000000000");
  optional<evmc::address> r_shorter = evmc::from_hex<evmc::address>("aaaa");
  optional<evmc::address> r_empty = evmc::from_hex<evmc::address>("");
  optional<evmc::address> r_bad_hex = evmc::from_hex<evmc::address>("aaax");
  optional<evmc::address> r_hex_not_even = evmc::from_hex<evmc::address>("aaa");

  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(r.value(),evmc::address{uint64_t{0xff}});

  BOOST_REQUIRE(r_shorter);
  BOOST_CHECK_EQUAL(r_shorter.value(),evmc::address{uint64_t{0xaaaa}});
  BOOST_REQUIRE(r_empty);
  BOOST_CHECK_EQUAL(r_empty.value(),evmc::address{uint64_t{0}});

  BOOST_REQUIRE(not r_too_long);
  BOOST_REQUIRE(not r_bad_hex);
  BOOST_REQUIRE(not r_hex_not_even);
}

BOOST_AUTO_TEST_CASE(test_from_hex_for_bytes){
  using evmc::bytes;
  optional<bytes> r = evmc::from_hex("abab");
  optional<bytes> r_hex_not_even = evmc::from_hex("aba");

  BOOST_REQUIRE(r);
  BOOST_CHECK_EQUAL(r.value(),bytes({0xab,0xab}));

  BOOST_REQUIRE(not r_hex_not_even);
}

BOOST_AUTO_TEST_SUITE_END();

#include "intx/intx.hpp"
BOOST_AUTO_TEST_CASE(test_intx){
  // uint64_t <-> bytes
  uint64_t n = 0x1122334455667788;
  uint8_t dst[8];
  intx::be::store<uint64_t>(dst,n);
  BOOST_CHECK_EQUAL(dst[0],0x11);
  BOOST_CHECK_EQUAL(dst[7],0x88);
  // 🦜 : Let's turn that to an std::string
  string s(reinterpret_cast<const char *>(&(dst[0])),8);
  BOOST_CHECK_EQUAL(s[0],'\x11');
  BOOST_CHECK_EQUAL(s[7],'\x88');
  BOOST_CHECK_EQUAL(s.size(),8);
}
