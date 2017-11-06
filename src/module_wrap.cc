#include <algorithm>
#include <limits.h>  // PATH_MAX
#include <sys/stat.h>  // S_IFDIR
#include "module_wrap.h"

#include "env.h"
#include "node_url.h"
#include "util-inl.h"
#include "node_internals.h"

namespace node {
namespace loader {

using node::url::URL;
using node::url::URL_FLAGS_FAILED;
using v8::Context;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::Integer;
using v8::IntegrityLevel;
using v8::Isolate;
using v8::JSON;
using v8::Just;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::Module;
using v8::Nothing;
using v8::Object;
using v8::Promise;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::String;
using v8::TryCatch;
using v8::Value;

static const char* const EXTENSIONS[] = {".mjs", ".js", ".json", ".node"};

ModuleWrap::ModuleWrap(Environment* env,
                       Local<Object> object,
                       Local<Module> module,
                       Local<String> url) : BaseObject(env, object) {
  module_.Reset(env->isolate(), module);
  url_.Reset(env->isolate(), url);
}

ModuleWrap::~ModuleWrap() {
  HandleScope scope(env()->isolate());
  Local<Module> module = module_.Get(env()->isolate());
  auto range = env()->module_map.equal_range(module->GetIdentityHash());
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second == this) {
      env()->module_map.erase(it);
      break;
    }
  }

  module_.Reset();
}

void ModuleWrap::New(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Isolate* isolate = args.GetIsolate();

  if (!args.IsConstructCall()) {
    env->ThrowError("constructor must be called using new");
    return;
  }

  if (args.Length() != 2) {
    env->ThrowError("constructor must have exactly 2 arguments "
                    "(string, string)");
    return;
  }

  if (!args[0]->IsString()) {
    env->ThrowError("first argument is not a string");
    return;
  }

  Local<String> source_text = args[0].As<String>();

  if (!args[1]->IsString()) {
    env->ThrowError("second argument is not a string");
    return;
  }

  Local<String> url = args[1].As<String>();

  Local<Module> module;

  // compile
  {
    ScriptOrigin origin(url,
                        Integer::New(isolate, 0),             // line offset
                        Integer::New(isolate, 0),             // column offset
                        False(isolate),                       // is cross origin
                        Local<Integer>(),                     // script id
                        Local<Value>(),                       // source map URL
                        False(isolate),                       // is opaque (?)
                        False(isolate),                       // is WASM
                        True(isolate));                       // is ES6 module
    ScriptCompiler::Source source(source_text, origin);
    if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module))
      return;
  }

  Local<Object> that = args.This();
  Local<Context> context = that->CreationContext();
  Local<String> url_str = FIXED_ONE_BYTE_STRING(isolate, "url");

  if (!that->Set(context, url_str, url).FromMaybe(false)) {
    return;
  }

  ModuleWrap* obj = new ModuleWrap(env, that, module, url);

  env->module_map.emplace(module->GetIdentityHash(), obj);
  Wrap(that, obj);

  that->SetIntegrityLevel(context, IntegrityLevel::kFrozen);
  args.GetReturnValue().Set(that);
}

void ModuleWrap::Link(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  if (!args[0]->IsFunction()) {
    env->ThrowError("first argument is not a function");
    return;
  }

  Local<Function> resolver_arg = args[0].As<Function>();

  Local<Object> that = args.This();
  ModuleWrap* obj = Unwrap<ModuleWrap>(that);
  CHECK_NE(obj, nullptr);
  Local<Context> mod_context = that->CreationContext();
  if (obj->linked_) return;
  obj->linked_ = true;
  Local<Module> module(obj->module_.Get(isolate));

  // call the dependency resolve callbacks
  for (int i = 0; i < module->GetModuleRequestsLength(); i++) {
    Local<String> specifier = module->GetModuleRequest(i);
    Utf8Value specifier_utf8(env->isolate(), specifier);
    std::string specifier_std(*specifier_utf8, specifier_utf8.length());

    Local<Value> argv[] = {
      specifier
    };

    MaybeLocal<Value> maybe_resolve_return_value =
        resolver_arg->Call(mod_context, that, 1, argv);
    if (maybe_resolve_return_value.IsEmpty()) {
      return;
    }
    Local<Value> resolve_return_value =
        maybe_resolve_return_value.ToLocalChecked();
    if (!resolve_return_value->IsPromise()) {
      env->ThrowError("linking error, expected resolver to return a promise");
    }
    Local<Promise> resolve_promise = resolve_return_value.As<Promise>();
    obj->resolve_cache_[specifier_std].Reset(env->isolate(), resolve_promise);
  }

  args.GetReturnValue().Set(that);
}

void ModuleWrap::Instantiate(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Object> that = args.This();
  Local<Context> context = that->CreationContext();

  ModuleWrap* obj = Unwrap<ModuleWrap>(that);
  CHECK_NE(obj, nullptr);
  Local<Module> module = obj->module_.Get(isolate);
  Maybe<bool> ok =
      module->InstantiateModule(context, ModuleWrap::ResolveCallback);

  // clear resolve cache on instantiate
  for (auto& entry : obj->resolve_cache_)
    entry.second.Reset();
  obj->resolve_cache_.clear();

  if (!ok.FromMaybe(false)) {
    return;
  }
}

void ModuleWrap::Evaluate(const FunctionCallbackInfo<Value>& args) {
  Isolate* isolate = args.GetIsolate();
  Local<Object> that = args.This();
  Local<Context> context = that->CreationContext();
  ModuleWrap* obj = Unwrap<ModuleWrap>(that);
  CHECK_NE(obj, nullptr);
  MaybeLocal<Value> result = obj->module_.Get(isolate)->Evaluate(context);

  if (result.IsEmpty()) {
    return;
  }

  args.GetReturnValue().Set(result.ToLocalChecked());
}

void ModuleWrap::Namespace(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  Local<Object> that = args.This();
  ModuleWrap* obj = Unwrap<ModuleWrap>(that);
  CHECK_NE(obj, nullptr);

  Local<Module> module = obj->module_.Get(isolate);

  switch (module->GetStatus()) {
    default:
      return env->ThrowError(
          "cannot get namespace, Module has not been instantiated");
    case v8::Module::Status::kInstantiated:
    case v8::Module::Status::kEvaluating:
    case v8::Module::Status::kEvaluated:
      break;
  }

  Local<Value> result = module->GetModuleNamespace();
  args.GetReturnValue().Set(result);
}

MaybeLocal<Module> ModuleWrap::ResolveCallback(Local<Context> context,
                                               Local<String> specifier,
                                               Local<Module> referrer) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();
  if (env->module_map.count(referrer->GetIdentityHash()) == 0) {
    env->ThrowError("linking error, unknown module");
    return MaybeLocal<Module>();
  }

  ModuleWrap* dependent = nullptr;
  auto range = env->module_map.equal_range(referrer->GetIdentityHash());
  for (auto it = range.first; it != range.second; ++it) {
    if (it->second->module_ == referrer) {
      dependent = it->second;
      break;
    }
  }

  if (dependent == nullptr) {
    env->ThrowError("linking error, null dep");
    return MaybeLocal<Module>();
  }

  Utf8Value specifier_utf8(env->isolate(), specifier);
  std::string specifier_std(*specifier_utf8, specifier_utf8.length());

  if (dependent->resolve_cache_.count(specifier_std) != 1) {
    env->ThrowError("linking error, not in local cache");
    return MaybeLocal<Module>();
  }

  Local<Promise> resolve_promise =
      dependent->resolve_cache_[specifier_std].Get(isolate);

  if (resolve_promise->State() != Promise::kFulfilled) {
    env->ThrowError("linking error, dependency promises must be resolved on "
                    "instantiate");
    return MaybeLocal<Module>();
  }

  Local<Object> module_object = resolve_promise->Result().As<Object>();
  if (module_object.IsEmpty() || !module_object->IsObject()) {
    env->ThrowError("linking error, expected a valid module object from "
                    "resolver");
    return MaybeLocal<Module>();
  }

  ModuleWrap* module;
  ASSIGN_OR_RETURN_UNWRAP(&module, module_object, MaybeLocal<Module>());
  return module->module_.Get(env->isolate());
}

namespace {

// Tests whether a path starts with /, ./ or ../
// In WhatWG terminology, the alternative case is called a "bare" specifier
// (e.g. in `import "jquery"`).
inline bool ShouldBeTreatedAsRelativeOrAbsolutePath(
    const std::string& specifier) {
  size_t len = specifier.length();
  if (len == 0)
    return false;
  if (specifier[0] == '/') {
    return true;
  } else if (specifier[0] == '.') {
    if (len == 1 || specifier[1] == '/')
      return true;
    if (specifier[1] == '.') {
      if (len == 2 || specifier[2] == '/')
        return true;
    }
  }
  return false;
}

std::string ReadFile(uv_file file) {
  std::string contents;
  uv_fs_t req;
  char buffer_memory[4096];
  uv_buf_t buf = uv_buf_init(buffer_memory, sizeof(buffer_memory));
  int r;

  do {
    r = uv_fs_read(uv_default_loop(),
                   &req,
                   file,
                   &buf,
                   1,
                   contents.length(),  // offset
                   nullptr);
    uv_fs_req_cleanup(&req);

    if (r <= 0)
      break;
    contents.append(buf.base, r);
  } while (true);
  return contents;
}

enum CheckFileOptions {
  LEAVE_OPEN_AFTER_CHECK,
  CLOSE_AFTER_CHECK
};

Maybe<uv_file> CheckFile(const URL& search,
                         CheckFileOptions opt = CLOSE_AFTER_CHECK) {
  uv_fs_t fs_req;
  std::string path = search.ToFilePath();
  if (path.empty()) {
    return Nothing<uv_file>();
  }

  uv_file fd = uv_fs_open(nullptr, &fs_req, path.c_str(), O_RDONLY, 0, nullptr);
  uv_fs_req_cleanup(&fs_req);

  if (fd < 0) {
    return Nothing<uv_file>();
  }

  uv_fs_fstat(nullptr, &fs_req, fd, nullptr);
  uint64_t is_directory = fs_req.statbuf.st_mode & S_IFDIR;
  uv_fs_req_cleanup(&fs_req);

  if (is_directory) {
    uv_fs_close(nullptr, &fs_req, fd, nullptr);
    uv_fs_req_cleanup(&fs_req);
    return Nothing<uv_file>();
  }

  if (opt == CLOSE_AFTER_CHECK) {
    uv_fs_close(nullptr, &fs_req, fd, nullptr);
    uv_fs_req_cleanup(&fs_req);
  }

  return Just(fd);
}

enum ResolveExtensionsOptions {
  TRY_EXACT_NAME,
  ONLY_VIA_EXTENSIONS
};

template<ResolveExtensionsOptions options>
Maybe<URL> ResolveExtensions(const URL& search) {
  if (options == TRY_EXACT_NAME) {
    Maybe<uv_file> check = CheckFile(search);
    if (!check.IsNothing()) {
      return Just(search);
    }
  }

  for (const char* extension : EXTENSIONS) {
    URL guess(search.path() + extension, &search);
    Maybe<uv_file> check = CheckFile(guess);
    if (!check.IsNothing()) {
      return Just(guess);
    }
  }

  return Nothing<URL>();
}

inline Maybe<URL> ResolveIndex(const URL& search) {
  return ResolveExtensions<ONLY_VIA_EXTENSIONS>(URL("index", search));
}

Maybe<URL> ResolveMain(Environment* env, const URL& search) {
  URL pkg("package.json", &search);
  Maybe<uv_file> check = CheckFile(pkg, LEAVE_OPEN_AFTER_CHECK);
  if (check.IsNothing()) {
    return Nothing<URL>();
  }

  Isolate* isolate = env->isolate();
  Local<Context> context = isolate->GetCurrentContext();
  std::string pkg_src = ReadFile(check.FromJust());
  uv_fs_t fs_req;
  uv_fs_close(nullptr, &fs_req, check.FromJust(), nullptr);
  uv_fs_req_cleanup(&fs_req);

  // It's not okay for the called of this method to not be able to tell
  // whether an exception is pending or not.
  TryCatch try_catch(isolate);

  Local<String> src;
  if (!String::NewFromUtf8(isolate,
                           pkg_src.c_str(),
                           v8::NewStringType::kNormal,
                           pkg_src.length()).ToLocal(&src)) {
    return Nothing<URL>();
  }

  Local<Value> pkg_json;
  if (!JSON::Parse(context, src).ToLocal(&pkg_json) || !pkg_json->IsObject())
    return Nothing<URL>();
  Local<Value> pkg_main;
  if (!pkg_json.As<Object>()->Get(context, env->main_string())
                              .ToLocal(&pkg_main) || !pkg_main->IsString()) {
    return Nothing<URL>();
  }
  Utf8Value main_utf8(isolate, pkg_main.As<String>());
  std::string main_std(*main_utf8, main_utf8.length());
  if (!ShouldBeTreatedAsRelativeOrAbsolutePath(main_std)) {
    main_std.insert(0, "./");
  }
  return Resolve(env, main_std, search);
}

Maybe<URL> ResolveModule(Environment* env,
                         const std::string& specifier,
                         const URL& base) {
  URL parent(".", base);
  URL dir("");
  do {
    dir = parent;
    Maybe<URL> check = Resolve(env, "./node_modules/" + specifier, dir, true);
    if (!check.IsNothing()) {
      const size_t limit = specifier.find('/');
      const size_t spec_len =
          limit == std::string::npos ? specifier.length() :
                                       limit + 1;
      std::string chroot =
          dir.path() + "node_modules/" + specifier.substr(0, spec_len);
      if (check.FromJust().path().substr(0, chroot.length()) != chroot) {
        return Nothing<URL>();
      }
      return check;
    } else {
      // TODO(bmeck) PREVENT FALLTHROUGH
    }
    parent = URL("..", &dir);
  } while (parent.path() != dir.path());
  return Nothing<URL>();
}

Maybe<URL> ResolveDirectory(Environment* env,
                            const URL& search,
                            bool read_pkg_json) {
  if (read_pkg_json) {
    Maybe<URL> main = ResolveMain(env, search);
    if (!main.IsNothing())
      return main;
  }
  return ResolveIndex(search);
}

}  // anonymous namespace


Maybe<URL> Resolve(Environment* env,
                   const std::string& specifier,
                   const URL& base,
                   bool read_pkg_json) {
  URL pure_url(specifier);
  if (!(pure_url.flags() & URL_FLAGS_FAILED)) {
    // just check existence, without altering
    Maybe<uv_file> check = CheckFile(pure_url);
    if (check.IsNothing()) {
      return Nothing<URL>();
    }
    return Just(pure_url);
  }
  if (specifier.length() == 0) {
    return Nothing<URL>();
  }
  if (ShouldBeTreatedAsRelativeOrAbsolutePath(specifier)) {
    URL resolved(specifier, base);
    Maybe<URL> file = ResolveExtensions<TRY_EXACT_NAME>(resolved);
    if (!file.IsNothing())
      return file;
    if (specifier.back() != '/') {
      resolved = URL(specifier + "/", base);
    }
    return ResolveDirectory(env, resolved, read_pkg_json);
  } else {
    return ResolveModule(env, specifier, base);
  }
}

void ModuleWrap::Resolve(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.IsConstructCall()) {
    env->ThrowError("resolve() must not be called as a constructor");
    return;
  }
  if (args.Length() != 2) {
    env->ThrowError("resolve must have exactly 2 arguments (string, string)");
    return;
  }

  if (!args[0]->IsString()) {
    env->ThrowError("first argument is not a string");
    return;
  }
  Utf8Value specifier_utf8(env->isolate(), args[0]);
  std::string specifier_std(*specifier_utf8, specifier_utf8.length());

  if (!args[1]->IsString()) {
    env->ThrowError("second argument is not a string");
    return;
  }
  Utf8Value url_utf8(env->isolate(), args[1]);
  URL url(*url_utf8, url_utf8.length());

  if (url.flags() & URL_FLAGS_FAILED) {
    env->ThrowError("second argument is not a URL string");
    return;
  }

  Maybe<URL> result = node::loader::Resolve(env, specifier_std, url, true);
  if (result.IsNothing() || (result.FromJust().flags() & URL_FLAGS_FAILED)) {
    std::string msg = "Cannot find module " + specifier_std;
    env->ThrowError(msg.c_str());
    return;
  }

  args.GetReturnValue().Set(result.FromJust().ToObject(env));
}

void ModuleWrap::Initialize(Local<Object> target,
                            Local<Value> unused,
                            Local<Context> context) {
  Environment* env = Environment::GetCurrent(context);
  Isolate* isolate = env->isolate();

  Local<FunctionTemplate> tpl = env->NewFunctionTemplate(New);
  tpl->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "ModuleWrap"));
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  env->SetProtoMethod(tpl, "link", Link);
  env->SetProtoMethod(tpl, "instantiate", Instantiate);
  env->SetProtoMethod(tpl, "evaluate", Evaluate);
  env->SetProtoMethod(tpl, "namespace", Namespace);

  target->Set(FIXED_ONE_BYTE_STRING(isolate, "ModuleWrap"), tpl->GetFunction());
  env->SetMethod(target, "resolve", node::loader::ModuleWrap::Resolve);
}

}  // namespace loader
}  // namespace node

NODE_MODULE_CONTEXT_AWARE_INTERNAL(module_wrap,
                                   node::loader::ModuleWrap::Initialize)
