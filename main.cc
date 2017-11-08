#include <node.h>
#include <uv.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <deque>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <zmq.hpp>
#include "fancy_int.pb.h"

using namespace v8;

std::mutex inputVectorAccess;
std::mutex outputQueueAccess;
std::vector<unsigned int> inputs;
std::deque<unsigned int> outputs;
bool outputThreadRunning;

// Helper(s)
static void SendUIntViaSocket(zmq::socket_t* sock, unsigned int n) {
  challenge::FancyInt fI;
  std::string fI_asString;

  fI.set_n(n);
  fI.SerializeToString(&fI_asString);

  zmq::message_t request(fI_asString.length());
  memcpy(request.data(), fI_asString.c_str(), fI_asString.length());

  sock->send(request);
}

// I/O Thread bodies
static void DoInputThread() {
  std::cout<<"Input thread body running..."<<std::endl;

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_SUB);

  socket.connect("ipc://challenge-in.ipc");
  socket.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  while (true) {
    zmq::message_t response;
    socket.recv(&response);

    challenge::FancyInt fI = challenge::FancyInt();
    fI.ParseFromArray(response.data(), response.size());

    inputVectorAccess.lock();
    inputs.push_back(fI.n());
    inputVectorAccess.unlock();
  }
}

static void DoOutputThread() {
  outputThreadRunning = true;

  std::cout<<"Output thread body running..."<<std::endl;

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_PUB);

  socket.bind("ipc://challenge-out.ipc");

  while (outputs.size() > 0) {
    outputQueueAccess.lock();

    std::cout<<"sending "<<outputs[0]<<std::endl;

    SendUIntViaSocket(&socket, outputs[0]);
    outputs.pop_front();

    outputQueueAccess.unlock();
  }

  outputThreadRunning = false;
}

void StartInputThread(const v8::FunctionCallbackInfo<v8::Value>&args) {
  Isolate* isolate = args.GetIsolate();

  std::thread inputThread(DoInputThread);
  inputThread.detach();

  args.GetReturnValue().Set(Undefined(isolate));
}

static void StartOutputThread() {
  std::thread outputThread(DoOutputThread);
  outputThread.detach();
}

void FlushPendingInputs(const v8::FunctionCallbackInfo<v8::Value>&args) {
  Isolate* isolate = args.GetIsolate();
  Local<Array> jsArray = Local<Array>::Cast(args[0]);

  for (unsigned int i = 0; i < inputs.size(); i++) {
    jsArray->Set(i, Integer::New(isolate, inputs[i]));
  }

  inputVectorAccess.lock();
  inputs.clear();
  inputVectorAccess.unlock();

  args.GetReturnValue().Set(Undefined(isolate));
}

void FlushToOutput(const v8::FunctionCallbackInfo<v8::Value>&args) {
  Isolate* isolate = args.GetIsolate();
  Local<Array> outputJSArray = Local<Array>::Cast(args[0]);

  if (outputJSArray->Length() > 0) {
    outputQueueAccess.lock();

    for (unsigned int i = 0; i < outputJSArray->Length(); i++) {
      outputs.push_back(outputJSArray->Get(i)->NumberValue());
    }

    outputQueueAccess.unlock();

    // On-demand output thread
    if (!outputThreadRunning) {
      StartOutputThread();
    }
  }

  args.GetReturnValue().Set(Undefined(isolate));
}

void init(Local<Object> exports) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  outputThreadRunning = false;

  NODE_SET_METHOD(exports, "startInputThread", StartInputThread);
  NODE_SET_METHOD(exports, "flushPendingInputs", FlushPendingInputs);
  NODE_SET_METHOD(exports, "flushToOutput", FlushToOutput);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
