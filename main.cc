#include <node.h>
#include <uv.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <zmq.hpp>
#include "fancy_int.pb.h"

using namespace v8;

std::mutex inputVectorAccess;
std::mutex outputVectorAccess;
std::vector<unsigned int> inputs;
std::vector<unsigned int> outputs;

// I/O Thread bodies
static void DoInputThread(Isolate* isolate) {
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

static void DoOutputThread(Isolate* isolate) {
  std::cout<<"Output thread body running..."<<std::endl;

  zmq::context_t context(1);
  zmq::socket_t socket(context, ZMQ_PUB);

  socket.bind("ipc://challenge-out.ipc");

  challenge::FancyInt fI;
  std::string fI_asString;

  while (true) {
    outputVectorAccess.lock();

    for (unsigned int i = 0; i < outputs.size(); i++) {
      fI.set_n(outputs[i]);
      fI.SerializeToString(&fI_asString);

      zmq::message_t request(fI_asString.length());
      memcpy(request.data(), fI_asString.c_str(), fI_asString.length());

      socket.send(request);
    }

    outputs.clear();
    outputVectorAccess.unlock();
  }
}

void StartInputThread(const v8::FunctionCallbackInfo<v8::Value>&args) {
  Isolate* isolate = args.GetIsolate();

  std::thread inputThread(DoInputThread, isolate);
  inputThread.detach();

  args.GetReturnValue().Set(Undefined(isolate));
}

void StartOutputThread(const v8::FunctionCallbackInfo<v8::Value>&args) {
  Isolate* isolate = args.GetIsolate();

  std::thread outputThread(DoOutputThread, isolate);
  outputThread.detach();

  args.GetReturnValue().Set(Undefined(isolate));
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

  outputVectorAccess.lock();

  for (unsigned int i = 0; i < outputJSArray->Length(); i++) {
    outputs.push_back(outputJSArray->Get(i)->NumberValue());
  }

  outputVectorAccess.unlock();

  args.GetReturnValue().Set(Undefined(isolate));
}

void init(Local<Object> exports) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  NODE_SET_METHOD(exports, "startInputThread", StartInputThread);
  NODE_SET_METHOD(exports, "startOutputThread", StartOutputThread);
  NODE_SET_METHOD(exports, "flushPendingInputs", FlushPendingInputs);
  NODE_SET_METHOD(exports, "flushToOutput", FlushToOutput);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
