#include <zmq.hpp>
#include <string>
#include <iostream>
#include "fancy_int.pb.h"

int main () {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_SUB);

  socket.connect("ipc://challenge-out.ipc");
  socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  while (true) {
    zmq::message_t response;
    socket.recv(&response);

    challenge::FancyInt fI = challenge::FancyInt();
    fI.ParseFromArray(response.data(), response.size());

    std::cout<<"received "<<fI.n()<<std::endl;
  }

  return 0;
}