#include <array>
#include <iostream>

#include <d3d.h>

#include "../common/bit.h"
#include "../common/com.h"
#include "../common/error.h"
#include "../common/str.h"

// This is the D3DDEVICEDESC shipped with D3D5
typedef struct _D3DDeviceDesc2 {
        DWORD            dwSize;
        DWORD            dwFlags;
        D3DCOLORMODEL    dcmColorModel;
        DWORD            dwDevCaps;
        D3DTRANSFORMCAPS dtcTransformCaps;
        BOOL             bClipping;
        D3DLIGHTINGCAPS  dlcLightingCaps;
        D3DPRIMCAPS      dpcLineCaps;
        D3DPRIMCAPS      dpcTriCaps;
        DWORD            dwDeviceRenderBitDepth;
        DWORD            dwDeviceZBufferBitDepth;
        DWORD            dwMaxBufferSize;
        DWORD            dwMaxVertexCount;

        DWORD            dwMinTextureWidth,dwMinTextureHeight;
        DWORD            dwMaxTextureWidth,dwMaxTextureHeight;
        DWORD            dwMinStippleWidth,dwMaxStippleWidth;
        DWORD            dwMinStippleHeight,dwMaxStippleHeight;
} D3DDEVICEDESC2;

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirectDraw4, 0x9C59509A, 0x39BD, 0x11D1, 0x8C, 0x4A, 0x00, 0xC0, 0x4F, 0xD9, 0x30, 0xC5);
__CRT_UUID_DECL(IDirectDraw2, 0xB3A6F3E0, 0x2B43, 0x11CF, 0xA2, 0xDE, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
__CRT_UUID_DECL(IDirect3D2,   0x6AAE1EC1, 0x662A, 0x11D0, 0x88, 0x9D, 0x00, 0xAA, 0x00, 0xBB, 0xB7, 0x6A);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("B3A6F3E0-2B43-11CF-A2DE-00AA00B93356") IDirectDraw2;
interface DECLSPEC_UUID("6AAE1EC1-662A-11D0-889D-00AA00BBB76A") IDirect3D2;
#endif

#define PTRCLEARED(ptr) (ptr == nullptr)
#define D3DCOLOR_ARGB(a,r,g,b) \
    ((D3DCOLOR)((((a)&0xff)<<24)|(((r)&0xff)<<16)|(((g)&0xff)<<8)|((b)&0xff)))
#define D3DCOLOR_XRGB(r,g,b)   D3DCOLOR_ARGB(0xff,r,g,b)

HRESULT WINAPI EnumModesCallback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext) {
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

        static constexpr const char* TRIANGLE_ID    = "D3D5_Triangle";
        static constexpr const char* TRIANGLE_TITLE = "D3D5 Triangle - Blisto Stone Age Testing Edition";

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

            // DDraw2 Interface
            status = ddrawBase->QueryInterface(__uuidof(IDirectDraw2), reinterpret_cast<void**>(&m_ddraw));
            if (FAILED(status))
                throw Error("Failed to create DDraw2 interface");

            Com<IDirectDraw4> ddrawDevIdtf;
            // DDraw4 Interface
            status = ddrawBase->QueryInterface(__uuidof(IDirectDraw4), reinterpret_cast<void**>(&ddrawDevIdtf));
            if (FAILED(status))
                throw Error("Failed to create DDraw4 interface");

            status = m_ddraw->SetCooperativeLevel(m_hWnd, DDSCL_NORMAL);
            if (FAILED(status))
                throw Error("Failed to set cooperative level");

            // Use DDraw4 to get the device identifier
            DDDEVICEIDENTIFIER deviceIdtf = { };
            status = ddrawDevIdtf->GetDeviceIdentifier(&deviceIdtf, 0);
            if (FAILED(status))
                throw Error("Failed to get D3D5 device identifier");

            std::cout << format("Using adapter: ", deviceIdtf.szDescription) << std::endl;

            // DWORD to hex printout
            char vendorID[16];
            char deviceID[16];
            sprintf(vendorID, "VendorId: %04lx", deviceIdtf.dwVendorId);
            sprintf(deviceID, "DeviceId: %04lx", deviceIdtf.dwDeviceId);
            std::cout << "  ~ " << vendorID << std::endl;
            std::cout << "  ~ " << deviceID << std::endl;

            // Note: driver versions aren't reported by any Windows XP native drivers on DDraw
            std::cout << format("  ~ Driver: ", deviceIdtf.szDriver) << std::endl;

            // NVIDIA stores the driver version in the lower half of the lower DWORD
            if (deviceIdtf.dwVendorId == uint32_t(0x10de)) {
                // Newer drivers will also spill over into the upper half
                DWORD driverVersionHigh = HIWORD(deviceIdtf.liDriverVersion.LowPart);
                DWORD driverVersionLow  = LOWORD(deviceIdtf.liDriverVersion.LowPart);
                DWORD majorVersion = driverVersionLow / 100;
                // Might want to revisit this cursed logic, but it should work fine for now
                if (driverVersionHigh >= 15 && driverVersionLow < 10000)
                    majorVersion += (driverVersionHigh % 10) * 100;
                DWORD minorVersion = driverVersionLow % 100;
                std::cout << format("  ~ Version: ", majorVersion, ".", minorVersion) << std::endl;
            }
            // for other vendors simply list the entire DriverVersion long int
            else {
                DWORD product = HIWORD(deviceIdtf.liDriverVersion.HighPart);
                DWORD version = LOWORD(deviceIdtf.liDriverVersion.HighPart);
                DWORD subVersion = HIWORD(deviceIdtf.liDriverVersion.LowPart);
                DWORD build = LOWORD(deviceIdtf.liDriverVersion.LowPart);
                std::cout << format("  ~ Version: ", product, ".", version, ".",
                    subVersion, ".", build) << std::endl;
            }

            DDSURFACEDESC desc = { };
            desc.dwSize = sizeof(DDSURFACEDESC);
            desc.dwFlags = DDSD_CAPS;
            desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

            status = m_ddraw->CreateSurface(&desc, &m_primarySurf, NULL);
            if (FAILED(status))
                throw Error("Failed to create the primary surface");

            desc = { };
            desc.dwSize = sizeof(DDSURFACEDESC);
            desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
            desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
            desc.dwWidth = WINDOW_WIDTH;
            desc.dwHeight = WINDOW_HEIGHT;
            status = m_ddraw->CreateSurface(&desc, &m_rt, NULL);
            if (FAILED(status))
                throw Error("Failed to create the render target");

            Com<IDirectDrawClipper> clipper;
            status = m_ddraw->CreateClipper(0, &clipper, NULL);
            if (FAILED(status))
                throw Error("Failed to create the clipper");

            status = clipper->SetHWnd(0, m_hWnd);
            if (FAILED(status))
                throw Error("Failed to set the HWND");

            status = m_primarySurf->SetClipper(clipper.ptr());
            if (FAILED(status))
                throw Error("Failed to set the clipper");

            // D3D5 Interface
            status = m_ddraw->QueryInterface(__uuidof(IDirect3D2), reinterpret_cast<void**>(&m_d3d));
            if (FAILED(status))
                throw Error("Failed to create D3D5 interface");

            // D3D5 Viewport
            status = m_d3d->CreateViewport(&m_viewport, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D5 viewport");

            createDeviceWithFlags(IID_IDirect3DHALDevice, true);
        }

        // D3D Adapter Display Mode enumeration
        void listAdapterDisplayModes() {
            UINT adapterModeCount = 0;

            std::cout << std::endl << "Enumerating supported adapter display modes:" << std::endl;

            HRESULT status = m_ddraw->EnumDisplayModes(DDEDM_REFRESHRATES, NULL, &adapterModeCount, &EnumModesCallback);
            if (FAILED(status)) {
                std::cout << "Failed to enumerate adapter display modes" << std::endl;
            } else {
                std::cout << format("Listed a total of ", adapterModeCount, " adapter display modes") << std::endl;
            }
        }

        // D3D Device capabilities check
        void listDeviceCapabilities() {
            D3DDEVICEDESC2 caps5HAL = { };
            caps5HAL.dwSize = sizeof(D3DDEVICEDESC2);
            D3DDEVICEDESC2 caps5HEL = { };
            caps5HEL.dwSize = sizeof(D3DDEVICEDESC2);

            // get the capabilities from the D3D device in HAL mode
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);
            m_device->GetCaps(reinterpret_cast<D3DDEVICEDESC*>(&caps5HAL),
                              reinterpret_cast<D3DDEVICEDESC*>(&caps5HEL));

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL)
                std::cout << "  + D3DDEVCAPS_CANBLTSYSTONONLOCAL is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANBLTSYSTONONLOCAL is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP)
                std::cout << "  + D3DDEVCAPS_CANRENDERAFTERFLIP is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANRENDERAFTERFLIP is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_DRAWPRIMTLVERTEX)
                std::cout << "  + D3DDEVCAPS_DRAWPRIMTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_DRAWPRIMTLVERTEX is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_EXECUTESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTESYSTEMMEMORY is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_EXECUTEVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTEVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTEVIDEOMEMORY is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_FLOATTLVERTEX)
                std::cout << "  + D3DDEVCAPS_FLOATTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_FLOATTLVERTEX is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION)
                std::cout << "  + D3DDEVCAPS_HWRASTERIZATION is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWRASTERIZATION is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                std::cout << "  + D3DDEVCAPS_HWTRANSFORMANDLIGHT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWTRANSFORMANDLIGHT is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_DRAWPRIMITIVES2)
                std::cout << "  + D3DDEVCAPS_DRAWPRIMITIVES2 is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_DRAWPRIMITIVES2 is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_DRAWPRIMITIVES2EX)
                std::cout << "  + D3DDEVCAPS_DRAWPRIMITIVES2EX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_DRAWPRIMITIVES2EX is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES)
                std::cout << "  + D3DDEVCAPS_SEPARATETEXTUREMEMORIES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SEPARATETEXTUREMEMORIES is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_SORTDECREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTDECREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTDECREASINGZ is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_SORTEXACT)
                std::cout << "  + D3DDEVCAPS_SORTEXACT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTEXACT is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_SORTINCREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTINCREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTINCREASINGZ is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM)
                std::cout << "  + D3DDEVCAPS_TEXTURENONLOCALVIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURENONLOCALVIDMEM is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTURESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURESYSTEMMEMORY is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTUREVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTUREVIDEOMEMORY is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is not supported" << std::endl;

            if (caps5HAL.dwDevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXVIDEOMEMORY is not supported" << std::endl;

            if (caps5HAL.dpcTriCaps.dwRasterCaps & D3DPRASTERCAPS_WBUFFER)
                std::cout << "  + D3DPRASTERCAPS_WBUFFER is supported" << std::endl;
            else
                std::cout << "  - D3DPRASTERCAPS_WBUFFER is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ dwMaxBufferSize: ", caps5HAL.dwMaxBufferSize) << std::endl;
            std::cout << format("  ~ dwMaxVertexCount: ", caps5HAL.dwMaxVertexCount) << std::endl;
            std::cout << format("  ~ dwMinTextureWidth: ", caps5HAL.dwMinTextureWidth) << std::endl;
            std::cout << format("  ~ dwMinTextureHeight: ", caps5HAL.dwMinTextureHeight) << std::endl;
            std::cout << format("  ~ dwMaxTextureWidth: ", caps5HAL.dwMaxTextureWidth) << std::endl;
            std::cout << format("  ~ dwMaxTextureHeight: ", caps5HAL.dwMaxTextureHeight) << std::endl;
            std::cout << format("  ~ dwMinStippleWidth: ", caps5HAL.dwMinStippleWidth) << std::endl;
            std::cout << format("  ~ dwMinStippleHeight: ", caps5HAL.dwMinStippleHeight) << std::endl;
            std::cout << format("  ~ dwMaxStippleWidth: ", caps5HAL.dwMaxStippleWidth) << std::endl;
            std::cout << format("  ~ dwMaxStippleHeight: ", caps5HAL.dwMaxStippleHeight) << std::endl;
        }

        void listAvailableTextureMemory() {
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);

            std::cout << std::endl << "Listing available texture memory:" << std::endl;

            DDSCAPS ddsCaps = { };
            ddsCaps.dwCaps = DDSCAPS_TEXTURE;
            DWORD    dwTotal  = 0;
            DWORD    dwFree   = 0;
            HRESULT status = m_ddraw->GetAvailableVidMem(&ddsCaps, &dwTotal, &dwFree);
            if (FAILED(status)) {
                std::cout << "Failed to list available texture memory" << std::endl;
            } else {
                std::cout << format("  ~ Bytes: ", dwFree) << std::endl;
            }
        }

        void startTests() {
            m_totalTests  = 0;
            m_passedTests = 0;

            std::cout << std::endl << "Running D3D5 tests:" << std::endl;
        }

        void printTestResults() {
            std::cout << std::endl << format("Passed ", m_passedTests, "/", m_totalTests, " tests") << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);

            // don't need any of these for 2D rendering
            HRESULT status = m_device->SetRenderState(D3DRENDERSTATE_ZENABLE, D3DZB_FALSE);
            if (FAILED(status))
                throw Error("Failed to set D3D5 render state for D3DRENDERSTATE_ZENABLE");
            status = m_device->SetRenderState(D3DRENDERSTATE_CULLMODE, D3DCULL_NONE);
            if (FAILED(status))
                throw Error("Failed to set D3D5 render state for D3DRENDERSTATE_CULLMODE");
        }

        void render() {
            DDSURFACEDESC primaryDesc = { };
            primaryDesc.dwSize = sizeof(DDSURFACEDESC);
            HRESULT status = m_primarySurf->GetSurfaceDesc(&primaryDesc);
            if (FAILED(status))
                throw Error("Failed to get primary surface description");
            D3DRECT clearRect = { 0, 0, static_cast<LONG>(primaryDesc.dwWidth),
                                        static_cast<LONG>(primaryDesc.dwHeight) };
            // Clears with a NULL D3DRECT are skipped in early D3D
            status = m_viewport->Clear(1, &clearRect, D3DCLEAR_TARGET);
            if (FAILED(status))
                throw Error("Failed to clear D3D5 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->DrawPrimitive(D3DPT_TRIANGLELIST, D3DVT_TLVERTEX, m_rgbVertices.data(),
                                                 m_rgbVertices.size(), D3DDP_DONOTCLIP);
                if (FAILED(status))
                    throw Error("Failed to draw D3D5 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    m_primarySurf->Blt(&s_rect, m_rt.ptr(), NULL, DDBLT_WAIT, NULL);
                } else {
                    throw Error("Failed to end D3D5 scene");
                }
            } else {
                throw Error("Failed to begin D3D5 scene");
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
                throw Error("The D3D5 interface hasn't been initialized");

            if (m_rt == nullptr)
                throw Error("The D3D5 render target hasn't been initialized");

            HRESULT status;

            if (m_device != nullptr) {
                status = m_device->DeleteViewport(m_viewport.ptr());
                if (throwErrorOnFail && FAILED(status))
                    throw Error("Failed to delete D3D5 viewport");
            }

            m_device = nullptr;

            status = m_d3d->CreateDevice(deviceIID, m_rt.ptr(), &m_device);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D5 device");

            if (m_device != nullptr) {
                status = m_device->AddViewport(m_viewport.ptr());
                if (throwErrorOnFail && FAILED(status))
                    throw Error("Failed to add D3D5 viewport");

                D3DVIEWPORT2 viewPortData2 = { };
                viewPortData2.dwSize       = sizeof(D3DVIEWPORT2);
                viewPortData2.dwWidth      = WINDOW_WIDTH;
                viewPortData2.dwHeight     = WINDOW_HEIGHT;
                viewPortData2.dvClipX      = -1.0f;
                viewPortData2.dvClipWidth  = 2.0f;
                viewPortData2.dvClipY      = 1.0f;
                viewPortData2.dvClipHeight = 2.0f;
                viewPortData2.dvMaxZ       = 1.0f;
                status = m_viewport->SetViewport2(&viewPortData2);
                if (throwErrorOnFail && FAILED(status))
                    throw Error("Failed to set D3D5 viewport");

                status = m_device->SetCurrentViewport(m_viewport.ptr());
                if (throwErrorOnFail && FAILED(status))
                    throw Error("Failed to set current D3D5 viewport");
            }

            return status;
        }

        static RECT                   s_rect;

        HWND                          m_hWnd;

        Com<IDirectDraw2>             m_ddraw;
        Com<IDirect3D2>               m_d3d;
        Com<IDirect3DDevice2>         m_device;
        Com<IDirect3DViewport2>       m_viewport;
        Com<IDirect3DVertexBuffer>    m_vb;

        Com<IDirectDrawSurface>       m_primarySurf;
        Com<IDirectDrawSurface>       m_rt;

        // tailored for 1024x768 and the appearance of being centered
        std::array<D3DTLVERTEX, 3>    m_rgbVertices = {{ { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 255), D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0.0f},
                                                         {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(128, 128, 128), D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0.0f},
                                                         {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(32, 32, 32), D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0.0f} }};

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
                             WS_OVERLAPPEDWINDOW, 25, 25,
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
        rgbTriangle.printTestResults();

        // D3D5 triangle
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
