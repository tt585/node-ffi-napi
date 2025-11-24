#include "ffi.h"

namespace FFI {

/*
 * Adapter for Node 20 napi_finalize type
 */
void closure_pointer_cb_adapter(const napi_env env, void* data, void* hint) {
    FFI::closure_pointer_cb(
        reinterpret_cast<FFI::Env>(env),
        static_cast<char*>(data),
        static_cast<FFI::callback_info*>(hint)
    );
}

/*
 * Called when the `ffi_closure *` pointer gets GC'd on the JavaScript side
 */
void closure_pointer_cb(Env env, char* data, callback_info* hint) {
    callback_info* info = static_cast<callback_info*>(hint);
    info->~callback_info();
    ffi_closure_free(info);
}

/*
 * Invokes the JS callback function.
 */
void CallbackInfo::DispatchToV8(callback_info* info, void* retval, void** parameters, bool dispatched) {
    Env env = info->instance_data->env;
    HandleScope handle_scope(env);

    static const char* errorMessage =
        "ffi fatal: callback has been garbage collected!";

    try {
        if (info->function.IsEmpty() || info->function.Value().IsEmpty()) {
            if (dispatched) {
                info->errorFunction.MakeCallback(Object::New(env),
                                                 { String::New(env, errorMessage) });
            } else {
                throw Error::New(env, errorMessage);
            }
        } else {
            Value e = info->function.MakeCallback(Object::New(env), {
                WrapPointer(env, retval, info->resultSize),
                WrapPointer(env, parameters, sizeof(char*) * info->argc)
            });
            if (!e.IsUndefined()) {
                if (dispatched) {
                    info->errorFunction.Call({ e });
                } else {
                    throw Error::New(env, e.ToString());
                }
            }
        }
    } catch (Error& err) {
        err.ThrowAsJavaScriptException();
    }
}

void CallbackInfo::WatcherCallback(uv_async_t* w) {
    InstanceData* data = static_cast<InstanceData*>(w->data);
    uv_mutex_lock(&data->mutex);

    while (!data->queue.empty()) {
        ThreadedCallbackInvokation* inv = data->queue.front();
        data->queue.pop();

        DispatchToV8(inv->m_cbinfo, inv->m_retval, inv->m_parameters, true);
        inv->SignalDoneExecuting();
    }

    uv_mutex_unlock(&data->mutex);
}

/*
 * Creates an `ffi_closure *` pointer around the given JS function.
 */
Value CallbackInfo::Callback(const Napi::CallbackInfo& args) {
    Env env = args.Env();

    if (args.Length() != 5 || !args[0].IsBuffer() ||
        !args[3].IsFunction() || !args[4].IsFunction()) {
        throw Error::New(env, "Signature: Buffer, int, int, Function, Function");
    }

    ffi_cif* cif = GetBufferData<ffi_cif>(args[0]);
    size_t resultSize = args[1].ToNumber().Int32Value();
    int32_t argc = args[2].ToNumber();
    Function errorReportCallback = args[3].As<Function>();
    Function callback = args[4].As<Function>();

    callback_info* cbInfo;
    ffi_status status;
    void* code;

    void* storage = ffi_closure_alloc(sizeof(callback_info), &code);
    if (storage == nullptr) {
        throw Error::New(env, "ffi_closure_alloc() Returned Error");
    }

    cbInfo = new(storage) callback_info();
    cbInfo->resultSize = resultSize;
    cbInfo->argc = argc;
    cbInfo->errorFunction = Reference<Function>::New(errorReportCallback, 1);
    cbInfo->function = Reference<Function>::New(callback, 1);
    cbInfo->instance_data = InstanceData::Get(env);
    cbInfo->code = code;

    status = ffi_prep_closure_loc(
        &cbInfo->closure,
        cif,
        Invoke,
        static_cast<void*>(cbInfo),
        code
    );

    if (status != FFI_OK) {
        ffi_closure_free(cbInfo);
        Error e = Error::New(env, "ffi_prep_closure() Returned Error");
        e.Set("status", Number::New(env, status));
        throw e;
    }

    TypedArray ret = WrapPointer(env, code, sizeof(void*));
    ret.ArrayBuffer().AddFinalizer(closure_pointer_cb_adapter, static_cast<char*>(code), cbInfo);
    return ret;
}

/*
 * This is the function that gets called when the C function pointer gets executed.
 */
void CallbackInfo::Invoke(ffi_cif* cif,
                          void* retval,
                          void** parameters,
                          void* user_data) {
    callback_info* info = static_cast<callback_info*>(user_data);
    InstanceData* data = info->instance_data;

#ifdef WIN32
    if (data->thread == GetCurrentThreadId()) {
#else
    uv_thread_t self_thread = uv_thread_self();
    if (uv_thread_equal(&self_thread, &data->thread)) {
#endif
        DispatchToV8(info, retval, parameters);
    } else {
        uv_ref(reinterpret_cast<uv_handle_t*>(&data->async));
        std::unique_ptr<ThreadedCallbackInvokation> inv (
            new ThreadedCallbackInvokation(info, retval, parameters));

        uv_mutex_lock(&data->mutex);
        data->queue.push(inv.get());
        uv_mutex_unlock(&data->mutex);

        uv_async_send(&data->async);
        inv->WaitForExecution();
        uv_unref(reinterpret_cast<uv_handle_t*>(&data->async));
    }
}

/*
 * Init stuff.
 */
Function CallbackInfo::Initialize(Env env) {
    Function fn = Function::New(env, Callback);

    InstanceData* instance_data = InstanceData::Get(env);

#ifdef WIN32
    instance_data->thread = GetCurrentThreadId();
#else
    instance_data->thread = uv_thread_self();
#endif

    uv_loop_t* loop = nullptr;
    // Node 20 compatibility
    napi_status (*get_uv_event_loop_fn)(napi_env, uv_loop_t**) =
        reinterpret_cast<napi_status (*)(napi_env, uv_loop_t**)>(napi_get_uv_event_loop);
    get_uv_event_loop_fn(env, &loop);

    uv_async_init(loop, &instance_data->async, CallbackInfo::WatcherCallback);
    instance_data->async.data = instance_data;
    uv_mutex_init(&instance_data->mutex);

    uv_unref(reinterpret_cast<uv_handle_t*>(&instance_data->async));
    return fn;
}

} // namespace FFI
