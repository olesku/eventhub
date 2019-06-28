#include <string>
#include <memory>
#include "connection.hpp"

namespace eventhub {
  class AccessController {
    typedef enum {
      PUBLISH_ACCESS,
      SUBSCRIBE_ACCESS
    } ACCESS_TYPE;

    private:

    public:
      AccessController();
      ~AccessController();

      static bool hasAccess(std::shared_ptr<Connection>& conn, ACCESS_TYPE type, const std::string& topic);
  };
}
