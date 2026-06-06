#include "ShaderCompiler.h"

#include "Types.h"

#include <fstream>

#include <iostream>

#include <vector>



namespace sv {



class IncludeHandler : public ID3DInclude {

public:

    explicit IncludeHandler(std::filesystem::path root) : root_(std::move(root)) {}



    HRESULT __stdcall Open(D3D_INCLUDE_TYPE type, LPCSTR fileName, LPCVOID, LPCVOID* data, UINT* bytes) override {

        (void)type;

        std::filesystem::path p = root_ / fileName;

        if (!std::filesystem::exists(p)) {

            p = root_ / std::filesystem::path(fileName).filename();

        }

        if (!std::filesystem::exists(p)) return E_FAIL;



        std::ifstream file(p, std::ios::binary);

        if (!file) return E_FAIL;

        const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        storage_.push_back(content);

        *data = storage_.back().data();

        *bytes = static_cast<UINT>(storage_.back().size());

        return S_OK;

    }



    HRESULT __stdcall Close(LPCVOID) override { return S_OK; }



private:

    std::filesystem::path root_;

    std::vector<std::string> storage_;

};



void ShaderCompiler::SetShaderRoot(const std::filesystem::path& root) {

    shaderRoot_ = root;

    stampsInitialized_ = false;

}



std::filesystem::file_time_type ShaderCompiler::FileStamp(const std::filesystem::path& path) const {

    std::error_code ec;

    if (!std::filesystem::exists(path, ec)) {

        return std::filesystem::file_time_type::min();

    }

    return std::filesystem::last_write_time(path, ec);

}



void ShaderCompiler::RefreshStamps() {

    commonStamp_ = FileStamp(shaderRoot_ / L"common.hlsli");

    vsStamp_ = FileStamp(shaderRoot_ / L"material_vs.hlsl");

    psStamp_ = FileStamp(shaderRoot_ / L"material_ps.hlsl");

    modifierStamp_ = FileStamp(shaderRoot_ / L"normal_modifier.hlsl");
    livePaintCommonStamp_ = FileStamp(shaderRoot_ / L"live_paint_common.hlsl");

    stampsInitialized_ = true;

}



ShaderCompiler::ChangeMask ShaderCompiler::DetectChanges(bool forceAll) const {

    if (forceAll || !stampsInitialized_) {

        return ChangeMask::All;

    }



    ChangeMask changed = ChangeMask::None;

    if (FileStamp(shaderRoot_ / L"common.hlsli") != commonStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::Common));

    }

    if (FileStamp(shaderRoot_ / L"material_vs.hlsl") != vsStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::Vertex));

    }

    if (FileStamp(shaderRoot_ / L"material_ps.hlsl") != psStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::Pixel));

    }

    if (FileStamp(shaderRoot_ / L"normal_modifier.hlsl") != modifierStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::NormalModifier));

    }

    if (FileStamp(shaderRoot_ / L"live_paint_common.hlsl") != livePaintCommonStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::NormalModifier));

    }

    return changed;

}



bool ShaderCompiler::CompileFile(

    const std::filesystem::path& path, const char* entry, const char* target, CompiledShader& out) {

    IncludeHandler include(shaderRoot_);

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

#ifdef SHADER_VIEWER_DEBUG

    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;

#else

    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;

#endif



    Microsoft::WRL::ComPtr<ID3DBlob> bytecode;

    Microsoft::WRL::ComPtr<ID3DBlob> errors;

    const HRESULT hr = D3DCompileFromFile(

        path.c_str(), nullptr, &include, entry, target, flags, 0, &bytecode, &errors);



    if (FAILED(hr)) {

        if (errors) {

            lastError_ = static_cast<const char*>(errors->GetBufferPointer());

        } else {

            lastError_ = "Unknown shader compile error for " + path.string();

        }

        std::cerr << lastError_ << std::endl;

        return false;

    }



    out.bytecode = bytecode;

    out.path = path.wstring();

    return true;

}



bool ShaderCompiler::CreateInputLayout(ID3D11Device* device, ID3DBlob* vsBlob) {

    D3D11_INPUT_ELEMENT_DESC layout[] = {

        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},

        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},

        {"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0},

        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0},

    };



    Microsoft::WRL::ComPtr<ID3D11InputLayout> layoutObj;

    const HRESULT hr = device->CreateInputLayout(

        layout, ARRAYSIZE(layout), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &layoutObj);

    if (FAILED(hr)) {

        lastError_ = "Failed to create input layout.";

        return false;

    }

    inputLayout_ = layoutObj;

    return true;

}



bool ShaderCompiler::ApplyVertexShader(ID3D11Device* device, const CompiledShader& vsCompiled) {

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;

    if (FAILED(device->CreateVertexShader(

            vsCompiled.bytecode->GetBufferPointer(), vsCompiled.bytecode->GetBufferSize(), nullptr, &vs))) {

        lastError_ = "Failed to create vertex shader object.";

        return false;

    }

    if (!CreateInputLayout(device, vsCompiled.bytecode.Get())) return false;

    vs_ = vs;

    return true;

}



bool ShaderCompiler::ApplyPixelShader(ID3D11Device* device, const CompiledShader& psCompiled) {

    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;

    if (FAILED(device->CreatePixelShader(

            psCompiled.bytecode->GetBufferPointer(), psCompiled.bytecode->GetBufferSize(), nullptr, &ps))) {

        lastError_ = "Failed to create pixel shader object.";

        return false;

    }

    ps_ = ps;

    return true;

}



bool ShaderCompiler::CompileAll(ID3D11Device* device) {

    return ReloadChanged(device, true);

}



bool ShaderCompiler::ReloadChanged(ID3D11Device* device, bool forceAll) {

    const ChangeMask changed = DetectChanges(forceAll);

    if (changed == ChangeMask::None) {

        return HasValidShaders();

    }



    const bool reloadCommon = forceAll || (static_cast<unsigned>(changed) & static_cast<unsigned>(ChangeMask::Common)) != 0;

    const bool reloadVS = forceAll || reloadCommon ||

        (static_cast<unsigned>(changed) & static_cast<unsigned>(ChangeMask::Vertex)) != 0;

    const bool reloadPS = forceAll || reloadCommon ||

        (static_cast<unsigned>(changed) & static_cast<unsigned>(ChangeMask::Pixel)) != 0 ||

        (static_cast<unsigned>(changed) & static_cast<unsigned>(ChangeMask::NormalModifier)) != 0;



    bool anySuccess = false;



    if (reloadVS) {

        CompiledShader vsCompiled;

        const auto vsPath = shaderRoot_ / L"material_vs.hlsl";

        if (!CompileFile(vsPath, "VSMain", "vs_5_0", vsCompiled)) {

            lastReloadScope_ = reloadPS ? "vertex+pixel (vertex failed)" : "vertex";

            return false;

        }

        if (!ApplyVertexShader(device, vsCompiled)) {

            lastReloadScope_ = "vertex";

            return false;

        }

        anySuccess = true;

        lastReloadScope_ = reloadPS ? "vertex+pixel" : "vertex";

    }



    if (reloadPS) {

        CompiledShader psCompiled;

        const auto psPath = shaderRoot_ / L"material_ps.hlsl";

        if (!CompileFile(psPath, "PSMain", "ps_5_0", psCompiled)) {

            if (!anySuccess) {

                lastReloadScope_ = reloadVS ? "vertex+pixel (pixel failed)" : "pixel";

            } else {

                lastReloadScope_ = "vertex ok, pixel failed";

            }

            return false;

        }

        if (!ApplyPixelShader(device, psCompiled)) {

            if (!anySuccess) lastReloadScope_ = "pixel";

            return false;

        }

        anySuccess = true;

        if (!reloadVS) {

            lastReloadScope_ = "pixel";

        }

    }



    if (anySuccess) {

        lastError_.clear();

        lastSuccess_ = std::chrono::system_clock::now();

        RefreshStamps();

        return true;

    }



    return HasValidShaders();

}



} // namespace sv


