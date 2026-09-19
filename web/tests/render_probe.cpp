// Pixel regression probe linked against the game's actual D3D9/WebGL backend.
#include <d3d9.h>
#include <emscripten.h>
#include <GLES3/gl3.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>

namespace {
EM_JS(void, record_result, (const char* name, const unsigned char* actual,
                            int r, int g, int b, int a, int pass, unsigned error), {
    const output = document.getElementById('results');
    const rows = JSON.parse(output.textContent || '[]');
    rows.push({name: UTF8ToString(name), actual: Array.from(HEAPU8.subarray(actual, actual + 4)),
        expected: [r, g, b, a], pass: Boolean(pass), glError: error});
    output.textContent = JSON.stringify(rows, null, 2);
});
EM_JS(void, finish_results, (int passed, int total), {
    document.documentElement.dataset.result = passed === total ? 'pass' : 'fail';
    document.documentElement.dataset.passed = String(passed);
    document.documentElement.dataset.total = String(total);
    document.getElementById('status').textContent = `${passed}/${total} pixel checks passed`;
});

IDirect3DDevice9* device{};
int passed{}, total{};

DWORD float_bits(float value) { DWORD result; std::memcpy(&result, &value, 4); return result; }
D3DMATRIX identity() { D3DMATRIX value{}; for (int i = 0; i < 4; ++i) value.m[i][i] = 1; return value; }
void state(DWORD key, DWORD value) { device->SetRenderState(key, value); }
void check(const char* name, std::array<int, 4> expected, int x = 64, int y = 64) {
    std::array<unsigned char, 4> actual{};
    glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, actual.data());
    const GLenum error = glGetError();
    bool success = error == GL_NO_ERROR;
    for (int i = 0; i < 4; ++i) success &= std::abs(int(actual[i]) - expected[i]) <= 1;
    ++total; if (success) ++passed;
    record_result(name, actual.data(), expected[0], expected[1], expected[2], expected[3], success, error);
}
struct Vertex { float x, y, z; DWORD color; };
struct ScreenVertex { float x, y, z, rhw; DWORD color; float u, v; };
void flat_depth(float z, DWORD color) {
    const Vertex vertices[] {{-.9f, .9f, z, color}, {.9f, .9f, z, color},
        {-.9f, -.9f, z, color}, {.9f, -.9f, z, color}};
    device->SetFVF(D3DFVF_XYZ | D3DFVF_DIFFUSE);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(Vertex));
}
void rectangle(float top, float bottom, DWORD color) {
    const ScreenVertex vertices[] {{-.5f, top-.5f, 0, 1, color, 0, 0},
        {127.5f, top-.5f, 0, 1, color, 1, 0},
        {-.5f, bottom-.5f, 0, 1, color, 0, 1},
        {127.5f, bottom-.5f, 0, 1, color, 1, 1}};
    device->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1);
    device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, vertices, sizeof(ScreenVertex));
}
void clear(DWORD color) { device->Clear(0, nullptr, D3DCLEAR_TARGET, color, 1, 0); }
void diffuse() {
    device->SetTexture(0, nullptr);
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
}
void texture(IDirect3DTexture9* value) {
    device->SetTexture(0, value);
    device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    device->SetSamplerState(0, 1, 3); device->SetSamplerState(0, 2, 3);
    device->SetSamplerState(0, 5, 1); device->SetSamplerState(0, 6, 1);
}
}

int main() {
    auto* api = Direct3DCreate9(D3D_SDK_VERSION);
    D3DPRESENT_PARAMETERS parameters{};
    parameters.BackBufferWidth = parameters.BackBufferHeight = 128;
    parameters.BackBufferFormat = D3DFMT_A8R8G8B8;
    parameters.Windowed = TRUE;
    if (api->CreateDevice(0, D3DDEVTYPE_HAL, nullptr, 0, &parameters, &device) != S_OK) return 2;
    IDirect3DSurface9* backbuffer{};
    IDirect3DTexture9* render_texture{};
    IDirect3DSurface9* render_surface{};
    device->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &backbuffer);
    device->CreateTexture(128, 128, 1, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
        D3DPOOL_DEFAULT, &render_texture, nullptr);
    render_texture->GetSurfaceLevel(0, &render_surface);
    // Exercise render-texture alpha separately from default-backbuffer alpha;
    // the latter is required by the game's destination-alpha transition pass.
    device->SetRenderTarget(0, render_surface);
    state(7, 0); state(27, 0); state(D3DRS_ALPHATESTENABLE, 0);
    diffuse();
    auto view = identity();
    auto projection = identity(); projection.m[2][2] = 1.f / 1024.f;
    device->SetTransform(D3DTS_PROJECTION, &projection);
    state(D3DRS_FOGENABLE, 1); state(D3DRS_FOGCOLOR, 0xff2040c0);
    state(140, 3); state(35, 0); state(48, 0);
    state(D3DRS_FOGSTART, float_bits(100.f)); state(D3DRS_FOGEND, float_bits(300.f));
    for (const auto& entry : std::array<std::pair<float, const char*>, 3>{{
            {50.f, "linear fog before start"}, {200.f, "linear fog midpoint"}, {400.f, "linear fog past end"}}}) {
        clear(0); flat_depth(entry.first, 0x80e06020);
        const float factor = std::max(0.f, std::min(1.f, (300.f-entry.first)/200.f));
        check(entry.second, {int(std::lround(32+192*factor)), int(std::lround(64+32*factor)),
            int(std::lround(192-160*factor)), 128});
    }
    view.m[3][2] = 100.f;
    device->SetTransform(D3DTS_VIEW, &view);
    clear(0); flat_depth(100.f, 0x80e06020);
    check("vertex fog uses view-space depth", {128, 80, 112, 128});
    state(D3DRS_FOGENABLE, 0);
    clear(0); flat_depth(100.f, 0x80e06020);
    check("fog disabled preserves diffuse RGBA", {224, 96, 32, 128});
    view = identity(); device->SetTransform(D3DTS_VIEW, &view);
    state(D3DRS_FOGENABLE, 1); state(140, 1); state(38, float_bits(.005f));
    clear(0); flat_depth(200.f, 0x80e06020);
    check("exponential vertex fog", {103, 76, 133, 128});
    state(140, 2);
    clear(0); flat_depth(200.f, 0x80e06020);
    check("exponential-squared vertex fog", {103, 76, 133, 128});

    state(D3DRS_FOGENABLE, 0); state(27, 1);
    state(D3DRS_SRCBLEND, 2); state(D3DRS_DESTBLEND, 2);
    state(D3DRS_SEPARATEALPHABLENDENABLE, 0);
    struct BlendCase { DWORD operation; const char* name; std::array<int, 4> expected; };
    const BlendCase blend_cases[] {
        {1, "add blend", {160, 160, 255, 192}},
        {3, "reverse-subtract blend", {0, 96, 128, 0}},
        {4, "minimum blend", {64, 32, 64, 64}},
        {5, "maximum blend", {96, 128, 192, 128}}
    };
    for (const auto& entry : blend_cases) {
        clear(0x404080c0); state(D3DRS_BLENDOP, entry.operation);
        rectangle(0, 128, 0x80602040); check(entry.name, entry.expected);
    }
    state(D3DRS_BLENDOP, 1); state(D3DRS_SRCBLEND, 1); state(D3DRS_DESTBLEND, 2);
    state(D3DRS_SEPARATEALPHABLENDENABLE, 1);
    state(D3DRS_SRCBLENDALPHA, 2); state(D3DRS_DESTBLENDALPHA, 1); state(D3DRS_BLENDOPALPHA, 1);
    clear(0x404080c0); rectangle(0, 128, 0x80602040);
    check("mask preserves RGB and replaces alpha", {64, 128, 192, 128});

    // The mask pass can leave the destination texture bound while selecting
    // only DIFFUSE. An inactive texture must not cause WebGL feedback errors.
    device->SetTexture(0, render_texture);
    clear(0x404080c0); rectangle(0, 128, 0x80602040);
    check("unused bound render texture does not cause feedback", {64, 128, 192, 128});
    device->SetTexture(0, nullptr);

    state(27, 0); state(D3DRS_SEPARATEALPHABLENDENABLE, 0);
    clear(0); rectangle(0, 64, 0xffff0000); rectangle(64, 128, 0xff0000ff);
    check("render target top row", {255, 0, 0, 255}, 64, 16);
    check("render target bottom row", {0, 0, 255, 255}, 64, 111);
    device->SetRenderTarget(0, backbuffer); clear(0xff000000);
    texture(render_texture); rectangle(0, 128, 0xffffffff);
    check("render texture sampled upright: top", {255, 0, 0, 255}, 64, 111);
    check("render texture sampled upright: bottom", {0, 0, 255, 255}, 64, 16);

    IDirect3DTexture9* uploaded{};
    device->CreateTexture(2, 2, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &uploaded, nullptr);
    D3DLOCKED_RECT locked{}; uploaded->LockRect(0, &locked, nullptr, 0);
    const DWORD colors[] {0xffff0000, 0xffff0000, 0xff0000ff, 0xff0000ff};
    std::memcpy(locked.pBits, colors, 8);
    std::memcpy(static_cast<unsigned char*>(locked.pBits)+locked.Pitch, colors+2, 8);
    uploaded->UnlockRect(0); texture(uploaded); rectangle(0, 128, 0xffffffff);
    check("uploaded texture sampled upright: top", {255, 0, 0, 255}, 64, 111);
    check("uploaded texture sampled upright: bottom", {0, 0, 255, 255}, 64, 16);

    // Stage transitions also use destination alpha on the default framebuffer.
    // An opaque WebGL context silently forces that alpha to one; RGB-only
    // render-target tests cannot catch that context-creation regression.
    diffuse();
    clear(0x004080c0);
    check("backbuffer clear retains zero alpha", {64, 128, 192, 0});
    clear(0x404080c0);
    check("backbuffer clear retains partial alpha", {64, 128, 192, 64});
    rectangle(0, 128, 0x80602040);
    check("backbuffer diffuse draw writes partial alpha", {96, 32, 64, 128});

    const auto alpha_mask = [&](DWORD alpha) {
        state(27, 1);
        state(D3DRS_BLENDOP, 1);
        state(D3DRS_SRCBLEND, 1); state(D3DRS_DESTBLEND, 2);
        state(D3DRS_SEPARATEALPHABLENDENABLE, 1);
        state(D3DRS_SRCBLENDALPHA, 2); state(D3DRS_DESTBLENDALPHA, 1);
        state(D3DRS_BLENDOPALPHA, 1);
        // Leave an unrelated sampled texture bound: both stage operations
        // select DIFFUSE, so the solid mask must ignore its RGB and alpha.
        device->SetTexture(0, uploaded);
        rectangle(0, 128, (alpha << 24) | 0x00602040);
    };
    const auto transition = [&] {
        state(D3DRS_SEPARATEALPHABLENDENABLE, 0);
        state(D3DRS_SRCBLEND, 7); state(D3DRS_DESTBLEND, 8);
        rectangle(0, 128, 0xffe06020);
    };
    clear(0x404080c0); alpha_mask(128);
    check("backbuffer solid texture-unused alpha mask", {64, 128, 192, 128});
    transition();
    check("backbuffer DESTALPHA transition at partial alpha", {144, 112, 112, 192});
    clear(0x404080c0); alpha_mask(0); transition();
    check("backbuffer DESTALPHA transition at zero alpha", {64, 128, 192, 0});
    clear(0x404080c0); alpha_mask(255); transition();
    check("backbuffer DESTALPHA transition at full alpha", {224, 96, 32, 255});

    // The browser presentation boundary makes the canvas opaque only after all
    // game blending has consumed its alpha; it must leave the RGB image intact.
    clear(0x404080c0);
    device->Present(nullptr, nullptr, nullptr, nullptr);
    check("Present preserves RGB and makes browser output opaque", {64, 128, 192, 255});
    finish_results(passed, total);
    std::printf("WebGL renderer pixel probe: %d/%d passed\n", passed, total);
    return passed == total ? 0 : 1;
}
