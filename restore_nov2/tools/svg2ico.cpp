#include <windows.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <wincodec.h>
#include <shlwapi.h>
#include <vector>
#include <iostream>
#include <fstream>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "ole32.lib")

#pragma pack(push, 2)
struct ICONDIR {
    WORD idReserved;
    WORD idType;
    WORD idCount;
};

struct ICONDIRENTRY {
    BYTE  bWidth;
    BYTE  bHeight;
    BYTE  bColorCount;
    BYTE  bReserved;
    WORD  wPlanes;
    WORD  wBitCount;
    DWORD dwBytesInRes;
    DWORD dwImageOffset;
};
#pragma pack(pop)

bool RenderSvgToPngStream(ID2D1Factory1* pD2DFactory, IWICImagingFactory* pWICFactory, 
                          const wchar_t* svgPath, UINT size, std::vector<BYTE>& outPngData) 
{
    // 1. Create WIC Bitmap
    IWICBitmap* pWicBitmap = nullptr;
    HRESULT hr = pWICFactory->CreateBitmap(size, size, GUID_WICPixelFormat32bppPBGRA, 
                                          WICBitmapCacheOnDemand, &pWicBitmap);
    if (FAILED(hr)) return false;

    // 2. Create D2D Render Target for WIC Bitmap
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0.0f, 0.0f, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
    );

    ID2D1RenderTarget* pRT = nullptr;
    hr = pD2DFactory->CreateWicBitmapRenderTarget(pWicBitmap, rtProps, &pRT);
    if (FAILED(hr)) {
        pWicBitmap->Release();
        return false;
    }

    ID2D1DeviceContext5* pDC5 = nullptr;
    hr = pRT->QueryInterface(__uuidof(ID2D1DeviceContext5), (void**)&pDC5);
    if (FAILED(hr)) {
        pRT->Release();
        pWicBitmap->Release();
        return false;
    }

    // 3. Open SVG file stream
    IStream* pStream = nullptr;
    hr = SHCreateStreamOnFileW(svgPath, STGM_READ, &pStream);
    if (FAILED(hr)) {
        pDC5->Release();
        pRT->Release();
        pWicBitmap->Release();
        return false;
    }

    // 4. Create SVG Document
    ID2D1SvgDocument* pSvgDoc = nullptr;
    hr = pDC5->CreateSvgDocument(pStream, D2D1::SizeF((float)size, (float)size), &pSvgDoc);
    pStream->Release();

    if (FAILED(hr)) {
        std::wcout << L"CreateSvgDocument failed: 0x" << std::hex << hr << std::endl;
        pDC5->Release();
        pRT->Release();
        pWicBitmap->Release();
        return false;
    }

    // 5. Draw SVG
    pDC5->BeginDraw();
    pDC5->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
    pDC5->DrawSvgDocument(pSvgDoc);
    hr = pDC5->EndDraw();

    pSvgDoc->Release();
    pDC5->Release();
    pRT->Release();

    if (FAILED(hr)) {
        pWicBitmap->Release();
        return false;
    }

    // 6. Encode to PNG in memory
    IStream* pMemStream = SHCreateMemStream(nullptr, 0);
    if (!pMemStream) {
        pWicBitmap->Release();
        return false;
    }

    IWICBitmapEncoder* pEncoder = nullptr;
    hr = pWICFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &pEncoder);
    if (SUCCEEDED(hr)) {
        hr = pEncoder->Initialize(pMemStream, WICBitmapEncoderNoCache);
    }
    IWICBitmapFrameEncode* pFrame = nullptr;
    if (SUCCEEDED(hr)) {
        hr = pEncoder->CreateNewFrame(&pFrame, nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->Initialize(nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->SetSize(size, size);
    }
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppPBGRA;
    if (SUCCEEDED(hr)) {
        hr = pFrame->SetPixelFormat(&format);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->WriteSource(pWicBitmap, nullptr);
    }
    if (SUCCEEDED(hr)) {
        hr = pFrame->Commit();
    }
    if (SUCCEEDED(hr)) {
        hr = pEncoder->Commit();
    }

    if (pFrame) pFrame->Release();
    if (pEncoder) pEncoder->Release();
    pWicBitmap->Release();

    if (FAILED(hr)) {
        pMemStream->Release();
        return false;
    }

    // 7. Read bytes from pMemStream
    STATSTG stat;
    pMemStream->Stat(&stat, STATFLAG_NONAME);
    ULONG streamSize = (ULONG)stat.cbSize.QuadPart;
    outPngData.resize(streamSize);

    LARGE_INTEGER li = {0};
    pMemStream->Seek(li, STREAM_SEEK_SET, nullptr);
    ULONG bytesRead = 0;
    pMemStream->Read(outPngData.data(), streamSize, &bytesRead);
    pMemStream->Release();

    return bytesRead == streamSize;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 3) {
        std::wcout << L"Usage: svg2ico <input.svg> <output.ico>" << std::endl;
        return 1;
    }

    CoInitialize(nullptr);

    ID2D1Factory1* pD2DFactory = nullptr;
    D2D1_FACTORY_OPTIONS options = {};
    HRESULT hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1), &options, (void**)&pD2DFactory);
    if (FAILED(hr)) {
        std::cout << "Failed to create D2D Factory" << std::endl;
        return 1;
    }

    IWICImagingFactory* pWICFactory = nullptr;
    hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&pWICFactory));
    if (FAILED(hr)) {
        std::cout << "Failed to create WIC Factory" << std::endl;
        pD2DFactory->Release();
        return 1;
    }

    std::vector<UINT> sizes = { 256, 128, 64, 48, 32, 16 };
    std::vector<std::vector<BYTE>> pngs;
    std::vector<UINT> validSizes;

    for (UINT s : sizes) {
        std::vector<BYTE> pngData;
        if (RenderSvgToPngStream(pD2DFactory, pWICFactory, argv[1], s, pngData)) {
            pngs.push_back(std::move(pngData));
            validSizes.push_back(s);
            std::wcout << L"Rendered size: " << s << L"x" << s << L" (" << pngs.back().size() << L" bytes)" << std::endl;
        } else {
            std::wcout << L"Failed to render size: " << s << std::endl;
        }
    }

    if (pngs.empty()) {
        std::wcout << L"No sizes rendered successfully!" << std::endl;
        pWICFactory->Release();
        pD2DFactory->Release();
        CoUninitialize();
        return 1;
    }

    // Build ICO file
    ICONDIR iconDir;
    iconDir.idReserved = 0;
    iconDir.idType = 1; // 1 = Icon
    iconDir.idCount = (WORD)pngs.size();

    std::vector<ICONDIRENTRY> entries(pngs.size());
    DWORD currentOffset = sizeof(ICONDIR) + (DWORD)(pngs.size() * sizeof(ICONDIRENTRY));

    for (size_t i = 0; i < pngs.size(); ++i) {
        UINT s = validSizes[i];
        entries[i].bWidth = (s >= 256) ? 0 : (BYTE)s;
        entries[i].bHeight = (s >= 256) ? 0 : (BYTE)s;
        entries[i].bColorCount = 0;
        entries[i].bReserved = 0;
        entries[i].wPlanes = 1;
        entries[i].wBitCount = 32;
        entries[i].dwBytesInRes = (DWORD)pngs[i].size();
        entries[i].dwImageOffset = currentOffset;

        currentOffset += (DWORD)pngs[i].size();
    }

    std::ofstream outFile(argv[2], std::ios::binary);
    if (!outFile) {
        std::wcout << L"Failed to open output file: " << argv[2] << std::endl;
        return 1;
    }

    outFile.write((char*)&iconDir, sizeof(iconDir));
    outFile.write((char*)entries.data(), entries.size() * sizeof(ICONDIRENTRY));
    for (const auto& png : pngs) {
        outFile.write((char*)png.data(), png.size());
    }
    outFile.close();

    std::wcout << L"Successfully created: " << argv[2] << std::endl;

    pWICFactory->Release();
    pD2DFactory->Release();
    CoUninitialize();
    return 0;
}
