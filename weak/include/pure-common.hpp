#pragma once
#include <boost/json.hpp>
#include <string>
#include <string_view>
#include <iostream>
#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>


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
#include <algorithm>
#include <numeric>
#include <vector>
#include <memory>
#include <optional>
#include <tuple>
#include <unordered_map>

namespace pure{
  using std::vector;
  using std::shared_ptr;
  using std::optional;
  using std::tuple;
  using std::unordered_map;

  namespace ranges = std::ranges;
  using boost::lexical_cast;
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

    // 🦜 : In fact, we can teach ostream how to display an IJsonizable:
    friend std::ostream& operator<<(std::ostream& os, const IJsonizable& I){
      os << I.toJsonString();
      return os;
    };
  };

  class ISerializable{
  public:
    virtual bool fromString(string_view s) noexcept =0;
    virtual string toString() const noexcept =0;
  };

#ifdef WITH_PROTOBUF
  /**
   * @brief The interface for serializing to and from protobuf.
   *
   * 🦜 : The implementer just need to implement two methods:
   *
   * + fromPbString() and toPb().
   *
   * Then it will get a method toPbString(). (for free.)
   */
  class ISerializableInPb{
  public:
    virtual bool fromPbString(string_view s) noexcept =0;
    virtual string toPbString() const = 0;
  };
#endif

  namespace json = boost::json;
  using json::value_to;



  inline string pluralizeOn(uint64_t x, string e1="", string e2="s"){
    return x == 1 ? e1 : e2;
  }




  // 🦜 : atomic<T> & is deleted

  // template<typename T>
  // // atomic get
  // T atm_get(std::atomic<T> x){
  //   return x.load(std::memory_order::relaxed);
  // }

  // template<typename T>
  // // atomic set
  // void atm_set(std::atomic<T> x, T y){
  //   x.store(y,std::memory_order::relaxed);
  // }


  namespace ranges = std::ranges;
  /**
   * @brief The contain for vector.
   *
   * 🦜 : Initially used for rbft cnss.
   */
  template<typename T>
  bool contains(const vector<T> v, const T x){
    return ranges::find(v,x) != v.end();
  }

  template<typename T>
  bool contains(const shared_ptr<vector<T>> v, const T x){
    BOOST_LOG_TRIVIAL(debug) << "Calling contains(ptr<vector<>>)";
    return ranges::find(v->begin(),v->end(),x) != v->end();
  }


} // namespace pure

// This helper function deduces the type and assigns the value with the matching key
// 🦜 : Defining this allows us to use json::value_to<T>
#define ADD_FROM_JSON_TAG_INVOKE(T)                           \
  T tag_invoke(json::value_to_tag<T>, json::value const& v){  \
    T t;                                                      \
    if (t.fromJson(v)) return t;                              \
    return {};                                                \
  }


#define BOOST_ENABLE_ASSERT_HANDLER
#include <boost/assert.hpp>

class my_assertion_error: public std::exception{
public:
  std::string w;
  my_assertion_error(std::string ww=""): w(ww){}
  std::string what(){
    return w;
  }
};

// Called when BOOST_ASSERT_MSG failed
void boost::assertion_failed_msg(char const * expr, char const * function,
                                 char const * msg, char const * file, long line){
  std::string s = (format("❌️ [%s]\n\tassertion %s has failed. (func=%s,file=%s,line=%ld)")
                   % msg % expr % function % file % line).str();
  BOOST_THROW_EXCEPTION(my_assertion_error(s));
}

// Called when BOOST_ASSERT failed
void boost::assertion_failed(char const * expr, char const * function,
                             char const * file, long line){
  std::string s = (format("❌️\n\tassertion %s has failed. (func=%s,file=%s,line=%ld)")
                   % expr % function % file % line).str();
  BOOST_THROW_EXCEPTION(my_assertion_error(s));
}

/**
 * @brief ADD_FROM_STR_WITH_JSON_OR_PB
 *
 *   🐢 : This pattern is common enough that we think it's worth a macro.
 *
 *   🦜 : What does this macro do?
 *
 *   🐢 : It defines two methods: toString() and fromString(). It uses protobuf
 *   if WITH_PROTOBUF is defined, otherwise it uses json.
 */
#ifdef WITH_PROTOBUF
/*
  🦜 : Why we can't write ISerializableInPb::fromPbString() just like in
  Json?

  🐢 : Because ::toJsonString() is a concrete method (although it calls the
  virtual method ::toPbString()). But ::fromPbString() is a virtual method,
  so the loader ld will complain...
*/
#define ADD_TO_FROM_STR_WITH_JSON_OR_PB             \
  string toString() const noexcept override {       \
    return this->toPbString();                      \
  }                                                 \
  bool fromString(string_view s) noexcept override{ \
    return this->fromPbString(s);                   \
  }
#else
#define ADD_TO_FROM_STR_WITH_JSON_OR_PB             \
  string toString() const noexcept override {       \
    return IJsonizable::toJsonString();             \
  }                                                 \
  bool fromString(string_view s) noexcept override{ \
    return IJsonizable::fromJsonString(s);          \
  }
#endif
