{
  "targets": [
    {
      "target_name": "addon",
      "sources": ["main.cc", "fancy_int.pb.cc"],
      "libraries": ["-lzmq", "-lprotobuf"],
      "cflags_cc!": ["-fno-exceptions", "-fno-rtti"],
      "cflags!": ["-fno-exceptions"]
    }
  ]
}