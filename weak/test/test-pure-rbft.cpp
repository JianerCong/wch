/**
 * @file test-pure-net.cpp
 * @brief Here we tests some helpler functions in the rcp server.
 *
 * 🦜 : We test them here, because they don't need to resort to an external
 * server or client. These are usually some static methods;
 */

#include "h.hpp"
#include "cnsss/pure-rbft.hpp"

#include <boost/algorithm/string.hpp>

#include <string>
using std::string;
#include <fstream>
#include <filesystem>
namespace filesystem = std::filesystem;
using std::filesystem::path;
using std::filesystem::current_path;
/**
 * @brief Read all the string from a file.
 *
 * @param p the path to the file
 *
 * @return the file content.
 */
string read_all_string(path & p){
  std::ifstream i(p);
  BOOST_REQUIRE(filesystem::exists(p));
  BOOST_CHECK(i.is_open());
  string s; char c;
  while (i.get(c)) s+=c;
  i.close();
  return s;
}

#include <vector>
using std::vector;

void check_same_as_json_file(string s0, path p){
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string
  s = boost::algorithm::erase_all_copy(s, " ");
  s = boost::algorithm::erase_all_copy(s, "\n");

  BOOST_CHECK_EQUAL(s,s0);
}

template<typename T>
void veq(vector<T> v, vector<T> v2){
  BOOST_CHECK_EQUAL(v.size(), v2.size());
  for (int i=0;i<v.size();i++) \
    BOOST_CHECK_EQUAL(v[i],v2[i]);
};

using namespace ::pure;
BOOST_AUTO_TEST_SUITE(test_new_view_certificate);
BOOST_AUTO_TEST_CASE(test_from_json){
  path p = current_path() / "example-jsons" / "rbft-NewViewCert-ok.json";
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string

  NewViewCertificate c;
  BOOST_REQUIRE(c.fromJsonString(s));
  BOOST_CHECK_EQUAL(c.msg,"abc");
  BOOST_CHECK_EQUAL(c.epoch,1);
  veq(c.new_view_certificate,{"v1","v2"});
  veq(c.sig_of_nodes_to_be_added,{"s1","s2"});
  veq(c.cmds,{"c1","c2"});
}

BOOST_AUTO_TEST_CASE(test_from_json_bad){
  path p = current_path() / "example-jsons" / "rbft-NewViewCert-bad.json";
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string

  NewViewCertificate c;
  BOOST_REQUIRE(not c.fromJsonString(s));
}
void erase_spaces(string & s){
  for (
       string::size_type i = s.find_first_of(string(" \n"));
       i != string::npos;
       i = s.find_first_of(string(" \n"))
       ){
    s.erase(i-1,i);
    BOOST_TEST_MESSAGE("String erased to: " + s);
  }
};

BOOST_AUTO_TEST_CASE(test_erase_spaces){
  string s = "a b   \nc";
  s = boost::algorithm::erase_all_copy(s, " ");
  s = boost::algorithm::erase_all_copy(s, "\n");
  BOOST_CHECK_EQUAL(s,"abc");
}

BOOST_AUTO_TEST_CASE(test_to_json){
  NewViewCertificate c{"abc",1,{"v1","v2"},
                       {"s1", "s2"},
                       {"c1","c2"}};
  // 🦜 : Thanks to a little overload magic, we can now display all IJsonizable
  BOOST_TEST_MESSAGE((format("Got NewViewCertificate: %s") % c).str());
  string s0 = c.toJsonString();
  // The expected content
  path p = current_path() / "example-jsons" / "rbft-NewViewCert-ok.json";
  check_same_as_json_file(s0,p);
}
BOOST_AUTO_TEST_SUITE_END();    // test_new_view_certificate

BOOST_AUTO_TEST_SUITE(test_LaidDownMsg);

BOOST_AUTO_TEST_CASE(test_from_json){
  path p = current_path() / "example-jsons" / "rbft-LaidDownMsg-ok.json";
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string

  LaidDownMsg c;
  BOOST_REQUIRE(c.fromJsonString(s));
  BOOST_CHECK_EQUAL(c.msg,"aaa");
  BOOST_CHECK_EQUAL(c.epoch,123);
  BOOST_CHECK_EQUAL(c.state,"bbb");
}

BOOST_AUTO_TEST_CASE(test_from_json_bad){
  path p = current_path() / "example-jsons" / "rbft-LaidDownMsg-bad.json";
  BOOST_TEST_MESSAGE("File to read: " + p.string());
  string s = read_all_string(p); // json string

  LaidDownMsg c;
  BOOST_REQUIRE(not c.fromJsonString(s));
}
BOOST_AUTO_TEST_CASE(test_to_json){
  LaidDownMsg m("aaa",123,"bbb");
  string s0 = m.toJsonString();

  // The expected content
  path p = current_path() / "example-jsons" / "rbft-LaidDownMsg-ok.json";
  check_same_as_json_file(s0, p);
  // 🦜 : After removing the spaces, they should be the same.
}

BOOST_AUTO_TEST_SUITE_END();    // test_LaidDownMsg
