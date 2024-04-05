/**
 * @file pure-greenHttpClient.hpp
 *
 * @brief  The http client that will try to reuse the previous connection. (green = reuse)
 *
 * 🐢 : Previously, our clients are just stateless functions. It
 *
 *    1. opens a connection to the server,
 *    2. sends the request,
 *    3. gets the response,
 *    4. and closed the connection.
 *
 *    The server is identified by the (Host,Port) pair, for example
 *    ("localhost",8080).
 *
 *    However, we can actually keep the connection open. So when it makes a
 *    request to, say, ("loalhost",8080) for the first time, it opens up a
 *    connection, say `c1`. And if it makes a request to the ("loalhost",8080)
 *    again, it will reuse `c1`.
 *
 * 🦜 : So this seems like the client has to be a stateful class instead of some functions.
 *
 * 🐢 : Yeah, and the dtor of class instance should close all the connections it remembers.
 *
 * 🦜 : So if it need to "remember" the connections, I guess it need a map<str,Conn> right?
 *
 * 🐢 : Yeah, so the actual request process will looks like:
 *
 *    string s = host + ':' + lexical_cast<string>(port);
 *
 *    shared_ptr<beast::tcp_stream> conn;
 *    if (this->conns.contains(s)
 *        and
 *        this->conns.at(s)->socket().is_open()){
 *      conn = this->conns[s];
 *     }else{
 *      optional<shared_ptr<beast::tcp_stream>> r = make_conns(host,port);
 *      if (not r)
 *        return (format("Error making conn with %s:%d") % host % port ).str();
 *      conn = r.value();
 *      // remember the conn
 *      this->conns[s] = conn;
 *     }
 *
 *    request_with_conn(conn);
 *
 *    Note that although we used shared_ptr<tcp_stream> here, the connection
 *    should not be accessed by concurrently by two threads.
 *
 * 🦜 : So I can't start two threads and post to (localhos:8080) at the same time right?
 *
 * 🐢 : For a single client, yes. But in fact you can create multiple instance
 * of GreenHttpClient.
 */


#pragma once
#include <boost/log/trivial.hpp>
#include <optional>
using std::optional;

#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <iostream>
#include <string>
#include <boost/format.hpp>
#include <unordered_map>
#include <memory>
#include <thread>
#include <string_view>

#include <boost/lexical_cast.hpp>


// colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

namespace pure {
  using boost::format;
  using std::string;

  using std::string_view;
  
  using boost::lexical_cast;
  namespace beast = boost::beast;     // from <boost/beast.hpp>
  namespace http = beast::http;       // from <boost/beast/http.hpp>
  using boost::asio::io_service;
  using boost::asio::ip::tcp;
  using boost::system::error_code;
  using boost::asio::buffer;
  using std::unordered_map;
  using std::make_shared;
  using std::shared_ptr;
  using beast::tcp_stream;
  class GreenHttpClient {
  private:
    template<bool is_post>
    optional<string> get_post_impl(string_view h, string_view t, uint16_t p, string_view b=""){
      optional<http::response<http::string_body>> ro = this->request<is_post>(h,t,p,b);

      if (not ro){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Error requesting %s: " S_MAGENTA "%s" S_NOR " to " S_MAGENTA "%s:%d" S_NOR)
          % (is_post ? "POST" : "GET")
          % t % h % p;
        return {};
      }

      auto r = ro.value();
      if (r.result_int() != 200){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Response got code %d (!=200)，Headers:\n")
          % r.result_int();
        for (auto it = r.cbegin(); it != r.cend(); it++ ){
          BOOST_LOG_TRIVIAL(debug) << format(S_MAGENTA "\t [%s]\t: [%s]" S_NOR)
            % it->name_string() %it->value();
        }
        BOOST_LOG_TRIVIAL(error) << format("Body:\n[" S_MAGENTA "%s" S_NOR "]")
          % r.body();
        return {};
      }
      return r.body();
    }

    /**
     * @brief Make a request
     *
     * 🦜 : This should be the function called by post() and get()
     */
        template<bool is_post>
        optional<http::response<http::string_body>> request(string_view host, string_view target,
                                                            uint16_t port, string_view body="", bool retry=false){
          BOOST_LOG_TRIVIAL(debug) <<  "🦜 : request called";
          string k = GreenHttpClient::combine_addr_port(host,port);
          shared_ptr<tcp_stream> conn;
          bool found{false};
          {
           std::unique_lock g(this->lock_for_conns);
           if (this->conns.contains(k)
               and
               this->conns.at(k)->socket().is_open()
               ){
             conn = this->conns.at(k);
             found = true;
             BOOST_LOG_TRIVIAL(debug) << format("📗️ Resuing existing connection with" S_CYAN " %s " S_NOR) % k;
           }
          } // unlocks here

          if (not found){
            BOOST_LOG_TRIVIAL(debug) << format("🌱 Making new connection with" S_CYAN " %s " S_NOR) % k;
            optional<shared_ptr<beast::tcp_stream>> r = make_conns(host,port);
            if (not r)
              return {};        // failed to make conn

            conn = std::move(r.value());
#if defined(_WIN32)
            // 🦜 : It seems like the above line has something wrong on
            // windows... I t causes writing to bad address x24
#else
            {
              std::unique_lock g(this->lock_for_conns);
              this->conns.insert_or_assign(k,conn);
              // copy
              // this->conns[k] = shared_ptr<tcp_stream>(conn);
            } // unlocks here
#endif
          }

          /* 🦜 : Now we should have an ok conn */
          BOOST_LOG_TRIVIAL(debug) << "\tPreparing request";
          auto req = this->prepare_request<is_post>(host, target, body);
          BOOST_LOG_TRIVIAL(debug) << "\tRequest prepared";

          try{
            auto r = GreenHttpClient::request_with_conn(conn,move(req));
            return r;
            // return {};
          }catch (const std::exception & e ){

            if (not retry){
              BOOST_LOG_TRIVIAL(error) << format("⚠️ [1st try:] Error sending request to " S_MAGENTA " %s: " S_NOR "\n%s\n") % k % e.what();
              BOOST_LOG_TRIVIAL(debug) << "\t " S_MAGENTA "Maybe the connection is closed, we try it again with a new conn." S_NOR;
              {
                // 🦜 : dump the conn (this should make a new one.)
                std::unique_lock g(this->lock_for_conns);

                BOOST_LOG_TRIVIAL(debug) << format("Closing conn: " S_CYAN "%s" S_NOR) % k;
                // Gracefully close the socket
                beast::error_code ec;
                this->conns.at(k)->socket().shutdown(tcp::socket::shutdown_both, ec);
                this->conns.erase(k);
                // not_connected happens sometimes
                // so don't bother reporting it.
                if(ec && ec != beast::errc::not_connected){
                  BOOST_LOG_TRIVIAL(warning) << format("⚠️ Warning: error closing connection with" S_MAGENTA "%s" S_NOR ) % k;
                }
              } // unlocks here
              return request<is_post>(host,target,port,body,true);
            }

            BOOST_LOG_TRIVIAL(error) << format("❌️ [2nd try:] Error sending request to " S_MAGENTA " %s: " S_NOR "\n%s\n") % k % e.what();
            return {};
          }
    }

    /**
     * @brief Prepare a HTTP request, called by request()
     *
     * @param host The target host, e.g. "localhost"
     *
     * @param target The target e.g. "/abc"
     *
     * @param body The payload
     *
     * @return the made request
     */
    template<bool is_post>
    http::request<http::string_body> prepare_request(string_view host, string_view target, string_view body = "") noexcept{
      BOOST_LOG_TRIVIAL(debug) << format("🌱 Preparing request with host=%s, target=%s, body size=%d")
        % host % target % body.length();
      // Set up an HTTP POST request message
      http::request<http::string_body> req;
      if (is_post){
        req.method(http::verb::post);
      }else{
        req.method(http::verb::get);
      }

      req.target(string(target));
      req.version(11);
      req.set(http::field::host, string(host));
      req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);


      if (is_post){
        req.body() = body;            // add the body
        req.prepare_payload(); // set content-length based on the body
        // 🦜 : No need to prepare payload for GET
      }
      BOOST_LOG_TRIVIAL(debug) << format("🐸 Request prepared");
      return req;
    }

    /**
     * @brief Make a request through a given connection.
     *
     * @param conn The connection through which request is made.
     *
     * @param req The request to send
     *
     * @return The result from the server.
     *
     * 🦜 : Most of the code below is copied from weakHttpClient.hpp
     */
    static http::response<http::string_body> request_with_conn(shared_ptr<tcp_stream> conn,
                                                               http::request<http::string_body> && req){
      BOOST_LOG_TRIVIAL(debug) << format("Requesting with conn");

      // Send the HTTP request to the remote host
      // BOOST_LOG_TRIVIAL(debug) << format("Write");
      http::write(*conn, req);

      // This buffer is used for reading and must be persisted
      beast::flat_buffer buffer;
      // Declare a container to hold the response
      http::response<http::string_body> res;
      // Receive the HTTP response
      // BOOST_LOG_TRIVIAL(debug) << format("Read");
      http::read(*conn, buffer, res);

      return res;
    }


    /**
     * @brief Make a conn using the ioc.
     *
     * 🦜 : This is used by request()
     */
    optional<shared_ptr<beast::tcp_stream>> make_conns(string_view host, uint16_t port){
      shared_ptr<tcp_stream> conn;
      BOOST_LOG_TRIVIAL(debug) << format("⚙️ Making connection with " S_CYAN "%s:%d" S_NOR) % host % port;
      try{
        tcp::resolver resolver(this->ioc);
        conn = make_shared<tcp_stream>(this->ioc);
        // Look up the domain name
        auto const results = resolver.resolve(host, lexical_cast<string>(port));
        // Make the connection on the IP address we get from a lookup
        conn->connect(results);
      }catch( const std::exception & e){
        BOOST_LOG_TRIVIAL(error) << format("❌️ Error making conn with %s:%d: " S_RED "\n%s" S_NOR)
          % host % port % e.what();
        return {};
      }
      return conn;
    }
  public:
    unordered_map<string,shared_ptr<beast::tcp_stream>> conns;
    mutable std::mutex lock_for_conns; // the mutex is mutable (m-m rule)
    boost::asio::io_service ioc;

    GreenHttpClient(){
      BOOST_LOG_TRIVIAL(debug) << format("GreenHttpClient started.");
      std::thread{[&](){ioc.run();}}.detach(); // grab a thread.
    }

    ~GreenHttpClient(){
      BOOST_LOG_TRIVIAL(info) << format("Closing " S_CYAN " GreenHttpClient" S_NOR);
      {
        std::unique_lock g(this->lock_for_conns);
        BOOST_LOG_TRIVIAL(debug) << format("Start closing conns");
        for (auto & [k,conn] : this->conns){
          BOOST_LOG_TRIVIAL(debug) << format("Closing conn: " S_CYAN "%s" S_NOR) % k;
          // Gracefully close the socket
          beast::error_code ec;
          conn->socket().shutdown(tcp::socket::shutdown_both, ec);
          // not_connected happens sometimes
          // so don't bother reporting it.
          if(ec && ec != beast::errc::not_connected){
            BOOST_LOG_TRIVIAL(warning) << format("❌️ Warning: error closing connection with" S_MAGENTA "%s" S_NOR ) % k;
          }

          BOOST_LOG_TRIVIAL(info) << format("👋 Client Connection " S_CYAN "%s" S_NOR" closed" S_GREEN " gracefully." S_NOR) % k;
        }
      }// unlock here
      // BOOST_LOG_TRIVIAL(debug) <<  "\tStopping ioc in GreenHttpClient";
      this->ioc.stop();
      // BOOST_LOG_TRIVIAL(debug) <<  "\tStopped ioc in GreenHttpClient";
    }


    /**
     * @brief get for dummy
     * @param h Host, such as "localhost"
     * @param t Target, such as "/"
     * @param p Port, such as 8080
     * @return the body if everything went well.
     */
    optional<string> get(string h, string t, uint16_t p){
      return this->get_post_impl<false>(h,t,p);
    }

    /**
     * @brief post for dummy
     * @param h Host, such as "localhost"
     * @param t Target, such as "/"
     * @param p Port, such as 8080
     * @param b body, such as "{\"a\":123}"
     * @return the body if everything went well.
     */
    optional<string> post(string_view h, string_view t, uint16_t p, string_view b) noexcept {
      // return {};
      return this->get_post_impl<true>(h,t,p,b);
    }

    // 🦜 : copied from pure-httpNetAsstn.hpp
    static string combine_addr_port(string_view addr,uint16_t port){
      return string(addr) + ':' + lexical_cast<string>(port);
      // return (format("%s:%d") % addr % port).str();
    }
  };
}
