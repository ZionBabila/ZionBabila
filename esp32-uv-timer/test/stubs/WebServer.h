#pragma once
#include "Arduino.h"
#include <map>
enum HTTPMethod { HTTP_GET = 1, HTTP_POST = 2 };
// Test double: the test sets args, calls a handler, then reads code/body.
struct WebServer {
  std::map<std::string, std::string> args;
  int code = 0; std::string body, type;
  std::map<std::string, std::string> headers;
  WebServer(int) {}
  void on(const char *, HTTPMethod, void (*)()) {}
  void onNotFound(void (*)()) {}
  void begin() {}
  void handleClient() {}
  void reset() { args.clear(); code = 0; body.clear(); headers.clear(); }
  bool hasArg(const char *k) { return args.count(k); }
  String arg(const char *k) { return args.count(k) ? String(args[k]) : String(""); }
  void sendHeader(const char *k, const char *v, bool = false) { headers[k] = v; }
  void send(int c, const char *t, const String &b) { code = c; type = t; body = b.s; }
  void send(int c, const char *t, const char *b) { code = c; type = t; body = b; }
  void send_P(int c, const char *t, const char *b) { code = c; type = t; body = b; }
};
