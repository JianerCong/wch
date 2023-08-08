#pragma once

#include <boost/program_options.hpp>
#include <string>
#include <tuple>
#include <boost/format.hpp>

#include <cstdlib>              // for std::exit

#include <filesystem>
#include <fstream>
// #include <iostream>
// #include <iterator>

#include <iostream>

namespace weak{
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
    string my_endpoint = "localhost";

    // listenToOne consensus
    string Solo_node_to_connect;

    string without_sealer{"yes"};
    string mock_exe{"no"};
    string verbose{"yes"};

    string light_exe{"yes"};

    tuple<options_description,options_description,
          program_options::positional_options_description
          > define_options(){

      // 0. --------------------------------------------------
      // program options
      options_description o1("Generic options");
      o1.add_options()
        ("help,h", "Produce help message")
        ("without-sealer", program_options::value<string>(&(this->without_sealer))->implicit_value("yes"),
         "Enable sealer")
        ("mock-exe", program_options::value<string>(&(this->mock_exe))->implicit_value("yes"),
         "Disable concrete executor, used for testing communication layers.")
        ("verbose", program_options::value<string>(&(this->verbose))->implicit_value("yes"),
         "Print debug info during execution, this might affect speed.")
        ("light-exe", program_options::value<string>(&(this->light_exe))->implicit_value("yes"),
         "Enable the executor optimized for speed. ('yes' by default)")
        ("config,c", program_options::value<string>(&(this->config_file)),
         "[Optional] Name of the configuration file.")
        ;

      // Declare a group of options that will be allowed both on command line and
      // in config file
      options_description o2("Configuration");
      o2.add_options()
        ("consensus,s",program_options::value<string>(&(this->consensus_name))->default_value("Solo"),
         "The name of consensus to use. Available options are:\n"
         "  Solo\n"
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

    }
  };
}
