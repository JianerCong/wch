#pragma once

#include "pure-httpCommon.hpp"


namespace pure {

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
  class WeakHttpServer : public WeakHttpServerBase {
  private:
    // {{{ private (implementation)

    inline void fail(beast::error_code ec, char const* what){
      std::cerr << what << ": " << ec.message() << "\n";
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
      while(not this->closed.test()){
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


        if((not keep_alive) or (this->closed.test())){
          printf("We should not keep alive\n");
          // This means we should close the connection, usually because
          // the response indicated the "Connection: close" semantic.
          break;
        }
      }
      // Send a TCP shutdown
      socket.shutdown(tcp::socket::shutdown_send, ec);
      // At this point the connection is closed gracefully
      BOOST_LOG_SEV(this->lg,debug) << format("🐸 Server Connection with " S_CYAN "%s:%d " S_NOR "closed " S_GREEN "gracefully" S_NOR)
        % s % p;
    }
    // }}}

  public:
    using logger_t =
      boost::log::sources::severity_logger<boost::log::trivial::severity_level>;
    // ^^^ 🦜 : This is the simplest logger compatible with the trivial-log in Boost.Log

    using enum boost::log::trivial::severity_level;
    /*
      🦜 :import the enum, which import the name: trace,debug,...,error.
    */
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

    WeakHttpServer(uint16_t portToListen = 7777,
                   logger_t l = {},
                   postMap_t pMap = {}, getMap_t gMap = {}
                   ):WeakHttpServerBase(pMap,gMap),lg{l}{

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
      std::thread{[&](){while(not this->closed.test()){
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
    ~WeakHttpServer(){
      BOOST_LOG_SEV(this->lg,debug) << format("👋🐸 Server closed");
      ioc.stop();
      this->closed.test_and_set();
      delete this->all_threads;
      BOOST_LOG_SEV(this->lg,debug) << format("👋 All handler closed");
    }
  };


}
