#include "h.hpp"

#include "div2Executor.hpp"
using namespace weak;

// 🦜 : Here we can test the Div2Executor, an extension of the EvmExecutor.

BOOST_AUTO_TEST_CASE(test_exec0_hi){
  auto [return_val, result] = Div2Executor::exec0("echo hi");
  BOOST_CHECK_EQUAL(result, "hi");
}


BOOST_AUTO_TEST_CASE(test_exec_py){
  auto [return_val, result] = Div2Executor::exec_py("print('hi')");
  BOOST_CHECK_EQUAL(result, "hi");
}
