#ifndef SRC_INSPECTOR_AGENT_H_
#define SRC_INSPECTOR_AGENT_H_

#include <stddef.h>

#if !HAVE_INSPECTOR
#error("This header can only be used when inspector is enabled")
#endif

#include "node_debug_options.h"

// Forward declaration to break recursive dependency chain with src/env.h.
namespace node {
class Environment;
}  // namespace node

namespace v8 {
class Platform;
template<typename T>
class Local;
class Value;
class Message;
}  // namespace v8

namespace node {
namespace inspector {

class AgentImpl;

class Agent {
 public:
  explicit Agent(node::Environment* env);
  ~Agent();

  // Start the inspector agent thread
  bool Start(v8::Platform* platform, const char* path,
             const DebugOptions& options);
  // Stop the inspector agent
  void Stop();

  bool IsStarted();
  bool IsConnected();
  void WaitForDisconnect();
  void FatalException(v8::Local<v8::Value> error,
                      v8::Local<v8::Message> message);
  void SchedulePauseOnNextStatement(const std::string& reason);
 private:
  AgentImpl* impl;
};

}  // namespace inspector
}  // namespace node

#endif  // SRC_INSPECTOR_AGENT_H_
