#ifndef EVENTHUB_TOPIC_HPP
#define EVENTHUB_TOPIC_HPP

#include <memory>
#include <deque>
#include "connection.hpp"

namespace eventhub {
  class topic { 
    public:
      topic(const std::string& topic_name) { _id = topic_name; };
      ~topic() {};

      void add_subscriber(std::shared_ptr<io::connection>& conn);
      void publish(const string& data);
      size_t garbage_collect();

    private:
      std::string _id;
      uint64_t _n_messages_sent;
      std::deque<std::weak_ptr<io::connection>> _subscriber_list;
  };
};

#endif
