#pragma once

#include <cstddef>
#include <map>
#include <string>

namespace eventhub::http {

class Parser;

class Request final {
public:
  const std::string& method() const { return _method; }
  const std::string& path() const { return _path; }
  const std::map<std::string, std::string>& headers() const { return _headers; }
  std::string header(std::string name) const;
  std::string queryParameter(std::string name) const;
  std::size_t queryParameterCount() const { return _query_parameters.size(); }

private:
  friend class Parser;

  void _reset();
  void _parseQueryString(const std::string& query);

  std::string _method;
  std::string _path;
  std::map<std::string, std::string> _headers;
  std::map<std::string, std::string> _query_parameters;
};

} // namespace eventhub::http
