#include <array>
#include <iostream>

#include <d3d.h>

#include "../common/bit.h"
#include "../common/com.h"
#include "../common/error.h"
#include "../common/str.h"

struct RGBVERTEX {
    FLOAT x, y, z, rhw;
    DWORD colour;
};

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirectDraw4, 0x9C59509A, 0x39BD, 0x11D1, 0x8C, 0x4A, 0x00, 0xC0, 0x4F, 0xD9, 0x30, 0xC5);
__CRT_UUID_DECL(IDirect3D3,   0xBB223240, 0xE72B, 0x11D0, 0xA9, 0xB4, 0x00, 0xAA, 0x00, 0xC0, 0x99, 0x3E);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("9C59509A-39BD-11D1-8C4A-00C04FD930C5") IDirectDraw4;
interface DECLSPEC_UUID("BB223240-E72B-11D0-A9B4-00AA00C0993E") IDirect3D3;
#endif

#define RGBT_FVF_CODES (D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
#define PTRCLEARED(ptr) (ptr == nullptr)
#define D3DCOLOR_ARGB(a,r,g,b) \
    ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#define D3DCOLOR_XRGB(r,g,b)   D3DCOLOR_ARGB(0xff,r,g,b)

HRESULT WINAPI EnumModesCallback2(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPVOID lpContext) {
    UINT* adapterModeCount = reinterpret_cast<UINT*>(lpContext);

    std::cout << format("    ", lpDDSurfaceDesc->dwWidth, " x ", lpDDSurfaceDesc->dwHeight,
                        " : ", lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount, " bpp",
                        " @ ", lpDDSurfaceDesc->dwRefreshRate, " Hz") << std::endl;

    *adapterModeCount = *adapterModeCount + 1;

    return DDENUMRET_OK;
}

class RGBTriangle {

    public:

        static constexpr IID IID_IDirect3DHALDevice     = { 0x84E63DE0, 0x46AA, 0x11CF, {0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E} };
        static constexpr IID IID_IDirect3DRGBDevice     = { 0xA4665C60, 0x2673, 0x11CF, {0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56} };

        static constexpr const char* TRIANGLE_ID    = "D3D6_Triangle";
        static constexpr const char* TRIANGLE_TITLE = "D3D6 Triangle - Blisto Bronze Age Testing Edition";

        static constexpr UINT WINDOW_WIDTH  = 700;
        static constexpr UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            std::cout << RGBTriangle::TRIANGLE_TITLE << std::endl << std::endl;

            decltype(DirectDrawCreate)* DirectDrawCreate = nullptr;
            HMODULE hm = LoadLibraryA("ddraw.dll");
            DirectDrawCreate = (decltype(DirectDrawCreate))GetProcAddress(hm, "DirectDrawCreate");

            // DDraw Interface
            Com<IDirectDraw> ddrawBase;
            HRESULT status = DirectDrawCreate(NULL, &ddrawBase, NULL);
            if (FAILED(status))
                throw Error("Failed to create DDraw interface");

            // DDraw4 Interface
            status = ddrawBase->QueryInterface(__uuidof(IDirectDraw4), reinterpret_cast<void**>(&m_ddraw));
            if (FAILED(status))
                throw Error("Failed to create DDraw4 interface");

            status = m_ddraw->SetCooperativeLevel(m_hWnd, DDSCL_NORMAL);
            if (FAILED(status))
                throw Error("Failed to set cooperative level");

            DDSURFACEDESC2 desc2 = { };
            desc2.dwSize = sizeof(DDSURFACEDESC2);
            desc2.dwFlags = DDSD_CAPS;
            desc2.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

            status = m_ddraw->CreateSurface(&desc2, &m_primarySurf, NULL);
            if (FAILED(status))
                throw Error("Failed to create the primary surface");

            desc2 = { };
            desc2.dwSize = sizeof(DDSURFACEDESC2);
            desc2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
            desc2.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
            desc2.dwWidth = WINDOW_WIDTH;
            desc2.dwHeight = WINDOW_HEIGHT;
            status = m_ddraw->CreateSurface(&desc2, &m_rt, NULL);
            if (FAILED(status))
                throw Error("Failed to create the render target");

            Com<IDirectDrawClipper> clipper;
            status = m_ddraw->CreateClipper(0, &clipper, NULL);
            if (FAILED(status))
                throw Error("Failed to create the clipper");

            clipper->SetHWnd(0, m_hWnd);
            m_primarySurf->SetClipper(clipper.ptr());

            // D3D6 Interface
            status = m_ddraw->QueryInterface(__uuidof(IDirect3D3), reinterpret_cast<void**>(&m_d3d));
            if (FAILED(status))
                throw Error("Failed to create D3D6 interface");

            // D3D6 Viewport
            D3DVIEWPORT2 viewPortData2 = { };
            viewPortData2.dwSize       = sizeof(D3DVIEWPORT2);
            viewPortData2.dwWidth      = WINDOW_WIDTH;
            viewPortData2.dwHeight     = WINDOW_HEIGHT;
            viewPortData2.dvClipX      = -1.0f;
            viewPortData2.dvClipWidth  = 2.0f;
            viewPortData2.dvClipY      = 1.0f;
            viewPortData2.dvClipHeight = 2.0f;
            viewPortData2.dvMaxZ       = 1.0f;
            status = m_d3d->CreateViewport(&m_viewport, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D6 viewport");
            m_viewport->SetViewport2(&viewPortData2);

            createDeviceWithFlags(IID_IDirect3DHALDevice, true);
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            UINT adapterModeCount = 0;

            std::cout << std::endl << "Enumerating supported adapter display modes:" << std::endl;

            HRESULT status = m_ddraw->EnumDisplayModes(DDEDM_REFRESHRATES, NULL, &adapterModeCount, &EnumModesCallback2);
            if (FAILED(status)) {
                std::cout << "Failed to enumerate adapter display modes" << std::endl;
            } else {
                std::cout << format("Listed a total of ", adapterModeCount, " adapter display modes") << std::endl;
            }
        }

        // D3D Device capabilities check
        void listDeviceCapabilities() {
            D3DDEVICEDESC caps6HAL = { };
            caps6HAL.dwSize = sizeof(D3DDEVICEDESC);
            D3DDEVICEDESC caps6HEL = { };
            caps6HEL.dwSize = sizeof(D3DDEVICEDESC);

            // get the capabilities from the D3D device in HAL mode
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);
            m_device->GetCaps(&caps6HAL, &caps6HEL);

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL)
                std::cout << "  + D3DDEVCAPS_CANBLTSYSTONONLOCAL is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANBLTSYSTONONLOCAL is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP)
                std::cout << "  + D3DDEVCAPS_CANRENDERAFTERFLIP is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANRENDERAFTERFLIP is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_DRAWPRIMTLVERTEX)
                std::cout << "  + D3DDEVCAPS_DRAWPRIMTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_DRAWPRIMTLVERTEX is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_EXECUTESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTESYSTEMMEMORY is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_EXECUTEVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTEVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTEVIDEOMEMORY is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_FLOATTLVERTEX)
                std::cout << "  + D3DDEVCAPS_FLOATTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_FLOATTLVERTEX is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION)
                std::cout << "  + D3DDEVCAPS_HWRASTERIZATION is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWRASTERIZATION is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                std::cout << "  + D3DDEVCAPS_HWTRANSFORMANDLIGHT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWTRANSFORMANDLIGHT is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES)
                std::cout << "  + D3DDEVCAPS_SEPARATETEXTUREMEMORIES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SEPARATETEXTUREMEMORIES is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_SORTDECREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTDECREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTDECREASINGZ is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_SORTEXACT)
                std::cout << "  + D3DDEVCAPS_SORTEXACT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTEXACT is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_SORTINCREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTINCREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTINCREASINGZ is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM)
                std::cout << "  + D3DDEVCAPS_TEXTURENONLOCALVIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURENONLOCALVIDMEM is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTURESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURESYSTEMMEMORY is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTUREVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTUREVIDEOMEMORY is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is not supported" << std::endl;

            if (caps6HAL.dwDevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXVIDEOMEMORY is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ dwMaxBufferSize: ", caps6HAL.dwMaxBufferSize) << std::endl;
            std::cout << format("  ~ dwMaxVertexCount: ", caps6HAL.dwMaxVertexCount) << std::endl;
            std::cout << format("  ~ dwMinTextureWidth: ", caps6HAL.dwMinTextureWidth) << std::endl;
            std::cout << format("  ~ dwMinTextureHeight: ", caps6HAL.dwMinTextureHeight) << std::endl;
            std::cout << format("  ~ dwMaxTextureWidth: ", caps6HAL.dwMaxTextureWidth) << std::endl;
            std::cout << format("  ~ dwMaxTextureHeight: ", caps6HAL.dwMaxTextureHeight) << std::endl;
            std::cout << format("  ~ dwMinStippleWidth: ", caps6HAL.dwMinStippleWidth) << std::endl;
            std::cout << format("  ~ dwMinStippleHeight: ", caps6HAL.dwMinStippleHeight) << std::endl;
            std::cout << format("  ~ dwMaxStippleWidth: ", caps6HAL.dwMaxStippleWidth) << std::endl;
            std::cout << format("  ~ dwMaxStippleHeight: ", caps6HAL.dwMaxStippleHeight) << std::endl;
            std::cout << format("  ~ dwMaxTextureRepeat: ", caps6HAL.dwMaxTextureRepeat) << std::endl;
            std::cout << format("  ~ dwMaxTextureAspectRatio: ", caps6HAL.dwMaxTextureAspectRatio) << std::endl;
            std::cout << format("  ~ dwMaxAnisotropy: ", caps6HAL.dwMaxAnisotropy) << std::endl;
            std::cout << format("  ~ dvGuardBandLeft: ", caps6HAL.dvGuardBandLeft) << std::endl;
            std::cout << format("  ~ dvGuardBandTop: ", caps6HAL.dvGuardBandTop) << std::endl;
            std::cout << format("  ~ dvGuardBandRight: ", caps6HAL.dvGuardBandRight) << std::endl;
            std::cout << format("  ~ dvGuardBandBottom: ", caps6HAL.dvGuardBandBottom) << std::endl;
            std::cout << format("  ~ dvExtentsAdjust: ", caps6HAL.dvExtentsAdjust) << std::endl;
            std::cout << format("  ~ wMaxTextureBlendStages: ", caps6HAL.wMaxTextureBlendStages) << std::endl;
            std::cout << format("  ~ wMaxSimultaneousTextures: ", caps6HAL.wMaxSimultaneousTextures) << std::endl;
        }

        void listAvailableTextureMemory() {
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);

            std::cout << std::endl << "Listing available texture memory:" << std::endl;

            DDSCAPS2 ddsCaps2 = { };
            ddsCaps2.dwCaps = DDSCAPS_TEXTURE;
            DWORD    dwTotal  = 0;
            DWORD    dwFree   = 0;
            HRESULT status = m_ddraw->GetAvailableVidMem(&ddsCaps2, &dwTotal, &dwFree);
            if (FAILED(status)) {
                std::cout << "Failed to list available texture memory" << std::endl;
            } else {
                std::cout << format("  ~ Bytes: ", dwFree) << std::endl;
            }
        }

        void prepare() {
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);

            // don't need any of these for 2D rendering
            HRESULT status = m_device->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D6 render state for D3DRENDERSTATE_ZENABLE");
            status = m_device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D6 render state for D3DRENDERSTATE_CULLMODE");
            status = m_device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D6 render state for D3DRENDERSTATE_LIGHTING");

            // Vertex Buffer
            void* vertices = nullptr;

            D3DVERTEXBUFFERDESC descVB = { };
            descVB.dwSize = sizeof(D3DVERTEXBUFFERDESC);
            descVB.dwFVF = RGBT_FVF_CODES;
            descVB.dwNumVertices = m_rgbVerticesSize;
            status = m_d3d->CreateVertexBuffer(&descVB, &m_vb, 0, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D6 vertex buffer");

            status = m_vb->Lock(0, &vertices, NULL);
            if (FAILED(status))
                throw Error("Failed to lock D3D6 vertex buffer");
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D6 vertex buffer");
        }

        void render() {
            HRESULT status = m_viewport->Clear2(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D6 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->DrawPrimitiveVB(D3DPT_TRIANGLELIST, m_vb.ptr(), 0, m_rgbVertices.size(), 0);
                if (FAILED(status))
                    throw Error("Failed to draw D3D6 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    status = m_primarySurf->Blt(&s_rect, m_rt.ptr(), NULL, DDBLT_WAIT, NULL);
                } else {
                    throw Error("Failed to end D3D6 scene");
                }
            } else {
                throw Error("Failed to begin D3D6 scene");
            }
        }

        static void AdjustRect(DWORD x, DWORD y) {
            s_rect.left   = x;
            s_rect.top    = y;
            s_rect.right  = x + WINDOW_WIDTH;
            s_rect.bottom = y + WINDOW_HEIGHT;
        }

    private:

        HRESULT createDeviceWithFlags(IID deviceIID,
                                      bool throwErrorOnFail) {
            if (m_d3d == nullptr)
                throw Error("The D3D6 interface hasn't been initialized");

            if (m_rt == nullptr)
                throw Error("The D3D6 render target hasn't been initialized");

            m_device = nullptr;

            HRESULT status = m_d3d->CreateDevice(deviceIID, m_rt.ptr(), &m_device, NULL);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D6 device");

            if (m_device != nullptr) {
                status = m_device->AddViewport(m_viewport.ptr());
                if (FAILED(status))
                    std::cout << "Failed to add D3D6 viewport" << std::endl;

                status = m_device->SetCurrentViewport(m_viewport.ptr());
                if (FAILED(status))
                    std::cout << "Failed to set D3D6 viewport" << std::endl;
            }

            return status;
        }

        static RECT                   s_rect;

        HWND                          m_hWnd;

        DWORD                         m_vendorID;

        Com<IDirectDraw4>             m_ddraw;
        Com<IDirect3D3>               m_d3d;
        Com<IDirect3DDevice3>         m_device;
        Com<IDirect3DViewport3>       m_viewport;
        Com<IDirect3DVertexBuffer>    m_vb;

        Com<IDirectDrawSurface4>      m_primarySurf;
        Com<IDirectDrawSurface4>      m_rt;

        // tailored for 1024x768 and the appearance of being centered
        std::array<RGBVERTEX, 3>      m_rgbVertices = {{ { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 127, 80),},
                                                         {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(184, 115, 51),},
                                                         {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(100, 40, 20),} }};
        const DWORD                   m_rgbVerticesSize = m_rgbVertices.size() * sizeof(RGBVERTEX);

        UINT                          m_totalTests;
        UINT                          m_passedTests;
};

RECT RGBTriangle::s_rect = { };

LRESULT WINAPI WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch(message) {
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
        // Technically should also handle/adjust on window resize, but I'd rather not
        case WM_MOVE:
            RGBTriangle::AdjustRect(LOWORD(lParam), HIWORD(lParam));
            break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}

int main(int, char**) {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_CLASSDC, WindowProc, 0L, 0L,
                     GetModuleHandle(NULL), NULL, LoadCursor(nullptr, IDC_ARROW), NULL, NULL,
                     RGBTriangle::TRIANGLE_ID, NULL};
    RegisterClassEx(&wc);

    HWND hWnd = CreateWindow(RGBTriangle::TRIANGLE_ID, RGBTriangle::TRIANGLE_TITLE,
                             WS_OVERLAPPEDWINDOW, 50, 50,
                             RGBTriangle::WINDOW_WIDTH, RGBTriangle::WINDOW_HEIGHT,
                             GetDesktopWindow(), NULL, wc.hInstance, NULL);

    MSG msg;

    try {
        RGBTriangle rgbTriangle(hWnd);

        // list various D3D Device stats
        rgbTriangle.listAdapterDisplayModes();
        rgbTriangle.listDeviceCapabilities();
        rgbTriangle.listAvailableTextureMemory();

        // D3D6 triangle
        rgbTriangle.prepare();

        ShowWindow(hWnd, SW_SHOWDEFAULT);
        UpdateWindow(hWnd);

        while (true) {
            if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);

                if (msg.message == WM_QUIT) {
                    UnregisterClass(RGBTriangle::TRIANGLE_ID, wc.hInstance);
                    return msg.wParam;
                }
            } else {
                rgbTriangle.render();
            }
        }

    } catch (const Error& e) {
        std::cerr << e.message() << std::endl;
        UnregisterClass(RGBTriangle::TRIANGLE_ID, wc.hInstance);
        return msg.wParam;
    }
}
