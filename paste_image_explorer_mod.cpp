// ==WindhawkMod==
// @id              explorer-paste-clipboard-images
// @name            Paste Clipboard Images as Files in Explorer
// @description     Allows pasting copied images (like WeChat screenshots) as PNG files directly in Windows Explorer (Desktop, folders, file dialogs) using Ctrl+V or the right-click Paste menu item.
// @version         1.0.0
// @author          Antigravity
// @github          https://github.com/StarsofDeduce/WindHawk
// @include         explorer.exe
// @compilerOptions -lgdiplus -lole32 -lgdi32 -luuid
// ==/WindhawkMod==

#include <windows.h>
#include <shlobj.h>
#include <vector>
#include <string>
#include <objidl.h>
#include <gdiplus.h>

// Helper to get CLSID of PNG encoder
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;          // number of image encoders
    UINT size = 0;         // size of the image encoder array in bytes

    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    std::vector<BYTE> buffer(size);
    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)buffer.data();

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            return j;
        }
    }
    return -1;
}

// Scoped GDI+ initializer
class CGdiPlusInit {
private:
    ULONG_PTR m_token;
    bool m_success;
public:
    CGdiPlusInit() : m_token(0), m_success(false) {
        Gdiplus::GdiplusStartupInput input;
        Gdiplus::Status status = Gdiplus::GdiplusStartup(&m_token, &input, NULL);
        m_success = (status == Gdiplus::Ok);
    }
    ~CGdiPlusInit() {
        if (m_success && m_token) {
            Gdiplus::GdiplusShutdown(m_token);
        }
    }
    bool IsSuccess() const { return m_success; }
};

UINT GetPngFormat() {
    static UINT cfPng = RegisterClipboardFormatW(L"PNG");
    return cfPng;
}

// Generate a nice timestamped file name in the temp directory
std::wstring GetTempImagePath() {
    wchar_t tempPath[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempPath)) {
        return L"";
    }
    
    SYSTEMTIME st;
    GetLocalTime(&st);
    
    wchar_t fileName[MAX_PATH];
    swprintf_s(fileName, MAX_PATH, L"PastedImage_%04d%02d%02d_%02d%02d%02d.png",
              st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
              
    std::wstring path = tempPath;
    if (path.back() != L'\\') {
        path += L'\\';
    }
    path += fileName;
    
    // De-duplicate if needed
    int suffix = 1;
    std::wstring finalPath = path;
    while (GetFileAttributesW(finalPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
        size_t dot = path.find_last_of(L'.');
        finalPath = path.substr(0, dot) + L"_" + std::to_wstring(suffix++) + L".png";
    }
    
    return finalPath;
}

// Extract raw PNG data from clipboard if available
bool SavePngFromClipboard(IDataObject* pDataObject, const std::wstring& filePath) {
    FORMATETC fe = { (CLIPFORMAT)GetPngFormat(), NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm;
    if (pDataObject->GetData(&fe, &sm) == S_OK) {
        void* pData = GlobalLock(sm.hGlobal);
        size_t size = GlobalSize(sm.hGlobal);
        bool success = false;
        if (pData && size > 0) {
            HANDLE hFile = CreateFileW(filePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
            if (hFile != INVALID_HANDLE_VALUE) {
                DWORD written = 0;
                success = WriteFile(hFile, pData, (DWORD)size, &written, NULL) && (written == size);
                CloseHandle(hFile);
            }
        }
        GlobalUnlock(sm.hGlobal);
        ReleaseStgMedium(&sm);
        return success;
    }
    return false;
}

// Convert DIB to HBITMAP
HBITMAP HBitmapFromDIB(void* pDIB) {
    BITMAPINFO* pBi = (BITMAPINFO*)pDIB;
    
    DWORD headerSize = pBi->bmiHeader.biSize;
    DWORD colorTableSize = 0;
    
    if (pBi->bmiHeader.biClrUsed > 0) {
        colorTableSize = pBi->bmiHeader.biClrUsed * sizeof(RGBQUAD);
    } else if (pBi->bmiHeader.biBitCount <= 8) {
        colorTableSize = (1 << pBi->bmiHeader.biBitCount) * sizeof(RGBQUAD);
    }
    
    if (pBi->bmiHeader.biCompression == BI_BITFIELDS && headerSize == sizeof(BITMAPINFOHEADER)) {
        colorTableSize += 3 * sizeof(DWORD);
    }
    
    void* pBits = (BYTE*)pDIB + headerSize + colorTableSize;
    
    HDC hdc = GetDC(NULL);
    HBITMAP hBitmap = CreateDIBitmap(hdc, &pBi->bmiHeader, CBM_INIT, pBits, pBi, DIB_RGB_COLORS);
    ReleaseDC(NULL, hdc);
    
    return hBitmap;
}

// Convert CF_DIB image to PNG file
bool SaveDibFromClipboard(IDataObject* pDataObject, const std::wstring& filePath) {
    FORMATETC fe = { CF_DIB, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
    STGMEDIUM sm;
    if (pDataObject->GetData(&fe, &sm) == S_OK) {
        bool success = false;
        void* pDIB = GlobalLock(sm.hGlobal);
        if (pDIB) {
            HBITMAP hBitmap = HBitmapFromDIB(pDIB);
            if (hBitmap) {
                CGdiPlusInit gdiInit;
                if (gdiInit.IsSuccess()) {
                    Gdiplus::Bitmap bmp(hBitmap, NULL);
                    CLSID pngClsid;
                    if (GetEncoderClsid(L"image/png", &pngClsid) != -1) {
                        Gdiplus::Status status = bmp.Save(filePath.c_str(), &pngClsid, NULL);
                        success = (status == Gdiplus::Ok);
                    }
                }
                DeleteObject(hBitmap);
            }
        }
        GlobalUnlock(sm.hGlobal);
        ReleaseStgMedium(&sm);
        return success;
    }
    return false;
}

// Convert CF_BITMAP to PNG file
bool SaveBitmapFromClipboard(IDataObject* pDataObject, const std::wstring& filePath) {
    FORMATETC fe = { CF_BITMAP, NULL, DVASPECT_CONTENT, -1, TYMED_GDI };
    STGMEDIUM sm;
    if (pDataObject->GetData(&fe, &sm) == S_OK) {
        bool success = false;
        HBITMAP hBitmap = sm.hBitmap;
        if (hBitmap) {
            CGdiPlusInit gdiInit;
            if (gdiInit.IsSuccess()) {
                Gdiplus::Bitmap bmp(hBitmap, NULL);
                CLSID pngClsid;
                if (GetEncoderClsid(L"image/png", &pngClsid) != -1) {
                    Gdiplus::Status status = bmp.Save(filePath.c_str(), &pngClsid, NULL);
                    success = (status == Gdiplus::Ok);
                }
            }
        }
        ReleaseStgMedium(&sm);
        return success;
    }
    return false;
}

// Determine if clipboard contains any image formats
bool HasImageData(IDataObject* pDataObject) {
    FORMATETC fe;
    fe.ptd = NULL;
    fe.dwAspect = DVASPECT_CONTENT;
    fe.lindex = -1;

    fe.cfFormat = (CLIPFORMAT)GetPngFormat();
    fe.tymed = TYMED_HGLOBAL;
    if (pDataObject->QueryGetData(&fe) == S_OK) return true;

    fe.cfFormat = CF_DIB;
    fe.tymed = TYMED_HGLOBAL;
    if (pDataObject->QueryGetData(&fe) == S_OK) return true;

    fe.cfFormat = CF_BITMAP;
    fe.tymed = TYMED_GDI;
    if (pDataObject->QueryGetData(&fe) == S_OK) return true;

    return false;
}

// Auto-cleaner class for global memory and temporary files
class CStgMediumRelease final : public IUnknown {
private:
    ULONG m_refCount;
    HGLOBAL m_hGlobal;
    std::wstring m_filePath;

public:
    CStgMediumRelease(HGLOBAL hGlobal, const std::wstring& filePath) 
        : m_refCount(1), m_hGlobal(hGlobal), m_filePath(filePath) {}

    virtual ~CStgMediumRelease() {
        if (m_hGlobal) {
            GlobalFree(m_hGlobal);
        }
        if (!m_filePath.empty()) {
            DeleteFileW(m_filePath.c_str());
        }
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) {
            delete this;
        }
        return ref;
    }
};

// Generates the HDROP structure containing the temp file path
HRESULT GetImageAsHDrop(IDataObject* pDataObject, STGMEDIUM* pmedium) {
    std::wstring filePath = GetTempImagePath();
    if (filePath.empty()) return E_FAIL;
    
    bool saved = false;
    if (SavePngFromClipboard(pDataObject, filePath)) {
        saved = true;
    } else if (SaveDibFromClipboard(pDataObject, filePath)) {
        saved = true;
    } else if (SaveBitmapFromClipboard(pDataObject, filePath)) {
        saved = true;
    }
    
    if (!saved) return E_FAIL;
    
    // Double null-terminated string needed for HDROP
    size_t pathLenWithNulls = filePath.length() + 2; 
    size_t sizeNeeded = sizeof(DROPFILES) + pathLenWithNulls * sizeof(wchar_t);
    
    HGLOBAL hGlobal = GlobalAlloc(GHND | GMEM_SHARE, sizeNeeded);
    if (!hGlobal) {
        DeleteFileW(filePath.c_str());
        return E_OUTOFMEMORY;
    }
    
    DROPFILES* pDrop = (DROPFILES*)GlobalLock(hGlobal);
    if (!pDrop) {
        GlobalFree(hGlobal);
        DeleteFileW(filePath.c_str());
        return E_OUTOFMEMORY;
    }
    
    pDrop->pFiles = sizeof(DROPFILES);
    pDrop->fWide = TRUE;
    
    wchar_t* pPathStart = (wchar_t*)((BYTE*)pDrop + sizeof(DROPFILES));
    wcscpy_s(pPathStart, pathLenWithNulls, filePath.c_str());
    pPathStart[filePath.length()] = L'\0';
    pPathStart[filePath.length() + 1] = L'\0';
    
    GlobalUnlock(hGlobal);
    
    CStgMediumRelease* pRelease = new CStgMediumRelease(hGlobal, filePath);
    
    pmedium->tymed = TYMED_HGLOBAL;
    pmedium->hGlobal = hGlobal;
    pmedium->pUnkForRelease = pRelease; // Hand off ownership of cleanup
    
    return S_OK;
}

// Wrapper for IEnumFORMATETC to append CF_HDROP
class CEnumFormatEtc final : public IEnumFORMATETC {
private:
    std::vector<FORMATETC> m_formats;
    ULONG m_index;
    ULONG m_refCount;

public:
    CEnumFormatEtc(const std::vector<FORMATETC>& formats) 
        : m_formats(formats), m_index(0), m_refCount(1) {}

    virtual ~CEnumFormatEtc() = default;

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == IID_IUnknown || riid == IID_IEnumFORMATETC) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    STDMETHODIMP Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) override {
        if (!rgelt) return E_POINTER;
        if (pceltFetched) *pceltFetched = 0;
        
        ULONG fetched = 0;
        while (m_index < m_formats.size() && fetched < celt) {
            rgelt[fetched] = m_formats[m_index];
            m_index++;
            fetched++;
        }
        
        if (pceltFetched) *pceltFetched = fetched;
        return (fetched == celt) ? S_OK : S_FALSE;
    }

    STDMETHODIMP Skip(ULONG celt) override {
        m_index += celt;
        if (m_index > m_formats.size()) {
            m_index = (ULONG)m_formats.size();
            return S_FALSE;
        }
        return S_OK;
    }

    STDMETHODIMP Reset() override {
        m_index = 0;
        return S_OK;
    }

    STDMETHODIMP Clone(IEnumFORMATETC** ppenum) override {
        if (!ppenum) return E_POINTER;
        CEnumFormatEtc* pClone = new CEnumFormatEtc(m_formats);
        pClone->m_index = m_index;
        *ppenum = pClone;
        return S_OK;
    }
};

IEnumFORMATETC* WrapEnumFormatEtc(IEnumFORMATETC* pRealEnum) {
    if (!pRealEnum) return nullptr;
    
    std::vector<FORMATETC> formats;
    FORMATETC fe;
    ULONG fetched = 0;
    
    bool hasHDrop = false;
    while (pRealEnum->Next(1, &fe, &fetched) == S_OK && fetched == 1) {
        formats.push_back(fe);
        if (fe.cfFormat == CF_HDROP) {
            hasHDrop = true;
        }
    }
    pRealEnum->Release();
    
    if (!hasHDrop) {
        FORMATETC hdropFe;
        hdropFe.cfFormat = CF_HDROP;
        hdropFe.ptd = NULL;
        hdropFe.dwAspect = DVASPECT_CONTENT;
        hdropFe.lindex = -1;
        hdropFe.tymed = TYMED_HGLOBAL;
        formats.push_back(hdropFe);
    }
    
    return new CEnumFormatEtc(formats);
}

// Wrapper for IDataObject
class CDataObjectWrapper final : public IDataObject {
private:
    IDataObject* m_pRealDataObject;
    ULONG m_refCount;

public:
    CDataObjectWrapper(IDataObject* pReal) : m_pRealDataObject(pReal), m_refCount(1) {
        // We take ownership of the ref returned by OleGetClipboard, so we do NOT call AddRef on it.
    }

    virtual ~CDataObjectWrapper() {
        if (m_pRealDataObject) {
            m_pRealDataObject->Release();
        }
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override {
        static const GUID IID_CDataObjectWrapper = { 0x5a2b3c4d, 0x1e2f, 0x3a4b, { 0x5c, 0x6d, 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d } };
        if (riid == IID_CDataObjectWrapper) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        if (riid == IID_IUnknown || riid == IID_IDataObject) {
            *ppvObject = this;
            AddRef();
            return S_OK;
        }
        return m_pRealDataObject->QueryInterface(riid, ppvObject);
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    STDMETHODIMP_(ULONG) Release() override {
        ULONG ref = InterlockedDecrement(&m_refCount);
        if (ref == 0) {
            delete this;
        }
        return ref;
    }

    STDMETHODIMP GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) override {
        if (pformatetcIn->cfFormat == CF_HDROP && (pformatetcIn->tymed & TYMED_HGLOBAL)) {
            HRESULT hr = m_pRealDataObject->QueryGetData(pformatetcIn);
            if (SUCCEEDED(hr)) {
                return m_pRealDataObject->GetData(pformatetcIn, pmedium);
            }
            
            if (HasImageData(m_pRealDataObject)) {
                return GetImageAsHDrop(m_pRealDataObject, pmedium);
            }
        }
        return m_pRealDataObject->GetData(pformatetcIn, pmedium);
    }

    STDMETHODIMP GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) override {
        return m_pRealDataObject->GetDataHere(pformatetc, pmedium);
    }

    STDMETHODIMP QueryGetData(FORMATETC* pformatetc) override {
        if (pformatetc->cfFormat == CF_HDROP && (pformatetc->tymed & TYMED_HGLOBAL)) {
            HRESULT hr = m_pRealDataObject->QueryGetData(pformatetc);
            if (SUCCEEDED(hr)) {
                return S_OK;
            }
            if (HasImageData(m_pRealDataObject)) {
                return S_OK;
            }
        }
        return m_pRealDataObject->QueryGetData(pformatetc);
    }

    STDMETHODIMP GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut) override {
        return m_pRealDataObject->GetCanonicalFormatEtc(pformatectIn, pformatetcOut);
    }

    STDMETHODIMP SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) override {
        return m_pRealDataObject->SetData(pformatetc, pmedium, fRelease);
    }

    STDMETHODIMP EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) override {
        HRESULT hr = m_pRealDataObject->EnumFormatEtc(dwDirection, ppenumFormatEtc);
        if (SUCCEEDED(hr) && dwDirection == DATADIR_GET && HasImageData(m_pRealDataObject)) {
            *ppenumFormatEtc = WrapEnumFormatEtc(*ppenumFormatEtc);
        }
        return hr;
    }

    STDMETHODIMP DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) override {
        return m_pRealDataObject->DAdvise(pformatetc, advf, pAdvSink, pdwConnection);
    }

    STDMETHODIMP DUnadvise(DWORD dwConnection) override {
        return m_pRealDataObject->DUnadvise(dwConnection);
    }

    STDMETHODIMP EnumDAdvise(IEnumSTATDATA** ppenumAdvise) override {
        return m_pRealDataObject->EnumDAdvise(ppenumAdvise);
    }
};

using OleGetClipboard_t = HRESULT (WINAPI *)(LPDATAOBJECT *);
OleGetClipboard_t OleGetClipboard_Original;

HRESULT WINAPI OleGetClipboard_Hook(LPDATAOBJECT *ppDataObj) {
    HRESULT hr = OleGetClipboard_Original(ppDataObj);
    if (SUCCEEDED(hr) && ppDataObj && *ppDataObj) {
        static const GUID IID_CDataObjectWrapper = { 0x5a2b3c4d, 0x1e2f, 0x3a4b, { 0x5c, 0x6d, 0x7e, 0x8f, 0x9a, 0x0b, 0x1c, 0x2d } };
        IUnknown* pCheck = nullptr;
        if ((*ppDataObj)->QueryInterface(IID_CDataObjectWrapper, (void**)&pCheck) == S_OK) {
            pCheck->Release(); // Release QueryInterface reference, object is already wrapped
        } else {
            *ppDataObj = new CDataObjectWrapper(*ppDataObj);
        }
    }
    return hr;
}

BOOL Wh_ModInit() {
    HMODULE hOle32 = GetModuleHandleW(L"ole32.dll");
    if (!hOle32) {
        hOle32 = LoadLibraryW(L"ole32.dll");
    }
    if (hOle32) {
        OleGetClipboard_t pOleGetClipboard = (OleGetClipboard_t)GetProcAddress(hOle32, "OleGetClipboard");
        if (pOleGetClipboard) {
            Wh_SetFunctionHook((void*)pOleGetClipboard, (void*)OleGetClipboard_Hook, (void**)&OleGetClipboard_Original);
        }
    }
    return TRUE;
}

void Wh_ModUninit() {
    // Wh_SetFunctionHook cleanups are automatically handled by Windhawk
}
