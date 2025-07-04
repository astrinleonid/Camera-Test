#include "DirectShowCamera.h"
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <vector>
#include <string>

// Link required DirectShow libraries
#pragma comment(lib, "strmiids.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

// Implementation of CSampleGrabberCB
CSampleGrabberCB::CSampleGrabberCB()
    : m_refCount(1), m_frameCount(0)
{
    m_startTime = GetTickCount();
    m_lastFrameTime = 0;
}

STDMETHODIMP CSampleGrabberCB::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_ISampleGrabberCB)
    {
        *ppv = static_cast<ISampleGrabberCB*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CSampleGrabberCB::AddRef()
{
    return ++m_refCount;
}

STDMETHODIMP_(ULONG) CSampleGrabberCB::Release()
{
    if (--m_refCount == 0)
    {
        delete this;
        return 0;
    }
    return m_refCount;
}

STDMETHODIMP CSampleGrabberCB::SampleCB(double sampleTime, IMediaSample* pSample)
{
    return S_OK; // Not used in this implementation
}

STDMETHODIMP CSampleGrabberCB::BufferCB(double sampleTime, BYTE* pBuffer, long bufferLen)
{
    m_frameCount++;
    m_lastFrameTime = GetTickCount();

    if (m_frameCount % 30 == 0) // Print every 30th frame to avoid spam
    {
        std::cout << "Frame " << m_frameCount << " received - Size: " << bufferLen << " bytes" << std::endl;
    }

    // Call the virtual processing methods
    ProcessFrame(pBuffer, bufferLen);
    OnFrameReceived(pBuffer, bufferLen, sampleTime);

    return S_OK;
}

void CSampleGrabberCB::ProcessFrame(BYTE* frameData, long frameSize)
{
    AnalyzeFrame(frameData, frameSize);
}

void CSampleGrabberCB::OnFrameReceived(BYTE* frameData, long frameSize, double timestamp)
{
    // Override this method for custom frame handling
}

void CSampleGrabberCB::ResetStatistics()
{
    m_frameCount = 0;
    m_startTime = GetTickCount();
}

double CSampleGrabberCB::GetAverageFPS() const
{
    DWORD elapsed = GetTickCount() - m_startTime;
    if (elapsed > 0)
        return (double)m_frameCount * 1000.0 / elapsed;
    return 0.0;
}

void CSampleGrabberCB::AnalyzeFrame(BYTE* frameData, long frameSize)
{
    if (frameSize > 0)
    {
        double brightness = CalculateAverageBrightness(frameData, frameSize);

        // Only print occasionally to avoid flooding console
        static int printCounter = 0;
        if (++printCounter % 60 == 0)
        {
            std::cout << "Frame " << m_frameCount << " - Brightness: " << std::fixed << std::setprecision(1)
                << brightness << " - FPS: " << std::setprecision(1) << GetAverageFPS() << std::endl;
        }
    }
}

double CSampleGrabberCB::CalculateAverageBrightness(BYTE* frameData, long frameSize)
{
    long totalBrightness = 0;
    for (long i = 0; i < frameSize; i += 3) // Assuming RGB24 format
    {
        if (i + 2 < frameSize)
        {
            totalBrightness += (frameData[i] + frameData[i + 1] + frameData[i + 2]) / 3;
        }
    }
    return frameSize > 0 ? (double)totalBrightness / (frameSize / 3) : 0.0;
}

// Custom callback class that saves frames
class CFrameSavingCallback : public CSampleGrabberCB
{
private:
    bool m_frameSaved;
    int m_imageWidth;
    int m_imageHeight;

public:
    CFrameSavingCallback() : m_frameSaved(false), m_imageWidth(0), m_imageHeight(0) {}

    void SetImageDimensions(int width, int height)
    {
        m_imageWidth = width;
        m_imageHeight = height;
    }

    void ProcessFrame(BYTE* frameData, long frameSize) override
    {
        // Call base class processing
        CSampleGrabberCB::ProcessFrame(frameData, frameSize);

        // Save the first frame we receive
        if (!m_frameSaved && frameData && frameSize > 0)
        {
            SaveFrameAsBMP(frameData, frameSize);
            m_frameSaved = true;
        }
    }

private:
    void SaveFrameAsBMP(BYTE* frameData, long frameSize)
    {
        // For RGB24 format: 3 bytes per pixel
        int bytesPerPixel = 3;

        if (m_imageWidth == 0 || m_imageHeight == 0)
        {
            // Try to estimate dimensions from frame size
            // Assume square-ish image for estimation
            int totalPixels = frameSize / bytesPerPixel;
            m_imageWidth = (int)std::sqrt(totalPixels * 1.33); // Assume 4:3 aspect ratio
            m_imageHeight = totalPixels / m_imageWidth;

            std::cout << "Estimated image dimensions: " << m_imageWidth << "x" << m_imageHeight << std::endl;
        }

        std::string filename = "captured_frame.bmp";

        // BMP file header
        BITMAPFILEHEADER bfh;
        BITMAPINFOHEADER bih;

        // Initialize BMP file header
        memset(&bfh, 0, sizeof(bfh));
        bfh.bfType = 0x4D42; // "BM"
        bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
        bfh.bfSize = bfh.bfOffBits + frameSize;

        // Initialize BMP info header
        memset(&bih, 0, sizeof(bih));
        bih.biSize = sizeof(BITMAPINFOHEADER);
        bih.biWidth = m_imageWidth;
        bih.biHeight = -m_imageHeight; // Negative height for top-down bitmap
        bih.biPlanes = 1;
        bih.biBitCount = 24; // RGB24
        bih.biCompression = BI_RGB;
        bih.biSizeImage = frameSize;

        // Create and write BMP file
        HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_WRITE, 0, NULL,
            CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD bytesWritten;

            // Write BMP headers
            WriteFile(hFile, &bfh, sizeof(bfh), &bytesWritten, NULL);
            WriteFile(hFile, &bih, sizeof(bih), &bytesWritten, NULL);

            // RGB24 data in DirectShow is usually BGR, so we need to convert to RGB
            std::vector<BYTE> rgbData(frameSize);
            for (long i = 0; i < frameSize; i += 3)
            {
                if (i + 2 < frameSize)
                {
                    // Convert BGR to RGB
                    rgbData[i] = frameData[i + 2];     // R
                    rgbData[i + 1] = frameData[i + 1]; // G  
                    rgbData[i + 2] = frameData[i];     // B
                }
            }

            // Write image data
            WriteFile(hFile, rgbData.data(), frameSize, &bytesWritten, NULL);

            CloseHandle(hFile);

            std::cout << "Frame saved as: " << filename << std::endl;
            std::cout << "Image size: " << m_imageWidth << "x" << m_imageHeight << std::endl;
            std::cout << "Data size: " << frameSize << " bytes" << std::endl;
        }
        else
        {
            std::cout << "Failed to create file: " << filename << std::endl;
        }
    }
};

// Implementation of DirectShowCamera
DirectShowCamera::DirectShowCamera()
    : m_pCallback(nullptr), m_isInitialized(false), m_isCapturing(false)
{
    ZeroMemory(&m_currentCapabilities, sizeof(m_currentCapabilities));
}

DirectShowCamera::~DirectShowCamera()
{
    Cleanup();
}

HRESULT DirectShowCamera::Initialize()
{
    if (m_isInitialized)
        return S_OK;

    HRESULT hr;

    // Initialize COM
    hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) return hr;

    hr = CreateFilterGraph();
    if (FAILED(hr)) return hr;

    hr = CreateCaptureGraphBuilder();
    if (FAILED(hr)) return hr;

    m_isInitialized = true;
    return S_OK;
}

HRESULT DirectShowCamera::CreateFilterGraph()
{
    HRESULT hr;

    // Create the Filter Graph Manager
    hr = m_pGraph.CoCreateInstance(CLSID_FilterGraph);
    if (FAILED(hr)) return hr;

    // Get Media Control interface
    hr = m_pGraph->QueryInterface(IID_IMediaControl, (void**)&m_pMediaControl);
    if (FAILED(hr)) return hr;

    // Get Media Event interface
    hr = m_pGraph->QueryInterface(IID_IMediaEvent, (void**)&m_pMediaEvent);

    return hr;
}

HRESULT DirectShowCamera::CreateCaptureGraphBuilder()
{
    HRESULT hr;

    // Create the Capture Graph Builder
    hr = m_pCaptureGraphBuilder.CoCreateInstance(CLSID_CaptureGraphBuilder2);
    if (FAILED(hr)) return hr;

    // Set the filter graph
    hr = m_pCaptureGraphBuilder->SetFiltergraph(m_pGraph);

    return hr;
}

HRESULT DirectShowCamera::Enumeratecameras(std::vector<CameraInfo>& cameras)
{
    HRESULT hr;
    cameras.clear();

    // Create system device enumerator
    CComPtr<ICreateDevEnum> pDevEnum;
    hr = pDevEnum.CoCreateInstance(CLSID_SystemDeviceEnum);
    if (FAILED(hr)) return hr;

    // Create enumerator for video capture devices
    CComPtr<IEnumMoniker> pEnum;
    hr = pDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnum, 0);
    if (hr == S_FALSE) return VFW_E_NOT_FOUND; // No devices found

    // Enumerate devices
    CComPtr<IMoniker> pMoniker;
    int index = 0;

    std::cout << "Available cameras:" << std::endl;

    while (pEnum->Next(1, &pMoniker, NULL) == S_OK)
    {
        CComPtr<IPropertyBag> pPropBag;
        hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void**)&pPropBag);

        if (SUCCEEDED(hr))
        {
            VARIANT var;
            VariantInit(&var);
            hr = pPropBag->Read(L"FriendlyName", &var, 0);

            if (SUCCEEDED(hr))
            {
                CameraInfo info;
                info.friendlyName = var.bstrVal;
                info.index = index;
                info.moniker = pMoniker;

                cameras.push_back(info);
                std::wcout << index << L": " << var.bstrVal << std::endl;
            }
            VariantClear(&var);
        }
        pMoniker.Release();
        index++;
    }

    return cameras.empty() ? VFW_E_NOT_FOUND : S_OK;
}

HRESULT DirectShowCamera::SelectCamera(int cameraIndex)
{
    std::vector<CameraInfo> cameras;
    HRESULT hr = Enumeratecameras(cameras);
    if (FAILED(hr)) return hr;

    if (cameraIndex < 0 || cameraIndex >= cameras.size())
        return E_INVALIDARG;

    // Bind to selected camera
    hr = cameras[cameraIndex].moniker->BindToObject(0, 0, IID_IBaseFilter, (void**)&m_pCameraFilter);
    if (FAILED(hr)) return hr;

    // Add camera filter to graph
    hr = m_pGraph->AddFilter(m_pCameraFilter, L"Camera");
    return hr;
}

HRESULT DirectShowCamera::SelectCameraByName(const std::wstring& friendlyName)
{
    std::vector<CameraInfo> cameras;
    HRESULT hr = Enumeratecameras(cameras);
    if (FAILED(hr)) return hr;

    for (const auto& camera : cameras)
    {
        if (camera.friendlyName == friendlyName)
        {
            return SelectCamera(camera.index);
        }
    }

    return VFW_E_NOT_FOUND;
}

HRESULT DirectShowCamera::SetupSampleGrabber(const GUID& mediaSubType)
{
    HRESULT hr;

    // Create Sample Grabber filter
    hr = m_pSampleGrabberFilter.CoCreateInstance(CLSID_SampleGrabber);
    if (FAILED(hr)) return hr;

    // Get Sample Grabber interface
    hr = m_pSampleGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&m_pSampleGrabber);
    if (FAILED(hr)) return hr;

    // Set media type for sample grabber
    AM_MEDIA_TYPE mt;
    ZeroMemory(&mt, sizeof(mt));
    mt.majortype = MEDIATYPE_Video;
    mt.subtype = mediaSubType;
    mt.formattype = FORMAT_VideoInfo;

    hr = m_pSampleGrabber->SetMediaType(&mt);
    if (FAILED(hr)) return hr;

    // Create default callback if none exists
    if (!m_pCallback)
    {
        m_pCallback = new CSampleGrabberCB();
    }

    // Set callback
    hr = m_pSampleGrabber->SetCallback(m_pCallback, 1); // 1 for BufferCB
    if (FAILED(hr)) return hr;

    // Don't buffer samples
    hr = m_pSampleGrabber->SetBufferSamples(FALSE);
    if (FAILED(hr)) return hr;

    // Add to graph
    hr = m_pGraph->AddFilter(m_pSampleGrabberFilter, L"Sample Grabber");
    return hr;
}

HRESULT DirectShowCamera::SetCustomCallback(CSampleGrabberCB* pCallback)
{
    if (m_pCallback && m_pCallback != pCallback)
    {
        m_pCallback->Release();
    }

    m_pCallback = pCallback;
    if (m_pCallback)
    {
        m_pCallback->AddRef();
    }

    // Update the sample grabber if it exists
    if (m_pSampleGrabber && m_pCallback)
    {
        return m_pSampleGrabber->SetCallback(m_pCallback, 1);
    }

    return S_OK;
}

HRESULT DirectShowCamera::SetupNullRenderer()
{
    HRESULT hr;

    // Create Null Renderer (to consume samples without displaying)
    hr = m_pNullRenderer.CoCreateInstance(CLSID_NullRenderer);
    if (FAILED(hr)) return hr;

    // Add to graph
    hr = m_pGraph->AddFilter(m_pNullRenderer, L"Null Renderer");
    return hr;
}

HRESULT DirectShowCamera::BuildFilterGraph()
{
    HRESULT hr;

    if (!m_pCameraFilter)
    {
        // Auto-select first available camera
        hr = SelectCamera(0);
        if (FAILED(hr)) return hr;
    }

    hr = SetupSampleGrabber();
    if (FAILED(hr)) return hr;

    hr = SetupNullRenderer();
    if (FAILED(hr)) return hr;

    hr = ConnectFilters();
    return hr;
}

HRESULT DirectShowCamera::ConnectFilters()
{
    HRESULT hr;

    // Connect Camera -> Sample Grabber -> Null Renderer
    hr = m_pCaptureGraphBuilder->RenderStream(&PIN_CATEGORY_CAPTURE, &MEDIATYPE_Video,
        m_pCameraFilter, m_pSampleGrabberFilter, m_pNullRenderer);
    return hr;
}

HRESULT DirectShowCamera::StartCapture()
{
    if (!m_pMediaControl) return E_FAIL;
    if (m_isCapturing) return S_OK;

    std::cout << "Starting camera capture..." << std::endl;
    HRESULT hr = m_pMediaControl->Run();

    if (SUCCEEDED(hr))
    {
        m_isCapturing = true;
        if (m_pCallback)
        {
            m_pCallback->ResetStatistics();
        }
        std::cout << "Camera capture started successfully!" << std::endl;
    }

    return hr;
}

HRESULT DirectShowCamera::StopCapture()
{
    if (!m_pMediaControl) return E_FAIL;
    if (!m_isCapturing) return S_OK;

    std::cout << "Stopping camera capture..." << std::endl;
    HRESULT hr = m_pMediaControl->Stop();

    if (SUCCEEDED(hr))
    {
        m_isCapturing = false;
        std::cout << "Camera capture stopped." << std::endl;
    }

    return hr;
}

HRESULT DirectShowCamera::PauseCapture()
{
    if (!m_pMediaControl) return E_FAIL;

    std::cout << "Pausing camera capture..." << std::endl;
    return m_pMediaControl->Pause();
}

HRESULT DirectShowCamera::GetGraphState(OAFilterState& state)
{
    if (!m_pMediaControl) return E_FAIL;
    return m_pMediaControl->GetState(100, &state);
}

HRESULT DirectShowCamera::GetCameraFormat(int& width, int& height)
{
    if (!m_pSampleGrabber)
        return E_FAIL;

    AM_MEDIA_TYPE mt;
    HRESULT hr = m_pSampleGrabber->GetConnectedMediaType(&mt);

    if (SUCCEEDED(hr) && mt.formattype == FORMAT_VideoInfo)
    {
        VIDEOINFOHEADER* pVih = (VIDEOINFOHEADER*)mt.pbFormat;
        width = pVih->bmiHeader.biWidth;
        height = abs(pVih->bmiHeader.biHeight);

        // Store in current capabilities
        m_currentCapabilities.width = width;
        m_currentCapabilities.height = height;

        // Free the media type
        if (mt.cbFormat != 0)
        {
            CoTaskMemFree((PVOID)mt.pbFormat);
            mt.cbFormat = 0;
            mt.pbFormat = NULL;
        }
        if (mt.pUnk != NULL)
        {
            mt.pUnk->Release();
            mt.pUnk = NULL;
        }
    }

    return hr;
}

void DirectShowCamera::Cleanup()
{
    if (m_isCapturing)
    {
        StopCapture();
    }

    if (m_pCallback)
    {
        m_pCallback->Release();
        m_pCallback = nullptr;
    }

    m_pNullRenderer.Release();
    m_pSampleGrabber.Release();
    m_pSampleGrabberFilter.Release();
    m_pCameraFilter.Release();
    m_pMediaEvent.Release();
    m_pMediaControl.Release();
    m_pCaptureGraphBuilder.Release();
    m_pGraph.Release();

    m_isInitialized = false;
    m_isCapturing = false;

    CoUninitialize();
}

std::wstring DirectShowCamera::GetErrorDescription(HRESULT hr)
{
    std::wstringstream ss;
    ss << L"HRESULT: 0x" << std::hex << hr;

    switch (hr)
    {
    case VFW_E_NOT_FOUND:
        ss << L" (No capture devices found)";
        break;
    case E_NOINTERFACE:
        ss << L" (Interface not supported)";
        break;
    case E_INVALIDARG:
        ss << L" (Invalid argument)";
        break;
    case VFW_E_CANNOT_CONNECT:
        ss << L" (Cannot connect filters)";
        break;
    default:
        ss << L" (Unknown error)";
        break;
    }

    return ss.str();
}

void DirectShowCamera::LogError(const std::wstring& operation, HRESULT hr)
{
    std::wcout << L"Error in " << operation << L": " << GetErrorDescription(hr) << std::endl;
}

// Updated main function with frame saving
int main()
{
    std::cout << "DirectShow USB Camera Capture Application" << std::endl;
    std::cout << "=========================================" << std::endl;

    DirectShowCamera camera;
    HRESULT hr;

    // Initialize DirectShow
    hr = camera.Initialize();
    if (FAILED(hr))
    {
        std::cout << "Failed to initialize DirectShow. Error: 0x" << std::hex << hr << std::endl;
        return -1;
    }

    // Enumerate cameras and let user select
    std::vector<CameraInfo> cameras;
    hr = camera.Enumeratecameras(cameras);
    if (FAILED(hr))
    {
        std::cout << "Failed to enumerate cameras. Error: 0x" << std::hex << hr << std::endl;
        return -1;
    }

    if (cameras.empty())
    {
        std::cout << "No cameras found!" << std::endl;
        return -1;
    }

    // Select camera
    int selectedCamera = 0;
    if (cameras.size() > 1)
    {
        std::cout << "Enter camera index (0-" << cameras.size() - 1 << "): ";
        std::cin >> selectedCamera;
        if (selectedCamera < 0 || selectedCamera >= cameras.size())
            selectedCamera = 0;
    }

    hr = camera.SelectCamera(selectedCamera);
    if (FAILED(hr))
    {
        std::cout << "Failed to select camera. Error: 0x" << std::hex << hr << std::endl;
        return -1;
    }

    // Create a custom callback that saves frames
    CFrameSavingCallback* pFrameSaver = new CFrameSavingCallback();
    camera.SetCustomCallback(pFrameSaver);

    // Build the filter graph
    hr = camera.BuildFilterGraph();
    if (FAILED(hr))
    {
        std::cout << "Failed to build filter graph. Error: 0x" << std::hex << hr << std::endl;
        return -1;
    }

    // Get the actual camera format after connection
    int width, height;
    hr = camera.GetCameraFormat(width, height);
    if (SUCCEEDED(hr))
    {
        std::cout << "Camera format: " << width << "x" << height << std::endl;
        pFrameSaver->SetImageDimensions(width, height);
    }
    else
    {
        std::cout << "Could not determine camera format, will estimate from data" << std::endl;
    }

    // Start capture
    hr = camera.StartCapture();
    if (FAILED(hr))
    {
        std::cout << "Failed to start capture. Error: 0x" << std::hex << hr << std::endl;
        return -1;
    }

    std::cout << "Capturing frames... The first frame will be saved as 'captured_frame.bmp'" << std::endl;
    std::cout << "Press Enter to stop capture..." << std::endl;
    std::cin.ignore();
    std::cin.get();

    // Stop capture
    camera.StopCapture();

    std::cout << "Application finished." << std::endl;
    return 0;
}