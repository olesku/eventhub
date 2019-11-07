#include "RPCHandler.hpp"
#include "Common.hpp"
#include "Connection.hpp"
#include "ConnectionWorker.hpp"
#include "HandlerContext.hpp"
#include "Redis.hpp"
#include "Server.hpp"
#include "TopicManager.hpp"
#include "Util.hpp"
#include "websocket/Response.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace eventhub {

/**
 * Get handler function for RPC method.
 * @param methodName Name of RPC method.
 * @throws std::bad_function_call if an invalid method is requested.
 */
RPCMethod RPCHandler::getHandler(const std::string& methodName) {
  static RPCHandlerList handlers = {
      {"subscribe", _handleSubscribe},
      {"unsubscribe", _handleUnsubscribe},
      {"unsubscribeAll", _handleUnsubscribeAll},
      {"publish", _handlePublish},
      {"list", _handleList},
      {"history", _handleHistory},
      {"disconnect", _handleDisconnect}};

  std::string methodNameLC = methodName;
  Util::strToLower(methodNameLC);

  for (auto handler : handlers) {
    if (methodNameLC == handler.first) {
      return handler.second;
    }
  }

  throw std::bad_function_call();
}

void RPCHandler::_sendInvalidParamsError(HandlerContext& ctx, jsonrpcpp::request_ptr req, const std::string& message) {
  websocket::response::sendData(ctx.connection(),
                                jsonrpcpp::Response(jsonrpcpp::InvalidParamsException(message, req->id())).to_json().dump(),
                                websocket::FrameType::TEXT_FRAME);
}

void RPCHandler::_sendSuccessResponse(HandlerContext& ctx, jsonrpcpp::request_ptr req, const nlohmann::json& result) {
  websocket::response::sendData(ctx.connection(),
                                jsonrpcpp::Response(*req, result).to_json().dump(),
                                websocket::FrameType::TEXT_FRAME);
}

/**
 * Handle subscribe RPC command.
 * Subscribe client to given topic pattern.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handleSubscribe(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  auto& accessController = ctx.connection()->getAccessController();
  auto params            = req->params();
  std::string topicName;
  std::string sinceEvent;
  std::stringstream msg;

  try {
    topicName = params.get("topic").get<std::string>();
    // TODO: Do some sinceEvent validation. Should only contain [0-9\-]+.
    sinceEvent = params.get("sinceEvent").get<std::string>();
  } catch (...) {
  }

  if (topicName.empty()) {
    _sendInvalidParamsError(ctx, req, "You must specify 'topic' to subscribe to.");
    return;
  }

  if (!TopicManager::isValidTopicFilter(topicName)) {
    msg << "Invalid topic in request: " << topicName;
    _sendInvalidParamsError(ctx, req, msg.str());
    return;
  }

  if (!accessController.allowSubscribe(topicName)) {
    msg << "You are not allowed to subscribe to topic: " << topicName;
    _sendInvalidParamsError(ctx, req, msg.str());
    return;
  }

  ctx.connection()->subscribe(topicName, req->id());

  nlohmann::json result;
  result["action"] = "subscribe";
  result["topic"]  = topicName;
  result["status"] = "ok";

  //DLOG(INFO) << "Client " << ctx.connection()->getIP() << " subscribed to " << topicName << " request id: " << req->id();
  _sendSuccessResponse(ctx, req, result);

  // Send cached events if sinceEvent is set.
  if (!sinceEvent.empty()) {
    try {
      //DLOG(INFO) << "Sending cache since '" << sinceEvent << "' to client " << ctx.connection()->getIP() << " topic: " << topicName << " request id: " << req->id();
      nlohmann::json result;
      auto& redis      = ctx.server()->getRedis();
      size_t cacheSize = redis.getCache(topicName, sinceEvent, 0, result);

      for (auto& cacheItem : result) {
        _sendSuccessResponse(ctx, req, cacheItem);
      }
    } catch (std::exception& e) {
      LOG(ERROR) << "Redis error while looking up cache " << e.what();
    }
  }
}

/**
 * Handle unsubscribe RPC command.
 * Unsubscribe client from given topic pattern.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handleUnsubscribe(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  auto& accessController = ctx.connection()->getAccessController();

  if (!req->params().is_array()) {
    _sendInvalidParamsError(ctx, req, "Parameter is not array of topics to unsubscribe from.");
    return;
  }

  auto topics        = req->params().to_json();
  unsigned int count = 0;
  for (auto topic : topics) {
    if (!TopicManager::isValidTopicFilter(topic) || !accessController.allowSubscribe(topic)) {
      continue;
    }

    if (ctx.connection()->unsubscribe(topic)) {
      count++;
    }
  }

  nlohmann::json result;
  result["unsubscribe_count"] = count;

  _sendSuccessResponse(ctx, req, result);
}

/**
 * Handle unsubscribeAll RPC command.
 * Unsubscribe client from all subscribed topics.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handleUnsubscribeAll(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  nlohmann::json result;
  result["unsubscribe_count"] = ctx.connection()->unsubscribeAll();

  _sendSuccessResponse(ctx, req, result);
}

/**
 * Handle publish RPC command.
 * Publish message to topic.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handlePublish(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  std::string topicName;
  std::string message;
  std::stringstream msg;

  auto& accessController = ctx.connection()->getAccessController();
  auto params            = req->params();

  try {
    topicName = params.get("topic").get<std::string>();
    message   = params.get("message").get<std::string>();
  } catch (...) {}

  if (topicName.empty() || message.empty()) {
    msg << "You need to specify topic and message to publish to.";
    _sendInvalidParamsError(ctx, req, msg.str());
    return;
  }

  if (!accessController.allowPublish(topicName)) {
    msg << "Insufficient access to topic: " << topicName;
    _sendInvalidParamsError(ctx, req, msg.str());
    return;
  }

  if (!TopicManager::isValidTopicFilter(topicName)) {
    msg << topicName << " is not a valid topic.";
    _sendInvalidParamsError(ctx, req, msg.str());
    return;
  }

  try {
    auto& redis = ctx.server()->getRedis();
    auto id     = redis.cacheMessage(topicName, message);

    if (id.length() == 0) {
      msg << "Failed to cache message to Redis, discarding.";
      _sendInvalidParamsError(ctx, req, msg.str());
      return;
    }

    redis.publishMessage(topicName, id, message);

    nlohmann::json result;
    result["action"] = "publish";
    result["topic"]  = topicName;
    result["id"]     = id;
    result["status"] = "ok";

    _sendSuccessResponse(ctx, req, result);
  } catch (std::exception& e) {
    LOG(ERROR) << "Redis error while publishing message: " << e.what();
    msg << "Redis error while publishing message: " << e.what();
    _sendInvalidParamsError(ctx, req, msg.str());
  }
}

/**
 * Handle list RPC command.
 * List all subscribed topics for client.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handleList(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  nlohmann::json j = nlohmann::json::array();

  for (auto subscription : ctx.connection()->listSubscriptions()) {
    j.push_back(subscription);
  }

  _sendSuccessResponse(ctx, req, j);
}

/**
 * Handle history RPC command.
 * Send history cache for topic to client.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handleHistory(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  LOG(INFO) << req->to_json().dump(2);
}

/**
 * Disconnect the client.
 * @param ctx Client issuing request.
 * @param req RPC request.
 */
void RPCHandler::_handleDisconnect(HandlerContext& ctx, jsonrpcpp::request_ptr req) {
  websocket::response::sendData(ctx.connection(), "", websocket::FrameType::CLOSE_FRAME);
  ctx.connection()->isShutdown();
}

} // namespace eventhub
