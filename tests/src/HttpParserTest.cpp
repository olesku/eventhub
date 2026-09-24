#include <string>
#include <utility>
#include <vector>

#include "catch.hpp"
#include "http/Parser.hpp"
#include "http/Request.hpp"
#include "http/Types.hpp"

namespace eventhub::http {

TEST_CASE("HTTP parser reports only complete requests", "[http]") {
  std::size_t requestCount = 0;
  std::vector<ParseError> errors;
  std::string method;
  std::string path;
  std::string host;
  std::string token;

  ParserCallbacks callbacks;
  callbacks.onRequest = [&](const Request& request) {
    ++requestCount;
    method = request.method();
    path   = request.path();
    host   = request.header("HOST");
    token  = request.queryParameter("Token");
  };
  callbacks.onError = [&](ParseError error) { errors.push_back(error); };
  Parser parser(std::move(callbacks));

  const std::string first = "GET /events?token=abc";
  parser.parse(first.data(), first.size());
  REQUIRE(requestCount == 0);
  REQUIRE(errors.empty());

  const std::string second = " HTTP/1.1\r\nHost: example.test\r\n\r\n";
  parser.parse(second.data(), second.size());
  REQUIRE(requestCount == 1);
  REQUIRE(errors.empty());
  REQUIRE(method == "GET");
  REQUIRE(path == "/events");
  REQUIRE(host == "example.test");
  REQUIRE(token == "abc");
}

TEST_CASE("HTTP parser separates parse errors from requests", "[http]") {
  std::size_t requestCount = 0;
  std::vector<ParseError> errors;

  ParserCallbacks callbacks;
  callbacks.onRequest = [&](const Request&) { ++requestCount; };
  callbacks.onError   = [&](ParseError error) { errors.push_back(error); };

  SECTION("invalid request") {
    Parser parser(std::move(callbacks));
    const std::string input = "invalid\r\n\r\n";
    parser.parse(input.data(), input.size());
    parser.parse(input.data(), input.size());
    REQUIRE(requestCount == 0);
    REQUIRE(errors == std::vector<ParseError>{ParseError::INVALID_REQUEST});
  }

  SECTION("request too large") {
    Parser parser(std::move(callbacks));
    const std::string input(8193, 'x');
    parser.parse(input.data(), input.size());
    parser.parse(input.data(), input.size());
    REQUIRE(requestCount == 0);
    REQUIRE(errors == std::vector<ParseError>{ParseError::REQUEST_TOO_LARGE});
  }
}

TEST_CASE("HTTP parser resets request data between messages", "[http]") {
  std::vector<std::string> paths;
  std::vector<std::string> staleHeaders;

  ParserCallbacks callbacks;
  callbacks.onRequest = [&](const Request& request) {
    paths.push_back(request.path());
    staleHeaders.push_back(request.header("x-first"));
  };
  Parser parser(std::move(callbacks));

  const std::string first  = "GET /first HTTP/1.1\r\nX-First: value\r\n\r\n";
  const std::string second = "GET /second HTTP/1.1\r\nHost: example.test\r\n\r\n";
  parser.parse(first.data(), first.size());
  parser.parse(second.data(), second.size());

  REQUIRE(paths == std::vector<std::string>{"/first", "/second"});
  REQUIRE(staleHeaders == std::vector<std::string>{"value", ""});
}

TEST_CASE("HTTP parser reports the exact header boundary", "[http]") {
  std::size_t requests = 0;
  Parser parser({[&](const Request&) { ++requests; }, {}});
  const std::string header = "GeT /events HTTP/1.1\r\nHost: example.test\r\n\r\n";
  const std::string suffix(9000, 'x');
  const auto result = parser.parse((header + suffix).data(), header.size() + suffix.size());

  REQUIRE(result.status == ParseStatus::COMPLETE);
  REQUIRE(result.consumed == header.size());
  REQUIRE(requests == 1);
}

TEST_CASE("HTTP parser counts fragmented headers but not trailing protocol bytes", "[http]") {
  std::size_t requests = 0;
  Parser parser({[&](const Request&) { ++requests; }, {}});
  const std::string first  = "GET /events HTTP/1.1\r\nX-Long: " + std::string(8000, 'a');
  const std::string second = "\r\n\r\n" + std::string(1000, 'b');

  const auto incomplete = parser.parse(first.data(), first.size());
  REQUIRE(incomplete.status == ParseStatus::NEED_MORE);
  REQUIRE(incomplete.consumed == first.size());
  const auto complete = parser.parse(second.data(), second.size());
  REQUIRE(complete.status == ParseStatus::COMPLETE);
  REQUIRE(complete.consumed == 4);
  REQUIRE(requests == 1);
}

} // namespace eventhub::http
