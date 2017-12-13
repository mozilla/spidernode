#ifndef SRC_TRACING_AGENT_H_
#define SRC_TRACING_AGENT_H_

#include "libplatform/v8-tracing.h"
#include "uv.h"
#include "v8.h"

namespace node {
namespace tracing {

using v8::platform::tracing::TracingController;

class Agent {
 public:
  Agent();
  void Start(const std::string& enabled_categories);
  void Stop();

  TracingController* GetTracingController() { return tracing_controller_; }

 private:
  static void ThreadCb(void* arg);

  uv_thread_t thread_;
  uv_loop_t tracing_loop_;
  bool started_ = false;
  TracingController* tracing_controller_ = nullptr;
};

}  // namespace tracing
}  // namespace node

#endif  // SRC_TRACING_AGENT_H_
