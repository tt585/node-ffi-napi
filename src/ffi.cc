#define NAPI_VERSION 6
#define NAPI_EXPERIMENTAL
#include "ffi.h"
#include "fficonfig.h"
#include <get-uv-event-loop-napi.h>

namespace FFI {

InstanceData* InstanceData::Get(Env env) {
  void* d = nullptr;
  napi_status status = napi_get_instance_data(env, &d);
  assert(status == napi_ok);
  return static_cast<InstanceData*>(d);
}

void InstanceData::Dispose() {
  if (async.type != UV_ASYNC) return;
  uv_close(reinterpret_cast<uv_handle_t*>(&async), [](uv_handle_t* handle) {
    InstanceData* self = static_cast<InstanceData*>(handle->data);
    uv_mutex_destroy(&self->mutex);
    delete self;
  });
}

TypedArray WrapPointerImpl(Env env, char* ptr, size_t length) {
  InstanceData* data = InstanceData::Get(env);
  assert(data->ref_napi_instance != nullptr);
  return TypedArray(env, data->ref_napi_instance->WrapPointer(ptr, length));
}

char* GetBufferDataImpl(Value val) {
  InstanceData* data = InstanceData::Get(val.Env());
  assert(data->ref_napi_instance != nullptr);
  return data->ref_napi_instance->GetBufferData(val);
}

static int __ffi_errno() { return errno; }

Object FFI::InitializeStaticFunctions(Env env) {
  Object o = Object::New(env);

  o["dlopen"] = WrapPointer(env, dlopen);
  o["dlclose"] = WrapPointer(env, dlclose);
  o["dlsym"] = WrapPointer(env, dlsym);
  o["dlerror"] = WrapPointer(env, dlerror);

  o["_errno"] = WrapPointer(env, __ffi_errno);

  return o;
}

#define SET_ENUM_VALUE(_value) \
  target[#_value] = Number::New(env, static_cast<uint32_t>(_value));

void FFI::InitializeBindings(Env env, Object target) {
  target["version"] = PACKAGE_VERSION;

  target["ffi_prep_cif"] = Function::New(env, FFIPrepCif);
  target["ffi_prep_cif_var"] = Function::New(env, FFIPrepCifVar);
  target["ffi_call"] = Function::New(env, FFICall);
  target["ffi_call_async"] = Function::New(env, FFICallAsync);

  SET_ENUM_VALUE(FFI_OK);
  SET_ENUM_VALUE(FFI_BAD_TYPEDEF);
  SET_ENUM_VALUE(FFI_BAD_ABI);

  SET_ENUM_VALUE(FFI_DEFAULT_ABI);
  SET_ENUM_VALUE(FFI_FIRST_ABI);
  SET_ENUM_VALUE(FFI_LAST_ABI);

#ifdef __arm__
  SET_ENUM_VALUE(FFI_SYSV);
  SET_ENUM_VALUE(FFI_VFP);
#elif defined(X86_WIN32)
  SET_ENUM_VALUE(FFI_SYSV);
  SET_ENUM_VALUE(FFI_STDCALL);
  SET_ENUM_VALUE(FFI_THISCALL);
  SET_ENUM_VALUE(FFI_FASTCALL);
  SET_ENUM_VALUE(FFI_MS_CDECL);
#elif defined(X86_WIN64)
  SET_ENUM_VALUE(FFI_WIN64);
#elif defined __aarch64__
  SET_ENUM_VALUE(FFI_SYSV);
#elif defined(X86_64) || (defined (__x86_64__) && defined (X86_DARWIN))
  SET_ENUM_VALUE(FFI_UNIX64);
#else
  SET_ENUM_VALUE(FFI_SYSV);
#endif

#ifdef RTLD_LAZY
  SET_ENUM_VALUE(RTLD_LAZY);
#endif
#ifdef RTLD_NOW
  SET_ENUM_VALUE(RTLD_NOW);
#endif
#ifdef RTLD_LOCAL
  SET_ENUM_VALUE(RTLD_LOCAL);
#endif
#ifdef RTLD_GLOBAL
  SET_ENUM_VALUE(RTLD_GLOBAL);
#endif
#ifdef RTLD_NOLOAD
  SET_ENUM_VALUE(RTLD_NOLOAD);
#endif
#ifdef RTLD_NODELETE
  SET_ENUM_VALUE(RTLD_NODELETE);
#endif
#ifdef RTLD_FIRST
  SET_ENUM_VALUE(RTLD_FIRST);
#endif

#ifdef RTLD_NEXT
  target["RTLD_NEXT"] = WrapPointer(env, RTLD_NEXT);
#endif
#ifdef RTLD_DEFAULT
  target["RTLD_DEFAULT"] = WrapPointer(env, RTLD_DEFAULT);
#endif
#ifdef RTLD_SELF
  target["RTLD_SELF"] = WrapPointer(env, RTLD_SELF);
#endif
#ifdef RTLD_MAIN_ONLY
  target["RTLD_MAIN_ONLY"] = WrapPointer(env, RTLD_MAIN_ONLY);
#endif

  target["FFI_ARG_SIZE"] = Number::New(env, sizeof(ffi_arg));
  target["FFI_SARG_SIZE"] = Number::New(env, sizeof(ffi_sarg));
  target["FFI_TYPE_SIZE"] = Number::New(env, sizeof(ffi_type));
  target["FFI_CIF_SIZE"] = Number::New(env, sizeof(ffi_cif));

  Object ftmap = Object::New(env);
  ftmap["void"] = WrapPointer(env, &ffi_type_void);
  ftmap["uint8"] = WrapPointer(env, &ffi_type_uint8);
  ftmap["int8"] = WrapPointer(env, &ffi_type_sint8);
  ftmap["uint16"] = WrapPointer(env, &ffi_type_uint16);
  ftmap["int16"] = WrapPointer(env, &ffi_type_sint16);
  ftmap["uint32"] = WrapPointer(env, &ffi_type_uint32);
  ftmap["int32"] = WrapPointer(env, &ffi_type_sint32);
  ftmap["uint64"] = WrapPointer(env, &ffi_type_uint64);
  ftmap["int64"] = WrapPointer(env, &ffi_type_sint64);
  ftmap["uchar"] = WrapPointer(env, &ffi_type_uchar);
  ftmap["char"] = WrapPointer(env, &ffi_type_schar);
  ftmap["ushort"] = WrapPointer(env, &ffi_type_ushort);
  ftmap["short"] = WrapPointer(env, &ffi_type_sshort);
  ftmap["uint"] = WrapPointer(env, &ffi_type_uint);
  ftmap["int"] = WrapPointer(env, &ffi_type_sint);
  ftmap["float"] = WrapPointer(env, &ffi_type_float);
  ftmap["double"] = WrapPointer(env, &ffi_type_double);
  ftmap["pointer"] = WrapPointer(env, &ffi_type_pointer);
  ftmap["ulonglong"] = WrapPointer(env, &ffi_type_ulong);
  ftmap["longlong"] = WrapPointer(env, &ffi_type_slong);

  target["FFI_TYPES"] = ftmap;
}

Value FFI::FFIPrepCif(const Napi::CallbackInfo& args) {
  Env env = args.Env();

  if (!args[0].IsBuffer()) throw TypeError::New(env, "Buffer required as cif arg");
  if (!args[2].IsBuffer()) throw TypeError::New(env, "Buffer required as rtype arg");
  if (!args[3].IsBuffer()) throw TypeError::New(env, "Buffer required as atypes arg");

  ffi_cif* cif = GetBufferData<ffi_cif>(args[0]);
  uint32_t nargs = args[1].ToNumber();
  ffi_type* rtype = GetBufferData<ffi_type>(args[2]);
  ffi_type** atypes = GetBufferData<ffi_type*>(args[3]);
  ffi_abi abi = static_cast<ffi_abi>(args[4].ToNumber().Int32Value());

  ffi_status status = ffi_prep_cif(cif, abi, nargs, rtype, atypes);

  return Number::New(env, status);
}

Value FFI::FFIPrepCifVar(const Napi::CallbackInfo& args) {
  Env env = args.Env();

  if (!args[0].IsBuffer()) throw TypeError::New(env, "Buffer required as cif arg");
  if (!args[3].IsBuffer()) throw TypeError::New(env, "Buffer required as rtype arg");
  if (!args[4].IsBuffer()) throw TypeError::New(env, "Buffer required as atypes arg");

  ffi_cif* cif = GetBufferData<ffi_cif>(args[0]);
  uint32_t fargs = args[1].ToNumber();
  uint32_t targs = args[2].ToNumber();
  ffi_type* rtype = GetBufferData<ffi_type>(args[3]);
  ffi_type** atypes = GetBufferData<ffi_type*>(args[4]);
  ffi_abi abi = static_cast<ffi_abi>(args[5].ToNumber().Int32Value());

  ffi_status status = ffi_prep_cif_var(cif, abi, fargs, targs, rtype, atypes);

  return Number::New(env, status);
}

void FFI::FFICall(const Napi::CallbackInfo& args) {
  Env env = args.Env();
  if (!args[0].IsBuffer() || !args[1].IsBuffer() || !args[2].IsBuffer() || !args[3].IsBuffer())
    throw TypeError::New(env, "ffi_call() requires 4 Buffer arguments!");

  ffi_cif* cif = GetBufferData<ffi_cif>(args[0]);
  char* fn = GetBufferData<char>(args[1]);
  char* res = GetBufferData<char>(args[2]);
  void** fnargs = GetBufferData<void*>(args[3]);

  ffi_call(cif, FFI_FN(fn), static_cast<void*>(res), fnargs);
}

void FFI::FFICallAsync(const Napi::CallbackInfo& args) {
  Env env = args.Env();
  if (!args[0].IsBuffer() || !args[1].IsBuffer() || !args[2].IsBuffer() || !args[3].IsBuffer())
    throw TypeError::New(env, "ffi_call_async() requires 4 Buffer arguments!");
  if (!args[4].IsFunction())
    throw TypeError::New(env, "ffi_call_async() requires a function argument");

  AsyncCallParams* p = new AsyncCallParams(env);
  p->cif = GetBufferData<ffi_cif>(args[0]);
  p->fn = GetBufferData<char>(args[1]);
  p->res = GetBufferData<char>(args[2]);
  p->argv = GetBufferData<void*>(args[3]);
  p->result = FFI_OK;
  p->callback = Reference<Function>::New(args[4].As<Function>(), 1);
  p->req.data = p;

  uv_loop_t* loop = nullptr;
  napi_get_uv_event_loop(env, &loop);
  uv_queue_work(loop, &p->req, FFI::AsyncFFICall, FFI::FinishAsyncFFICall);
}

void FFI::AsyncFFICall(uv_work_t* req) {
  AsyncCallParams* p = static_cast<AsyncCallParams*>(req->data);

  try {
    ffi_call(p->cif, FFI_FN(p->fn), p->res, p->argv);
  } catch (std::exception& e) {
    p->err = e.what();
  }
}

void FFI::FinishAsyncFFICall(uv_work_t* req, int status) {
  AsyncCallParams* p = static_cast<AsyncCallParams*>(req->data);
  Env env = p->env;
  HandleScope scope(env);

  std::vector<napi_value> argv = { env.Null() };
  if (p->result != FFI_OK) argv[0] = String::New(env, p->err);

  p->callback.MakeCallback(Object::New(env), argv);
  delete p;
}

Value InitializeBindings(const Napi::CallbackInfo& args) {
  Env env = args.Env();

  assert(args[0].IsExternal());
  InstanceData* data = InstanceData::Get(env);
  data->ref_napi_instance = args[0].As<External<RefNapi::Instance>>().Data();

  Object exports = Object::New(env);
  FFI::InitializeBindings(env, exports);
  exports["StaticFunctions"] = FFI::InitializeStaticFunctions(env);
  exports["Callback"] = CallbackInfo::Initialize(env);
  return exports;
}

}  // namespace FFI

Napi::Object BindingHook(Napi::Env env, Napi::Object exports) {
  FFI::InstanceData* data = new FFI::InstanceData(env);
  napi_status status = napi_set_instance_data(
      env, data, [](napi_env env, void* data, void* hint) {
        static_cast<FFI::InstanceData*>(data)->Dispose();
      }, nullptr);
  if (status != napi_ok) delete data;

  exports["initializeBindings"] =
      Napi::Function::New(env, FFI::InitializeBindings);
  return exports;
}

NODE_API_MODULE(ffi_bindings, BindingHook)
