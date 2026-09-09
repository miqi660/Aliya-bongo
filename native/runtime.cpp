#include "runtime.h"
#include <Live2DCubismCore.hpp>
#include <new>
#include <thread>
struct AliyaRuntime {
    std::thread::id owner = std::this_thread::get_id();
    uint32_t version = Live2D::Cubism::Core::csmGetVersion();
};
static AliyaStatus validate(AliyaRuntime* r) noexcept {
    if (!r) return ALIYA_INVALID_ARGUMENT;
    return r->owner == std::this_thread::get_id() ? ALIYA_OK : ALIYA_WRONG_THREAD;
}
extern "C" AliyaStatus runtime_create(AliyaRuntime** out) noexcept {
    if (!out) return ALIYA_INVALID_ARGUMENT;
    *out = nullptr;
    try { *out = new AliyaRuntime(); return ALIYA_OK; }
    catch (const std::bad_alloc&) { return ALIYA_OUT_OF_MEMORY; }
    catch (...) { return ALIYA_INTERNAL_ERROR; }
}
extern "C" AliyaStatus runtime_destroy(AliyaRuntime** r) noexcept {
    if (!r) return ALIYA_INVALID_ARGUMENT;
    if (!*r) return ALIYA_OK;
    auto status = validate(*r);
    if (status != ALIYA_OK) return status;
    try { delete *r; *r = nullptr; return ALIYA_OK; }
    catch (...) { return ALIYA_INTERNAL_ERROR; }
}
extern "C" AliyaStatus runtime_core_version(AliyaRuntime* r, uint32_t* out) noexcept {
    auto status = validate(r);
    if (status != ALIYA_OK) return status;
    if (!out) return ALIYA_INVALID_ARGUMENT;
    *out = r->version; return ALIYA_OK;
}
extern "C" AliyaStatus model_load(AliyaRuntime* r, const char*) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus model_unload(AliyaRuntime* r) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus runtime_resize(AliyaRuntime* r, uint32_t, uint32_t) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus parameter_set(AliyaRuntime* r, const char*, float) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus parameter_add(AliyaRuntime* r, const char*, float) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus motion_start(AliyaRuntime* r, const char*, uint32_t) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus motion_stop(AliyaRuntime* r) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus expression_set(AliyaRuntime* r, const char*) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus runtime_update(AliyaRuntime* r, float) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus runtime_render(AliyaRuntime* r) noexcept {
    auto status = validate(r);
    return status == ALIYA_OK ? ALIYA_NOT_IMPLEMENTED : status;
}
extern "C" AliyaStatus runtime_is_dirty(AliyaRuntime* r, uint8_t* out) noexcept {
    auto status = validate(r);
    if (status != ALIYA_OK) return status;
    if (!out) return ALIYA_INVALID_ARGUMENT;
    *out = 0; return ALIYA_OK;
}
extern "C" AliyaStatus runtime_is_animating(AliyaRuntime* r, uint8_t* out) noexcept {
    auto status = validate(r);
    if (status != ALIYA_OK) return status;
    if (!out) return ALIYA_INVALID_ARGUMENT;
    *out = 0; return ALIYA_OK;
}
