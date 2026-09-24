#pragma once

#include <functional>
#include <memory>

#include "Config.hpp"
#include "EventhubBase.hpp"
#include "Forward.hpp"
#include "metrics/Types.hpp"

namespace eventhub {

class HandlerContext final : public EventhubBase {
public:
  HandlerContext(Config& config, Redis& redis, KVStore& kvStore,
                 std::function<metrics::AggregatedMetrics()> metricsSnapshot,
                 std::shared_ptr<Connection> connection) : EventhubBase(config), _redis(redis), _kv_store(kvStore),
                                                           _metrics_snapshot(std::move(metricsSnapshot)), _connection(std::move(connection)) {}

  ~HandlerContext() {}

  Redis& redis() { return _redis; }
  KVStore& kvStore() { return _kv_store; }
  metrics::AggregatedMetrics metricsSnapshot() { return _metrics_snapshot(); }
  std::shared_ptr<Connection> connection() { return _connection; }

private:
  Redis& _redis;
  KVStore& _kv_store;
  std::function<metrics::AggregatedMetrics()> _metrics_snapshot;
  std::shared_ptr<Connection> _connection;
};

} // namespace eventhub
