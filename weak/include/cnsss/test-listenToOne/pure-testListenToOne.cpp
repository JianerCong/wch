#include "pure-listenToOne.hpp"
#include <iostream>

using std::unordered_map;
using std::unique_ptr;
using std::make_unique;
using std::cout;
using std::cin;

#include <vector>
using std::vector;


// #include <map>
// using std::map;

using namespace pure;

struct NodeFactory{
  unordered_map<string,shared_ptr<ListenToOneConsensus>> nodes;
  string primary_name;

  // 🐢 : You need to keep the exe and network available until the consensus
  // finishes.
  vector<unique_ptr<mock::Executable>> ve;
  vector<unique_ptr<mock::EndpointNetworkNode>> vn;

  void make_node(){
    int i = nodes.size();
    string s = (format("N%d") % i).str();
    ve.push_back(make_unique<mock::Executable>(s));
    vn.push_back(make_unique<mock::EndpointNetworkNode>(s));

    IEndpointBasedNetworkable * n = dynamic_cast<IEndpointBasedNetworkable*>(vn.back().get());
    IForConsensusExecutable * e = dynamic_cast<IForConsensusExecutable*>(ve.back().get());

    if (i == 0){
      // this->nodes[s] = ListenToOneConsensus(move(n),move(e));
      this->nodes.insert({s,ListenToOneConsensus::create(n,e)});
      this->primary_name = s;
      return;
    }
    // 🦜: To use the following you need to have the empty ctor ()
    // this->nodes[s] = ListenToOneConsensus(move(n),move(e),
    //                                       this->primary_name);

    this->nodes.insert({s,ListenToOneConsensus::create(n,e,
                                               this->primary_name
                                               )});
  }
};

int main(int argc, char *argv[]){
  NodeFactory  fac;
  for (int i : {1,2,3}){
    fac.make_node();
  }

  unique_ptr<IEndpointBasedNetworkable> nClient{
    new mock::EndpointNetworkNode("ClientAAA")
  };

  while (true){
    string reply;
    cout << "Enter: ";
    cin >> reply;
    if (reply == "stop")
      break;
    if (reply == "append"){
      cout << S_MAGENTA << "Appending node\n" << S_NOR;
      fac.make_node();
      while (cin.get() != '\n') continue;
      continue;
    }

    if (reply == "kick"){
      string nodeToKick;
      cin >> nodeToKick;
      cout << S_MAGENTA << "Kicking node " << nodeToKick
           << "\n" << S_NOR;

      fac.nodes.at(nodeToKick)->net->clear();

      while (cin.get() != '\n') continue;
      continue;
    }

    string cmd;
    cin >> cmd;
    while (cin.get() != '\n') continue;

    cout << S_MAGENTA "Sending " + cmd +
      " To " + reply +  S_NOR << "\n";
    nClient->send(reply,"/pleaseExecuteThis",cmd);
}
  return 0;
}
