/**
 *  @file
 *  @copyright defined in eosio/LICENSE.txt
 */
#include <appbase/application.hpp>

#include <eosio/chain_plugin/chain_plugin.hpp>
#include <eosio/http_plugin/http_plugin.hpp>
#include <eosio/net_plugin/net_plugin.hpp>

#include <fc/log/logger_config.hpp>
#include <fc/log/appender.hpp>
#include <fc/exception/exception.hpp>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/exception/diagnostic_information.hpp>

#include "config.hpp"

using namespace appbase;
using namespace eosio;

namespace fc {
   std::unordered_map<std::string,appender::ptr>& get_appender_map();
}

namespace detail {

void configure_logging(const bfs::path& config_path)
{
   try {
      try {
         fc::configure_logging(config_path);
      } catch (...) {
         elog("Error reloading logging.json");
         throw;
      }
   } catch (const fc::exception& e) {
      elog("${e}", ("e",e.to_detail_string()));
   } catch (const boost::exception& e) {
      elog("${e}", ("e",boost::diagnostic_information(e)));
   } catch (const std::exception& e) {
      elog("${e}", ("e",e.what()));
   } catch (...) {
      // empty
   }
}

} // namespace detail

void logging_conf_loop()
{
   std::shared_ptr<boost::asio::signal_set> sighup_set(new boost::asio::signal_set(app().get_io_service(), SIGHUP));
   sighup_set->async_wait([sighup_set](const boost::system::error_code& err, int /*num*/) {
      if(!err)
      {
         ilog("Received HUP.  Reloading logging configuration.");
         auto config_path = app().get_logging_conf();
         if(fc::exists(config_path))
            ::detail::configure_logging(config_path);
         for(auto iter : fc::get_appender_map())
            iter.second->initialize(app().get_io_service());
         logging_conf_loop();
      }
   });
}

void initialize_logging()
{
   auto config_path = app().get_logging_conf();
   if(fc::exists(config_path))
     fc::configure_logging(config_path); // intentionally allowing exceptions to escape
   for(auto iter : fc::get_appender_map())
     iter.second->initialize(app().get_io_service());

   logging_conf_loop();
}

bfs::path determine_root_directory()
{
   bfs::path root;
   char* path = std::getenv("EOSIO_ROOT");
   if(path != nullptr)
      root = bfs::path(path);
   else {
      bfs::path p = boost::dll::program_location();
      while(p != p.root_directory()) {
         p = p.parent_path();
         if(exists(p / "etc")) {
            root = p;
            break;
         }
      }
      if(p == p.root_directory())
         root = p;
   }
   return root;
}

int main(int argc, char** argv)
{
   try {
      app().set_version(eosio::nodeos::config::version);
      bfs::path root = determine_root_directory();
      app().set_default_data_dir(root / "var/lib/eosio/node_00");
      app().set_default_config_dir(root / "etc/eosio/node_00");
      if(!app().initialize<chain_plugin, http_plugin, net_plugin>(argc, argv))
         return -1;
      initialize_logging();
      ilog("nodeos version ${ver}", ("ver", eosio::nodeos::config::itoh(static_cast<uint32_t>(app().version()))));
      ilog("eosio root is ${root}", ("root", root.string()));
      app().startup();
      app().exec();
   } catch (const fc::exception& e) {
      elog("${e}", ("e",e.to_detail_string()));
   } catch (const boost::exception& e) {
      elog("${e}", ("e",boost::diagnostic_information(e)));
   } catch (const std::exception& e) {
      elog("${e}", ("e",e.what()));
   } catch (...) {
      elog("unknown exception");
   }
   return 0;
}
