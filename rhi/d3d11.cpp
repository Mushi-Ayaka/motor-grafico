#include "rhi.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

// Link with dxguid.lib for IID_IDXGIFactory etc.
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

namespace mg {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static const char* dxErrorStr(HRESULT hr) {
    switch (hr) {
        case S_OK: return "S_OK";
        case E_INVALIDARG: return "E_INVALIDARG";
        case E_OUTOFMEMORY: return "E_OUTOFMEMORY";
        case DXGI_ERROR_UNSUPPORTED: return "DXGI_ERROR_UNSUPPORTED";
        case DXGI_ERROR_NOT_FOUND: return "DXGI_ERROR_NOT_FOUND";
        case D3D11_ERROR_FILE_NOT_FOUND: return "D3D11_ERROR_FILE_NOT_FOUND";
        default: return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Init / Shutdown
// ---------------------------------------------------------------------------

bool Rhi::init(void* hwnd, int w, int h) {
    shutdown();
    _arena.init(1024 * 1024);

    width  = w;
    height = h;

    UINT flags = D3D11_CREATE_DEVICE_SINGLETHREADED;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_level;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                                    nullptr, 0, D3D11_SDK_VERSION,
                                    (ID3D11Device**)&device, &feature_level,
                                    (ID3D11DeviceContext**)&ctx);
    if (FAILED(hr)) return false;

    // Create swapchain
    IDXGIDevice* dxgi_device = nullptr;
    ((ID3D11Device*)device)->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgi_device);
    if (!dxgi_device) { shutdown(); return false; }

    IDXGIAdapter* adapter = nullptr;
    dxgi_device->GetParent(__uuidof(IDXGIAdapter), (void**)&adapter);
    dxgi_device->Release();

    IDXGIFactory* factory = nullptr;
    if (adapter) adapter->GetParent(__uuidof(IDXGIFactory), (void**)&factory);

    DXGI_SWAP_CHAIN_DESC scd = {};
    scd.BufferCount       = 2;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width  = w;
    scd.BufferDesc.Height = h;
    scd.BufferUsage       = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow      = (HWND)hwnd;
    scd.SampleDesc.Count  = 1;
    scd.Windowed          = TRUE;
    scd.SwapEffect        = DXGI_SWAP_EFFECT_DISCARD;

    hr = factory->CreateSwapChain((IUnknown*)device, &scd, (IDXGISwapChain**)&swapchain);
    if (adapter) adapter->Release();
    if (factory) factory->Release();
    if (FAILED(hr)) { shutdown(); return false; }

    // Get backbuffer
    ((IDXGISwapChain*)swapchain)->GetBuffer(0, __uuidof(ID3D11Texture2D), &backbuffer);
    if (!backbuffer) { shutdown(); return false; }

    // Create RTV
    ((ID3D11Device*)device)->CreateRenderTargetView((ID3D11Resource*)backbuffer, nullptr,
                                                     (ID3D11RenderTargetView**)&backbuffer_rtv);

    return true;
}

void Rhi::shutdown() {
    if (backbuffer_rtv) { ((ID3D11RenderTargetView*)backbuffer_rtv)->Release(); backbuffer_rtv = nullptr; }
    if (backbuffer) { ((ID3D11Texture2D*)backbuffer)->Release(); backbuffer = nullptr; }
    if (swapchain) { ((IDXGISwapChain*)swapchain)->Release(); swapchain = nullptr; }
    if (ctx) { ((ID3D11DeviceContext*)ctx)->Release(); ctx = nullptr; }
    if (device) { ((ID3D11Device*)device)->Release(); device = nullptr; }
    _arena.shutdown();
}

// ---------------------------------------------------------------------------
// Buffer creation
// ---------------------------------------------------------------------------

RhiBuffer* Rhi::createConstantBuffer(const void* data, int size) {
    // Pad to 16 bytes
    int padded = (size + 15) & ~15;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = padded;
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = data;

    RhiBuffer* buf = (RhiBuffer*)_arena.alloc(sizeof(RhiBuffer));
    if (!buf) return nullptr;
    buf->size = padded;

    ID3D11Buffer* d3d_buf = nullptr;
    HRESULT hr = ((ID3D11Device*)device)->CreateBuffer(&bd, &init, &d3d_buf);
    if (FAILED(hr)) return nullptr;
    buf->ptr = d3d_buf;
    return buf;
}

RhiBuffer* Rhi::createStructuredBuffer(const void* data, int stride, int count) {
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth      = stride * count;
    bd.StructureByteStride = stride;
    bd.Usage          = D3D11_USAGE_DEFAULT;
    bd.BindFlags      = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    bd.MiscFlags      = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = data;

    RhiBuffer* buf = (RhiBuffer*)_arena.alloc(sizeof(RhiBuffer));
    if (!buf) return nullptr;
    buf->size = stride * count;

    ID3D11Buffer* d3d_buf = nullptr;
    HRESULT hr = ((ID3D11Device*)device)->CreateBuffer(&bd, &init, &d3d_buf);
    if (FAILED(hr)) return nullptr;
    buf->ptr = d3d_buf;
    return buf;
}

// ---------------------------------------------------------------------------
// Texture (output)
// ---------------------------------------------------------------------------

RhiTexture* Rhi::createOutputTexture(int w, int h) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width     = w;
    td.Height    = h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage     = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;

    RhiTexture* tex = (RhiTexture*)_arena.alloc(sizeof(RhiTexture));
    if (!tex) return nullptr;
    tex->width  = w;
    tex->height = h;

    ID3D11Texture2D* d3d_tex = nullptr;
    HRESULT hr = ((ID3D11Device*)device)->CreateTexture2D(&td, nullptr, &d3d_tex);
    if (FAILED(hr)) return nullptr;
    tex->ptr = d3d_tex;

    // UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavd = {};
    uavd.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    uavd.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    ((ID3D11Device*)device)->CreateUnorderedAccessView(d3d_tex, &uavd,
                                                       (ID3D11UnorderedAccessView**)&tex->uav);

    // SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srvd = {};
    srvd.Format        = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MipLevels = 1;
    ((ID3D11Device*)device)->CreateShaderResourceView(d3d_tex, &srvd,
                                                      (ID3D11ShaderResourceView**)&tex->srv);

    return tex;
}

// ---------------------------------------------------------------------------
// Compute shader compilation
// ---------------------------------------------------------------------------

RhiShader* Rhi::createComputeShader(const char* hlsl_source, const char* entry) {
    ID3DBlob* code   = nullptr;
    ID3DBlob* errors = nullptr;
    UINT flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG;
#endif

    HRESULT hr = D3DCompile(hlsl_source, strlen(hlsl_source), nullptr, nullptr, nullptr,
                            entry, "cs_5_0", flags, 0, &code, &errors);
    if (FAILED(hr)) {
        if (errors) {
            OutputDebugStringA((const char*)errors->GetBufferPointer());
            errors->Release();
        }
        return nullptr;
    }
    if (errors) errors->Release();

    ID3D11ComputeShader* cs = nullptr;
    hr = ((ID3D11Device*)device)->CreateComputeShader(code->GetBufferPointer(),
                                                       code->GetBufferSize(), nullptr, &cs);
    code->Release();
    if (FAILED(hr)) return nullptr;

    RhiShader* sh = (RhiShader*)_arena.alloc(sizeof(RhiShader));
    if (!sh) { cs->Release(); return nullptr; }
    sh->ptr = cs;
    return sh;
}

// ---------------------------------------------------------------------------
// Bindings & dispatch
// ---------------------------------------------------------------------------

void Rhi::bindConstantBuffer(RhiBuffer* buf, int slot) {
    ID3D11Buffer* d3d_buf = (ID3D11Buffer*)buf->ptr;
    ((ID3D11DeviceContext*)ctx)->CSSetConstantBuffers(slot, 1, &d3d_buf);
}

void Rhi::bindOutputTexture(RhiTexture* tex, int slot) {
    ID3D11UnorderedAccessView* uav = (ID3D11UnorderedAccessView*)tex->uav;
    ((ID3D11DeviceContext*)ctx)->CSSetUnorderedAccessViews(slot, 1, &uav, nullptr);
}

void Rhi::bindShader(RhiShader* shader) {
    ((ID3D11DeviceContext*)ctx)->CSSetShader((ID3D11ComputeShader*)shader->ptr, nullptr, 0);
}

void Rhi::dispatch(int gx, int gy, int gz) {
    ((ID3D11DeviceContext*)ctx)->Dispatch(gx, gy, gz);
    // Unbind UAV to allow copy to backbuffer
    ID3D11UnorderedAccessView* null_uav[1] = {nullptr};
    ((ID3D11DeviceContext*)ctx)->CSSetUnorderedAccessViews(0, 1, null_uav, nullptr);
}

void Rhi::present() {
    ((IDXGISwapChain*)swapchain)->Present(0, 0);
}

} // namespace mg
