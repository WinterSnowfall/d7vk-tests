#include <array>
#include <iostream>

#include <d3d.h>

#include "../common/bit.h"
#include "../common/com.h"
#include "../common/error.h"
#include "../common/str.h"

// This is the D3DDEVICEDESC shipped with D3D3
typedef struct _D3DDeviceDesc3 {
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
} D3DDEVICEDESC3;

#ifdef __CRT_UUID_DECL
__CRT_UUID_DECL(IDirect3D, 0x3BBA0080, 0x2421, 0x11CF, 0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56);
#elif defined(_MSC_VER)
interface DECLSPEC_UUID("3BBA0080-2421-11CF-A31A-00AA00B93356") IDirect3D;
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

        static constexpr IID IID_IDirect3DHALDevice = { 0x84E63DE0, 0x46AA, 0x11CF, {0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E} };
        static constexpr IID IID_IDirect3DRGBDevice = { 0xA4665C60, 0x2673, 0x11CF, {0xA3, 0x1A, 0x00, 0xAA, 0x00, 0xB9, 0x33, 0x56} };

        static constexpr const char* TRIANGLE_ID    = "D3D3_Triangle";
        static constexpr const char* TRIANGLE_TITLE = "D3D3 Triangle - Blisto Cretaceous Testing Edition";

        static constexpr UINT WINDOW_WIDTH  = 700;
        static constexpr UINT WINDOW_HEIGHT = 700;

        RGBTriangle(HWND hWnd) : m_hWnd(hWnd) {
            std::cout << RGBTriangle::TRIANGLE_TITLE << std::endl << std::endl;

            decltype(DirectDrawCreate)* DirectDrawCreate = nullptr;
            HMODULE hm = LoadLibraryA("ddraw.dll");
            DirectDrawCreate = (decltype(DirectDrawCreate))GetProcAddress(hm, "DirectDrawCreate");

            // DDraw Interface
            HRESULT status = DirectDrawCreate(NULL, &m_ddraw, NULL);
            if (FAILED(status))
                throw Error("Failed to create DDraw interface");

            status = m_ddraw->SetCooperativeLevel(m_hWnd, DDSCL_NORMAL);
            if (FAILED(status))
                throw Error("Failed to set cooperative level");

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

            clipper->SetHWnd(0, m_hWnd);
            m_primarySurf->SetClipper(clipper.ptr());

            // D3D3 Interface
            status = m_ddraw->QueryInterface(__uuidof(IDirect3D), reinterpret_cast<void**>(&m_d3d));
            if (FAILED(status))
                throw Error("Failed to create D3D3 interface");

            // D3D3 Viewport
            D3DVIEWPORT viewPortData = { };
            viewPortData.dwSize   = sizeof(D3DVIEWPORT);
            viewPortData.dwWidth  = WINDOW_WIDTH;
            viewPortData.dwHeight = WINDOW_HEIGHT;
            viewPortData.dvScaleX = 1.0f;
            viewPortData.dvScaleY = 1.0f;
            viewPortData.dvMaxX   = 1.0f;
            viewPortData.dvMaxY   = 1.0f;
            viewPortData.dvMaxZ   = 1.0f;
            status = m_d3d->CreateViewport(&m_viewport, NULL);
            if (FAILED(status))
                throw Error("Failed to create D3D3 viewport");
            m_viewport->SetViewport(&viewPortData);

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
            D3DDEVICEDESC3 caps3HAL = { };
            caps3HAL.dwSize = sizeof(D3DDEVICEDESC3);
            D3DDEVICEDESC3 caps3HEL = { };
            caps3HEL.dwSize = sizeof(D3DDEVICEDESC3);

            // get the capabilities from the D3D device in HAL mode
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);
            m_device->GetCaps(reinterpret_cast<D3DDEVICEDESC*>(&caps3HAL),
                              reinterpret_cast<D3DDEVICEDESC*>(&caps3HEL));

            std::cout << std::endl << "Listing device capabilities support:" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_CANBLTSYSTONONLOCAL)
                std::cout << "  + D3DDEVCAPS_CANBLTSYSTONONLOCAL is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANBLTSYSTONONLOCAL is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_CANRENDERAFTERFLIP)
                std::cout << "  + D3DDEVCAPS_CANRENDERAFTERFLIP is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_CANRENDERAFTERFLIP is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_DRAWPRIMTLVERTEX)
                std::cout << "  + D3DDEVCAPS_DRAWPRIMTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_DRAWPRIMTLVERTEX is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_EXECUTESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTESYSTEMMEMORY is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_EXECUTEVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_EXECUTEVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_EXECUTEVIDEOMEMORY is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_FLOATTLVERTEX)
                std::cout << "  + D3DDEVCAPS_FLOATTLVERTEX is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_FLOATTLVERTEX is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_HWRASTERIZATION)
                std::cout << "  + D3DDEVCAPS_HWRASTERIZATION is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWRASTERIZATION is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                std::cout << "  + D3DDEVCAPS_HWTRANSFORMANDLIGHT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_HWTRANSFORMANDLIGHT is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_SEPARATETEXTUREMEMORIES)
                std::cout << "  + D3DDEVCAPS_SEPARATETEXTUREMEMORIES is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SEPARATETEXTUREMEMORIES is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_SORTDECREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTDECREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTDECREASINGZ is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_SORTEXACT)
                std::cout << "  + D3DDEVCAPS_SORTEXACT is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTEXACT is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_SORTINCREASINGZ)
                std::cout << "  + D3DDEVCAPS_SORTINCREASINGZ is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_SORTINCREASINGZ is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM)
                std::cout << "  + D3DDEVCAPS_TEXTURENONLOCALVIDMEM is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURENONLOCALVIDMEM is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_TEXTURESYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTURESYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTURESYSTEMMEMORY is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_TEXTUREVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TEXTUREVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TEXTUREVIDEOMEMORY is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_TLVERTEXSYSTEMMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXSYSTEMMEMORY is not supported" << std::endl;

            if (caps3HAL.dwDevCaps & D3DDEVCAPS_TLVERTEXVIDEOMEMORY)
                std::cout << "  + D3DDEVCAPS_TLVERTEXVIDEOMEMORY is supported" << std::endl;
            else
                std::cout << "  - D3DDEVCAPS_TLVERTEXVIDEOMEMORY is not supported" << std::endl;

            std::cout << std::endl << "Listing device capability limits:" << std::endl;

            std::cout << format("  ~ dwMaxBufferSize: ", caps3HAL.dwMaxBufferSize) << std::endl;
            std::cout << format("  ~ dwMaxVertexCount: ", caps3HAL.dwMaxVertexCount) << std::endl;
        }

        void prepare() {
            createDeviceWithFlags(IID_IDirect3DHALDevice, true);

            D3DEXECUTEBUFFERDESC ebDesc = { };
            ebDesc.dwSize = sizeof(D3DEXECUTEBUFFERDESC);
            ebDesc.dwFlags = D3DDEB_BUFSIZE;
            ebDesc.dwBufferSize = 1024;
            HRESULT status = m_device->CreateExecuteBuffer(&ebDesc, &m_eb, nullptr);
            if (FAILED(status))
                std::cout << "Failed to create D3D3 execute buffer" << std::endl;

            ebDesc = { };
            ebDesc.dwSize = sizeof(D3DEXECUTEBUFFERDESC);
            status = m_eb->Lock(&ebDesc);
            if (FAILED(status))
                throw Error("Failed to lock D3D3 execute buffer");

            uint8_t *ptr = reinterpret_cast<uint8_t*>(ebDesc.lpData);

            D3DINSTRUCTION* instruction = reinterpret_cast<D3DINSTRUCTION*>(ptr);
            instruction->bOpcode = D3DOP_STATERENDER;
            instruction->bSize = sizeof(D3DSTATE);
            instruction->wCount = 1;
            ptr += sizeof(D3DINSTRUCTION);
            D3DSTATE* state = reinterpret_cast<D3DSTATE*>(ptr);
            state->drstRenderStateType = D3DRENDERSTATE_CULLMODE;
            state->dwArg[0] = D3DCULL_NONE;
            ptr += sizeof(D3DSTATE);

            instruction = reinterpret_cast<D3DINSTRUCTION*>(ptr);
            instruction->bOpcode = D3DOP_PROCESSVERTICES;
            instruction->bSize = sizeof(D3DPROCESSVERTICES);
            instruction->wCount = 1;
            ptr += sizeof(D3DINSTRUCTION);

            D3DPROCESSVERTICES* pv = reinterpret_cast<D3DPROCESSVERTICES*>(ptr);
            pv->dwFlags = D3DPROCESSVERTICES_COPY;
            pv->wStart = 0;
            pv->wDest = 0;
            pv->dwCount = m_rgbVertices.size();
            ptr += sizeof(D3DPROCESSVERTICES);

            instruction = reinterpret_cast<D3DINSTRUCTION*>(ptr);
            instruction->bOpcode = D3DOP_TRIANGLE;
            instruction->bSize = sizeof(D3DTRIANGLE);
            instruction->wCount = m_rgbVertices.size() / 3;
            ptr += sizeof(D3DINSTRUCTION);

            for (DWORD i = 0; i < instruction->wCount; i+=3) {
                D3DTRIANGLE* t = reinterpret_cast<D3DTRIANGLE*>(ptr);
                t->v1 = i;
                t->v2 = i + 1;
                t->v3 = i + 2;
                t->wFlags = 0;
                ptr += sizeof(D3DTRIANGLE);
            }

            instruction = reinterpret_cast<D3DINSTRUCTION*>(ptr);
            instruction->bOpcode = D3DOP_EXIT;
            instruction->bSize = 0;
            instruction->wCount = 0;
            ptr += sizeof(D3DINSTRUCTION);

            D3DEXECUTEDATA executeData = { };
            executeData.dwSize = sizeof(D3DEXECUTEDATA);
            executeData.dwInstructionOffset = 0;
            executeData.dwInstructionLength = static_cast<DWORD>(ptr - reinterpret_cast<uint8_t*>(ebDesc.lpData));
            executeData.dwVertexOffset = static_cast<DWORD>(ptr - reinterpret_cast<uint8_t*>(ebDesc.lpData));
            executeData.dwVertexCount = m_rgbVertices.size();
            executeData.dwHVertexOffset = executeData.dwVertexOffset;

            memcpy(ptr, m_rgbVertices.data(), m_rgbVerticesSize);
            status = m_eb->Unlock();
            if (FAILED(status))
                throw Error("Failed to unlock D3D3 execute buffer");

            status = m_eb->SetExecuteData(&executeData);
            if (FAILED(status))
                throw Error("Failed to set D3D3 execute buffer data");
        }

        void render() {
            HRESULT status = m_viewport->Clear(0, NULL, D3DCLEAR_TARGET);
            if (FAILED(status))
                throw Error("Failed to clear D3D3 viewport");
            if (SUCCEEDED(m_device->BeginScene())) {
                status = m_device->Execute(m_eb.ptr(), m_viewport.ptr(), D3DEXECUTE_UNCLIPPED);
                if (FAILED(status))
                    throw Error("Failed to draw D3D3 triangle list");
                if (SUCCEEDED(m_device->EndScene())) {
                    status = m_primarySurf->Blt(&s_rect, m_rt.ptr(), NULL, DDBLT_WAIT, NULL);
                } else {
                    throw Error("Failed to end D3D3 scene");
                }
            } else {
                throw Error("Failed to begin D3D3 scene");
            }
        }

        static void AdjustRect(DWORD x, DWORD y) {
            s_rect.left   = x;
            s_rect.top    = y;
            s_rect.right  = x + WINDOW_WIDTH;
            s_rect.bottom = y + WINDOW_HEIGHT;
        }

    private:

        inline void createDeviceWithFlags(IID deviceIID,
                                      bool throwErrorOnFail) {
            if (m_d3d == nullptr)
                throw Error("The D3D3 interface hasn't been initialized");

            if (m_rt == nullptr)
                throw Error("The D3D3 render target hasn't been initialized");

            // Avoid casting to void** here, as it blows up during device tear-down
            void* device = nullptr;
            HRESULT status = m_rt->QueryInterface(deviceIID, &device);
            if (throwErrorOnFail && FAILED(status))
                throw Error("Failed to create D3D3 device");

            m_device = static_cast<IDirect3DDevice*>(device);

            if (m_device != nullptr) {
                status = m_device->AddViewport(m_viewport.ptr());
                if (FAILED(status))
                    std::cout << "Failed to add D3D3 viewport" << std::endl;
            }
        }

        static RECT                   s_rect;

        HWND                          m_hWnd;

        DWORD                         m_vendorID;

        Com<IDirectDraw>              m_ddraw;
        Com<IDirect3D>                m_d3d;
        Com<IDirect3DDevice>          m_device;
        Com<IDirect3DViewport>        m_viewport;
        Com<IDirect3DExecuteBuffer>   m_eb;

        Com<IDirectDrawSurface>       m_primarySurf;
        Com<IDirectDrawSurface>       m_rt;

        // tailored for 1024x768 and the appearance of being centered
        std::array<D3DTLVERTEX, 3>    m_rgbVertices = {{ { 60.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(0, 255, 255), D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0.0f},
                                                         {350.0f,  45.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 0, 255), D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0.0f},
                                                         {640.0f, 625.0f, 0.5f, 1.0f, D3DCOLOR_XRGB(255, 255, 0), D3DCOLOR_XRGB(0, 0, 0), 0.0f, 0.0f} }};
        const DWORD                   m_rgbVerticesSize = m_rgbVertices.size() * sizeof(D3DTLVERTEX);

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

        // D3D3 triangle
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
