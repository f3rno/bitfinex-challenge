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
std::vector<unsigned int> inputs;
std::deque<unsigned int> outputs;
bool outputThreadRunning;

zmq::context_t zmqContext(1);
zmq::socket_t pushSocket(zmqContext, ZMQ_PUB);
zmq::socket_t pullSocket(zmqContext, ZMQ_SUB);

// Helper(s)
static void SendUIntViaPushSocket(unsigned int n) {
  challenge::FancyInt fI;
  std::string fI_asString;

  fI.set_n(n);
  fI.SerializeToString(&fI_asString);

  zmq::message_t request(fI_asString.length());
  memcpy(request.data(), fI_asString.c_str(), fI_asString.length());

  pushSocket.send(request);
}

static unsigned int ReceiveUIntViaPullSocket() {
  zmq::message_t response;
  pullSocket.recv(&response);

  challenge::FancyInt fI = challenge::FancyInt();
  fI.ParseFromArray(response.data(), response.size());

  return fI.n();
}

// I/O Thread bodies
static void DoInputThread() {
  while (true) {
    unsigned int n = ReceiveUIntViaPullSocket();

    inputVectorAccess.lock();
    inputs.push_back(n);
    inputVectorAccess.unlock();
  }
}

static void DoOutputThread() {
  outputThreadRunning = true;

  while (outputs.size() > 0) {
    SendUIntViaPushSocket(outputs[0]);
    outputs.pop_front();
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

  inputVectorAccess.lock();

  for (unsigned int i = 0; i < inputs.size(); i++) {
    jsArray->Set(i, Integer::New(isolate, inputs[i]));
  }

  inputs.clear();
  inputVectorAccess.unlock();

  args.GetReturnValue().Set(Undefined(isolate));
}

void FlushToOutput(const v8::FunctionCallbackInfo<v8::Value>&args) {
  Isolate* isolate = args.GetIsolate();
  Local<Array> outputJSArray = Local<Array>::Cast(args[0]);

  if (outputJSArray->Length() > 0) {
    for (unsigned int i = 0; i < outputJSArray->Length(); i++) {
      outputs.push_back(outputJSArray->Get(i)->NumberValue());
    }

    // On-demand output thread
    if (!outputThreadRunning) {

      // To be safe, in case we are called before the thread is scheduled
      outputThreadRunning = true;

      StartOutputThread();
    }
  }

  args.GetReturnValue().Set(Undefined(isolate));
}

void init(Local<Object> exports) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  outputThreadRunning = false;

  pushSocket.bind("ipc://challenge-out.ipc");
  pullSocket.connect("ipc://challenge-in.ipc");
  pullSocket.setsockopt(ZMQ_SUBSCRIBE, "", 0);

  NODE_SET_METHOD(exports, "startInputThread", StartInputThread);
  NODE_SET_METHOD(exports, "flushPendingInputs", FlushPendingInputs);
  NODE_SET_METHOD(exports, "flushToOutput", FlushToOutput);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, init)
