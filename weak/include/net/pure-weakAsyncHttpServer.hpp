/**
 * @brief The async http server.
 *
 * 🦜 : This async server will ask boost::asio to do some thread management
 * magic for us, so we don't need to lauch thread by ourselves. (So it's pretty
 * much a coroutine service managed by boost::asio.). Hopefully this can do a
 * better job than us when we close the server.
 *
 * The reason why we created this in preference to WeakHttpServer is that: we
 * kinda have to use thread::detach() for each request, and that is not very
 * healthy and often goes wrong when closing the server.
 *
 * [Update on 2024-01-19] 🦜 : We add the unix-domain-socket support. So now
 * there're two usable classes `WeakAsyncUnixHttpServer` and
 * `WeakAsyncTcpHttpServer`, and they are supposed to be stacked using
 * `MultiStackHttpServer`.
 */
#pragma once


#include "pure-httpCommon.hpp"
// #include <boost/beast/core.hpp>
// #include <boost/beast/http.hpp>
// #include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <filesystem>

// 🦜 : if unix, we try to add the unix_domain_socket
#ifdef __unix__
#include <boost/asio/local/stream_protocol.hpp>
#endif
namespace pure{

  // --------------------------------------------------
  // using stream_t = beast::basic_stream<unix_domain::stream_protocol>;
  // using socket_t = unix_domain::stream_protocol::socket;
  // using acceptor_t = unix_domain::stream_protocol::acceptor;
  // using endpoint_t = unix_domain::stream_protocol::endpoint;

  template <class stream_t,
            class socket_t,
            class acceptor_t,
            class endpoint_t
            >
  class WeakAsyncHttpServerBase: public WeakHttpServerBase {

  public:
    // Handles an HTTP server connection
    class session : public std::enable_shared_from_this<session>{

      // Return a response for the given request.
      //
      // The concrete type of the response message (which depends on the
      // request), is type-erased in message_generator.
      template <class Body, class Allocator>
      http::message_generator handle_request(http::request<Body, http::basic_fields<Allocator>>&& req){
        // make Response --------------------------------------------------
        /*
          🦜 : Here we use our old code
         */
        return this->srv->handle_request(std::move(req),"<mock-client>",1234);
      } // handle_request

      // beast::tcp_stream
      stream_t stream_;
      beast::flat_buffer buffer_;
      http::request<http::string_body> req_;

    public:
      // Take ownership of the stream

      WeakAsyncHttpServerBase * const srv;
      // session(WeakAsyncHttpServerBase * const ssrv, tcp::socket&& socket)
      session(WeakAsyncHttpServerBase * const ssrv, socket_t&& socket)
        : stream_(std::move(socket)),srv(ssrv){
        BOOST_LOG_TRIVIAL(debug) <<  "Session started";
      }

      // Start the asynchronous operation
      void run(){
        // We need to be executing within a strand to perform async operations
        // on the I/O objects in this session. Although not strictly necessary
        // for single-threaded contexts, this example code is written to be
        // thread-safe by default.
        asio::dispatch(stream_.get_executor(),
                      beast::bind_front_handler(
                                                &session::do_read,
                                                this->shared_from_this()));
      }

      void do_read(){
        // Make the request empty before reading,
        // otherwise the operation behavior is undefined.
        req_ = {};

        // Set the timeout.
        stream_.expires_after(std::chrono::seconds(60 * 4)); // 4 minutes.

        // Read a request
        http::async_read(stream_, buffer_, req_,
                         beast::bind_front_handler(
                                                   &session::on_read,
                                                   this->shared_from_this()));
      }

      void on_read(beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);

        // This means they closed the connection
        if(ec == http::error::end_of_stream)
          return do_close();

        if(ec)
          return fail(ec, "read");

        // Send the response
        send_response(handle_request(std::move(req_)));
      }

      void send_response(http::message_generator&& msg){
        bool keep_alive = msg.keep_alive();

        // Write the response
        beast::async_write(
                           stream_,
                           std::move(msg),
                           beast::bind_front_handler(&session::on_write, this->shared_from_this(), keep_alive));
      }

      void on_write(
                    bool keep_alive,
                    beast::error_code ec,
                    std::size_t bytes_transferred) {
        boost::ignore_unused(bytes_transferred);

        if(ec)
          return fail(ec, "write");

        if(! keep_alive){
          // This means we should close the connection, usually because
          // the response indicated the "Connection: close" semantic.
          return do_close();
        }

        // Read another request
        do_read();
      }

      void do_close() {
        // Send a TCP shutdown
        beast::error_code ec;
        // stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
        stream_.socket().shutdown(socket_t::shutdown_send, ec);

        // At this point the connection is closed gracefully
      }
    };                            // class session

    //------------------------------------------------------------------------------
    // Report a failure
    static void fail(beast::error_code ec, char const* what)
    {
      BOOST_THROW_EXCEPTION(std::runtime_error(string(what) + ": " + ec.message()));
    }

    // Accepts incoming connections and launches the sessions
    class listener : public std::enable_shared_from_this<listener>{
      asio::io_context& ioc_;
      // tcp::acceptor acceptor_;
      acceptor_t acceptor_;
      WeakAsyncHttpServerBase * const srv;
    public:
      listener(WeakAsyncHttpServerBase * const ssrv,
               asio::io_context& ioc,
               endpoint_t endpoint
               ) : ioc_(ioc) , acceptor_(asio::make_strand(ioc)), srv(ssrv){
        BOOST_LOG_TRIVIAL(debug) <<  "listener started";
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec){
          fail(ec, "failed to open acceptor");
        }

        // Allow address reuse
        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if(ec){
          fail(ec, "failed in set_option");
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec){
          fail(ec, "failed to bind server address");
        }

        // Start listening for connections
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if(ec){
          fail(ec, "failed to listen for connection");
        }
      }

      // Start accepting incoming connections
      void run(){do_accept();}
    private:
      void
      do_accept(){
        // The new connection gets its own strand
        acceptor_.async_accept(asio::make_strand(ioc_),
                               beast::bind_front_handler(&listener::on_accept, this->shared_from_this())
                               );
      }

      // void on_accept(beast::error_code ec, tcp::socket socket) {
        void on_accept(beast::error_code ec, socket_t socket) {
        if(ec){
          // fail(ec, "accept");
          BOOST_LOG_TRIVIAL(error) << S_RED "❌️ failed " S_NOR "to accept socket[" \
            S_MAGENTA "ignoring it" S_NOR "]: " << ec.message();
          return; // To avoid infinite loop
        }else{
          // Create the session and run it
          std::make_shared<session>(this->srv,std::move(socket))->run();
        }
        // Accept another connection
        do_accept();
      }
    };                            // class listener
    //------------------------------------------------------------------------------


    std::unique_ptr<asio::io_context> ioc;
    vector<std::jthread> * all_threads;
    std::shared_ptr<listener> lstn;
    int n_threads;

    // WeakAsyncHttpServerBase(unsigned short port = 7777, const int threads = 4){
    WeakAsyncHttpServerBase(const int threads = 4):
      n_threads(threads) {
      /*
        🦜 : Usually you don't wanna set threads to be 1.... It does block
        sometimes. And set it to something like 2 usually will work...
       */

      // BOOST_LOG_TRIVIAL(debug) <<  "🐸 acceptor start listening on port " << port;
      // The io_context is required for all I/O
      this->ioc = std::make_unique<asio::io_context>(threads);
      // Create and launch a listening port

    }

    void run(){
      // Run the I/O service on the requested number of threads
      this->all_threads = new vector<std::jthread>();
      this->all_threads->reserve(n_threads);
      for(auto i = n_threads; i > 0; --i)
        this->all_threads->emplace_back([this] {this->ioc->run();});
    }

    virtual ~WeakAsyncHttpServerBase(){
      BOOST_LOG_TRIVIAL(debug) << "👋🐸 Server closed";
      this->ioc->stop();
      delete this->all_threads;   // join them all
    }

  };                            // class WeakAsyncHttpServerBase

  class WeakAsyncTcpHttpServer: public WeakAsyncHttpServerBase</*stream_t*/ beast::tcp_stream,
                                                                            /*socket_t*/ tcp::socket,
                                                                            /*acceptor_t*/ tcp::acceptor,
                                                                            /*endpoint_t*/ tcp::endpoint
                                                                            >{
  public:
    WeakAsyncTcpHttpServer(unsigned short port = 7777, const int threads = 4):
      WeakAsyncHttpServerBase(threads){
      BOOST_LOG_TRIVIAL(debug) <<  S_GREEN "\t🐸 TCP acceptor" S_NOR " starts listening on port " S_GREEN << port << S_NOR;
      auto const address = asio::ip::make_address("0.0.0.0");
      this->lstn = std::make_shared<listener>(this,*ioc, tcp::endpoint{address, port});
      // this->lstn = std::make_shared<listener>(this,*ioc, unix_domain::stream_protocol::endpoint{"/tmp/hi.sock"});
      this->lstn->run();
      this->run();
    }
  };                            // class WeakAsyncTcpHttpServer

  using std::filesystem::path;

  // if unix, add unix domain socket support
  #if defined(__unix__)
  namespace unix_domain = boost::asio::local;
  // using stream_t = beast::basic_stream<unix_domain::stream_protocol>;
  // using socket_t = unix_domain::stream_protocol::socket;
  // using acceptor_t = unix_domain::stream_protocol::acceptor;
  // using endpoint_t = unix_domain::stream_protocol::endpoint;
  class WeakAsyncUnixHttpServer: public WeakAsyncHttpServerBase< /*stream_t*/ beast::basic_stream<unix_domain::stream_protocol>,
                                                                              /*socket_t*/ unix_domain::stream_protocol::socket,
                                                                              /*acceptor_t*/ unix_domain::stream_protocol::acceptor,
                                                                              /*endpoint_t*/ unix_domain::stream_protocol::endpoint
                                                                              >{
  public:
    path pp;
    WeakAsyncUnixHttpServer(path p = "/tmp/hi.sock", const int threads = 4):
      WeakAsyncHttpServerBase(threads), pp(p){
      BOOST_LOG_TRIVIAL(debug) <<  S_GREEN "\t🐸 Unix-domain socket acceptor" S_NOR " starts listening on  " S_GREEN << p << S_NOR;

      // remove the file if it exists
      if (std::filesystem::exists(p)){
        BOOST_LOG_TRIVIAL(debug) <<  "🚮️ recreating socket file" << S_GREEN <<  p.string() << S_NOR;
        std::filesystem::remove(p);
      }

      this->lstn = std::make_shared<listener>(this,*ioc, unix_domain::stream_protocol::endpoint{p.string()});
      this->lstn->run();
      this->run();
    }

    ~WeakAsyncUnixHttpServer(){
      BOOST_LOG_TRIVIAL(debug) <<  "\t🚮️ removing socket file" << S_GREEN <<  pp.string() << S_NOR;
      std::filesystem::remove(pp);
    }
  };                            // class WeakAsyncTcpHttpServer
  #endif
}
