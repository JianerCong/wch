/**
 * @brief The udp-based network for cnsss.
 *
 * 🐢 : For some consensuses (e.g. *BFT), the p2p communication is based on an
 * asynchronous model. That is, one node just a msg to another node, without
 * waiting for a response. This is especially important when there are random
 * nodes (byzantine nodes), because usually, they don't send response in time if
 * at all.
 *
 * So, HTTP request for this kind of communication is somewhat wasteful, because
 * the response is ignored. But, UDP suits this situation very well. And so here
 * is our udp-based IAsyncEndpointBasedNetworkable.
 *
 * It's pretty similar in that it will hire a msg manager to sign and unsign the
 * outgoing and incomming msg.
 *
 * However, In contrast to the HTTP one, this one will just start an UDP server
 * and manage it itself.
 *
 * 🦜 : Oh, because our UDP server is not shared right?
 *
 * 🐢 : Yeah.
 *
 * 🦜 : Oh, it looks like we have another benefit. We don't need to remember
 * what listeners we have listened anymore, because we can just clear all the
 * handler in the server at once.
 *
 * 🐢 : True...
 */

#pragma once

#include "pure-common.hpp"
#include "cnsss/pure-forCnsss.hpp"

#include "pure-weakUdpServer.hpp"
#include "pure-weakUdpClient.hpp"
#include "pure-netAsstn.hpp"

namespace pure{
  class IPBasedUdpNetAsstn :
    public virtual IAsyncEndpointBasedNetworkable,
    public NetAsstn
  {
  public:
    using handler_t = WeakUdpServer::handler_t;
    WeakUdpServer * const serv;
    /**
     * @brief Construct an IPBasedUdpNetAsstn
     *
     * @param port The UDP port to be listened
     *
     * @param m The msg manager.
     */
    IPBasedUdpNetAsstn(const uint16_t port, IMsgManageable* const m): NetAsstn(m),
                                                                      serv(new WeakUdpServer(port))
    {}

    void clear()noexcept override{
      this->serv->clear();
    }
    void listen(string target,function<void(string,string)> f) noexcept override{
      handler_t h = this->make_handler(f);
      // BOOST_LOG_TRIVIAL(debug) << format("Adding listener %s") % target;
      this->serv->listen(target,h);
    }

    void send(string endpoint, string target,string data) noexcept override{
      /*
        🦜 : parse the endpoint, which should have the form
        "<pk><addr:port><"">". It should be created from something like:

        string pk,e, endpoint;
        pk = my_public_key();
        e = IPBasedHttpNetAsstn::combine_addr_port("my_endpoint",1234);
        endpoint = SignedData.serialize_3_strs(pk,e,"");

        🐢 : Note that, in this function, we ignored the "pk"
      */

      optional<string> r = NetAsstn::extract_addr_and_port_from_endpoint(endpoint);
      if (not r){
        BOOST_LOG_TRIVIAL(debug) << format("\t❌️ Error parsing endpoint passed from Cnsss: " S_RED "%s" S_NOR )
          % endpoint;
        return;
      }
      string addr_and_port = r.value();
      try{
        auto [addr, port] = NetAsstn::split_addr_port(addr_and_port);
        // BOOST_LOG_TRIVIAL(debug) << format("Sending UDP to %s:%d, target=%s") % addr % port % (target);
        string msg = this->mgr->prepare_msg(move(data));
        BOOST_LOG_TRIVIAL(debug) << format("Sending UDP to %s:%d, msg=%s") % addr % port % msg;

        /*
          🐢 : ⚠️ : Here we need to prepand the <target>: in the payload. The
          server side will receive this, and pass the msg to the handler
          selected by <target>. This is different from the http counterpart.
         */
        WeakUdpClient::send(addr,lexical_cast<string>(port), target + ':' + msg);
      }
      catch (const std::exception & e){
        BOOST_LOG_TRIVIAL(error) << S_RED "❌️ Something wrong happened when preparing UDP msg: " << e.what() << S_NOR << " But igore it";
        return;
      }
    }

    handler_t make_handler(function<void(string,string)> f) noexcept{
      return [f,this](string_view msg){
        // BOOST_LOG_TRIVIAL(debug) << format(" post hander called with msg=%s") % msg;

        optional<tuple<string,string>> r0 = this->mgr->tear_msg_open(msg);// "supposed to verify signature here."

        if (not r0){
          BOOST_LOG_TRIVIAL(error) << S_RED
            "❌️ Error unpacking msg. Maybe the msg is ill-formed or"
            "or the signature verification is failed." S_NOR "(Ignoring it)"
            ;
          return ;
        }

        const auto [from,data] = r0.value();
        BOOST_LOG_TRIVIAL(debug) << format("\tmsg unpacked, from=" S_BLUE "%s" S_NOR ",data=" S_GREEN "%s" S_NOR) % from % data;

        /*🦜 : Do we need to launch a thread to handle it ?

          🐢 No. I think the UDP server would already have called this handler in a seperate 'handler thread'.

          🦜 : Oh, so we need to make sure that this->mgr->tear_msg_open is thread-safe right ?

          🐢 : Yeah.

          🦜 : In short, we can just call:
        */
        f(from,data);
      };
    }

    ~IPBasedUdpNetAsstn(){
      BOOST_LOG_TRIVIAL(debug) << "\t👋 " S_MAGENTA " IPBasedUdpNetAsstn" S_NOR " closing";
      delete this->serv;        // 🦜 : this should wait for the threads in it.
    }
  };
}
