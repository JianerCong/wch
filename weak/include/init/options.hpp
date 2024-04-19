#pragma once
#include <boost/program_options.hpp>
#include <string>
#include <tuple>
#include <boost/format.hpp>
#include <vector>
#include <cstdlib>              // for std::exit

#include <boost/algorithm/string.hpp>
using boost::split;
#include <filesystem>
#include <fstream>
// #include <iostream>
// #include <iterator>

#include <iostream>

namespace weak{
  using std::vector;
  namespace program_options = boost::program_options;
  using program_options::options_description;
  using std::filesystem::current_path;
  using std::string;
  using std::ifstream;
  using std::tuple;
  using std::make_tuple;
  using boost::format;
  using std::cout;

  /**
   * @brief The option class.
   *
   * 🐢 : Thanks to boost::program_options, it can automatically bind the value
   * for us.
   *
   * 🦜 : I feel like this saved us a lot of code.
   */
  struct Options{
    // private
    string config_file;
    // --------------------------------------------------
    string consensus_name;
    int port;
    string data_dir;

    int txs_per_blk = 10;
    string my_address;

    // listenToOne consensus
    string Solo_node_to_connect;

    string without_sealer{"yes"};
    string mock_exe{"no"};
    string verbose{"yes"};
    string light_exe{"yes"};

#if defined(__unix__)
    string unix_socket;
#endif

    string without_crypto{"yes"};
    struct {
      string ca_public_key_pem_file;
      string node_secret_key_pem_file;
      string node_cert_file;
      string peer_json_file_or_string;
    } crypto;

    vector<string> Bft_node_list;
    vector<string> Raft_node_list;

    string tx_mode_serious;

    tuple<options_description,options_description,
          program_options::positional_options_description
          > define_options(){

      // 0. --------------------------------------------------
      // program options
      options_description o1("Generic options");
      o1.add_options()
        ("help,h", "Produce help message")
        ("config,c", program_options::value<string>(&(this->config_file)),
         "[Optional] Name of the configuration file.")
        ;

      // Declare a group of options that will be allowed both on command line and
      // in config file
      options_description o2("Configuration");
      o2.add_options()
        ("without-sealer", program_options::value<string>(&(this->without_sealer))->implicit_value("yes"),
         "Enable sealer")
        ("tx-mode-serious", program_options::value<string>(&(this->tx_mode_serious))->implicit_value("debug"),
         R"---(

The tx mode to use. Available options are:
    debug(default)  public, ca@<ca_pk_pem_file>
This determines how the tx is checked.

🦜 : In `debug` mode, we don't check the tx at all. So no signature is needed,
and user can fill the `.from` field at will. This is the default mode.

🦜 : In `public` mode, we check the signature of the tx. So `tx.signature` and
`tx.pk_pem` should be filled. In addition to that, we also derive the `tx.from`
from the `tx.pk_pem` (so the `tx.from` field is ignored). `tx.signature` should
be in hex when using json.

🦜 : In `ca` mode, we, in addition to the signature, we also require that
`tx.pk_crt` is filled. (hex when using json) This shows that the `tx.pk_pem` is
signed by the CA's private key. The ca public key is read from file following
the `@` char. So one can do

   --tx-mode-serious ca@/home/me/ca_pk.pem

)---"
         )
#if !defined(_WIN32)
        ("unix-socket", program_options::value<string>(&(this->unix_socket))->implicit_value("/tmp/weak.sock"),
         "the unix-domain socket to listen on, default to /tmp/weak.sock")
#endif
        ("mock-exe", program_options::value<string>(&(this->mock_exe))->implicit_value("yes"),
         "Disable concrete executor, used for testing communication layers.")
        ("verbose", program_options::value<string>(&(this->verbose))->implicit_value("yes"),
         "Print debug info during execution, this might affect speed.")
        ("light-exe", program_options::value<string>(&(this->light_exe))->implicit_value("yes"),
         "Enable the executor optimized for speed. ('yes' by default)")
        ("this-host",program_options::value<string>(&(this->my_address))->default_value("localhost"),
         "The address of this host. In some consensuses, this will be the <host> exposed "
         "to other nodes. Default value: \"localhost\""
         )
        ("consensus,s",program_options::value<string>(&(this->consensus_name))->default_value("Solo"),
         "The name of consensus to use. Available options are:\n"
         "  Solo, Solo-static, Rbft, Raft\n"
         )
        ("port,p",program_options::value<int>(&(this->port))->default_value(7777),
         "The port to listen")
        ("data-dir,d",program_options::value<string>(&(this->data_dir))->default_value(""),
         "The folder in which the data is stored. If not given, then the chain is run in RAM-mode,"
         "which will not save anything persistant on the disk, and instead use a in-memory KV as backup storage.")
        ("Solo.node-to-connect,n",program_options::value<string>(&(this->Solo_node_to_connect)),
         "The endpoint to connect to in the Solo conesnsus. This option is ignored if consensus is not Solo."
         "This will specify the primary node the "
         "node wants to connect to. If not present"
         "    The format should be <host>:<port> e.g. 10.0.0.1:12345."
         )
        ("Raft.node-list", program_options::value<vector<string>>(&(this->Raft_node_list))->multitoken(),
         R"---(The list of peers in the cluster. This option is ignored if consensus is not Raft.
For example for a cluster of 3 nodes:
    --Raft.node-list 10.0.0.2:7777 10.0.0.3:7777

This list represents the others, so do not specify the current node in this list.
         )---")
        ("Bft.node-list", program_options::value<vector<string>>(&(this->Bft_node_list))->multitoken(),
         "The list of all nodes in the cluster. This option is ignored if consensus is not Rbft.\n"
         "For example:\n"
         "    --Bft.node-list localhost:7777 10.0.0.2:7777 10.0.0.3:7777 localhost:7778\n"
         "In particular, if `localhost:<port>` is in this list, then the node is"
         "considered to be one of the `initial node` in the cluster. Otherwise,"
         "the node is considered to be a `newcomer` and it will send request to the existing "
         "nodes to try to get in."
         )
        ("without-crypto", program_options::value<string>(&(this->without_crypto))->implicit_value("yes"),
         "When set to 'no', Enable all those crypto stuff about CA, key pair, "
         "peer validation, etc. Otherwise, we skip any of those, and all arguments starting with 'crypto' are ignored."
         "Default value: 'yes'.")
        ("crypto.ca_public_key_pem_file",
         program_options::value<string>(&(this->crypto.ca_public_key_pem_file)),
         "The path to the CA's public key."
         )
        ("crypto.node_secret_key_pem_file",
         program_options::value<string>(&(this->crypto.node_secret_key_pem_file)),
          "The path to the node's secret key."
         )
        ("crypto.node_cert_file",
         program_options::value<string>(&(this->crypto.node_cert_file)),
          "The path to the node's certificate."
         )
        ("crypto.peer_json_file_or_string",
         program_options::value<string>(&(this->crypto.peer_json_file_or_string)),
          "Path to the <peer json> file, or the json string itself.\n"
         "If you are specifying file path, then prefix the path with '@', for example.\n"
         "     --crypto.peer_json_file_or_string @./peer.json\n"
         "This json contains an object something like this: \n"
         "{\"<addr:port>\" : \"<addr:port>\" : {\"pk_pem_file\" : \"<path to pk pem file>\", \"cert_file\" : \"<path to cert pem file>\"},} \n"
         "for example: \n"
             "{\n"
               "\"localhost:7777\" : {\"pk_pem_file\" : \"./N0-pk.pem\", \"cert_file\" : \"./N0-cert.sig\"},\n"
               "\"localhost:7778\" : {\"pk_pem_file\" : \"./N1-pk.pem\", \"cert_file\" : \"./N1-cert.sig\"},\n"
               "\"localhost:7779\" : {\"pk_pem_file\" : \"./N2-pk.pem\", \"cert_file\" : \"./N2-cert.sig\"}\n"
             "}"
         )
        ;


      options_description Ocmd("Options for the cmdline"), Ocfg("Options for the config file");
      Ocmd.add(o1).add(o2);
      Ocfg.add(o2);

      program_options::positional_options_description p;
      p.add("port",-1); // map port to position (-1)
      return make_tuple(Ocmd,Ocfg,p);
    }

    program_options::variables_map
    prepare_map_and_read_cmdline(const options_description & Ocmd,
                const program_options::positional_options_description & p,
                int argc, char* argv[]
                ){
      // 1. --------------------------------------------------
      // The variable map
      program_options::variables_map m;

      // 2. --------------------------------------------------
      // Read from cmdline
      program_options::store(program_options::command_line_parser(argc, argv).
                             options(Ocmd).positional(p).run(), m);
      program_options::notify(m);

      return m;
    }

    void show_help(options_description Ocmd){
      cout << Ocmd << "\n";
    }

    void read_config_file(program_options::variables_map & m, options_description Ocfg){
      ifstream ifs(config_file.c_str());
      if (!ifs){
        cout << format("Cannot open config file %s\n") % config_file;
        std::exit(0);
      }else{
        program_options::store(program_options::parse_config_file(ifs, Ocfg), m);
        program_options::notify(m);
      }
    }

    Options(int argc, char* argv[]){
      auto [Ocmd, Ocfg, p] = define_options();
      auto m = prepare_map_and_read_cmdline(Ocmd,p,argc,argv);

      if (m.count("help")){
        show_help(Ocmd);
        std::exit(0);
      }

      // 3. --------------------------------------------------
      // Read from config file
      if (m.count("config"))
        read_config_file(m,Ocfg);

      // 4. --------------------------------------------------
      // Check the values
      this->Bft_node_list = Options::split_space_vector_maybe(this->Bft_node_list);
    }

    /**
     * @brief Turn a space seperated string into a vector.
     *
     * 🐢 : Inside the config file, we can't parse vector. Instead,space-separated
     * values are parsed as the first element in the vector. So we kinda have to
     * split them ourselve.
     */
    static vector<string> split_space_vector_maybe(vector<string> s){
      if (s.size() != 1)
        return s;

      vector<string> v;
      boost::split(v,s[0],boost::is_any_of(" "));
      return v;
    }
  };                            // class Options
} // namespace weak
