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

bool RenderSvgToBitmap(ID2D1Factory1* pD2DFactory, IWICImagingFactory* pWICFactory, 
                       const wchar_t* svgPath, UINT size, IWICBitmap** ppWicBitmap) 
{
    // 1. Create WIC Bitmap
    HRESULT hr = pWICFactory->CreateBitmap(size, size, GUID_WICPixelFormat32bppPBGRA, 
                                          WICBitmapCacheOnDemand, ppWicBitmap);
    if (FAILED(hr)) return false;

    // 2. Create D2D Render Target for WIC Bitmap
    D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
        0.0f, 0.0f, D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT
    );

    ID2D1RenderTarget* pRT = nullptr;
    hr = pD2DFactory->CreateWicBitmapRenderTarget(*ppWicBitmap, rtProps, &pRT);
    if (FAILED(hr)) {
        (*ppWicBitmap)->Release();
        *ppWicBitmap = nullptr;
        return false;
    }

    ID2D1DeviceContext5* pDC5 = nullptr;
    hr = pRT->QueryInterface(__uuidof(ID2D1DeviceContext5), (void**)&pDC5);
    if (FAILED(hr)) {
        pRT->Release();
        (*ppWicBitmap)->Release();
        *ppWicBitmap = nullptr;
        return false;
    }

    // 3. Open SVG file stream
    IStream* pStream = nullptr;
    hr = SHCreateStreamOnFileW(svgPath, STGM_READ, &pStream);
    if (FAILED(hr)) {
        pDC5->Release();
        pRT->Release();
        (*ppWicBitmap)->Release();
        *ppWicBitmap = nullptr;
        return false;
    }

    // 4. Create SVG Document (base design is 256x256)
    ID2D1SvgDocument* pSvgDoc = nullptr;
    hr = pDC5->CreateSvgDocument(pStream, D2D1::SizeF(256.0f, 256.0f), &pSvgDoc);
    pStream->Release();

    if (FAILED(hr)) {
        pDC5->Release();
        pRT->Release();
        (*ppWicBitmap)->Release();
        *ppWicBitmap = nullptr;
        return false;
    }

    // 5. Draw SVG with scaling
    pDC5->BeginDraw();
    pDC5->Clear(D2D1::ColorF(0, 0, 0, 0.0f));
    float scale = (float)size / 256.0f;
    pDC5->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale));
    pDC5->DrawSvgDocument(pSvgDoc);
    hr = pDC5->EndDraw();

    pSvgDoc->Release();
    pDC5->Release();
    pRT->Release();

    if (FAILED(hr)) {
        (*ppWicBitmap)->Release();
        *ppWicBitmap = nullptr;
        return false;
    }

    return true;
}

// Convert IWICBitmap to 32-bit DIB icon resource buffer (BITMAPINFOHEADER + BGRA bottom-up + 1bpp AND mask)
bool BitmapToDibResource(IWICBitmap* pWicBitmap, UINT size, std::vector<BYTE>& outDibData) {
    UINT width = size, height = size;
    std::vector<BYTE> bgraPixels(width * height * 4);
    HRESULT hr = pWicBitmap->CopyPixels(nullptr, width * 4, (UINT)bgraPixels.size(), bgraPixels.data());
    if (FAILED(hr)) return false;

    BITMAPINFOHEADER bih = {};
    bih.biSize = sizeof(BITMAPINFOHEADER);
    bih.biWidth = (LONG)width;
    bih.biHeight = (LONG)height * 2; // In ICO format, biHeight = 2 * height (XOR + AND)
    bih.biPlanes = 1;
    bih.biBitCount = 32;
    bih.biCompression = BI_RGB;
    bih.biSizeImage = width * height * 4;

    int andPitch = ((width + 31) / 32) * 4;
    std::vector<BYTE> andMask(andPitch * height, 0);

    // DIB stores rows bottom-up
    std::vector<BYTE> bottomUpPixels(width * height * 4);
    for (UINT y = 0; y < height; ++y) {
        UINT srcY = y;
        UINT dstY = height - 1 - y;
        const BYTE* srcRow = &bgraPixels[srcY * width * 4];
        BYTE* dstRow = &bottomUpPixels[dstY * width * 4];
        memcpy(dstRow, srcRow, width * 4);

        BYTE* maskRow = &andMask[dstY * andPitch];
        for (UINT x = 0; x < width; ++x) {
            BYTE alpha = srcRow[x * 4 + 3];
            if (alpha == 0) {
                maskRow[x / 8] |= (0x80 >> (x % 8)); // 1 = transparent
            }
        }
    }

    size_t totalResSize = sizeof(BITMAPINFOHEADER) + bottomUpPixels.size() + andMask.size();
    outDibData.resize(totalResSize);
    BYTE* ptr = outDibData.data();

    memcpy(ptr, &bih, sizeof(bih));
    ptr += sizeof(bih);
    memcpy(ptr, bottomUpPixels.data(), bottomUpPixels.size());
    ptr += bottomUpPixels.size();
    memcpy(ptr, andMask.data(), andMask.size());

    return true;
}

// Convert IWICBitmap to PNG resource buffer (for 256x256)
bool BitmapToPngResource(IWICImagingFactory* pWICFactory, IWICBitmap* pWicBitmap, UINT size, std::vector<BYTE>& outPngData) {
    IStream* pMemStream = SHCreateMemStream(nullptr, 0);
    if (!pMemStream) return false;

    IWICBitmapEncoder* pEncoder = nullptr;
    HRESULT hr = pWICFactory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &pEncoder);
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

    if (FAILED(hr)) {
        pMemStream->Release();
        return false;
    }

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

    // Windows standard icon sizes: 16, 24, 32, 48, 64, 128, 256
    std::vector<UINT> sizes = { 256, 128, 64, 48, 32, 24, 16 };
    std::vector<std::vector<BYTE>> imageBuffers;
    std::vector<UINT> validSizes;

    for (UINT s : sizes) {
        IWICBitmap* pBitmap = nullptr;
        if (RenderSvgToBitmap(pD2DFactory, pWICFactory, argv[1], s, &pBitmap)) {
            std::vector<BYTE> resData;
            bool ok = false;
            if (s >= 256) {
                ok = BitmapToPngResource(pWICFactory, pBitmap, s, resData);
            } else {
                // Sizes <= 128 use standard DIB format for 100% universal compatibility with Win32 APIs
                ok = BitmapToDibResource(pBitmap, s, resData);
            }
            pBitmap->Release();

            if (ok) {
                imageBuffers.push_back(std::move(resData));
                validSizes.push_back(s);
                std::wcout << L"Rendered size: " << s << L"x" << s << L" (" << imageBuffers.back().size() << L" bytes, "
                           << (s >= 256 ? L"PNG" : L"DIB") << L")" << std::endl;
            } else {
                std::wcout << L"Failed to encode size: " << s << std::endl;
            }
        } else {
            std::wcout << L"Failed to render size: " << s << std::endl;
        }
    }

    if (imageBuffers.empty()) {
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
    iconDir.idCount = (WORD)imageBuffers.size();

    std::vector<ICONDIRENTRY> entries(imageBuffers.size());
    DWORD currentOffset = sizeof(ICONDIR) + (DWORD)(imageBuffers.size() * sizeof(ICONDIRENTRY));

    for (size_t i = 0; i < imageBuffers.size(); ++i) {
        UINT s = validSizes[i];
        entries[i].bWidth = (s >= 256) ? 0 : (BYTE)s;
        entries[i].bHeight = (s >= 256) ? 0 : (BYTE)s;
        entries[i].bColorCount = 0;
        entries[i].bReserved = 0;
        entries[i].wPlanes = 1;
        entries[i].wBitCount = 32;
        entries[i].dwBytesInRes = (DWORD)imageBuffers[i].size();
        entries[i].dwImageOffset = currentOffset;

        currentOffset += (DWORD)imageBuffers[i].size();
    }

    std::ofstream outFile(argv[2], std::ios::binary);
    if (!outFile) {
        std::wcout << L"Failed to open output file: " << argv[2] << std::endl;
        return 1;
    }

    outFile.write((char*)&iconDir, sizeof(iconDir));
    outFile.write((char*)entries.data(), entries.size() * sizeof(ICONDIRENTRY));
    for (const auto& buf : imageBuffers) {
        outFile.write((char*)buf.data(), buf.size());
    }
    outFile.close();

    std::wcout << L"Successfully created: " << argv[2] << std::endl;

    pWICFactory->Release();
    pD2DFactory->Release();
    CoUninitialize();
    return 0;
}
