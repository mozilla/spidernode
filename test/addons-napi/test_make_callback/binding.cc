#include <node_api.h>
#include "../common.h"
#include <vector>

namespace {

napi_value MakeCallback(napi_env env, napi_callback_info info) {
  const int kMaxArgs = 10;
  size_t argc = kMaxArgs;
  napi_value args[kMaxArgs];
  // NOLINTNEXTLINE (readability/null_usage)
  NAPI_CALL(env, napi_get_cb_info(env, info, &argc, args, NULL, NULL));

  NAPI_ASSERT(env, argc > 0, "Wrong number of arguments");

  napi_value recv = args[0];
  napi_value func = args[1];

  std::vector<napi_value> argv;
  for (size_t n = 2; n < argc; n += 1) {
    argv.push_back(args[n]);
  }

  napi_valuetype func_type;

  NAPI_CALL(env, napi_typeof(env, func, &func_type));

  napi_value resource_name;
  NAPI_CALL(env, napi_create_string_utf8(
      env, "test", NAPI_AUTO_LENGTH, &resource_name));

  napi_async_context context;
  NAPI_CALL(env, napi_async_init(env, func, resource_name, &context));

  napi_value result;
  if (func_type == napi_function) {
    NAPI_CALL(env, napi_make_callback(
        env, context, recv, func, argv.size(), argv.data(), &result));
  } else {
    NAPI_ASSERT(env, false, "Unexpected argument type");
  }

  NAPI_CALL(env, napi_async_destroy(env, context));

  return result;
}

napi_value Init(napi_env env, napi_value exports) {
  napi_value fn;
  NAPI_CALL(env, napi_create_function(
      // NOLINTNEXTLINE (readability/null_usage)
      env, NULL, NAPI_AUTO_LENGTH, MakeCallback, NULL, &fn));
  NAPI_CALL(env, napi_set_named_property(env, exports, "makeCallback", fn));
  return exports;
}

}  // namespace

NAPI_MODULE(NODE_GYP_MODULE_NAME, Init)
