#pragma once
#include <vector>
using std::vector;

#include <boost/format.hpp>
using boost::format;
using namespace pure;
#include "cnsss/pure-forCnsss.hpp"
namespace mockedCnsss{

  // The static cnsss
  class A : public virtual ICnsssPrimaryBased {
    optional<string> handle_execute_for_primary(string endpoint,
                                                string data)noexcept override{
      BOOST_TEST_MESSAGE((format("handle_execute_for_primary "
                                 "called with endpoint=[%s],data=[%s]")
                          % endpoint % data).str()
                         );
      return "Primary result (ignored)";
    }
    optional<string> handle_execute_for_sub(string endpoint,
                                            string data)noexcept override{
      BOOST_TEST_MESSAGE((format("handle_execute_for_sub "
                                 "called with endpoint=[%s],data=[%s]")
                          % endpoint % data).str()
                         );
      return "Sub result (ignored)";
    }
  public:
    bool p;
    A(bool pp=true): p(pp){}
    bool is_primary() const noexcept override{
      return p;
    }
  };


  // The cnsss that remembers the cmd
  class B : public virtual ICnsssPrimaryBased {
    optional<string> handle_execute_for_primary(string /* endpoint */,
                                                string data)noexcept override{
      // BOOST_TEST_MESSAGE((format("handle_execute_for_primary "
      //                            "called with endpoint=[%s],data=[%s]")
      //                     % endpoint % data).str()
      //                    );
      this->cmds.push_back(data);
      return "Primary result (ignored)";
    }
    optional<string> handle_execute_for_sub(string /*endpoint*/,
                                            string data)noexcept override{
      // BOOST_TEST_MESSAGE((format("handle_execute_for_sub "
      //                            "called with endpoint=[%s],data=[%s]")
      //                     % endpoint % data).str()
      //                    );
      this->cmds.push_back(data);
      return "Sub result (ignored)";
    }
  public:
    vector<string> cmds;
    bool p;
    B(bool pp=true): p(pp){}
    bool is_primary() const noexcept override{
      return p;
    }
  };
}
