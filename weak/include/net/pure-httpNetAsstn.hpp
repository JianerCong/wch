#pragma once

#include "pure-common.hpp"
#include "cnsss/pure-forCnsss.hpp"


// #include "pure-weakHttpClient.hpp"
#include "pure-greenHttpClient.hpp"
/*
  🐢 : Previously, we used WeakHttpClient for p2p which will send the request
  and close the connection. Here we have migrated to greenHttpClient, which will
  establisk long connections for p2p.
 */

#include "pure-httpCommon.hpp"
#include "pure-netAsstn.hpp"

namespace pure{

  class IPBasedHttpNetAsstn : public virtual IEndpointBasedNetworkable,
                              public NetAsstn
  {
  public:
    using postHandler_t = IHttpServable::postHandler_t;
    IHttpServable * const serv;
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
    IPBasedHttpNetAsstn(IHttpServable * const s,
                       IMsgManageable * const m): serv(s), NetAsstn(m){}

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

      optional<string> r = NetAsstn::extract_addr_and_port_from_endpoint(endpoint);
      if (not r){
        BOOST_LOG_TRIVIAL(debug) << format("\t❌️ Error parsing endpoint passed from Cnsss: " S_RED "%s" S_NOR )
          % endpoint;
        return {};
      }

      string addr_and_port = r.value();
      try {
        auto [addr, port] = NetAsstn::split_addr_port(addr_and_port);

        // BOOST_LOG_TRIVIAL(debug) << format("Sending to %s:%d, target=%s") % addr % port % (this->PREFIX + target);

        string msg = this->mgr->prepare_msg(move(data));

        // return weakHttpClient::post(addr,this->PREFIX + target,port,msg);
        return this->cln.post(addr,this->PREFIX + target,port,msg);
      }catch (const std::runtime_error & e){
        // BOOST_LOG_TRIVIAL(debug) << e.what();
        BOOST_LOG_TRIVIAL(error) << S_RED "❌️ Exception happened when sending HTTP request: " << e.what() << S_NOR;
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

        BOOST_LOG_TRIVIAL(debug) << format("\tmsg unpacked, from=" S_BLUE "%s" S_NOR ",data=" S_GREEN "%s" S_NOR) % from % get_data_for_log(data);

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

  };                            // class IPBasedHttpNetAsstn


}

