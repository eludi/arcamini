#pragma once
/// language bindings + event dispatch for arcajs minimal subset C API
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// creates and initializes a VM and returns its state
/// @param script      Lua source code to evaluate
/// @param scriptName  filename (used in stack traces / error messages)
/// @return void* or NULL if initialization failed
extern void* initVM(const char* script, const char* scriptName);

/// deinitializes a  VM
/// @param state  vm handle returned by initVM
extern void shutdownVM(void* vm);

/// dispatch main loop events to handlers if they exist
void dispatchLifecycleEvent(const char* evtName, void* callback);
void dispatchAxisEvent(size_t id, uint8_t axis, float value, void* callback);
void dispatchButtonEvent(size_t id, uint8_t button, float value, void* callback);
bool dispatchUpdateEvent(double deltaT, void* callback);
void dispatchDrawEvent(void* callback);

#ifdef __cplusplus
}
#endif
