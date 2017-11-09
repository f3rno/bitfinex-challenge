#include <zmq.hpp>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include "fancy_int.pb.h"

int main () {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_PUB);

  socket.bind("ipc://challenge-in.ipc");

  unsigned int i = 0;

  while (true) {
    challenge::FancyInt fI;
    std::string fI_asString;

    fI.set_n(i);
    fI.SerializeToString(&fI_asString);

    zmq::message_t request(fI_asString.length());
    memcpy(request.data(), fI_asString.c_str(), fI_asString.length());

    socket.send(request);

    std::cout<<"sent "<<i<<std::endl;
    i++;
  }

  return 0;
}