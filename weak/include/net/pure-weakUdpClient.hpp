#pragma once
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>
using boost::format;

#include <boost/array.hpp>
#include <boost/asio.hpp>
using boost::asio::io_service;
using boost::asio::ip::tcp;
using boost::system::error_code;
using boost::asio::buffer;

#include <string>
using std::string;

namespace asio = boost::asio;            // from <boost/asio.hpp>
using boost::asio::ip::udp;

namespace pure{
  namespace WeakUdpClient{
    void send(string host = "localhost",string port = "7777",string msg =
              "aaa"){
      try
        {
          boost::asio::io_context io_context;

          udp::resolver resolver(io_context);
          udp::endpoint receiver_endpoint =
            *resolver.resolve(udp::v4(), host, port).begin();

          udp::socket socket(io_context);
          socket.open(udp::v4());

          boost::system::error_code ignored_error;
          socket.send_to(boost::asio::buffer(msg), receiver_endpoint, 0 /*msg flag*/ ,
                         ignored_error); // send a request

        }
      catch (std::exception& e)
        {
          BOOST_LOG_TRIVIAL(error) << e.what();
        }
    }
  } // namespace WeakUdpClient
} // namespace pure
