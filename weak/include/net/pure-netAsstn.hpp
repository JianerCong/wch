/**
 * @file pure-netAsstn.hpp
 * @author Jianer Cong
 * @brief Common things for net asstns. Such as signed data
 */
#pragma once
#include "cnsss/pure-forCnsss.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

#include <bit>

#include <tuple>
namespace pure{
  using std::bit_cast;
  using std::tuple;
  using std::make_tuple;
  using std::vector;
  using boost::lexical_cast;
  using boost::algorithm::split;
  using boost::lockfree::spsc_queue;


  /**
   * @brief Interface for a message manager.
   *
   * The implementer is supposed to help IPBasedHttpNetAsstn managing the data
   * input and output.
   */
  class IMsgManageable{
  public:
    /**
     * @brief Prepare a msg to be sent.
     * @param data The data to be sent.
     * @return A signed msg, which should contains the following:
     *     1. who sent the data
     *     2. a digital signature
     *     3. the payload (the actual data)
     *
     * The returned string is supposed to be passed to the remote host's
     * `tear_msg_open()` function.
     *
     * 🦜 : Note that in order to prepare the data, the implementer should knows
     * "Who am I" (i.e. the identity of `this host`, usually the public key of this node)
     */
    virtual string prepare_msg(string && data)const noexcept =0;
    /**
     * @brief Tear a msg open.
     *
     * @param msg The message prepared by the sender, usually prepared by a
     * `prepare_msg()` call.
     * @return The pair <from,data> if the signature verification is passed.
     *
     * This function should unpack the `msg` recieved, verify the digital
     * signature therein and return the `sender` and `data` if the verification is
     * passed.
     */
    virtual optional<tuple<string,string>> tear_msg_open(string_view msg /*throw a string in */
                                                         )const noexcept =0;
    virtual string my_endpoint()const noexcept=0;
  };                              // class IMsgManageable

  /**
   * @brief The signed data.
   *
   * This class wraps three string:
   *
   *     1. `from`, identifying the sender, usually a public key. This should have
   *     length >= 1.
   *
   *     2. `sig`, the signature, can be empty.
   *     3. `data`, the payload, can be empty.
   *
   * 🦜 : Because the structure of this class is simple enough, we parse and
   *     serialize ourselves. We use the following encoding. Suppose `s` is the
   *     serialized string, then
   *
   *     s = <from.size()-1><from>[<sig.size()><sig><data>]
   *
   *     In detail:
   *         + s[0] = from.size() - 1
   *         + s[1:1+from.size()] = from
   *         + s[1+from.size()] = sig.size()
   *         + s[2+from.size(),sig.size()] = sig
   *         + the rest is the data.
   *     Some examples:
   *
   *         + (from='a',sig='',data='') ⇒ '\x00' + 'a'
   *         + (from='a',sig='b',data='') ⇒ '\0x00' + 'a' + '\x01' + 'b'
   *         + (from='aa',sig='',data='bbb') ⇒ '\0x01' + 'aa' + '\0x00' + 'bbb'
   *         + (from='aa',sig='b',data='ccc') ⇒ '\0x01' + 'aa' + '\0x01' + 'b' + 'ccc'
   */

  class SignedData : public virtual ISerializable{
  public:
    SignedData() = default;
    SignedData( string f, string s="",string d=""): from(f),sig(s),data(d){
      if (f.empty())
        BOOST_THROW_EXCEPTION(std::runtime_error("SignedData.from cannot be empty."));
    }
    string from;
    string sig;
    string data;

    /**
     * @brief Parse the data from a string.
     *
     * This expects that the instance is just initialized. Example usage:
     *
     *     SignedData d;
     *     d.fromString(s);
     */
    bool fromString(string_view s) noexcept override{
      auto r = parse_3_strs(s);
      if (not r) return false;
      std::tie(this->from,this->sig,this->data) = r.value();
      return true;

    }

    string toString() const noexcept override{
      return serialize_3_strs(this->from, this->sig,this->data);
    }

    /**
     * @brief Parse three string.
     *
     * @param s The string that is to be parsed. It should have been serialized
     *  using `serialize_3_strs()`
     *
     * @return the parsed three string if successful.
     *
     *  🦜 : The core logic of this type is in fact parsing and serializing
     *  three strings. We can draw them out.
     */
    static optional<tuple<string,string,string>> parse_3_strs(string_view s){
      string s1,s2,s3;

      // BOOST_LOG_TRIVIAL(debug) << format("fromString() started");
      if (s.size() < 2) return {};
      // #include <bit>

      // BOOST_LOG_TRIVIAL(debug) << format("fromString() started to parse");
      // BOOST_LOG_TRIVIAL(debug) << format("started s=%s") % s;

      string_view::iterator it = s.begin();
      uint8_t l1 = bit_cast<uint8_t>(*(it++)) + 1; // f.size()
      // BOOST_LOG_TRIVIAL(debug) << format("l1 = %d") % static_cast<int>(l1);

      // s1.clear();
      s1.reserve(l1);

      for (int i = 0;i < l1;i++){
        s1 += *(it++);
        // BOOST_LOG_TRIVIAL(debug) << format("Now from=%s,i=%d") % s1 % i;
      }

      if (it == s.end()){
        // BOOST_LOG_TRIVIAL(trace) << "No more data to parse";
        return make_tuple(s1,s2,s3);
      }

      uint8_t l2 = bit_cast<uint8_t>(*(it++));
      s2.reserve(l2);
      for (int i = 0;i < l2;i++){
        s2 += *(it++);
        // BOOST_LOG_TRIVIAL(debug) << format("Now sig=%s,i=%d") % this->sig % i;
      }

      if (it == s.end()) return make_tuple(s1,s2,s3);  // no more data to parse
      s3.reserve(s.size() - l1 - l2);
      while (it != s.end()){
        s3 += *(it++);
      }

      return make_tuple(s1,s2,s3);  // no more data to parse
    }
    static string serialize_3_strs(string_view s1, string_view s2,string_view s3) noexcept {
      string s;
      s.reserve(2 + s1.size() +
                s2.size() +
                s3.size());

      s += bit_cast<char>(static_cast<uint8_t>(s1.size() - 1));
      s += s1;

      if (s2.empty() and s3.empty()){
        return s;
      } // 🦜 We can stop here only if both `sig` and `data` are empty.
      s += bit_cast<char>(static_cast<uint8_t>(s2.size()));
      s += s2;
      s += s3;
      return s;
    }
  };                            // class SignedData


  /**
   * @brief The message manager that does not sign at all.
   */
  class NaiveMsgMgr: public virtual IMsgManageable{
  public:
    const string id;

    NaiveMsgMgr(string iid): id(iid){}
    string prepare_msg(string && data)const noexcept override{
      return SignedData(this->id,"",data).toString();
    }

    optional<tuple<string,string>> tear_msg_open(string_view msg)const noexcept override {
      // BOOST_LOG_TRIVIAL(debug) << format("tearing msg open");
      SignedData d;
      if (not d.fromString(msg))
        return {};
      return make_tuple(d.from,d.data);
    }

    string my_endpoint()const noexcept override{
      return this->id;
    }
  };                            // class NaiveMsgMgr

  /**
   * @brief The base class for network Asstn.
   *
   * 🦜 : This one is created because IPBasedHttpNetAsstn and IPBasedUdpNetAsstn
   * shared these code.
   */
  class NetAsstn : public virtual IForCnsssNetworkable{
  public:
    IMsgManageable * const mgr;
    NetAsstn(IMsgManageable * const m): mgr(m){}

    string listened_endpoint() noexcept override{
      return this->mgr->my_endpoint();
    }

    static tuple<string,uint16_t> split_addr_port(string s){
      using boost::algorithm::split;
      using boost::is_any_of;
      vector<string> r;
      split(r,s,is_any_of(":"));
      if (r.size() != 2)
        BOOST_THROW_EXCEPTION(std::runtime_error(format("Error splitting <addr>:<port>").str()));
      return make_tuple(r[0],lexical_cast<uint16_t>(r[1]));
    }

    optional<string> extract_addr_and_port_from_endpoint(const string & endpoint){
      /*
        🦜 : parse the endpoint, which should have the form
        "<pk><addr:port><"">". It should be created from something like:

        string pk,e, endpoint;
        pk = my_public_key();
        e = IPBasedHttpNetAsstn::combine_addr_port("my_endpoint",1234);
        endpoint = SignedData.serialize_3_strs(pk,e,"");

        🐢 : Note that, in this function, we ignored the "pk"
      */

      optional<tuple<string,string,string>> r = SignedData::parse_3_strs(endpoint);
      if (not r){
        return {};
      }

      string addr_and_port;
      std::tie(std::ignore, /*pk is ignored*/
               addr_and_port,/*<addr>:<port>*/
               std::ignore /*should be empty*/) = r.value();

      return addr_and_port;
}

    /*
      🦜 : It looks like this function is unused?

      🐢 : Or I think it should be used on the client side...

     */
    static string combine_addr_port(string addr,uint16_t port){
      return addr + ':' + boost::lexical_cast<std::string>(port);
      // return (format("%s:%d") % addr % port).str();
    }
  };

} // namespace pure

