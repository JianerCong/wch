#pragma once
#include "cnsss/pure-forCnsss.hpp"
// #include "pure-weakHttpClient.hpp"
#include "pure-greenHttpClient.hpp"
/*
  🐢 : Previously, we used WeakHttpClient for p2p which will send the request
  and close the connection. Here we have migrated to greenHttpClient, which will
  establisk long connections for p2p.
 */
#include "pure-weakHttpServer.hpp"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/lockfree/spsc_queue.hpp>
#include <vector>

#include <bit>

#include <tuple>
#include "pure-common.hpp"

namespace pure{

  using std::bit_cast;
  using std::tuple;
  using std::make_tuple;
  using std::vector;
  using boost::lexical_cast;
  using boost::algorithm::split;
  using boost::lockfree::spsc_queue;


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
  };



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
};




  class IPBasedHttpNetAsstn : public virtual IEndpointBasedNetworkable{
  public:
    using postHandler_t = WeakHttpServer::postHandler_t;
    WeakHttpServer * const serv;
    IMsgManageable * const mgr;
    GreenHttpClient cln;
    const string PREFIX{"/p2p"};

    /**
     * @brief The http based endpoint network that a consensus can use.
     * @param s The http server
     * @param e The listened endpoint in the form "<addr>:port".
     *
     * 🐢 : `e` is supposed to be the address that other nodes can use to
     * contact you.
     *
     * 🦜 : Well, but usually we don't known "what address" they will contact us
     * through right? Usually we just bind to something like 0.0.0.0:7777, and
     * other nodes can do something like
     *    $ curl 10.0.0.2:7777
     * If 10.0.0.2 is the address of one of our network interface.
     *
     * 🐢 : Yeah, that's true, we usually have multiple network interface, so
     * different hosts can usually contact us through different port.But I think
     * this string is just used for debugging purpose. If in case the endpoint
     * of a particular host should be remembered, the first argument of handler
     * function should be used.
     */
    IPBasedHttpNetAsstn(WeakHttpServer * const s,
                       IMsgManageable * const m): serv(s), mgr(m){}

    ~IPBasedHttpNetAsstn(){
      BOOST_LOG_TRIVIAL(debug) << format("👋 " S_MAGENTA " IPBasedHttpNetAsstn " S_NOR " closing");
      if (this->serv != nullptr)
        this->clear();
    }

    spsc_queue<string, boost::lockfree::capacity<32>> q;
    /*
      🐢 : For now, we set the buffer to 32. This is just a list of all the
      listened target. We need to remember that because we need to clean up
      ourselves.
     */

    void listen(const string target,
                function<optional<string>(string,string)>
                handler) noexcept override {

      if (not this->q.push(target))
        BOOST_THROW_EXCEPTION(std::runtime_error(format("Stack Overflow in ringbuffuer of listener.\n"
                                                        "Probably you forgot to clear() the listeners "
                                                        "before adding new ones?").str()));

      postHandler_t f = this->make_post_handler(handler);
      BOOST_LOG_TRIVIAL(debug) << format("Adding listener %s") % (this->PREFIX + target);
      this->serv->listenToPost(this->PREFIX + target,f);
    }

    string listened_endpoint() noexcept override{
      return this->mgr->my_endpoint();
    }

    optional<string> send(string endpoint,
                          string target,
                          string data)noexcept override{

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
        BOOST_LOG_TRIVIAL(debug) << format("\t❌️ Error parsing endpoint passed from Cnsss: " S_RED "%s" S_NOR )
          % endpoint;
        return {};
      }

      string addr_and_port;
      std::tie(std::ignore, /*pk is ignored*/
               addr_and_port,/*<addr>:<port>*/
               std::ignore /*should be empty*/) = r.value();
      try {
        auto [addr, port] = split_addr_port(addr_and_port);

        // BOOST_LOG_TRIVIAL(debug) << format("Sending to %s:%d, target=%s") % addr % port % (this->PREFIX + target);

        string msg = this->mgr->prepare_msg(move(data));

        // return weakHttpClient::post(addr,this->PREFIX + target,port,msg);
        return this->cln.post(addr,this->PREFIX + target,port,msg);
      }catch (const std::runtime_error & e){
        BOOST_LOG_TRIVIAL(debug) << e.what();
        return {};
      }

    }

    void clear() noexcept override {
      string s;
      while (this->q.pop(s)){
        BOOST_LOG_TRIVIAL(debug) << format("\tClearing listener %s") % (this->PREFIX + s);
        this->serv->removeFromPost(this->PREFIX + s);
      }
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

    /**
     * @brief Make a postHandler_t around the "cnsss handler"
     *
     * @param h The cnsss handler, this accept (from,data) and returns an optional string
     */
    postHandler_t make_post_handler(function<optional<string>(string,string)> h) noexcept {
      return [h,this](string /*addr*/, uint16_t /*port*/,string_view msg) -> tuple<bool,string> {
        // BOOST_LOG_TRIVIAL(debug) << format(" post hander called with msg=%s") % msg;
        /* 🦜 : Ignore the addr and port. They represent random endpoint that
           the sender uses. The identity of the sender is contained in `body`
         */
        optional<tuple<string,string>> r0 = this->mgr->tear_msg_open(msg);// "supposed to verify signature here."
        if (not r0)
          return make_tuple(false,"❌️ Error unpacking msg. Maybe the msg is ill-formed or"
                            "or the signature verification is failed.");
        const auto [from,data] = r0.value();

        BOOST_LOG_TRIVIAL(debug) << format("\tmsg unpacked, from=" S_BLUE "%s" S_NOR ",data=" S_GREEN "%s" S_NOR) % from % data;

        optional<string> r = h(from,data);
        /*
             🐢 : Data copied here from string_view into string, because cnsss
             may modify the data and resend it.
         */

        if (r)
          return make_tuple(true,r.value());
        else
          return make_tuple(false, "Error executing handler.");
      };
    }

    static string combine_addr_port(string addr,uint16_t port){
      return addr + ':' + boost::lexical_cast<std::string>(port);
      // return (format("%s:%d") % addr % port).str();
    }
  };



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
  };

}
