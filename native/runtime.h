#pragma once
#include <stdint.h>
#ifdef __cplusplus
#define ALIYA_NOEXCEPT noexcept
extern "C" {
#else
#define ALIYA_NOEXCEPT
#endif
/* 所有调用限创建线程。句柄由 C++ 独占分配，调用者不得复制所有权。
 * 字符串为借用的 NUL 结尾 UTF-8，仅在调用期间有效，不保留指针。
 * create 输出位置不得覆盖尚未释放的所有权。
 * destroy 接受句柄地址并清空；禁止使用已释放句柄的别名。
 * 输出参数必须可写；错误时不读取输出。绝不传递纹理大块数据。
 */
typedef struct AliyaRuntime AliyaRuntime;
typedef int32_t AliyaStatus;
enum { ALIYA_OK=0, ALIYA_INVALID_ARGUMENT=1, ALIYA_WRONG_THREAD=2,
       ALIYA_NOT_IMPLEMENTED=3, ALIYA_OUT_OF_MEMORY=4, ALIYA_INTERNAL_ERROR=5,
       ALIYA_INVALID_STATE=6, ALIYA_IO_ERROR=7, ALIYA_ASSET_ERROR=8,
       ALIYA_GL_ERROR=9, ALIYA_BUSY=10 };
AliyaStatus runtime_create(AliyaRuntime** out) ALIYA_NOEXCEPT;
/* 借用 HWND，必须在创建线程保持有效直到 destroy 返回；Runtime 独占其 DC/GL。
 * HWND 应使用 CS_OWNDC，不能与其他 Renderer 共用。单进程仅一个绑定 Runtime。
 * attach 失败后销毁 Runtime 再重试。Runtime 不负责销毁 HWND。
 */
AliyaStatus runtime_attach_window(AliyaRuntime* runtime, void* hwnd) ALIYA_NOEXCEPT;
AliyaStatus runtime_destroy(AliyaRuntime** runtime) ALIYA_NOEXCEPT;
AliyaStatus runtime_core_version(AliyaRuntime* runtime, uint32_t* out) ALIYA_NOEXCEPT;
AliyaStatus model_load(AliyaRuntime* runtime, const char* path) ALIYA_NOEXCEPT;
AliyaStatus model_unload(AliyaRuntime* runtime) ALIYA_NOEXCEPT;
AliyaStatus runtime_resize(AliyaRuntime* runtime, uint32_t width, uint32_t height) ALIYA_NOEXCEPT;
AliyaStatus parameter_set(AliyaRuntime* runtime, const char* id, float value) ALIYA_NOEXCEPT;
AliyaStatus parameter_add(AliyaRuntime* runtime, const char* id, float value) ALIYA_NOEXCEPT;
AliyaStatus parameter_get(AliyaRuntime* runtime, const char* id, float* out) ALIYA_NOEXCEPT;
AliyaStatus motion_start(AliyaRuntime* runtime, const char* group, uint32_t index) ALIYA_NOEXCEPT;
AliyaStatus motion_stop(AliyaRuntime* runtime) ALIYA_NOEXCEPT;
AliyaStatus expression_set(AliyaRuntime* runtime, const char* id) ALIYA_NOEXCEPT;
AliyaStatus runtime_update(AliyaRuntime* runtime, float delta_seconds) ALIYA_NOEXCEPT;
AliyaStatus runtime_render(AliyaRuntime* runtime) ALIYA_NOEXCEPT;
/* render 只绘制，present 显式交换缓冲；两者均限创建线程。 */
AliyaStatus runtime_present(AliyaRuntime* runtime) ALIYA_NOEXCEPT;
AliyaStatus runtime_is_dirty(AliyaRuntime* runtime, uint8_t* out) ALIYA_NOEXCEPT;
AliyaStatus runtime_is_animating(AliyaRuntime* runtime, uint8_t* out) ALIYA_NOEXCEPT;

#ifdef __cplusplus
}
#endif
