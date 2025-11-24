#include "ffi.h"

namespace FFI {

ThreadedCallbackInvokation::ThreadedCallbackInvokation(callback_info* cbinfo, void* retval, void** parameters)
    : m_cbinfo(cbinfo), m_retval(retval), m_parameters(parameters) {
  uv_mutex_init(&m_mutex);
  uv_cond_init(&m_cond);
  uv_mutex_lock(&m_mutex);  // 锁在构造时加，WaitForExecution 会释放
}

ThreadedCallbackInvokation::~ThreadedCallbackInvokation() {
  // 确保锁已释放再销毁
  uv_mutex_unlock(&m_mutex);
  uv_cond_destroy(&m_cond);
  uv_mutex_destroy(&m_mutex);
}

void ThreadedCallbackInvokation::SignalDoneExecuting() {
  uv_mutex_lock(&m_mutex);
  uv_cond_signal(&m_cond);
  uv_mutex_unlock(&m_mutex);
}

void ThreadedCallbackInvokation::WaitForExecution() {
  uv_cond_wait(&m_cond, &m_mutex);
  uv_mutex_unlock(&m_mutex);  // 确保 Wait 后释放锁
}

}  // namespace FFI
