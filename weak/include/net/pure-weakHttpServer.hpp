#pragma once
#include <boost/asio.hpp>
using boost::asio::io_service;
using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::asio::buffer;
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
using boost::format;

#include <string>
using std::string;
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/config.hpp>

#include <boost/algorithm/string.hpp>
using boost::split;
using boost::is_any_of;
#include <vector>
using std::vector;
#include <optional>
using std::optional;
#include <string_view>
using std::string_view;

#include <cstdlib>
#include <iostream>
#include <thread>
#include <cstdio>

#include<tuple> // for tuple

#include <mutex>                // std::mutex

#include <unordered_map>
#include <functional>

// colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"


namespace pure {
  using std::unordered_map;
  using std::function;
  using std::tuple;
  using std::make_tuple;

  namespace beast = boost::beast;         // from <boost/beast.hpp>
  namespace http = beast::http;           // from <boost/beast/http.hpp>
  namespace asio = boost::asio;            // from <boost/asio.hpp>
  using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
  // using std::cout;
  using http::request;
  using http::response;

 /**
  * @brief The http server that all other languages have.
  *
  * What's a little different is that this server also expose three methods:
  *
  *    + removeFromPost(string key)
  *    + listenToPost(string k, postHandler_t f)
  *    + listenToGet(string k, getHandler_t f)
  *
  * This allows handlers for POST and GET to be added or removed during runtime
  * (sometime even by other handlers).
  */
  class WeakHttpServer {
  private:
    // {{{ private (implementation)

    tuple<bool,string> getGetResponseBody(string  target /*e.g. "/"*/ ,
                                          string clientAddr,uint16_t clientPort){

      // <! the function dispatch map, each entry accepts (string address, uint16_t
      // port). the key is the target such as "\hi", "\"

      BOOST_LOG_SEV(this->lg,debug) << format(" ❄ Handling GET\n\t"
                                         "target: %s \t client: %s:%d"
                                         ) % target % clientAddr % clientPort;

      optional<unordered_map<string,string>> r = WeakHttpServer::parse_query_param(target
                                                 /*target trimmed here*/);
      BOOST_LOG_TRIVIAL(debug) << format("Target trimmed to " S_CYAN "%s" S_NOR) % target;

      getHandler_t f;
      {
        std::unique_lock g(this->lockForGetLisnMap);
        if (getLisnMap.contains(target))
          f = getLisnMap[target]; // grab the handler
      } // unlocks here
      if (f)
        return f(clientAddr,clientPort,r);

      BOOST_LOG_SEV(this->lg,debug) << format("Unregistered GET method");
      return make_tuple(false,"NOT FOUND");
    }

    tuple<bool,string> getPostResponseBody(string  target /*e.g. "/"*/ ,
                                           string clientAddr,uint16_t clientPort, string_view body){


      BOOST_LOG_SEV(this->lg,debug) << format(" ❄ Handling POST\n\t"
                                         "target: %s \t client: %s:%d\n\tbody:>>>%s<<<"
                                         ) % target % clientAddr % clientPort % body;

      postHandler_t f;
      {
        std::unique_lock g(this->lockForPostLisnMap);
        if (postLisnMap.contains(target))
          f = postLisnMap[target]; // grab the handler
      } // unlock here

      if (f)
        return f(clientAddr,clientPort,body);

      BOOST_LOG_SEV(this->lg,debug) << format("Unregistered POST method");
      return make_tuple(false,"NOT FOUND");
    }

    inline void fail(beast::error_code ec, char const* what){
      std::cerr << what << ": " << ec.message() << "\n";
    }

    /**
     * @brief Here we actually dispatch the appropriate functions and handle the
     * request. We have entered the boost.beast world from the boost.asio world.
     *
     * @param req The request to handle
     * @param a The client address
     * @param p The client port
     */
    response<http::string_body>
    handle_request(request<http::string_body>&& req, string a, uint16_t p){
      // Returns a bad request response
      auto const bad_request =
        [&req](beast::string_view why){
          response<http::string_body> res{http::status::bad_request, req.version()};
          res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
          res.set(http::field::content_type, "text/html");
          res.keep_alive(req.keep_alive());
          res.body() = std::string(why);
          res.prepare_payload();
          return res;
        };

      // Returns a server error response
      auto const server_error =
        [&req](beast::string_view what, http::status r = http::status::internal_server_error){
          response<http::string_body> res{r,req.version()};
          res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
          res.set(http::field::content_type, "text/html");
          res.keep_alive(req.keep_alive());
          res.body() = "An error occurred: '" + std::string(what) + "'";
          res.prepare_payload();
          return res;
        };

      // 1.log --------------------------------------------------
      BOOST_LOG_SEV(this->lg,debug) << format("Start handling");
      BOOST_LOG_SEV(this->lg,debug) << format("Handling request\n\tmethod: %s\n\ttarget: %s\n\tcontent type: %s\n\tFrom: %s:%d")
        % req.method_string() % req.target() % req[http::field::content_type] % a % p;

      if (req.payload_size() and req.payload_size().value() > 0){
        BOOST_LOG_SEV(this->lg,debug) << format("payload_size:%d\n\tdata:%s")
          % req.payload_size().value() % req.body();
        // body should be string for http::string_body
      }

      // 2. make Response --------------------------------------------------
      response<http::string_body> res;

      // 3. dispatch --------------------------------------------------
      // if( req.method() != http::verb::get) &&
      //     req.method() != http::verb::post)
      //   return bad_request("Unknown HTTP-method");

      bool ok; string body;
      if (req.method() == http::verb::get){
        std::tie(ok,body) = getGetResponseBody(req.target(),a,p);
      }else if (req.method() == http::verb::post){
        std::tie(ok,body) = getPostResponseBody(req.target(),a,p,req.body());
      }else{
        return bad_request("Unknown method");
      }

      if (not ok){
        if (body == "NOT FOUND")
          return server_error((format("target >>%s<< is not found") % req.target()).str(),
                              http::status::not_found); // 404
        else
          return server_error(body);
      }

      // Got valid body
      BOOST_LOG_SEV(this->lg,debug) << format("Returning body " S_CYAN "%s" S_NOR) % body;
      res.body() = body;

      res.version(11);   // HTTP/1.1
      res.set(http::field::server, "Beast");
      res.result(http::status::ok);
      res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
      res.set(http::field::content_type, "application/json");
      res.keep_alive(req.keep_alive());

      res.prepare_payload();
      return res;
    }

    void do_session(tcp::socket & socket){

      // log the endpoint info
      boost::asio::ip::tcp::endpoint e1 = socket.remote_endpoint();
      boost::asio::ip::address a = e1.address();
      uint16_t p = e1.port();
      string s = a.to_string();

      BOOST_LOG_SEV(this->lg,debug) << format("Got connection from %s:%d")
        % s % p;

      // --------------------------------------------------
      // the mode
      beast::error_code ec;
      // This buffer is required to persist across reads
      beast::flat_buffer buffer;

      // Keep reading request and handle the request until END OF STREAM or result
      // in not keep_alive
      for(;;){
        // Read a request
        request<http::string_body> req; // the request container
        http::read(socket, buffer, req, ec);
        if(ec == http::error::end_of_stream) break;
        if(ec) return fail(ec, "read");

        // Handle request
        response<http::string_body> res =
          handle_request(std::move(req),s,p);

        // Determine if we should close the connection
        bool keep_alive = res.keep_alive();

        // Send the response
        beast::http::write(socket, std::move(res), ec);
        if(ec) return fail(ec, "write");


        if(not keep_alive){
          printf("We should not keep alive\n");
          // This means we should close the connection, usually because
          // the response indicated the "Connection: close" semantic.
          break;
        }
      }
      // Send a TCP shutdown
      socket.shutdown(tcp::socket::shutdown_send, ec);
      // At this point the connection is closed gracefully
      BOOST_LOG_SEV(this->lg,debug) << format("🐸 Server Connection closed " S_GREEN "gracefully" S_NOR);
    }
    // }}}

  public:
    // <! the function dispatch map, each entry accepts (string address, uint16_t
    // port, string body). the key is the target such as "\hi", "\"
    using postHandler_t = function<tuple<bool,string> (string, uint16_t, string_view)>;
    using getHandler_t = function<tuple<bool,string> (string, uint16_t,
      optional<unordered_map<string,string>> // the query param
      )>;
    using postMap_t = unordered_map<string,postHandler_t>;
    using getMap_t = unordered_map<string, getHandler_t>;
    using logger_t =
      boost::log::sources::severity_logger<boost::log::trivial::severity_level>;
    // ^^^ 🦜 : This is the simplest logger compatible with the trivial-log in Boost.Log

    using enum boost::log::trivial::severity_level;
    /*
      🦜 :import the enum, which import the name: trace,debug,...,error.
    */


    mutable std::mutex lockForPostLisnMap; // the mutex is mutable (m-m rule)
    mutable std::mutex lockForGetLisnMap; // the mutex is mutable (m-m rule)
    postMap_t postLisnMap;
    getMap_t getLisnMap;
    logger_t lg;

    std::atomic_flag closed = ATOMIC_FLAG_INIT; // false
    // The acceptor receives incoming connections
    boost::asio::io_service ioc;
    tcp::acceptor acceptor{ioc};
    vector<std::jthread> * all_threads;
    /*
      the threads to be waited in d'tor.

      🦜 : We use this, because we should (probably) never use detach...
     */

    void listenToGet(string k, getHandler_t f)noexcept{
      std::unique_lock g(this->lockForGetLisnMap);
      this->getLisnMap[k] = f;
    }

    int removeFromPost(string k) noexcept{
      std::unique_lock g(this->lockForPostLisnMap);
      return this->postLisnMap.erase(k);
    }

    // 🐢  : You ain't gonna need it (YAGN rule)
    // void clearPost()noexcept {
    //   std::unique_lock g(this->lockForPostLisnMap);
    //   this->postLisnMap.clear();
    // }

    void listenToPost(string k, postHandler_t f) noexcept{
      std::unique_lock g(this->lockForPostLisnMap);
      this->postLisnMap[k] = f;
    }

    WeakHttpServer(uint16_t portToListen = 7777,
                   logger_t l = {},
                   postMap_t pMap = {}, getMap_t gMap = {}
                   ):postLisnMap(pMap), getLisnMap(gMap),lg{l}{

      // start the thread pool
      this->all_threads = new vector<std::jthread>();

      boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), portToListen);

      acceptor.open(endpoint.protocol());
      acceptor.bind(endpoint);    // this throws exception if failed to bind port
      acceptor.listen();
      BOOST_LOG_SEV(this->lg,debug) << format("🐸 acceptor start listening on port %d")
        % portToListen;

      // The service run in one thread
      // BOOST_LOG_SEV(this->lg,debug) << format("This will go to hi.log");

      BOOST_LOG_SEV(this->lg,debug) << format("Starting service thread");

      this->all_threads->push_back(std::jthread{[&](){ioc.run();}});

      /*
        🦜 : We somewhat have to detach() this until we have mastered the magic of acceptor
       */
      std::thread{[&](){while(true){
            BOOST_LOG_SEV(this->lg,debug) << format("❄ Waiting for request");
            // This will receive the new connection
            tcp::socket socket{ioc};
            BOOST_LOG_SEV(this->lg,debug) << format("Start accepting");
            // Block until we get a connection
            acceptor.accept(socket);

            // 🦜 : doesn't work yet
            // bool accepted{false};
            // while ((not this->closed.test()) and (not accepted)){
            //   acceptor.async_accept(socket,[&accepted](const boost::system::error_code& error){
            //     if (!error){
            //         // Accept succeeded.
            //         accepted = true;
            //       }
            //   });
            // }
            // if (this->closed.test())
            //   break;

            BOOST_LOG_SEV(this->lg,debug) << format("Accepted");

            // Launch the session, transferring ownership of the socket
            // 🦜 : You have to do it this way
            this->all_threads->push_back(std::jthread{
                std::bind(&WeakHttpServer::do_session, this, std::move(socket))
              });
          }
          // BOOST_LOG_TRIVIAL(debug) << "TCP acceptor closed";
      }}.detach();
    }


    /*
      🦜 : With the help of boost::split we can parse the query param in a
      relatively simple way.
    */
    /**
     * @brief parse the query parameter
     *
     * @param s The target string to be parsed. For example "/aaa?a=1&b=2". The
     * input parameter will be trimmed to the actual target. For example, we have:
     *
     *    string s = "/abc?a=1&b=2";
     *    parse_query_param(s);
     *    BOOST_CHECK_EQUAL(s,"/abc");
     *
     * @return If the parsing is successful return the map containing parsed
     * query parameters.
     */
    static optional<unordered_map<string,string>> parse_query_param(string & s){
      string::size_type i = s.find_first_of('?');

      if (i == string::npos){
        BOOST_LOG_TRIVIAL(debug) << S_MAGENTA "No query param specified" S_NOR;
        return {};                  // no query param given
      }else if (i == (s.size() - 1)){
        BOOST_LOG_TRIVIAL(debug) << S_MAGENTA "'?' char appears at the last popped" S_NOR;
        s = s.substr(0,i); // 🦜 : modify s=s[:i]. ⚠️ Only do it before return
        return {};                  // no query param given
      }


      string_view qs{s.begin() + i + 1,s.end()};

      BOOST_LOG_TRIVIAL(debug) << format("query to parse: " S_CYAN "%s" S_NOR) % qs;

      vector<string> v;
      split(v,qs,is_any_of("=&"));

      if (v.size() % 2 != 0){
        BOOST_LOG_TRIVIAL(debug) << format( S_MAGENTA "mismatched size" S_NOR " of query param = "
                                           S_CYAN "%d" S_NOR ", which is not even")
          % v.size();
        s = s.substr(0,i);
      /*
        🦜 : modify s=s[:i]. ⚠️ Only do it just before returning. (There's a view to it)
       */
        return {};
      }

      unordered_map<string,string> m;
      for (auto it = v.begin(); it != v.end();){
        string k = *(it++);
        string v = *(it++);
        if (not m.insert({k,v}).second) [[unlikely]] {
          BOOST_LOG_TRIVIAL(debug) << format("Error inserting k=%s,v=%s") % k % v;
          s = s.substr(0,i); // 🦜 : modify s=s[:i]. ⚠️ Only do it before return
          return {};
        }
      }

      s = s.substr(0,i); // 🦜 : modify s=s[:i]. ⚠️ Only do it before return
      return m;
    }
    ~WeakHttpServer(){
      BOOST_LOG_SEV(this->lg,debug) << format("👋🐸 Server closed");
      ioc.stop();
      this->closed.test_and_set();
      delete this->all_threads;
      BOOST_LOG_SEV(this->lg,debug) << format("👋 All handler closed");
    }
  };


}
