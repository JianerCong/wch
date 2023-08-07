#pragma once
#include <boost/json.hpp>
#include <string>
#include <string_view>



// colors
#define S_RED     "\x1b[31m"
#define S_GREEN   "\x1b[32m"
#define S_YELLOW  "\x1b[33m"
#define S_BLUE    "\x1b[34m"
#define S_MAGENTA "\x1b[35m"
#define S_CYAN    "\x1b[36m"
#define S_NOR "\x1b[0m"

#include <boost/format.hpp>
#include <functional>

namespace pure{
  using std::function;
  using boost::format;
  using std::string_view;
  using std::string;
  namespace json = boost::json;
  using json::value_to;
  /**
   * @brief Representing a Jsonizable type. Client only need to implement two methods
   * fromJson() and toJson(), then it will get two methods for Json string.
   */
  class IJsonizable{
  public:
    virtual json::value toJson() const noexcept=0;
    virtual bool fromJson(const json::value &)noexcept=0;

    /**
     * @brief Deserialize from Json String
     */
    bool fromJsonString(string_view s) noexcept{
      // Parse the Json
      json::error_code ec;

      /* 🦜: std:string_view -> json::string_view is
         available in Boost 1.82.0*/
      // json::value v = json::parse(s ,ec );
      json::value v = json::parse(string(s),ec );

      if (ec){
        // conversion-specifier does not impose the concerned argument to be of
        // a restricted set of types, but merely sets the ios flags that are
        // associated with a type specification:
        BOOST_LOG_TRIVIAL(error) <<  format("Invalid Json %s") % ec;
        return false;
      }

      // Pass it to the to-be-implemented method
      return fromJson(v);
    };
    string toJsonString() const noexcept{
      json::value v = toJson();
      string d = json::serialize(v);
      return d;
    };
  };

  class ISerializable{
    virtual bool fromString(string_view s) noexcept =0;
    virtual string toString() const noexcept =0;
  };
}
