#include <boost/asio.hpp>
#include <iostream>
#include <boost/log/trivial.hpp>

/*
  a program that quit when the user press [enter] OR ctrl-c (SIGINT) or SIGTERM signal
*/
namespace weak{
  void hold_on(){
#if defined(__unix__)
#if defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
    boost::asio::io_service ios;
    boost::asio::signal_set signals(ios, SIGINT, SIGTERM);
    signals.async_wait([&ios](const boost::system::error_code & ec, int signal_number){
      if (!ec) {
        BOOST_LOG_TRIVIAL(debug) <<  "Got 📗️ Signal " << signal_number;
        ios.stop();
      }
    });

    boost::asio::posix::stream_descriptor input(ios, ::dup(STDIN_FILENO));
    boost::asio::streambuf input_buffer(8);
    boost::asio::async_read_until(input, input_buffer, '\n',
                                  [&ios](boost::system::error_code ec, std::size_t length) {
                                    if (ec) {
                                      BOOST_LOG_TRIVIAL(error) <<  "Error reading stdin: " << ec.message();
                                      return;
                                    }
                                    BOOST_LOG_TRIVIAL(debug) <<  "📗️ Enter pressed";
                                    ios.stop();
                                  });
    ios.run();
    input.close();
#else
    BOOST_LOG_TRIVIAL(error) <<  "❌️ The POSIX stream descriptor is not supported on this platform.";
    std::exit(1);
#endif // defined(BOOST_ASIO_HAS_POSIX_STREAM_DESCRIPTOR)
#else // for windows, just wait for the user to press enter
    std::cin.get();
#endif // defined(__unix__)
  }
}
