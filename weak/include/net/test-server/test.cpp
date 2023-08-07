#include "../pure-weakHttpServer.hpp"
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

#include <functional>
#include <tuple>
#include <unordered_map>
using std::unordered_map;
using std::tuple;
using std::make_tuple;

#include <boost/json.hpp>
namespace json = boost::json;
using std::string;
using pure::WeakHttpServer;

BOOST_LOG_ATTRIBUTE_KEYWORD(tag_attr, "Tag", std::string)
void init(){
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
}

tuple<bool,string> addHandler(WeakHttpServer & serv,
                              string a,uint16_t p, string d){
  // expected json: {"url" : "..."}

  json::error_code ec;
  json::value jv = json::parse(d, ec );

  if (ec || not jv.is_object()){
    return make_tuple(false,(format("%s is not valid json") % d).str());
  }

  using json::value_to;
  string s;

  try {
    json::object const& obj = jv.as_object();
    s = value_to<string>(obj.at("url"));
    auto g = [s](string ,uint16_t,string)->tuple<bool,string> {return make_tuple(true,
                                                                                 (format("%s is handled") % s).str());};

    serv.listenToPost(s,g);
  }catch (std::exception &e){
    return make_tuple(false,(format("%s is not valid json") % d).str());
  }

  return make_tuple(true,(format("%s is added to the url too") % s).str());
};


tuple<bool,string> removeHandler(WeakHttpServer & serv,
                              string a,uint16_t p, string d){
  // expected json: {"url" : "..."}

  json::error_code ec;
  json::value jv = json::parse(d, ec );

  if (ec || not jv.is_object()){
    return make_tuple(false,(format("%s is not valid json") % d).str());
  }

  using json::value_to;
  string s;
  bool removed = false;

  try {
    json::object const& obj = jv.as_object();
    s = value_to<string>(obj.at("url"));

    if (serv.removeFromPost(s) == 1)
      removed = true;
  }catch (std::exception &e){
    return make_tuple(false,(format("%s is not valid json") % d).str());
  }

  if (removed)
    return make_tuple(true,(format("%s is removed from the url") % s).str());
  else
    return make_tuple(true,(format("%s doesn't exist in the url") % s).str());
};


int main(int argc, char* argv[]){
  uint16_t port = 7777;
  if (argc > 1)
    port = boost::lexical_cast<uint16_t>(argv[1]);
  try{
    init();
    // 1. --------------------------------------------------
    // Initialize a logger and add Tag to it, logging to this logger will be
    // written to file.
    boost::log::sources::severity_logger<boost::log::trivial::severity_level> lg;
    // ^^^ 🦜 : Yes, this is the simplest logger in Boost.Log
    namespace attrs = boost::log::attributes;
    lg.add_attribute("Tag", attrs::constant<std::string>("AAA"));

    WeakHttpServer serv{port,lg};
    // WeakHttpServer serv{7777};         //write log to console

    using namespace std::placeholders;  // for _1, _2, _3...

    /*
      🐢 : This allows the client to add hanler.
     */
    serv.postLisnMap["/addHandler"] = std::bind(addHandler,std::ref(serv),_1,_2,_3);
    serv.postLisnMap["/removeHandler"] = std::bind(removeHandler,std::ref(serv),_1,_2,_3);

    /*
      🐢 : A simple get
     */
    serv.getLisnMap["/aaa"] = [](string h, uint16_t p,
                                 optional<unordered_map<string,string>> /* the query param */
                                 ) -> tuple<bool,string>{
      return make_tuple(true,"aaa from server");
    };

    BOOST_LOG_TRIVIAL(debug) << format("Press any key to quit: ");
    std::cin.get();
  } // here the server is closed

  catch (const std::exception& e){
    std::cerr << "Error: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
}



