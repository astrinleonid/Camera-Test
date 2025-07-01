#ifndef DIRECTSHOW_CAMERA_H
#define DIRECTSHOW_CAMERA_H

#include <windows.h>
#include <dshow.h>
#include <atlbase.h>
#include <iostream>
#include <vector>

// Forward declarations
interface ISampleGrabber;
interface ISampleGrabberCB;

// ISampleGrabberCB interface declaration (must come first)
#ifndef __ISampleGrabberCB_INTERFACE_DEFINED__
#define __ISampleGrabberCB_INTERFACE_DEFINED__

MIDL_INTERFACE("0579154A-2B53-4994-B0D0-E773148EFF85")
ISampleGrabberCB : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SampleCB( 
        double SampleTime,
        IMediaSample *pSample) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE BufferCB( 
        double SampleTime,
        BYTE *pBuffer,
        long BufferLen) = 0;
};

#endif // __ISampleGrabberCB_INTERFACE_DEFINED__

// ISampleGrabber interface declaration
#ifndef __ISampleGrabber_INTERFACE_DEFINED__
#define __ISampleGrabber_INTERFACE_DEFINED__

MIDL_INTERFACE("6B652FFF-11FE-4fce-92AD-0266B5D7C78F")
ISampleGrabber : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE SetOneShot( 
        BOOL OneShot) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE SetMediaType( 
        const AM_MEDIA_TYPE *pType) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE GetConnectedMediaType( 
        AM_MEDIA_TYPE *pType) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE SetBufferSamples( 
        BOOL BufferThem) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE GetCurrentBuffer( 
        long *pBufferSize,
        long *pBuffer) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE GetCurrentSample( 
        IMediaSample **ppSample) = 0;
        
    virtual HRESULT STDMETHODCALLTYPE SetCallback( 
        ISampleGrabberCB *pCallback,
        long WhichMethodToCallback) = 0;
};

#endif // __ISampleGrabber_INTERFACE_DEFINED__

// Forward declarations for classes
class CSampleGrabberCB;
class DirectShowCamera;

// Frame processing callback interface
class CSampleGrabberCB : public ISampleGrabberCB
{
private:
    ULONG m_refCount;
    
public:
    CSampleGrabberCB();
    virtual ~CSampleGrabberCB() = default;
    
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;
    
    // ISampleGrabberCB methods
    STDMETHODIMP SampleCB(double sampleTime, IMediaSample* pSample) override;
    STDMETHODIMP BufferCB(double sampleTime, BYTE* pBuffer, long bufferLen) override;
    
    // Custom frame processing methods
    virtual void ProcessFrame(BYTE* frameData, long frameSize);
    virtual void OnFrameReceived(BYTE* frameData, long frameSize, double timestamp);
    
    // Statistics and monitoring
    void ResetStatistics();
    long GetFrameCount() const { return m_frameCount; }
    double GetAverageFPS() const;
    
private:
    long m_frameCount;
    DWORD m_startTime;
    DWORD m_lastFrameTime;
    
    // Frame analysis helpers
    void AnalyzeFrame(BYTE* frameData, long frameSize);
    double CalculateAverageBrightness(BYTE* frameData, long frameSize);
};

// Camera information structure
struct CameraInfo
{
    std::wstring friendlyName;
    std::wstring devicePath;
    CComPtr<IMoniker> moniker;
    int index;
};

// Camera capabilities structure
struct CameraCapabilities
{
    int width;
    int height;
    int bitsPerPixel;
    double frameRate;
    GUID subType;
    std::wstring formatName;
};

// Main DirectShow camera class
class DirectShowCamera
{
private:
    // DirectShow interfaces
    CComPtr<IGraphBuilder> m_pGraph;
    CComPtr<ICaptureGraphBuilder2> m_pCaptureGraphBuilder;
    CComPtr<IMediaControl> m_pMediaControl;
    CComPtr<IMediaEvent> m_pMediaEvent;
    CComPtr<IBaseFilter> m_pCameraFilter;
    CComPtr<IBaseFilter> m_pSampleGrabberFilter;
    CComPtr<ISampleGrabber> m_pSampleGrabber;
    CComPtr<IBaseFilter> m_pNullRenderer;
    
    // Callback and state
    CSampleGrabberCB* m_pCallback;
    bool m_isInitialized;
    bool m_isCapturing;
    CameraCapabilities m_currentCapabilities;
    
public:
    DirectShowCamera();
    virtual ~DirectShowCamera();
    
    // Initialization and cleanup
    HRESULT Initialize();
    void Cleanup();
    
    // Camera enumeration and selection
    HRESULT Enumeratecameras(std::vector<CameraInfo>& cameras);
    HRESULT SelectCamera(int cameraIndex);
    HRESULT SelectCameraByName(const std::wstring& friendlyName);
    
    // Camera capabilities
    HRESULT GetCameraCapabilities(std::vector<CameraCapabilities>& capabilities);
    HRESULT SetCameraFormat(const CameraCapabilities& format);
    HRESULT GetCameraFormat(int& width, int& height);
    CameraCapabilities GetCurrentFormat() const { return m_currentCapabilities; }
    
    // Sample grabber setup
    HRESULT SetupSampleGrabber(const GUID& mediaSubType = MEDIASUBTYPE_RGB24);
    HRESULT SetCustomCallback(CSampleGrabberCB* pCallback);
    
    // Filter graph management
    HRESULT BuildFilterGraph();
    HRESULT ConnectFilters();
    
    // Capture control
    HRESULT StartCapture();
    HRESULT StopCapture();
    HRESULT PauseCapture();
    
    // Status and monitoring
    bool IsInitialized() const { return m_isInitialized; }
    bool IsCapturing() const { return m_isCapturing; }
    HRESULT GetGraphState(OAFilterState& state);
    
    // Event handling
    HRESULT HandleGraphEvent();
    HRESULT WaitForCompletion(long timeout = INFINITE);
    
    // Utility methods
    static std::wstring GUIDToString(const GUID& guid);
    static std::wstring GetMediaSubTypeName(const GUID& subType);
    static HRESULT SaveGraphToFile(IGraphBuilder* pGraph, const std::wstring& filename);
    
    // Error handling
    static std::wstring GetErrorDescription(HRESULT hr);
    static void LogError(const std::wstring& operation, HRESULT hr);
    
private:
    // Internal helper methods
    HRESULT CreateFilterGraph();
    HRESULT CreateCaptureGraphBuilder();
    HRESULT SetupNullRenderer();
    HRESULT EnumerateFormats(IPin* pPin, std::vector<CameraCapabilities>& formats);
    HRESULT GetPinMediaType(IPin* pPin, AM_MEDIA_TYPE** ppMediaType);
    HRESULT SetPinMediaType(IPin* pPin, const AM_MEDIA_TYPE* pMediaType);
    
    // Pin management
    HRESULT FindPin(IBaseFilter* pFilter, PIN_DIRECTION direction, IPin** ppPin);
    HRESULT GetOutputPin(IBaseFilter* pFilter, IPin** ppPin);
    HRESULT GetInputPin(IBaseFilter* pFilter, IPin** ppPin);
    
    // Media type helpers
    void FreeMediaType(AM_MEDIA_TYPE* pMediaType);
    HRESULT CopyMediaType(AM_MEDIA_TYPE* pDest, const AM_MEDIA_TYPE* pSource);
    bool IsMediaTypeSupported(const AM_MEDIA_TYPE* pMediaType);
};

// Custom callback class for specialized processing
class CCustomFrameProcessor : public CSampleGrabberCB
{
public:
    CCustomFrameProcessor();
    virtual ~CCustomFrameProcessor() = default;
    
    // Override for custom processing
    void ProcessFrame(BYTE* frameData, long frameSize) override;
    
    // Specialized processing methods
    virtual void ProcessRGBFrame(BYTE* rgbData, int width, int height);
    virtual void ProcessYUVFrame(BYTE* yuvData, int width, int height);
    virtual void ProcessGrayscaleFrame(BYTE* grayData, int width, int height);
    
    // Frame saving
    HRESULT SaveFrameToBMP(BYTE* frameData, int width, int height, const std::wstring& filename);
    HRESULT SaveFrameToJPEG(BYTE* frameData, int width, int height, const std::wstring& filename);
    
    // Motion detection
    bool DetectMotion(BYTE* currentFrame, BYTE* previousFrame, int frameSize, int threshold = 30);
    
    // Frame statistics
    struct FrameStats {
        double avgBrightness;
        double avgContrast;
        bool motionDetected;
        DWORD timestamp;
    };
    
    FrameStats AnalyzeFrameStatistics(BYTE* frameData, int width, int height);
    
private:
    std::vector<BYTE> m_previousFrame;
    FrameStats m_lastStats;
    int m_frameWidth;
    int m_frameHeight;
};

// Utility macros
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p) = nullptr; } }
#define CHECK_HR(hr) { if (FAILED(hr)) { DirectShowCamera::LogError(L"Operation failed", hr); return hr; } }
#define LOG_HR(operation, hr) { if (FAILED(hr)) { DirectShowCamera::LogError(operation, hr); } }

// GUID definitions for DirectShow components
#ifndef CLSID_SampleGrabber
DEFINE_GUID(CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
#endif

#ifndef IID_ISampleGrabber
DEFINE_GUID(IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce, 0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);
#endif

#ifndef IID_ISampleGrabberCB
DEFINE_GUID(IID_ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994, 0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85);
#endif

#ifndef CLSID_NullRenderer
DEFINE_GUID(CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3, 0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
#endif

#endif // DIRECTSHOW_CAMERA_H