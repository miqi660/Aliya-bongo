#include "runtime.h"
#include <GL/glew.h>
#include <windows.h>
#include <CubismFramework.hpp>
#include <ICubismAllocator.hpp>
#include <CubismModelSettingJson.hpp>
#include <Model/CubismUserModel.hpp>
#include <Motion/CubismMotion.hpp>
#include <Motion/CubismExpressionMotion.hpp>
#include <Motion/CubismMotionManager.hpp>
#include <Motion/CubismExpressionMotionManager.hpp>
#include <Id/CubismIdManager.hpp>
#include <Math/CubismModelMatrix.hpp>
#include <Rendering/OpenGL/CubismRenderer_OpenGLES2.hpp>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <map>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include <stb_image.h>
#include "embedded_shaders.h"
using namespace Live2D::Cubism::Framework;
using namespace Live2D::Cubism::Framework::Rendering;
namespace {
struct Failure { AliyaStatus status; };
void require(bool ok, AliyaStatus status) { if (!ok) throw Failure{status}; }
struct Allocator final : ICubismAllocator {
    void* Allocate(csmSizeType n) override { auto p = malloc(n); if (!p) throw std::bad_alloc(); return p; }
    void Deallocate(void* p) override { free(p); }
    void* AllocateAligned(csmSizeType n, csmUint32 a) override {
        auto p = _aligned_malloc(n, a); if (!p) throw std::bad_alloc(); return p;
    }
    void DeallocateAligned(void* p) override { _aligned_free(p); }
} allocator;
// Framework、GLEW 和 SDK Shader 为全局状态：本阶段只允许一个已绑定 Runtime。
std::mutex frameworkMutex;
bool frameworkOwned = false;
csmByte* loadShader(const std::string path, csmSizeInt* size) {
    auto name = std::filesystem::path(path).filename().string();
    for (auto& shader : shaders) if (name == shader.name) {
        *size = static_cast<csmSizeInt>(strlen(shader.source));
        auto p = new csmByte[*size]; memcpy(p, shader.source, *size); return p;
    }
    *size = 0; return nullptr;
}
void releaseShader(csmByte* p) { delete[] p; }
void logMessage(const char* text) { fprintf(stderr, "%s", text); }
std::vector<csmByte> readFile(const std::filesystem::path& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    require(f.good(), ALIYA_IO_ERROR);
    auto n = f.tellg();
    require(n > 0 && n <= INT_MAX, ALIYA_ASSET_ERROR);
    std::vector<csmByte> data(static_cast<size_t>(n));
    f.seekg(0); f.read(reinterpret_cast<char*>(data.data()), n);
    require(f.good(), ALIYA_IO_ERROR); return data;
}
using MotionPtr = std::unique_ptr<ACubismMotion, decltype(&ACubismMotion::Delete)>;
struct Model final : CubismUserModel {
    std::vector<GLuint> textures;
    std::map<std::pair<std::string, uint32_t>, MotionPtr> motions;
    std::map<std::string, MotionPtr> expressions;
    ~Model() {
        _motionManager->StopAllMotions(); _expressionManager->StopAllMotions();
        DeleteRenderer();
        if (!textures.empty()) glDeleteTextures(static_cast<GLsizei>(textures.size()), textures.data());
    }
    void load(const char* path, uint32_t width, uint32_t height) {
        auto file = std::filesystem::u8path(path);
        auto json = readFile(file); CubismModelSettingJson setting(json.data(), static_cast<csmSizeInt>(json.size()));
        require(setting.GetJsonPointer() && setting.GetJsonPointer()->GetRoot().IsMap(), ALIYA_ASSET_ERROR);
        require(strlen(setting.GetModelFileName()) > 0 && setting.GetTextureCount() > 0, ALIYA_ASSET_ERROR);
        auto read = [&](const char* name) { return readFile(file.parent_path() / std::filesystem::u8path(name)); };
        auto moc = read(setting.GetModelFileName());
        LoadModel(moc.data(), static_cast<csmSizeInt>(moc.size()), true);
        require(_model != nullptr, ALIYA_ASSET_ERROR);
        if (_model->GetCanvasWidth() > _model->GetCanvasHeight()) _modelMatrix->SetWidth(2.0f);
        csmMap<csmString, csmFloat32> layout;
        setting.GetLayoutMap(layout); _modelMatrix->SetupFromLayout(layout);
        for (int g = 0; g < setting.GetMotionGroupCount(); ++g) {
            auto group = setting.GetMotionGroupName(g);
            for (int i = 0; i < setting.GetMotionCount(group); ++i) {
                auto data = read(setting.GetMotionFileName(group, i));
                MotionPtr motion(LoadMotion(data.data(), static_cast<csmSizeInt>(data.size()), group,
                    nullptr, nullptr, &setting, group, i, true), ACubismMotion::Delete);
                require(motion != nullptr, ALIYA_ASSET_ERROR);
                motions.emplace(std::make_pair(std::string(group), i), std::move(motion));
            }
        }
        for (int i = 0; i < setting.GetExpressionCount(); ++i) {
            auto data = read(setting.GetExpressionFileName(i));
            // SDK 的 Expression Create 对解析失败也可能返回空表情，先拒绝损坏文档。
            std::unique_ptr<Utils::CubismJson, decltype(&Utils::CubismJson::Delete)> expressionJson(
                Utils::CubismJson::Create(data.data(), static_cast<csmSizeInt>(data.size())), Utils::CubismJson::Delete);
            require(expressionJson && expressionJson->GetRoot().IsMap()
                && expressionJson->GetRoot()["Parameters"].IsArray(), ALIYA_ASSET_ERROR);
            MotionPtr exp(LoadExpression(data.data(), static_cast<csmSizeInt>(data.size()), setting.GetExpressionName(i)), ACubismMotion::Delete);
            require(exp != nullptr, ALIYA_ASSET_ERROR);
            expressions.emplace(setting.GetExpressionName(i), std::move(exp));
        }
        CreateRenderer(width, height);
        auto renderer = GetRenderer<CubismRenderer_OpenGLES2>();
        require(renderer != nullptr, ALIYA_GL_ERROR);
        renderer->IsPremultipliedAlpha(false);
        textures.reserve(setting.GetTextureCount());
        for (int i = 0; i < setting.GetTextureCount(); ++i) {
            auto data = read(setting.GetTextureFileName(i));
            int w, h, channels;
            std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> pixels(
                stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &w, &h, &channels, 4), stbi_image_free);
            require(pixels != nullptr, ALIYA_ASSET_ERROR);
            GLuint texture = 0; glGenTextures(1, &texture); textures.push_back(texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glGenerateMipmap(GL_TEXTURE_2D);
            require(glGetError() == GL_NO_ERROR, ALIYA_GL_ERROR);
            renderer->BindTexture(i, texture);
        }
        _model->Update();
    }
    int parameter(const char* name) {
        require(name && *name, ALIYA_INVALID_ARGUMENT);
        auto id = CubismFramework::GetIdManager()->GetId(name);
        // SDK 对不存在的 ID 会创建虚拟参数；这里明确返回错误。
        for (int i = 0; i < _model->GetParameterCount(); ++i)
            if (_model->GetParameterId(i) == id) return i;
        throw Failure{ALIYA_INVALID_ARGUMENT};
    }
    void update(float dt) {
        _model->LoadParameters();
        _motionManager->UpdateMotion(_model, dt);
        _model->SaveParameters();
        _expressionManager->UpdateMotion(_model, dt);
        _model->Update();
    }
    bool animating() { return !_motionManager->IsFinished() || !_expressionManager->IsFinished(); }
    void start(const char* group, uint32_t index) {
        require(group != nullptr, ALIYA_INVALID_ARGUMENT);
        auto it = motions.find({group, index}); require(it != motions.end(), ALIYA_INVALID_ARGUMENT);
        _motionManager->StopAllMotions();
        _motionManager->StartMotionPriority(it->second.get(), false, 3);
    }
    void stop() { _motionManager->StopAllMotions(); }
    void expression(const char* name) {
        require(name != nullptr, ALIYA_INVALID_ARGUMENT);
        auto it = expressions.find(name); require(it != expressions.end(), ALIYA_INVALID_ARGUMENT);
        _expressionManager->StartMotion(it->second.get(), false);
    }
    void set(const char* id, float value, bool add) {
        require(std::isfinite(value), ALIYA_INVALID_ARGUMENT);
        int index = parameter(id);
        _model->LoadParameters();
        if (add) _model->AddParameterValue(index, value); else _model->SetParameterValue(index, value);
        _model->SaveParameters(); _model->Update();
    }
    float get(const char* id) { return _model->GetParameterValue(parameter(id)); }
    void draw(uint32_t width, uint32_t height) {
        auto renderer = GetRenderer<CubismRenderer_OpenGLES2>();
        CubismMatrix44 projection;
        // 保持模型比例，并完整容纳宽高比不同的画布。
        const float aspect = static_cast<float>(width) / height;
        if (aspect >= 1) projection.Scale(1 / aspect, 1); else projection.Scale(1, aspect);
        projection.MultiplyByMatrix(_modelMatrix);
        renderer->SetMvpMatrix(&projection);
        renderer->DrawModel();
    }
};
}
struct AliyaRuntime {
    std::thread::id owner = std::this_thread::get_id();
    HWND window = nullptr; HDC dc = nullptr; HGLRC context = nullptr;
    // SDK 借用 Option 指针，必须保持到 CleanUp 完成。
    CubismFramework::Option option{};
    bool framework = false, dirty = false;
    uint32_t width = 640, height = 640;
    std::unique_ptr<Model> model;
    void current() {
        require(context != nullptr, ALIYA_INVALID_STATE);
        require(wglGetCurrentContext() == context || wglMakeCurrent(dc, context), ALIYA_GL_ERROR);
    }
    ~AliyaRuntime() {
        // destroy 先确保 Context current，再释放 Model/Shader/Framework，最后释放 WGL。
        if (context) wglMakeCurrent(dc, context);
        model.reset();
        if (framework) {
            CubismRenderer::StaticRelease();
            CubismFramework::Dispose(); CubismFramework::CleanUp();
            std::lock_guard<std::mutex> lock(frameworkMutex); frameworkOwned = false;
        }
        if (context) { wglMakeCurrent(nullptr, nullptr); wglDeleteContext(context); }
        if (dc) ReleaseDC(window, dc);
    }
};
static AliyaStatus validate(AliyaRuntime* r) noexcept {
    if (!r) return ALIYA_INVALID_ARGUMENT;
    return r->owner == std::this_thread::get_id() ? ALIYA_OK : ALIYA_WRONG_THREAD;
}
template<class F> AliyaStatus invoke(AliyaRuntime* r, F operation) noexcept {
    auto status = validate(r); if (status != ALIYA_OK) return status;
    try { operation(); return ALIYA_OK; }
    catch (Failure e) { return e.status; }
    catch (const std::bad_alloc&) { return ALIYA_OUT_OF_MEMORY; }
    catch (...) { return ALIYA_INTERNAL_ERROR; }
}
static Model& loaded(AliyaRuntime* r) { r->current(); require(r->model != nullptr, ALIYA_INVALID_STATE); return *r->model; }
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
    return invoke(*r, [&] { if ((*r)->context) (*r)->current(); delete *r; *r = nullptr; });
}
extern "C" AliyaStatus runtime_core_version(AliyaRuntime* r, uint32_t* out) noexcept {
    return invoke(r, [&] { require(out != nullptr, ALIYA_INVALID_ARGUMENT); *out = Live2D::Cubism::Core::csmGetVersion(); });
}
extern "C" AliyaStatus runtime_attach_window(AliyaRuntime* r, void* hwnd) noexcept {
    return invoke(r, [&] {
        require(hwnd && IsWindow(static_cast<HWND>(hwnd)), ALIYA_INVALID_ARGUMENT);
        require(GetWindowThreadProcessId(static_cast<HWND>(hwnd), nullptr) == GetCurrentThreadId(), ALIYA_WRONG_THREAD);
        require(!r->context && !r->dc, ALIYA_INVALID_STATE);
        std::lock_guard<std::mutex> lock(frameworkMutex);
        require(!frameworkOwned, ALIYA_BUSY);
        r->window = static_cast<HWND>(hwnd); r->dc = GetDC(r->window);
        require(r->dc != nullptr, ALIYA_GL_ERROR);
        PIXELFORMATDESCRIPTOR p{}; p.nSize = sizeof(p); p.nVersion = 1;
        p.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        p.iPixelType = PFD_TYPE_RGBA; p.cColorBits = 32; p.cAlphaBits = 8;
        int format = GetPixelFormat(r->dc);
        if (!format) { format = ChoosePixelFormat(r->dc, &p); require(format && SetPixelFormat(r->dc, format, &p), ALIYA_GL_ERROR); }
        require(DescribePixelFormat(r->dc, format, sizeof(p), &p) != 0 && p.cAlphaBits >= 8
            && (p.dwFlags & (PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER)) == (PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER), ALIYA_GL_ERROR);
        r->context = wglCreateContext(r->dc); r->current();
        glewExperimental = GL_TRUE;
        require(glewInit() == GLEW_OK && GLEW_VERSION_2_1, ALIYA_GL_ERROR);
        while (glGetError() != GL_NO_ERROR) {}
        auto& option = r->option; option.LogFunction = logMessage;
        option.LoggingLevel = CubismFramework::Option::LogLevel_Warning;
        option.LoadFileFunction = loadShader; option.ReleaseBytesFunction = releaseShader;
        require(CubismFramework::StartUp(&allocator, &option), ALIYA_INTERNAL_ERROR);
        r->framework = true; frameworkOwned = true;
        CubismFramework::Initialize();
    });
}
extern "C" AliyaStatus model_load(AliyaRuntime* r, const char* path) noexcept {
    return invoke(r, [&] {
        require(path && *path, ALIYA_INVALID_ARGUMENT); r->current();
        require(r->framework, ALIYA_INVALID_STATE);
        auto next = std::make_unique<Model>(); next->load(path, r->width, r->height);
        r->model = std::move(next); r->dirty = true;
    });
}
extern "C" AliyaStatus model_unload(AliyaRuntime* r) noexcept {
    return invoke(r, [&] { r->current(); r->model.reset(); r->dirty = true; });
}
extern "C" AliyaStatus runtime_resize(AliyaRuntime* r, uint32_t w, uint32_t h) noexcept {
    return invoke(r, [&] {
        require(w && h && w <= 16384 && h <= 16384, ALIYA_INVALID_ARGUMENT); r->current();
        r->width = w; r->height = h;
        if (r->model) r->model->GetRenderer<CubismRenderer_OpenGLES2>()->SetRenderTargetSize(w, h);
        glViewport(0, 0, w, h); r->dirty = true;
    });
}
extern "C" AliyaStatus parameter_set(AliyaRuntime* r, const char* id, float value) noexcept {
    return invoke(r, [&] { loaded(r).set(id, value, false); r->dirty = true; });
}
extern "C" AliyaStatus parameter_add(AliyaRuntime* r, const char* id, float value) noexcept {
    return invoke(r, [&] { loaded(r).set(id, value, true); r->dirty = true; });
}
extern "C" AliyaStatus parameter_get(AliyaRuntime* r, const char* id, float* out) noexcept {
    return invoke(r, [&] { require(out != nullptr, ALIYA_INVALID_ARGUMENT); *out = loaded(r).get(id); });
}
extern "C" AliyaStatus motion_start(AliyaRuntime* r, const char* group, uint32_t index) noexcept {
    return invoke(r, [&] { loaded(r).start(group, index); r->dirty = true; });
}
extern "C" AliyaStatus motion_stop(AliyaRuntime* r) noexcept {
    return invoke(r, [&] { loaded(r).stop(); r->dirty = true; });
}
extern "C" AliyaStatus expression_set(AliyaRuntime* r, const char* id) noexcept {
    return invoke(r, [&] { loaded(r).expression(id); r->dirty = true; });
}
extern "C" AliyaStatus runtime_update(AliyaRuntime* r, float dt) noexcept {
    return invoke(r, [&] { require(std::isfinite(dt) && dt >= 0, ALIYA_INVALID_ARGUMENT); loaded(r).update(std::min(dt, 0.25f)); r->dirty = true; });
}
extern "C" AliyaStatus runtime_render(AliyaRuntime* r) noexcept {
    return invoke(r, [&] {
        r->current(); glViewport(0, 0, r->width, r->height);
        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
        if (r->model) r->model->draw(r->width, r->height);
        require(glGetError() == GL_NO_ERROR, ALIYA_GL_ERROR); r->dirty = false;
    });
}
extern "C" AliyaStatus runtime_present(AliyaRuntime* r) noexcept {
    return invoke(r, [&] { r->current(); require(SwapBuffers(r->dc), ALIYA_GL_ERROR); });
}
extern "C" AliyaStatus runtime_is_dirty(AliyaRuntime* r, uint8_t* out) noexcept {
    return invoke(r, [&] { require(out != nullptr, ALIYA_INVALID_ARGUMENT); *out = r->dirty; });
}
extern "C" AliyaStatus runtime_is_animating(AliyaRuntime* r, uint8_t* out) noexcept {
    return invoke(r, [&] { require(out != nullptr, ALIYA_INVALID_ARGUMENT); *out = r->model && r->model->animating(); });
}
