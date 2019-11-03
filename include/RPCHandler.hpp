#ifndef EVENTHUB_RPC_HANDLER_HPP
#define EVENTHUB_RPC_HANDLER_HPP

#include "jsonrpc/jsonrpcpp.hpp"
#include "HandlerContext.hpp"
#include "Connection.hpp"
#include <vector>
#include <functional>

namespace eventhub {
using RPCMethod = std::function<void(HandlerContext& hCtx, jsonrpcpp::request_ptr)>;
using RPCHandlerList = std::vector<std::pair<std::string, RPCMethod>>;

class RPCHandler {
  public:
    static RPCMethod getHandler(const std::string& methodName);

  private:
    RPCHandler();
    ~RPCHandler();

    static void _sendSuccessResponse(HandlerContext& hCtx, jsonrpcpp::request_ptr req, const nlohmann::json& result);
    static void _sendInvalidParamsError(HandlerContext& hCtx, jsonrpcpp::request_ptr req, const std::string& message);

    static void _handleSubscribe(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
    static void _handleUnsubscribe(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
    static void _handleUnsubscribeAll(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
    static void _handlePublish(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
    static void _handleList(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
    static void _handleHistory(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
    static void _handleDisconnect(HandlerContext& hCtx, jsonrpcpp::request_ptr req);
};

}

#endif
