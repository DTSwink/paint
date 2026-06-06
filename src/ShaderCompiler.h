#pragma once

#include <wrl/client.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <filesystem>
#include <string>
#include <chrono>

namespace sv {

struct CompiledShader {
    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;
    std::wstring path;
};

class ShaderCompiler {
public:
    void SetShaderRoot(const std::filesystem::path& root);
    bool CompileAll(ID3D11Device* device);
    bool ReloadChanged(ID3D11Device* device, bool forceAll = false);

    ID3D11VertexShader* VertexShader() const { return vs_.Get(); }
    ID3D11PixelShader* PixelShader() const { return ps_.Get(); }
    ID3D11InputLayout* InputLayout() const { return inputLayout_.Get(); }

    const std::string& LastError() const { return lastError_; }
    const std::string& LastReloadScope() const { return lastReloadScope_; }
    const std::filesystem::path& ShaderRoot() const { return shaderRoot_; }
    bool HasValidShaders() const { return vs_ && ps_; }
    std::chrono::system_clock::time_point LastSuccessTime() const { return lastSuccess_; }

private:
    enum class ChangeMask : unsigned {
        None = 0,
        Common = 1,
        Vertex = 2,
        Pixel = 4,
        NormalModifier = 8,
        All = Common | Vertex | Pixel | NormalModifier
    };

    std::filesystem::path shaderRoot_;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs_;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    std::string lastError_;
    std::string lastReloadScope_;
    std::chrono::system_clock::time_point lastSuccess_{};

    std::filesystem::file_time_type commonStamp_{};
    std::filesystem::file_time_type vsStamp_{};
    std::filesystem::file_time_type psStamp_{};
    std::filesystem::file_time_type modifierStamp_{};
    std::filesystem::file_time_type livePaintCommonStamp_{};
    bool stampsInitialized_ = false;

    bool CompileFile(const std::filesystem::path& path, const char* entry, const char* target, CompiledShader& out);
    bool CreateInputLayout(ID3D11Device* device, ID3DBlob* vsBlob);
    bool ApplyVertexShader(ID3D11Device* device, const CompiledShader& vsCompiled);
    bool ApplyPixelShader(ID3D11Device* device, const CompiledShader& psCompiled);

    std::filesystem::file_time_type FileStamp(const std::filesystem::path& path) const;
    void RefreshStamps();
    ChangeMask DetectChanges(bool forceAll) const;
};

} // namespace sv
