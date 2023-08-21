#pragma once
#include <vector>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include <boost/lexical_cast.hpp>
#include <memory>
#include <functional>
#include <unordered_map>
#include <tuple>
#include <optional>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <string>
#include <string_view>
#include <atomic>
#include <thread>

namespace pure{
  using std::string;
  using std::string_view;
  using boost::format;
  using boost::asio::io_service;
  using boost::asio::ip::tcp;
  using boost::system::error_code;
  using boost::asio::buffer;


  using std::vector;
  using std::move;


  namespace asio = boost::asio;            // from <boost/asio.hpp>
  using boost::asio::ip::udp;

  /**
   * @brief A naive Udp server.
   *
   * 🐢 : This Udp server doesn't send any response. It's expected to dispatch handlers
   * for the incoming datagram. The datagram is expected to have the following form:
   *
   *     <target>:<data>
   *
   * where <target> should be something like "/aaa" and <data> is passed to the
   * handler.
   *
   * If the target is not listened, then nothing will happen. (🐢 : Okay, probably
   * log the complain a little, but won't do anything useful.)
   */
  using std::unique_ptr;
  using std::make_unique;

  using std::function;
  using std::unordered_map;

  // colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

  using std::tuple;

  using boost::lexical_cast;
  using std::optional;


  class WeakUdpServer{
  public:
    using handler_t = function<void(string_view)>;
    unordered_map<string,handler_t> lisn_map;

    boost::asio::io_context ioc;
    std::atomic_flag closed = ATOMIC_FLAG_INIT; // false
    unique_ptr<udp::socket> socket;
    std::mutex lock_for_lisn_map;
    std::thread sess;

    vector<std::jthread> * req_handlers;

    WeakUdpServer(uint16_t port=7777){
      this->socket = make_unique<udp::socket>(this->ioc, udp::endpoint(udp::v4(), port));
      BOOST_LOG_TRIVIAL(debug) << format("🌐️ Listening on UDP port " S_CYAN "%d" S_NOR) % port;

      this->req_handlers = new vector<std::jthread>();

      this->sess = std::thread{std::bind(&WeakUdpServer::do_session, this)};
    }


    int remove(string k) noexcept{
      std::unique_lock g(this->lock_for_lisn_map);
      return this->lisn_map.erase(k);
    }

    void clear() noexcept{
      std::unique_lock g(this->lock_for_lisn_map);
      return this->lisn_map.clear();
    }

    void listen(string k, handler_t f) noexcept{
      BOOST_LOG_TRIVIAL(debug) <<  "Adding handler " S_GREEN << k << S_NOR;
      {
        std::unique_lock g(this->lock_for_lisn_map);
        this->lisn_map[k] = f;
}
      BOOST_LOG_TRIVIAL(debug) <<  "Handler" S_GREEN << k << S_NOR << " added";
    }

    /*

      🦜 : It looks like there are something magical that wreak havoc when passing
      around socket. So unique_ptr<> should just help
    */

    /**
     * @brief Split the string into two.
     *
     * @param s The string to split
     */
    static optional<tuple<string_view,string_view>> split_first(string_view s, char c = ':'){
      string::size_type pos = s.find_first_of(c);
      if (pos == string::npos)
        return {};

      return make_tuple(string_view(s.begin(),s.begin()+pos),
                        string_view(s.begin()+pos+1,s.end()));
    }

    void handle_req(vector<char> & data){
      string_view s(data.data(), data.size());
      BOOST_LOG_TRIVIAL(debug) << format("Handling data: %s") % s;

      auto r = WeakUdpServer::split_first(s);
      if (not r){
        BOOST_LOG_TRIVIAL(debug) << format("❌️ Invalid datagram format:" S_RED " %s" S_NOR)
          % s;
        return ;
      }

      handler_t f;
      auto [t,d] = r.value();
      {
        std::unique_lock g(this->lock_for_lisn_map);
        string k{t};              // string_view ⇒ string
        if (not this->lisn_map.contains(k)){
          BOOST_LOG_TRIVIAL(error) << format("\t ❌️: unknown target " S_RED "%s" S_NOR ", Do nothing.")
            % k;
          return ;
        }
        f = this->lisn_map.at(k);
      } // unlocks here

      /*
        🐢  : Launch the function.

        🦜 : Do we need to start a new thread for it?

        🐢 : I think there's no need to do that because `handle_req()` is already
        launched in a separate thread? 

      */
      f(d);
    }

    void do_session(){
      BOOST_LOG_TRIVIAL(debug) << format("🐸 Server started");
      while (not this->closed.test()){
        /*
          🦜 : How do we know the request_size?
        */

        while (not this->closed.test() and this->socket->available() == 0){} // blk until we get some

        if (this->socket->available() == 0)
          break; // server closed

        std::size_t bytes_readable = this->socket->available();

        BOOST_LOG_TRIVIAL(debug) << format("Got %d byte") % bytes_readable;
        std::vector<char> data(bytes_readable); // allocate the data

        udp::endpoint remote_endpoint;
        boost::system::error_code ignored_error;
        this->socket->receive_from(boost::asio::buffer(data), remote_endpoint,0,ignored_error);

        BOOST_LOG_TRIVIAL(debug) << format("From = " S_CYAN "%s:%d" S_NOR) % remote_endpoint.address().to_string()
          % remote_endpoint.port(); // see the data

        // handle it
        this->req_handlers->push_back(std::jthread{std::bind(&WeakUdpServer::handle_req, this, move(data))});
      }

      BOOST_LOG_TRIVIAL(debug) << S_MAGENTA "\t 🕒 Waiting for req handlers to finish." S_NOR;
      delete this->req_handlers; // delete the req_handlers, all req_handlers will join here.
      BOOST_LOG_TRIVIAL(debug) << S_MAGENTA "\t 👋 All req handlers ended." S_NOR;
    }

    ~WeakUdpServer(){
      this->closed.test_and_set();
      this->sess.join();
      BOOST_LOG_TRIVIAL(debug) << format(S_GREEN "👋 Udp server closed" S_NOR);
    }
  };
} // namespace pure
