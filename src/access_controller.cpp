#include "access_controller.hpp"

namespace eventhub {

AccessController::AccessController() {

}

AccessController::~AccessController() {

}

bool AccessController::hasAccess(std::shared_ptr<Connection>& conn, ACCESS_TYPE type, const std::string& topic) {
  return false;
}

}
