#include <unistd.h>
#include <stdlib.h>
#include <fstream>
#include <string>
#include <vector>

#include "Config.hpp"
#include "catch.hpp"

using namespace eventhub;

using Catch::Matchers::Contains;

TEST_CASE("Config test") {
  SECTION("Test numbers") {
    Config cfg;
    cfg.defineOption<int>("test_number", ConfigValueSettings::REQUIRED);
    cfg << "test_number = 1337";
    cfg.load();
    REQUIRE(cfg.get<int>("test_number") == 1337);
  }

  SECTION("Test lowercase booleans") {
    Config cfg;
    cfg.defineOption<bool>("false_boolean_lowercase", ConfigValueSettings::REQUIRED);
    cfg.defineOption<bool>("true_boolean_lowercase", ConfigValueSettings::REQUIRED);

    cfg << "false_boolean_lowercase = false\n";
    cfg << "true_boolean_lowercase = true\n";
    cfg.load();

    REQUIRE(cfg.get<bool>("false_boolean_lowercase") == false);
    REQUIRE(cfg.get<bool>("true_boolean_lowercase") == true);
  }

  SECTION("Test uppercase booleans") {
    Config cfg;
    cfg.defineOption<bool>("false_boolean_uppercase", ConfigValueSettings::REQUIRED);
    cfg.defineOption<bool>("true_boolean_uppercase", ConfigValueSettings::REQUIRED);

    cfg << "false_boolean_uppercase = FALSE\n";
    cfg << "true_boolean_uppercase = TRUE\n";
    cfg.load();

    REQUIRE(cfg.get<bool>("false_boolean_uppercase") == false);
    REQUIRE(cfg.get<bool>("true_boolean_uppercase") == true);
  }

  SECTION("Test booleans defined as numbers") {
    Config cfg;
    cfg.defineOption<bool>("0_boolean", ConfigValueSettings::REQUIRED);
    cfg.defineOption<bool>("1_boolean", ConfigValueSettings::REQUIRED);

    cfg << "0_boolean = 0\n";
    cfg << "1_boolean = 1\n";
    cfg.load();

    REQUIRE(cfg.get<bool>("0_boolean") == false);
    REQUIRE(cfg.get<bool>("1_boolean") == true);
  }

  SECTION("Test strings") {
    Config cfg;
    cfg.defineOption<std::string>("test_string", ConfigValueSettings::REQUIRED);
    cfg << "test_string = Hello world!\n";
    cfg.load();
    REQUIRE(cfg.get<std::string>("test_string") == "Hello world!");
  }

  SECTION("Test that requesting invalid type throws exception") {
    Config cfg;
    cfg.defineOption<int>("my_number");
    cfg << "my_number = 4\n";
    cfg.load();
    REQUIRE_THROWS_WITH(cfg.get<bool>("my_number"), Contains("Requested invalid type for config parameter"));
  }

  SECTION("Check that we throw exception if option is defined, but empty") {
    Config cfg;
    cfg.defineOption<bool>("my_number2");
    cfg << "my_number =\n";
    REQUIRE_THROWS_AS(cfg.load(), SyntaxErrorException);
  }

  SECTION("Check that we throw exception if = is missing in a option definition") {
    Config cfg;
    cfg.defineOption<int>("my_number");
    cfg << "my_number\n";
    REQUIRE_THROWS_AS(cfg.load(), SyntaxErrorException);
  }

  SECTION("Test that we support comments") {
    Config cfg;
    cfg.defineOption<std::string>("my_option");
    cfg << "# This is my option.\n";
    cfg << "my_option = foobar\n";
    cfg.load();
    REQUIRE(cfg.get<std::string>("my_option") == "foobar");
  }

  SECTION("Test that we support whitespaces") {
    Config cfg;
    cfg.defineOption<std::string>("whitespace_option");
    cfg << "  whitespace_option             =      foobar\n";
    cfg.load();
    REQUIRE(cfg.get<std::string>("whitespace_option") == "foobar");
  }

  SECTION("Throw exception if required value is not set") {
    Config cfg;
    cfg.defineOption<int>("required_option", ConfigValueSettings::REQUIRED);
    REQUIRE_THROWS_AS(cfg.load(), RequiredOptionMissingException);
  }

  SECTION("Check that we support quoted values") {
    Config cfg;
    cfg.defineOption<std::string>("test_string_double_quote", ConfigValueSettings::REQUIRED);
    cfg.defineOption<std::string>("test_string_single_quote", ConfigValueSettings::REQUIRED);
    cfg << "test_string_double_quote = \"Hello world!\"\n";
    cfg << "test_string_single_quote = 'Hello world!'\n";
    cfg.load();

    REQUIRE(cfg.get<std::string>("test_string_double_quote") == "Hello world!");
    REQUIRE(cfg.get<std::string>("test_string_single_quote") == "Hello world!");
  }

  SECTION("Check setting values through envvars") {
    Config cfg;
    cfg.defineOption<int>("int_opt");
    cfg.defineOption<bool>("bool_opt");
    cfg.defineOption<std::string>("string_opt");
    cfg.setLoadFromEnv(true);

    setenv("int_opt", "1337", 0);
    setenv("bool_opt", "true", 0);
    setenv("string_opt", "hello world", 0);

    cfg.load();

    REQUIRE(cfg.get<int>("int_opt") == 1337);
    REQUIRE(cfg.get<bool>("bool_opt") == true);
    REQUIRE(cfg.get<std::string>("string_opt") == "hello world");
  }

  SECTION("Uppercase version of envvar should also work") {
    Config cfg;
    cfg.defineOption<std::string>("my_opt");
    cfg.setLoadFromEnv(true);

    setenv("MY_OPT", "Howdy", 0);

    cfg.load();

    REQUIRE(cfg.get<std::string>("my_opt") == "Howdy");
  }

  SECTION("Test reading from config file that contains all possible types") {
    const char* evilconfig =
    "# Booleans\n                                \
    my_bool_n         =      1\n                 \
    my_bool_n_dquote  =     \"1\"\n              \
    my_bool_n_squote  =     \'0\'\n\n            \
    my_bool           =      true\n              \
    my_bool_dquote    =     \"TRUE\"\n           \
    my_bool_squote    =     \'FALSE\'\n\n        \
    # Integers\n                                 \
    my_int            =      100\n               \
    my_int_dquote     =     \"101\"\n            \
    my_int_squote     =     \'102\'\n\n          \
    # Strings\n                                  \
    my_string         =      Hello world!\n      \
    my_string_dquote  =     \"Hello world!!\"\n  \
    my_string_squote  =     \'Hello world!!!\'";

    unlink("evilconfig_test.conf");
    std::ofstream f;
    f.open("evilconfig_test.conf");
    f << evilconfig;
    f.close();

    Config cfg;
    cfg.defineOption<bool>("my_bool_n");
    cfg.defineOption<bool>("my_bool_n_dquote");
    cfg.defineOption<bool>("my_bool_n_squote");
    cfg.defineOption<bool>("my_bool");
    cfg.defineOption<bool>("my_bool_dquote");
    cfg.defineOption<bool>("my_bool_squote");
    cfg.defineOption<int>("my_int");
    cfg.defineOption<int>("my_int_dquote");
    cfg.defineOption<int>("my_int_squote");
    cfg.defineOption<std::string>("my_string");
    cfg.defineOption<std::string>("my_string_dquote");
    cfg.defineOption<std::string>("my_string_squote");

    cfg.setFile("evilconfig_test.conf");
    cfg.load();

    REQUIRE(cfg.get<bool>("my_bool_n") == true);
    REQUIRE(cfg.get<bool>("my_bool_n_dquote") == true);
    REQUIRE(cfg.get<bool>("my_bool_n_squote") == false);
    REQUIRE(cfg.get<bool>("my_bool") == true);
    REQUIRE(cfg.get<bool>("my_bool_dquote") == true);
    REQUIRE(cfg.get<bool>("my_bool_squote") == false);
    REQUIRE(cfg.get<int>("my_int") == 100);
    REQUIRE(cfg.get<int>("my_int_dquote") == 101);
    REQUIRE(cfg.get<int>("my_int_squote") == 102);
    REQUIRE(cfg.get<std::string>("my_string") == "Hello world!");
    REQUIRE(cfg.get<std::string>("my_string_dquote") == "Hello world!!");
    REQUIRE(cfg.get<std::string>("my_string_squote") == "Hello world!!!");

    unlink("evilconfig_test.conf");
  }

  SECTION("Test ConfigMap") {
    ConfigMap cfgMap = {
      { "listen_port",              ConfigValueType::INT,    "8080",      ConfigValueSettings::REQUIRED },
      { "worker_threads",           ConfigValueType::INT,    "0",         ConfigValueSettings::REQUIRED },
      { "jwt_secret",               ConfigValueType::STRING, "FooBarBaz", ConfigValueSettings::REQUIRED },
      { "log_level",                ConfigValueType::STRING, "info",      ConfigValueSettings::REQUIRED },
      { "disable_auth",             ConfigValueType::STRING, "true",     ConfigValueSettings::REQUIRED },
      { "prometheus_metric_prefix", ConfigValueType::STRING, "eventhub",  ConfigValueSettings::REQUIRED },
      { "redis_host",               ConfigValueType::STRING, "localhost", ConfigValueSettings::REQUIRED },
      { "redis_port",               ConfigValueType::STRING, "6379",      ConfigValueSettings::REQUIRED },
      { "redis_password",           ConfigValueType::STRING, "password",  ConfigValueSettings::REQUIRED },
      { "redis_prefix",             ConfigValueType::STRING, "eventhub",  ConfigValueSettings::REQUIRED },
      { "redis_pool_size",          ConfigValueType::INT,    "5",         ConfigValueSettings::REQUIRED },
      { "enable_cache",             ConfigValueType::BOOL,   "false",     ConfigValueSettings::REQUIRED },
      { "max_cache_length",         ConfigValueType::INT,    "1000",      ConfigValueSettings::REQUIRED },
      { "max_cache_request_limit",  ConfigValueType::INT,    "100",       ConfigValueSettings::REQUIRED },
      { "default_cache_ttl",        ConfigValueType::INT,    "60",        ConfigValueSettings::REQUIRED },
      { "ping_interval",            ConfigValueType::INT,    "30",        ConfigValueSettings::REQUIRED },
      { "handshake_timeout",        ConfigValueType::INT,    "5",         ConfigValueSettings::REQUIRED },
      { "enable_sse",               ConfigValueType::BOOL,   "0",         ConfigValueSettings::REQUIRED },
      { "enable_ssl",               ConfigValueType::BOOL,   "1",         ConfigValueSettings::REQUIRED }
    };

    Config cfg(cfgMap);
    cfg.load();

    for (const auto& cfgOpt : cfgMap ) {
      switch(cfgOpt.type) {
        case ConfigValueType::INT:
          REQUIRE(cfg.get<int>(cfgOpt.name) == stoi(cfgOpt.defaultValue));
        break;

        case ConfigValueType::BOOL:
          if (cfgOpt.defaultValue == "true" || cfgOpt.defaultValue == "1")
            REQUIRE(cfg.get<bool>(cfgOpt.name));
          else if (cfgOpt.defaultValue == "false" || cfgOpt.defaultValue == "0")
            REQUIRE(!cfg.get<bool>(cfgOpt.name));
        break;

        case ConfigValueType::STRING:
          REQUIRE(cfg.get<std::string>(cfgOpt.name) == cfgOpt.defaultValue);
        break;
      }
    }

  }
}
