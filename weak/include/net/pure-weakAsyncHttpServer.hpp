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
 */
#pragma once


#include "pure-httpCommon.hpp"
// #include <boost/beast/core.hpp>
// #include <boost/beast/http.hpp>
// #include <boost/beast/version.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
namespace pure{
  class WeakAsyncHttpServer: public WeakHttpServerBase {

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

      beast::tcp_stream stream_;
      beast::flat_buffer buffer_;
      http::request<http::string_body> req_;

    public:
      // Take ownership of the stream

      WeakAsyncHttpServer * const srv;
      session(WeakAsyncHttpServer * const ssrv, tcp::socket&& socket)
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
                                                shared_from_this()));
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
                                                   shared_from_this()));
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
                           beast::bind_front_handler(&session::on_write, shared_from_this(), keep_alive));
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
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);

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
      tcp::acceptor acceptor_;
      WeakAsyncHttpServer * const srv;
    public:
      listener(WeakAsyncHttpServer * const ssrv,
               asio::io_context& ioc,
               tcp::endpoint endpoint
               ) : ioc_(ioc) , acceptor_(asio::make_strand(ioc)), srv(ssrv){
        BOOST_LOG_TRIVIAL(debug) <<  "listener started";
        beast::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec){
          fail(ec, "failed to open acceptor");
          return;
        }

        // Allow address reuse
        acceptor_.set_option(asio::socket_base::reuse_address(true), ec);
        if(ec){
          fail(ec, "failed in set_option");
          return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec){
          fail(ec, "failed to bind server address");
          return;
        }

        // Start listening for connections
        acceptor_.listen(asio::socket_base::max_listen_connections, ec);
        if(ec){
          fail(ec, "failed to listen for connection");
          return;
        }
      }

      // Start accepting incoming connections
      void
      run(){do_accept();}
    private:
      void
      do_accept(){
        // The new connection gets its own strand
        acceptor_.async_accept(asio::make_strand(ioc_),
                               beast::bind_front_handler(&listener::on_accept, shared_from_this())
                               );
      }

      void on_accept(beast::error_code ec, tcp::socket socket) {
        if(ec){
          fail(ec, "accept");
          return; // To avoid infinite loop
        }else{
          // Create the session and run it
          std::make_shared<session>(this->srv,std::move(socket))->run();
        }

        // Accept another connection
        do_accept();
      }
    };                            // class listener
  public:
    //------------------------------------------------------------------------------


    std::unique_ptr<asio::io_context> ioc;
    vector<std::jthread> * all_threads;
    std::shared_ptr<listener> lstn;

    WeakAsyncHttpServer(unsigned short port = 7777,
                        const int threads = 4){
      /*
        🦜 : Usually you don't wanna set threads to be 1.... It does block
        sometimes. And set it to something like 2 usually will work...
       */

      BOOST_LOG_TRIVIAL(debug) <<  "🐸 acceptor start listening on port " << port;
      auto const address = asio::ip::make_address("0.0.0.0");
      // The io_context is required for all I/O
      this->ioc = std::make_unique<asio::io_context>(threads);

      // Create and launch a listening port
      this->lstn = std::make_shared<listener>(this,*ioc, tcp::endpoint{address, port});
      this->lstn->run();

      // Run the I/O service on the requested number of threads
      this->all_threads = new vector<std::jthread>();
      this->all_threads->reserve(threads);

      for(auto i = threads; i > 0; --i)
        this->all_threads->emplace_back([this] {this->ioc->run();});

    }

    ~WeakAsyncHttpServer(){
      BOOST_LOG_TRIVIAL(debug) << "👋🐸 Server closed";
      this->ioc->stop();
      delete this->all_threads;   // join them all
    }

  };                            // class WeakAsyncHttpServer
}
