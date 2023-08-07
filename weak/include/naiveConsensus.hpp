#pragma once
#include "core.hpp"
#include "weakHttpServer.hpp"
#include "weakHttpClient.hpp"
#include "evmExecutor.hpp"
#include "storageManager.hpp"


#include <memory>
using std::unique_ptr;
#include <boost/log/attributes/scoped_attribute.hpp> // BOOST_LOG_SCOPED_THREAD_TAG(...);
#include <boost/log/attributes/value_extraction.hpp>
#include <boost/log/core.hpp>
#include <boost/core/null_deleter.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks/sync_frontend.hpp>
#include <boost/log/sinks/text_ostream_backend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/formatting_ostream.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <fstream>
#include <ostream>

namespace weak{

  namespace naiveConsensus{
    void initLog();
    using WeakHttpClient::postMap_t;
    using WeakHttpClient::getMap_t;
    tuple<postMap_t,getMap_t> prepareListeners();
    void init(){
      initLog();
      BOOST_LOG_TRIVIAL(debug) << format("🐸 NaiveConsensus started.");
      // init the mempool
      // init the server
      try{
        auto [getLisnMap, postLisnMap] = prepareMaps()
          // 1. --------------------------------------------------
          // Initialize a logger and add Tag to it, logging to this logger will be
          // written to file.
          boost::log::sources::severity_logger<boost::log::trivial::severity_level> lg;
        // ^^^ 🦜 : Yes, this is the simplest logger in Boost.Log
        namespace attrs = boost::log::attributes;
        lg.add_attribute("Tag", attrs::constant<std::string>("AAA"));
        WeakHttpServer serv{7777, postLisnMap, getLisnMap,lg};
        BOOST_LOG_TRIVIAL(debug) << format("Press any key to quit: ");
        std::cin.get();
      } // here the server is closed
      catch (const std::exception& e){
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
      }
    }
  }

  BOOST_LOG_ATTRIBUTE_KEYWORD(tag_attr, "Tag", std::string)
  static void initLog(){
    namespace log = boost::log;
    namespace src = boost::log::sources;
    namespace expr = boost::log::expressions;
    namespace sinks = boost::log::sinks;


    // set up two sinks, one to std::out, one to hi.log

    // 1. --------------------------------------------------
    // Setup the common formatter for all sinks
    log::formatter fmt = expr::stream
      << expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S")
      << ": <" << log::trivial::severity
      << "> "
      << expr::if_(expr::has_attr(tag_attr)) [expr::stream << "[" << tag_attr << "] "]
      << expr::smessage;

    // 2. --------------------------------------------------
    // Sink1
    typedef sinks::synchronous_sink< sinks::text_ostream_backend > text_sink;
    unique_ptr< text_sink > sink = std::make_unique< text_sink >();

    // sink 1 is to cout
    // sink->locked_backend()->add_stream(boost::make_shared< std::ostream >(&std::clog));
    boost::shared_ptr< std::ostream > stream(&std::clog, boost::null_deleter());
    sink->locked_backend()->add_stream(stream);

    sink->set_formatter(fmt);
    sink->set_filter(not expr::has_attr(tag_attr));
    log::core::get()->add_sink(std::move(sink));

    // 3. --------------------------------------------------
    // sink2 is to file
    sink = std::make_unique<text_sink>();
    sink->locked_backend()->add_stream(boost::make_shared< std::ofstream >("hi.log"));
    sink->set_formatter(fmt);
    sink->set_filter(expr::has_attr(tag_attr) && tag_attr == "AAA");
    log::core::get()->add_sink(std::move(sink));

    // 4. --------------------------------------------------
    // Add attributes
    log::add_common_attributes();
  };
  tuple<postMap_t,getMap_t> prepareListeners(){
    // --------------------------------------------------
    // The handlers
    // <! the function dispatch map, each entry accepts (string address, uint16_t
    // port, string body). the key is the target such as "\hi", "\"
    unordered_map<string,
                  function<tuple<bool,string>
                           (string, uint16_t, string)
                           >
                  > postLisnMap{
      {"/aaa", [](string ,uint16_t,string){return make_tuple(true,"\"aaa from POST too\"");}},
        {"/bbb", [](string a,uint16_t p, string d){return make_tuple(true,
      (format("\"bbb too %s:%d, recieved data: %s\"") % a % p % d).str()
      );}}
  };

    unordered_map<string,
                  function<tuple<bool,string>
                           (string, uint16_t)
                           >
                  > getLisnMap{
    {"/aaa", [](string,uint16_t){return make_tuple(true,"\"aaa too\"");}},
      {"/bbb", [](string a,uint16_t p){return make_tuple(true,
      (format("\"bbb too %s:%d\"") % a % p).str()
      );}}
  };
    return make_tuple(postLisnMap,getLisnMap);
  }
}
