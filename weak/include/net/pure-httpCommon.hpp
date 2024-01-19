
#pragma once
/**
 * @file pure-httpCommon.hpp
 * @brief Common things for http servers.
 */
// colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

#include <boost/format.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/config.hpp>
#include <boost/log/trivial.hpp>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <mutex>                // std::mutex
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>
#include<tuple> // for tuple


namespace pure{

  using boost::asio::buffer;
  using boost::asio::io_service;
  using boost::asio::ip::tcp;
  using boost::format;
  using boost::system::error_code;
  using std::function;
  using std::make_tuple;
  using std::optional;
  using std::tuple;
  using std::unordered_map;

  namespace beast = boost::beast;         // from <boost/beast.hpp>
  namespace http = beast::http;           // from <boost/beast/http.hpp>
  namespace asio = boost::asio;            // from <boost/asio.hpp>
  using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
  // using std::cout;
  using http::request;
  using http::response;
  using std::vector;
  using boost::split;
  using boost::is_any_of;
  using std::string_view;
  using std::string;

  /**
   * @brief The interface of an HTTP server
   */
  class IHttpServable{
  public:

    // <! the function dispatch map, each entry accepts (string address, uint16_t
    // port, string body). the key is the target such as "\hi", "\"
    // <! the function dispatch map, each entry accepts (string address, uint16_t
    // port, string body). the key is the target such as "\hi", "\"
    using postHandler_t = function<tuple<bool,string> (string, uint16_t, string_view)>;
    using getHandler_t = function<tuple<bool,string> (string, uint16_t,
      optional<unordered_map<string,string>> // the query param
      )>;
    using postMap_t = unordered_map<string,postHandler_t>;
    using getMap_t = unordered_map<string, getHandler_t>;

    virtual void listenToGet(string k, getHandler_t f)noexcept=0;
    virtual int removeFromPost(string k) noexcept=0;
    virtual void listenToPost(string k, postHandler_t f) noexcept=0;
  };                            // class IHttpServable

  /**
   * @brief Common code for WeakHttpServer and WeakAsyncHttpServer
   */
  class WeakHttpServerBase: public virtual IHttpServable{
  public:
    tuple<bool,string> getGetResponseBody(string  target /*e.g. "/"*/ ,
                                          string clientAddr,uint16_t clientPort){

      // <! the function dispatch map, each entry accepts (string address, uint16_t
      // port). the key is the target such as "\hi", "\"

      BOOST_LOG_TRIVIAL(debug) << format(" ❄ Handling GET\n\t"
                                         "target: %s \t client: %s:%d"
                                         ) % target % clientAddr % clientPort;

      optional<unordered_map<string,string>> r = WeakHttpServerBase::parse_query_param(target
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

      BOOST_LOG_TRIVIAL(debug) << format("Unregistered GET method");
      return make_tuple(false,"NOT FOUND");
    }

    tuple<bool,string> getPostResponseBody(string  target /*e.g. "/"*/ ,
                                           string clientAddr,uint16_t clientPort, string_view body){


      BOOST_LOG_TRIVIAL(debug) << format(" ❄ Handling POST\n\t"
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

      BOOST_LOG_TRIVIAL(debug) << format("Unregistered POST method");
      return make_tuple(false,"NOT FOUND");
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
      BOOST_LOG_TRIVIAL(debug) << format("Start handling");
      BOOST_LOG_TRIVIAL(debug) << format("Handling request\n\tmethod: %s\n\ttarget: %s\n\tcontent type: %s\n\tFrom: %s:%d")
        % req.method_string() % req.target() % req[http::field::content_type] % a % p;

      if (req.payload_size() and req.payload_size().value() > 0){
        BOOST_LOG_TRIVIAL(debug) << format("payload_size:%d\n\tdata:%s")
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
      BOOST_LOG_TRIVIAL(debug) << format("Returning body " S_CYAN "%s" S_NOR) % body;
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

    mutable std::mutex lockForPostLisnMap; // the mutex is mutable (m-m rule)
    mutable std::mutex lockForGetLisnMap; // the mutex is mutable (m-m rule)
    postMap_t postLisnMap;
    getMap_t getLisnMap;

    WeakHttpServerBase(postMap_t pMap = {}, getMap_t gMap = {}):
      postLisnMap(pMap), getLisnMap(gMap){}

    void listenToGet(string k, getHandler_t f)noexcept override{
      std::unique_lock g(this->lockForGetLisnMap);
      this->getLisnMap[k] = f;
    }

    int removeFromPost(string k) noexcept override{
      std::unique_lock g(this->lockForPostLisnMap);
      return this->postLisnMap.erase(k);
    }

    // 🐢  : You ain't gonna need it (YAGN rule)
    // void clearPost()noexcept {
    //   std::unique_lock g(this->lockForPostLisnMap);
    //   this->postLisnMap.clear();
    // }

    void listenToPost(string k, postHandler_t f) noexcept override{
      std::unique_lock g(this->lockForPostLisnMap);
      this->postLisnMap[k] = f;
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
  };                            // class WeakHttpBase

  /**
   * @brief A container for multiple http servers.
   *
   * 🦜 : These servers usually have different transport layers. For example,
   * tcp and unix-domain socket.
   *
   */
  class MultiStackHttpServer: public virtual IHttpServable{
  public:
    vector<IHttpServable*> srvs;
    MultiStackHttpServer(vector<IHttpServable*> ssrvs = {}): srvs(ssrvs){}

    void listenToGet(string k, getHandler_t f)noexcept override{
      for (auto & srv : srvs)
        srv->listenToGet(k,f);
    }

    void listenToPost(string k, postHandler_t f) noexcept override{
      for (auto & srv : srvs)
        srv->listenToPost(k,f);
    }

    int removeFromPost(string k) noexcept override{
      int n = 0;
      for (auto & srv : srvs)
        n += srv->removeFromPost(k);
      return n;
    }

  };
} // namespace pure
