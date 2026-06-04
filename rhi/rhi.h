#pragma once
#include "../os/os.h"

namespace mg {

// ---------------------------------------------------------------------------
// RHI — DirectX 11 Compute Shader abstraction
// ---------------------------------------------------------------------------
struct RhiBuffer {
    void* ptr = nullptr; // ID3D11Buffer*
    int   size = 0;
};

struct RhiTexture {
    void* ptr     = nullptr; // ID3D11Texture2D*
    void* uav     = nullptr; // ID3D11UnorderedAccessView*
    void* srv     = nullptr; // ID3D11ShaderResourceView*
    int   width   = 0;
    int   height  = 0;
};

struct RhiShader {
    void* ptr = nullptr; // ID3D11ComputeShader*
};

struct Rhi {
    bool init(void* hwnd, int width, int height);
    void shutdown();

    RhiBuffer* createConstantBuffer(const void* data, int size);
    RhiBuffer* createStructuredBuffer(const void* data, int stride, int count);
    RhiTexture* createOutputTexture(int w, int h);
    RhiShader*  createComputeShader(const char* hlsl_source, const char* entry = "main");

    void bindConstantBuffer(RhiBuffer* buf, int slot);
    void bindOutputTexture(RhiTexture* tex, int slot);
    void bindShader(RhiShader* shader);
    void dispatch(int groups_x, int groups_y, int groups_z = 1);
    void present();

    void* device       = nullptr; // ID3D11Device*
    void* ctx          = nullptr; // ID3D11DeviceContext*
    void* swapchain    = nullptr; // IDXGISwapChain*
    void* backbuffer   = nullptr; // ID3D11Texture2D*
    void* backbuffer_rtv = nullptr; // ID3D11RenderTargetView*
    int   width  = 0;
    int   height = 0;

    // Internal allocator arena for RHI
    Arena _arena;
};

} // namespace mg
