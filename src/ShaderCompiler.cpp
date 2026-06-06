#include "ShaderCompiler.h"

#include "Types.h"

#include <fstream>

#include <iostream>

#include <vector>

#include <cstring>



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
    modifierCommonStamp_ = FileStamp(shaderRoot_ / L"modifier_common.hlsl");
    blurVsStamp_ = FileStamp(shaderRoot_ / L"normal_blur_vs.hlsl");
    blurPsStamp_ = FileStamp(shaderRoot_ / L"normal_blur_ps.hlsl");
    akfPsStamp_ = FileStamp(shaderRoot_ / L"normal_akf_ps.hlsl");
    akfCommonStamp_ = FileStamp(shaderRoot_ / L"kuwahara_common.hlsl");

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

    if (FileStamp(shaderRoot_ / L"modifier_common.hlsl") != modifierCommonStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::NormalModifier));

    }

    if (FileStamp(shaderRoot_ / L"normal_blur_vs.hlsl") != blurVsStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::NormalBlur));

    }

    if (FileStamp(shaderRoot_ / L"normal_blur_ps.hlsl") != blurPsStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::NormalBlur));

    }

    if (FileStamp(shaderRoot_ / L"normal_akf_ps.hlsl") != akfPsStamp_ ||
        FileStamp(shaderRoot_ / L"kuwahara_common.hlsl") != akfCommonStamp_) {

        changed = static_cast<ChangeMask>(static_cast<unsigned>(changed) | static_cast<unsigned>(ChangeMask::NormalAkf));

    }

    return changed;

}



bool ShaderCompiler::CompileFile(

    const std::filesystem::path& path, const char* entry, const char* target, CompiledShader& out) {

    IncludeHandler include(shaderRoot_);

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

    flags |= D3DCOMPILE_SKIP_OPTIMIZATION;



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
    vsBytecode_ = vsCompiled.bytecode;
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
    psBytecode_ = psCompiled.bytecode;
    return true;

}



bool ShaderCompiler::ApplyBlurVertexShader(ID3D11Device* device, const CompiledShader& blurCompiled) {

    Microsoft::WRL::ComPtr<ID3D11VertexShader> vs;

    if (FAILED(device->CreateVertexShader(

            blurCompiled.bytecode->GetBufferPointer(), blurCompiled.bytecode->GetBufferSize(), nullptr, &vs))) {

        lastError_ = "Failed to create normal blur vertex shader object.";

        return false;

    }

    blurVs_ = vs;
    blurVsBytecode_ = blurCompiled.bytecode;
    return true;

}



bool ShaderCompiler::ApplyBlurPixelShader(ID3D11Device* device, const CompiledShader& blurCompiled) {

    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;

    if (FAILED(device->CreatePixelShader(

            blurCompiled.bytecode->GetBufferPointer(), blurCompiled.bytecode->GetBufferSize(), nullptr, &ps))) {

        lastError_ = "Failed to create normal blur pixel shader object.";

        return false;

    }

    blurPs_ = ps;
    blurPsBytecode_ = blurCompiled.bytecode;
    return true;

}



bool ShaderCompiler::ApplyAkfPixelShader(ID3D11Device* device, const CompiledShader& akfCompiled) {

    Microsoft::WRL::ComPtr<ID3D11PixelShader> ps;

    if (FAILED(device->CreatePixelShader(

            akfCompiled.bytecode->GetBufferPointer(), akfCompiled.bytecode->GetBufferSize(), nullptr, &ps))) {

        lastError_ = "Failed to create normal akf pixel shader object.";

        return false;

    }

    akfPs_ = ps;
    akfPsBytecode_ = akfCompiled.bytecode;
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

    const bool reloadBlurPS = forceAll || reloadCommon || reloadPS ||

        (static_cast<unsigned>(changed) & static_cast<unsigned>(ChangeMask::NormalBlur)) != 0;

    const bool reloadAkfPS = forceAll || reloadCommon || reloadPS ||

        (static_cast<unsigned>(changed) & static_cast<unsigned>(ChangeMask::NormalAkf)) != 0;



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



    if (reloadBlurPS) {

        CompiledShader blurVsCompiled;
        CompiledShader blurPsCompiled;

        const auto blurVsPath = shaderRoot_ / L"normal_blur_vs.hlsl";
        const auto blurPsPath = shaderRoot_ / L"normal_blur_ps.hlsl";

        if (!CompileFile(blurVsPath, "VSMain", "vs_5_0", blurVsCompiled)) {

            if (!anySuccess) lastReloadScope_ = "normal blur vertex";

            return false;

        }

        if (!CompileFile(blurPsPath, "PSMain", "ps_5_0", blurPsCompiled)) {

            if (!anySuccess) lastReloadScope_ = "normal blur pixel";

            return false;

        }

        if (!ApplyBlurVertexShader(device, blurVsCompiled)) {

            if (!anySuccess) lastReloadScope_ = "normal blur vertex";

            return false;

        }

        if (!ApplyBlurPixelShader(device, blurPsCompiled)) {

            if (!anySuccess) lastReloadScope_ = "normal blur pixel";

            return false;

        }

        anySuccess = true;

    }



    if (reloadAkfPS) {

        CompiledShader akfPsCompiled;

        const auto akfPsPath = shaderRoot_ / L"normal_akf_ps.hlsl";

        if (!CompileFile(akfPsPath, "PSMain", "ps_5_0", akfPsCompiled)) {

            if (!anySuccess) lastReloadScope_ = "normal akf pixel";

            return false;

        }

        if (!ApplyAkfPixelShader(device, akfPsCompiled)) {

            if (!anySuccess) lastReloadScope_ = "normal akf pixel";

            return false;

        }

        anySuccess = true;

    }



    if (anySuccess) {

        lastError_.clear();

        lastSuccess_ = std::chrono::system_clock::now();

        RefreshStamps();

        return true;

    }



    return HasValidShaders();

}



bool ShaderCompiler::EnsurePostProcessShaders(ID3D11Device* device) {

    if (blurVs_ && blurPs_ && akfPs_) return true;

    CompiledShader blurVsCompiled;
    CompiledShader blurPsCompiled;
    CompiledShader akfPsCompiled;

    const auto blurVsPath = shaderRoot_ / L"normal_blur_vs.hlsl";
    const auto blurPsPath = shaderRoot_ / L"normal_blur_ps.hlsl";
    const auto akfPsPath = shaderRoot_ / L"normal_akf_ps.hlsl";

    if (!CompileFile(blurVsPath, "VSMain", "vs_5_0", blurVsCompiled)) return false;
    if (!CompileFile(blurPsPath, "PSMain", "ps_5_0", blurPsCompiled)) return false;
    if (!CompileFile(akfPsPath, "PSMain", "ps_5_0", akfPsCompiled)) return false;
    if (!ApplyBlurVertexShader(device, blurVsCompiled)) return false;
    if (!ApplyBlurPixelShader(device, blurPsCompiled)) return false;
    if (!ApplyAkfPixelShader(device, akfPsCompiled)) return false;

    blurVsStamp_ = FileStamp(blurVsPath);
    blurPsStamp_ = FileStamp(blurPsPath);
    akfPsStamp_ = FileStamp(akfPsPath);
    akfCommonStamp_ = FileStamp(shaderRoot_ / L"kuwahara_common.hlsl");

    return true;

}



std::filesystem::path ShaderCompiler::PrecompiledDir() const {

    return shaderRoot_ / "builtin";

}



bool ShaderCompiler::LoadBytecodeFile(const std::filesystem::path& path, CompiledShader& out) const {

    std::ifstream file(path, std::ios::binary);

    if (!file) return false;

    file.seekg(0, std::ios::end);

    const std::streamsize size = file.tellg();

    if (size <= 0) return false;

    file.seekg(0, std::ios::beg);

    std::vector<char> bytes(static_cast<size_t>(size));

    if (!file.read(bytes.data(), size)) return false;



    Microsoft::WRL::ComPtr<ID3DBlob> blob;

    if (FAILED(D3DCreateBlob(bytes.size(), &blob))) return false;

    std::memcpy(blob->GetBufferPointer(), bytes.data(), bytes.size());

    out.bytecode = blob;

    out.path = path.wstring();

    return true;

}



bool ShaderCompiler::WriteBytecodeFile(const std::filesystem::path& path, ID3DBlob* blob) const {

    if (!blob || blob->GetBufferSize() == 0) return false;

    std::error_code ec;

    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    if (!file) return false;

    file.write(static_cast<const char*>(blob->GetBufferPointer()), static_cast<std::streamsize>(blob->GetBufferSize()));

    return static_cast<bool>(file);

}



bool ShaderCompiler::TryLoadPrecompiled(ID3D11Device* device) {

    const auto dir = PrecompiledDir();

    CompiledShader vsCompiled;

    CompiledShader psCompiled;

    if (!LoadBytecodeFile(dir / "material_vs.cso", vsCompiled)) return false;

    if (!LoadBytecodeFile(dir / "material_ps.cso", psCompiled)) return false;



    if (!ApplyVertexShader(device, vsCompiled)) return false;

    if (!ApplyPixelShader(device, psCompiled)) return false;



    lastError_.clear();

    lastReloadScope_ = "precompiled";

    lastSuccess_ = std::chrono::system_clock::now();

    RefreshStamps();

    return true;

}



bool ShaderCompiler::ExportPrecompiled(const std::filesystem::path& outputDir) const {

    if (!vsBytecode_ || !psBytecode_) return false;

    const bool okVs = WriteBytecodeFile(outputDir / "material_vs.cso", vsBytecode_.Get());

    const bool okPs = WriteBytecodeFile(outputDir / "material_ps.cso", psBytecode_.Get());

    return okVs && okPs;

}



} // namespace sv


