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
__CRT_UUID_DECL(IDirectDraw7, 0x15E65EC0, 0x3B9C, 0x11D2, 0xB9, 0x2F, 0x00, 0x60, 0x97, 0x97, 0xEA, 0x5B);
__CRT_UUID_DECL(IDirect3D7,   0xF5049E77, 0x4861, 0x11D2, 0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("15E65EC0-3B9C-11D2-B92F-00609797EA5B") IDirectDraw7;
interface DECLSPEC_UUID("F5049E77-4861-11D2-A407-00A0C90629A8") IDirect3D7;
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

HRESULT WINAPI ListMipChainSurfaces7Callback(IDirectDrawSurface7* subsurf, DDSURFACEDESC2* desc, void* ctx) {
    IDirectDrawSurface7** nextMip = static_cast<IDirectDrawSurface7**>(ctx);

    if ((desc->ddsCaps.dwCaps  & DDSCAPS_MIPMAP)
     || (desc->ddsCaps.dwCaps2 & DDSCAPS2_MIPMAPSUBLEVEL)) {
      *nextMip = subsurf;
      return DDENUMRET_CANCEL;
    }

    return DDENUMRET_OK;
}

class RGBTriangle {

    public:

        static constexpr IID IID_IDirect3DTnLHalDevice  = { 0xF5049E78, 0x4861, 0x11D2, {0xA4, 0x07, 0x00, 0xA0, 0xC9, 0x06, 0x29, 0xA8} };
        static constexpr IID IID_IDirect3DHALDevice     = { 0x84E63DE0, 0x46AA, 0x11CF, {0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E} };
        static constexpr IID IID_IDirect3DRGBDevice     = { 0xA4665C60, 0x2673, 0x11CF, {0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56} };

        static constexpr const char* TRIANGLE_ID    = "D3D7_Triangle";
        static constexpr const char* TRIANGLE_TITLE = "D3D7 Triangle - Blisto Ancient Testing Edition";

        static constexpr UINT WINDOW_WIDTH  = 700;
        static constexpr UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            std::cout << RGBTriangle::TRIANGLE_TITLE << std::endl << std::endl;

            decltype(DirectDrawCreateEx)* DirectDrawCreateEx = nullptr;
            HMODULE hm = LoadLibraryA("ddraw.dll");
            DirectDrawCreateEx = (decltype(DirectDrawCreateEx))GetProcAddress(hm, "DirectDrawCreateEx");

            // DDraw7 Interface
            HRESULT status = DirectDrawCreateEx(NULL, reinterpret_cast<void**>(&m_ddraw), __uuidof(IDirectDraw7), NULL);
            if (FAILED(status))
                throw Error("Failed to create DDraw7 interface");

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

            // D3D7 Interface
            status = m_ddraw->QueryInterface(__uuidof(IDirect3D7), reinterpret_cast<void**>(&m_d3d));
            if (FAILED(status))
                throw Error("Failed to create D3D7 interface");

            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);
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
            D3DDEVICEDESC7 caps7TNLHAL = { };
            D3DDEVICEDESC7 caps7RGB = { };

            // get the capabilities from the D3D device in HAL mode
            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);
            m_device->GetCaps(&caps7TNLHAL);

            // get the capabilities from the D3D device in RGB mode
            createDeviceWithFlags(IID_IDirect3DRGBDevice, true);
            m_device->GetCaps(&caps7RGB);

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL)
                std::cout << "  + D3DDEVCAPS_CANBLTSYSTONONLOCAL is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANBLTSYSTONONLOCAL is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP)
                std::cout << "  + D3DDEVCAPS_CANRENDERAFTERFLIP is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANRENDERAFTERFLIP is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_DRAWPRIMTLVERTEX)
                std::cout << "  + D3DDEVCAPS_DRAWPRIMTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_DRAWPRIMTLVERTEX is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_EXECUTESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTESYSTEMMEMORY is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_EXECUTEVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTEVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTEVIDEOMEMORY is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_FLOATTLVERTEX)
                std::cout << "  + D3DDEVCAPS_FLOATTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_FLOATTLVERTEX is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION)
                std::cout << "  + D3DDEVCAPS_HWRASTERIZATION is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWRASTERIZATION is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                std::cout << "  + D3DDEVCAPS_HWTRANSFORMANDLIGHT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWTRANSFORMANDLIGHT is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES)
                std::cout << "  + D3DDEVCAPS_SEPARATETEXTUREMEMORIES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SEPARATETEXTUREMEMORIES is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_SORTDECREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTDECREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTDECREASINGZ is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_SORTEXACT)
                std::cout << "  + D3DDEVCAPS_SORTEXACT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTEXACT is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_SORTINCREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTINCREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTINCREASINGZ is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM)
                std::cout << "  + D3DDEVCAPS_TEXTURENONLOCALVIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURENONLOCALVIDMEM is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTURESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURESYSTEMMEMORY is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTUREVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTUREVIDEOMEMORY is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is not supported" << std::endl;

            if (caps7TNLHAL.dwDevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXVIDEOMEMORY is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ dwMinTextureWidth: ", caps7TNLHAL.dwMinTextureWidth) << std::endl;
            std::cout << format("  ~ dwMinTextureHeight: ", caps7TNLHAL.dwMinTextureHeight) << std::endl;
            std::cout << format("  ~ dwMaxTextureWidth: ", caps7TNLHAL.dwMaxTextureWidth) << std::endl;
            std::cout << format("  ~ dwMaxTextureHeight: ", caps7TNLHAL.dwMaxTextureHeight) << std::endl;
            std::cout << format("  ~ dwMaxTextureRepeat: ", caps7TNLHAL.dwMaxTextureRepeat) << std::endl;
            std::cout << format("  ~ dwMaxTextureAspectRatio: ", caps7TNLHAL.dwMaxTextureAspectRatio) << std::endl;
            std::cout << format("  ~ dwMaxAnisotropy: ", caps7TNLHAL.dwMaxAnisotropy) << std::endl;
            std::cout << format("  ~ dvGuardBandLeft: ", caps7TNLHAL.dvGuardBandLeft) << std::endl;
            std::cout << format("  ~ dvGuardBandTop: ", caps7TNLHAL.dvGuardBandTop) << std::endl;
            std::cout << format("  ~ dvGuardBandRight: ", caps7TNLHAL.dvGuardBandRight) << std::endl;
            std::cout << format("  ~ dvGuardBandBottom: ", caps7TNLHAL.dvGuardBandBottom) << std::endl;
            std::cout << format("  ~ dvExtentsAdjust: ", caps7TNLHAL.dvExtentsAdjust) << std::endl;
            std::cout << format("  ~ wMaxTextureBlendStages: ", caps7TNLHAL.wMaxTextureBlendStages) << std::endl;
            std::cout << format("  ~ wMaxSimultaneousTextures: ", caps7TNLHAL.wMaxSimultaneousTextures) << std::endl;
            std::cout << format("  ~ dwMaxActiveLights: ", caps7TNLHAL.dwMaxActiveLights) << std::endl;
            std::cout << format("  ~ dvMaxVertexW: ", caps7TNLHAL.dvMaxVertexW) << std::endl;
            std::cout << format("  ~ wMaxUserClipPlanes: ", caps7TNLHAL.wMaxUserClipPlanes) << std::endl;
            std::cout << format("  ~ wMaxVertexBlendMatrices: ", caps7TNLHAL.wMaxVertexBlendMatrices) << std::endl;
        }

        void listAvailableTextureMemory() {
            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);

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

        void startTests() {
            m_totalTests  = 0;
            m_passedTests = 0;

            std::cout << std::endl << "Running D3D7 tests:" << std::endl;
        }

        // Test setting a viewport with zero MinZ/MaxZ
        void testZeroViewport() {
            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);

            m_totalTests++;

            D3DVIEWPORT7 viewport7 = { };
            viewport7.dwX = 0;
            viewport7.dwY = 0;
            viewport7.dwWidth = WINDOW_WIDTH;
            viewport7.dwHeight = WINDOW_HEIGHT;
            viewport7.dvMinZ = 0.0f;
            viewport7.dvMaxZ = 0.0f;
            HRESULT status = m_device->SetViewport(&viewport7);

            viewport7 = { };
            m_device->GetViewport(&viewport7);
            //std::cout << format("  ~ dvMaxZ: ", viewport7.dvMaxZ) << std::endl;

            if (FAILED(status) || viewport7.dvMaxZ != 0.001f) {
                std::cout << "  - The ZeroViewport test has failed" << std::endl;
            } else {
                m_passedTests++;
                std::cout << "  + The ZeroViewport test has passed" << std::endl;
            }
        }

        // Test creating a surface with an invalid mip map count vs surface size
        void testMipMapLevels() {
            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);

            m_totalTests++;

            Com<IDirectDrawSurface7> tex7;
            DDSURFACEDESC2 desc2 = { };
            desc2.dwSize = sizeof(DDSURFACEDESC2);
            desc2.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_MIPMAPCOUNT;
            desc2.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX;
            desc2.dwMipMapCount = 10; // Too many mips in theory for the texture size
            desc2.dwWidth = 8;
            desc2.dwHeight = 4;
            HRESULT status = m_ddraw->CreateSurface(&desc2, &tex7, NULL);

            // Outright fails on native, but we have to account for WineD3D behavior
            if (FAILED(status)) {
                m_passedTests++;
                std::cout << "  + The mip map level test has succeded" << std::endl;
            } else {
                Com<IDirectDrawSurface7> mip7 = tex7;
                uint32_t mipCount = 1;

                while (mip7 != nullptr) {
                    IDirectDrawSurface7* parentSurface = mip7.ptr();
                    mip7 = nullptr;
                    parentSurface->EnumAttachedSurfaces(&mip7, ListMipChainSurfaces7Callback);
                    if (mip7 != nullptr) {
                        //desc2 = { };
                        //desc2.dwSize = sizeof(DDSURFACEDESC2);
                        //mip7->GetSurfaceDesc(&desc2);
                        mipCount++;
                        //std::cout << format("  ~ Mip ", mipCount, " size: ", desc2.dwWidth , "x", desc2.dwHeight) << std::endl;
                    }
                }

                // WineD3D creates the surface with the requested mip count,
                // even if surface dimensions generate multiple 1x1 mip maps
                if (FAILED(status) || mipCount != 10) {
                    std::cout << "  - The mip map level test has failed" << std::endl;
                } else {
                    m_passedTests++;
                    std::cout << "  + The mip map level test has succeded" << std::endl;
                }
            }
        }

        // Test setting a few render states which are obsolete in D3D7
        void testObsoleteRenderStates() {
            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);

            m_totalTests++;

            HRESULT status1 = m_device->SetRenderState(D3DRENDERSTATE_ANISOTROPY, TRUE);
            HRESULT status2 = m_device->SetRenderState(D3DRENDERSTATE_MONOENABLE, TRUE);
            HRESULT status3 = m_device->SetRenderState(D3DRENDERSTATE_ROP2, TRUE);
            HRESULT status4 = m_device->SetRenderState(D3DRENDERSTATE_STIPPLEENABLE, TRUE);

            if (FAILED(status1) && FAILED(status2) && FAILED(status3) && FAILED(status4)) {
                m_passedTests++;
                std::cout << "  + The obsolete render state test has succeded" << std::endl;
            } else {
                std::cout << "  - The obsolete render state test has failed" << std::endl;
            }
        }

        void printTestResults() {
            std::cout << std::endl << format("Passed ", m_passedTests, "/", m_totalTests, " tests") << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(IID_IDirect3DTnLHalDevice, true);

            // don't need any of these for 2D rendering
            HRESULT status = m_device->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D7 render state for D3DRENDERSTATE_ZENABLE");
            status = m_device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D7 render state for D3DRENDERSTATE_CULLMODE");
            status = m_device->SetRenderState(D3DRENDERSTATE_LIGHTING, FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D7 render state for D3DRENDERSTATE_LIGHTING");

            // Vertex Buffer
            void* vertices = nullptr;

            D3DVERTEXBUFFERDESC descVB = { };
            descVB.dwSize = sizeof(D3DVERTEXBUFFERDESC);
            descVB.dwFVF = RGBT_FVF_CODES;
            descVB.dwNumVertices = m_rgbVerticesSize;
            status = m_d3d->CreateVertexBuffer(&descVB, &m_vb, 0);
            if (FAILED(status))
                throw Error("Failed to create D3D7 vertex buffer");

            status = m_vb->Lock(0, &vertices, NULL);
            if (FAILED(status))
                throw Error("Failed to lock D3D7 vertex buffer");
            memcpy(vertices, m_rgbVertices.data(), m_rgbVerticesSize);
            status = m_vb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D7 vertex buffer");
        }

        void render() {
            HRESULT status = m_device->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
            if (FAILED(status))
                throw Error("Failed to clear D3D7 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->DrawPrimitiveVB(D3DPT_TRIANGLELIST, m_vb.ptr(), 0, m_rgbVertices.size(), 0);
                if (FAILED(status))
                    throw Error("Failed to draw D3D7 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    status = m_primarySurf->Blt(&s_rect, m_rt.ptr(), NULL, DDBLT_WAIT, NULL);
                } else {
                    throw Error("Failed to end D3D7 scene");
                }
            } else {
                throw Error("Failed to begin D3D7 scene");
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
                throw Error("The D3D7 interface hasn't been initialized");

            if (m_rt == nullptr)
                throw Error("The D3D7 render target hasn't been initialized");

            m_device = nullptr;

            HRESULT status = m_d3d->CreateDevice(deviceIID, m_rt.ptr(), &m_device);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D7 device");

            return status;
        }

        static RECT                   s_rect;

        HWND                          m_hWnd;

        DWORD                         m_vendorID;

        Com<IDirectDraw7>             m_ddraw;
        Com<IDirect3D7>               m_d3d;
        Com<IDirect3DDevice7>         m_device;
        Com<IDirect3DVertexBuffer7>   m_vb;

        Com<IDirectDrawSurface7>      m_primarySurf;
        Com<IDirectDrawSurface7>      m_rt;

        // tailored for 1024x768 and the appearance of being centered
        std::array<RGBVERTEX, 3>      m_rgbVertices = {{ { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 0),},
                                                         {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 0),},
                                                         {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 0, 255),} }};
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

        // run D3D Device tests
        rgbTriangle.startTests();
        rgbTriangle.testZeroViewport();
        rgbTriangle.testMipMapLevels();
        rgbTriangle.testObsoleteRenderStates();
        rgbTriangle.printTestResults();

        // D3D7 triangle
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
