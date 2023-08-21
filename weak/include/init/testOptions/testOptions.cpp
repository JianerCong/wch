#include "../options.hpp"

#include <boost/algorithm/string.hpp>
#include <iostream>
using std::cout;
using std::endl;
using weak::Options;

// use # to create string
#define SHOW(x) cout << #x " : " << o.x << endl
// ## means "glue", a ## b ⇒ ab

int main(int argc, char *argv[]){
  Options o{argc,argv};

  // string config_file;
  // string consensus_name;
  // int port;
  // string data_dir;
  // string node_to_connect;

  SHOW(config_file);
  SHOW(consensus_name);
  SHOW(port);
  SHOW(data_dir);
  SHOW(Solo_node_to_connect);
  if (not o.Bft_node_list.empty()){
    cout <<  "Bft_node_list : " <<
      boost::algorithm::join(o.Bft_node_list,",")
         << endl;
  }

  return 0;
}
