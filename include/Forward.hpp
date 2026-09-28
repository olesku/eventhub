namespace eventhub {
class Config;
class Connection;
class HandlerContext;
class KVStore;
class Redis;
class Server;
class Topic;
class Worker;
class TopicManager;
class AccessController;

namespace http {
class Parser;
class Request;
class Response;
enum class ParseError;
}

namespace websocket {
class Handler;
class Parser;
class Response;
}

namespace sse {
class Handler;
class Response;
}
}
