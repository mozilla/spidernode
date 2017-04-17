#include "node_debug_options.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include "util.h"

namespace node {

namespace {
const int default_debugger_port = 5858;
#if HAVE_INSPECTOR
const int default_inspector_port = 9229;
#endif  // HAVE_INSPECTOR

inline std::string remove_brackets(const std::string& host) {
  if (!host.empty() && host.front() == '[' && host.back() == ']')
    return host.substr(1, host.size() - 2);
  else
    return host;
}

int parse_and_validate_port(const std::string& port) {
  char* endptr;
  errno = 0;
  const long result = strtol(port.c_str(), &endptr, 10);  // NOLINT(runtime/int)
  if (errno != 0 || *endptr != '\0'|| result < 1024 || result > 65535) {
    fprintf(stderr, "Debug port must be in range 1024 to 65535.\n");
    exit(12);
  }
  return static_cast<int>(result);
}

std::pair<std::string, int> split_host_port(const std::string& arg) {
  // remove_brackets only works if no port is specified
  // so if it has an effect only an IPv6 address was specified
  std::string host = remove_brackets(arg);
  if (host.length() < arg.length())
    return {host, -1};

  size_t colon = arg.rfind(':');
  if (colon == std::string::npos) {
    // Either a port number or a host name.  Assume that
    // if it's not all decimal digits, it's a host name.
    for (char c : arg) {
      if (c < '0' || c > '9') {
        return {arg, -1};
      }
    }
    return {"", parse_and_validate_port(arg)};
  }
  // host and port found
  return std::make_pair(remove_brackets(arg.substr(0, colon)),
                        parse_and_validate_port(arg.substr(colon + 1)));
}

}  // namespace

DebugOptions::DebugOptions() : debugger_enabled_(false),
#if HAVE_INSPECTOR
                               inspector_enabled_(false),
#endif  // HAVE_INSPECTOR
                               wait_connect_(false), http_enabled_(false),
                               host_name_("127.0.0.1"), port_(-1) { }

void DebugOptions::EnableDebugAgent(DebugAgentType tool) {
  switch (tool) {
#if HAVE_INSPECTOR
    case DebugAgentType::kInspector:
      inspector_enabled_ = true;
      debugger_enabled_ = true;
      break;
#endif  // HAVE_INSPECTOR
    case DebugAgentType::kDebugger:
      debugger_enabled_ = true;
      break;
    case DebugAgentType::kNone:
      break;
  }
}

bool DebugOptions::ParseOption(const std::string& option) {
  bool enable_inspector = false;
  bool has_argument = false;
  std::string option_name;
  std::string argument;

  auto pos = option.find("=");
  if (pos == std::string::npos) {
    option_name = option;
  } else {
    has_argument = true;
    option_name = option.substr(0, pos);
    argument = option.substr(pos + 1);
  }

  if (option_name == "--inspect") {
    debugger_enabled_ = true;
    enable_inspector = true;
  } else if (option_name == "--inspect-brk") {
    debugger_enabled_ = true;
    enable_inspector = true;
    wait_connect_ = true;
  } else if ((option_name != "--debug-port" &&
              option_name != "--inspect-port") ||
              !has_argument) {
    // only other valid possibility is --inspect-port,
    // which requires an argument
    return false;
  }

  if (enable_inspector) {
#if HAVE_INSPECTOR
    inspector_enabled_ = true;
#else
    fprintf(stderr,
            "Inspector support is not available with this Node.js build\n");
    return false;
#endif
  }

  // argument can be specified for *any* option to specify host:port
  if (has_argument) {
    std::pair<std::string, int> host_port = split_host_port(argument);
    if (!host_port.first.empty()) {
      host_name_ = host_port.first;
    }
    if (host_port.second >= 0) {
      port_ = host_port.second;
    }
  }

  return true;
}

int DebugOptions::port() const {
  int port = port_;
  if (port < 0) {
    port = default_debugger_port;
#if HAVE_INSPECTOR
    if (!debugger_enabled_ || inspector_enabled_)
      port = default_inspector_port;
#endif  // HAVE_INSPECTOR
  }
  return port;
}

}  // namespace node
