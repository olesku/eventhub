#include <iostream>
#include "websocket_response.hpp"

using namespace std;

int main(int argc, char* argv[]) {
  eventhub::websocket::response resp("Hello world!");

  cout << resp.ws_format() << std::endl; 
}
