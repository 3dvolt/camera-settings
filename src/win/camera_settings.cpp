#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#include <windows.h>
#include <objbase.h>
#include <dshow.h>
#include <iostream>
#include <sstream>
#include "camera_settings_base.h"

HRESULT QueryIBaseFilter(const wchar_t *wszName, int index, IBaseFilter **ppVideoCaptureFilter);

// Simple logging helper: prints to stderr and OutputDebugString (visible in debugger)
static void DebugLog(const std::string &msg)
{
  std::cerr << "[camera-settings] " << msg << std::endl;
  std::string withPrefix = "[camera-settings] " + msg + "\n";
  OutputDebugStringA(withPrefix.c_str());
}

// Helper to format HRESULT as hex
static std::string HResultToHex(HRESULT hr)
{
  std::ostringstream oss;
  oss << "0x" << std::hex << hr;
  return oss.str();
}

// Helper function to safely initialize COM
// Returns true if COM was initialized by this call, false if it was already initialized
bool SafeCoInitialize(bool *pWasInitialized)
{
  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
  if (hr == RPC_E_CHANGED_MODE)
  {
    // Try with multithreaded mode if apartment threaded failed
    hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  }

  if (pWasInitialized)
  {
    *pWasInitialized = (hr == S_OK);
  }

  if (FAILED(hr))
  {
    DebugLog("SafeCoInitialize failed, hr=" + HResultToHex(hr));
  }
  else
  {
    DebugLog(std::string("SafeCoInitialize succeeded, hr=") + HResultToHex(hr) +
             (pWasInitialized && *pWasInitialized ? " (initialized here)" : " (already initialized)"));
  }

  return SUCCEEDED(hr);
}

// Helper function to safely uninitialize COM
void SafeCoUninitialize(bool wasInitialized)
{
  if (wasInitialized)
  {
    DebugLog("SafeCoUninitialize: calling CoUninitialize");
    CoUninitialize();
  }
}

std::map<int, std::string> propMapVideo = {
    {VideoProcAmp_Brightness, "Brightness"},
    {VideoProcAmp_Contrast, "Contrast"},
    {VideoProcAmp_Hue, "Hue"},
    {VideoProcAmp_Saturation, "Saturation"},
    {VideoProcAmp_Sharpness, "Sharpness"},
    {VideoProcAmp_Gamma, "Gamma"},
    {VideoProcAmp_ColorEnable, "ColorEnable"},
    {VideoProcAmp_WhiteBalance, "WhiteBalance"},
    {VideoProcAmp_BacklightCompensation, "BacklightCompensation"},
    {VideoProcAmp_Gain, "Gain"},
};
std::map<int, std::string> propMapCamera = {
    {CameraControl_Pan, "Pan"},
    {CameraControl_Tilt, "Tilt"},
    {CameraControl_Roll, "Roll"},
    {CameraControl_Zoom, "Zoom"},
    {CameraControl_Exposure, "Exposure"},
    {CameraControl_Iris, "Iris"},
    {CameraControl_Focus, "Focus"},
};

std::map<std::string, int> propMapVideoReverse = {
    {"Brightness", VideoProcAmp_Brightness},
    {"Contrast", VideoProcAmp_Contrast},
    {"Hue", VideoProcAmp_Hue},
    {"Saturation", VideoProcAmp_Saturation},
    {"Sharpness", VideoProcAmp_Sharpness},
    {"Gamma", VideoProcAmp_Gamma},
    {"ColorEnable", VideoProcAmp_ColorEnable},
    {"WhiteBalance", VideoProcAmp_WhiteBalance},
    {"BacklightCompensation", VideoProcAmp_BacklightCompensation},
    {"Gain", VideoProcAmp_Gain},
};
std::map<std::string, int> propMapCameraReverse = {
    {"Pan", CameraControl_Pan},
    {"Tilt", CameraControl_Tilt},
    {"Roll", CameraControl_Roll},
    {"Zoom", CameraControl_Zoom},
    {"Exposure", CameraControl_Exposure},
    {"Iris", CameraControl_Iris},
    {"Focus", CameraControl_Focus},
};

class IBaseFilterMap
{
public:
  void Set(const wchar_t *wszName, int index, IBaseFilter *pVideoCaptureFilter)
  {
    if (index == -1)
    {
      iBaseFilterNameMap[std::wstring(wszName)] = pVideoCaptureFilter;
    }
    else
    {
      iBaseFilterIndexMap[index] = pVideoCaptureFilter;
    }
  }

  IBaseFilter *Get(const wchar_t *wszName, int index)
  {
    if (index == -1)
    {
      return iBaseFilterNameMap[std::wstring(wszName)];
    }
    else
    {
      return iBaseFilterIndexMap[index];
    }
  }

  bool Has(const wchar_t *wszName, int index)
  {
    if (index == -1)
    {
      return iBaseFilterNameMap.find(std::wstring(wszName)) != iBaseFilterNameMap.end();
    }
    else
    {
      return iBaseFilterIndexMap.find(index) != iBaseFilterIndexMap.end();
    }
  }

  void Delete(const wchar_t *wszName, int index)
  {
    if (index == -1)
    {
      iBaseFilterNameMap.erase(std::wstring(wszName));
    }
    else
    {
      iBaseFilterIndexMap.erase(index);
    }
  }

  int GetCacheCount()
  {
    return iBaseFilterNameMap.size() + iBaseFilterIndexMap.size();
  }

private:
  std::map<std::wstring, IBaseFilter *> iBaseFilterNameMap;
  std::map<int, IBaseFilter *> iBaseFilterIndexMap;
} IFM;

int GetCacheCount() {
  return IFM.GetCacheCount();
}

void OpenCameraSettings(const wchar_t *wszName, int index)
{
  bool wasInitialized = false;
  if (!SafeCoInitialize(&wasInitialized))
  {
    DebugLog("OpenCameraSettings: COM init failed");
    throw std::runtime_error("Failed to initialize COM");
  }

  DebugLog("OpenCameraSettings: starting, index=" + std::to_string(index));

  IBaseFilter *pVideoCaptureFilter = NULL;
  HRESULT hr = QueryIBaseFilter(wszName, index, &pVideoCaptureFilter);

  // We don't cache/use the filter across threads anymore; release it immediately if obtained
  if (SUCCEEDED(hr) && pVideoCaptureFilter)
  {
    pVideoCaptureFilter->Release();
    pVideoCaptureFilter = NULL;
  }

  SafeCoUninitialize(wasInitialized);

  if (FAILED(hr))
  {
    DebugLog("OpenCameraSettings: QueryIBaseFilter failed, hr=" + HResultToHex(hr));
    throw std::runtime_error("Failed to query device");
  }

  DebugLog("OpenCameraSettings: success");
}

void CloseCameraSettings(const wchar_t *wszName, int index)
{
  if (IFM.Has(wszName, index))
  {
    DebugLog("CloseCameraSettings: found cached filter, index=" + std::to_string(index));

    bool wasInitialized = false;
    if (!SafeCoInitialize(&wasInitialized))
    {
      DebugLog("CloseCameraSettings: COM init failed");
      throw std::runtime_error("Failed to initialize COM");
    }

    IBaseFilter *pVideoCaptureFilter = IFM.Get(wszName, index);
    pVideoCaptureFilter->Release();
    IFM.Delete(wszName, index);

    SafeCoUninitialize(wasInitialized);
  }
}

/**
 * Query IAMCameraControl by camera name
 */
HRESULT QueryIBaseFilter(const wchar_t *wszName, int index, IBaseFilter **ppVideoCaptureFilter)
{
  DebugLog("QueryIBaseFilter: start, index=" + std::to_string(index));

  // create system device enumerator
  ICreateDevEnum *pCreateDevEnum = NULL;
  HRESULT hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC_SERVER, IID_ICreateDevEnum, (void **)&pCreateDevEnum);
  if (FAILED(hr))
  {
    DebugLog("QueryIBaseFilter: CoCreateInstance failed, hr=" + HResultToHex(hr));
    return hr;
  }

  // create video input device enumerator
  IEnumMoniker *pEnumMoniker = NULL;
  hr = pCreateDevEnum->CreateClassEnumerator(CLSID_VideoInputDeviceCategory, &pEnumMoniker, 0);
  if (FAILED(hr))
  {
    DebugLog("QueryIBaseFilter: CreateClassEnumerator failed, hr=" + HResultToHex(hr));
    pCreateDevEnum->Release();
    return hr;
  }

  // enumerate video input devices
  IMoniker *pMoniker = NULL;
  int count = -1;
  while (pEnumMoniker->Next(1, &pMoniker, NULL) == S_OK)
  {
    count++;
    // get device friendly name
    IPropertyBag *pPropertyBag = NULL;
    hr = pMoniker->BindToStorage(NULL, NULL, IID_IPropertyBag, (void **)&pPropertyBag);
    if (FAILED(hr))
    {
      pMoniker->Release();
      continue;
    }

    VARIANT varName;
    VariantInit(&varName);
    hr = pPropertyBag->Read(L"FriendlyName", &varName, NULL);
    if (FAILED(hr))
    {
      pPropertyBag->Release();
      pMoniker->Release();
      continue;
    }

    // compare device friendly name with specified name
    if (count == index || wcscmp(varName.bstrVal, wszName) == 0)
    {
      DebugLog("QueryIBaseFilter: found matching device, index=" + std::to_string(count));

      hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter, (void **)ppVideoCaptureFilter);
      if (FAILED(hr))
      {
        DebugLog("QueryIBaseFilter: BindToObject failed, hr=" + HResultToHex(hr));
        VariantClear(&varName);
        pPropertyBag->Release();
        pMoniker->Release();
        continue;
      }

      // release resources
      VariantClear(&varName);
      pPropertyBag->Release();
      pMoniker->Release();

      // found the device and got the IAMCameraControl interface
      pEnumMoniker->Release();
      pCreateDevEnum->Release();
      return S_OK;
    }

    // release resources
    VariantClear(&varName);
    pPropertyBag->Release();
    pMoniker->Release();
  }

  // device not found
  DebugLog("QueryIBaseFilter: device not found");
  pEnumMoniker->Release();
  pCreateDevEnum->Release();
  return E_FAIL;
}

/**
 * Query IAMCameraControl by camera name
 */
HRESULT QueryAllInterface(const wchar_t *wszName, int index, IAMVideoProcAmp **ppProcAmp, IAMCameraControl **ppCameraControl)
{
  DebugLog("QueryAllInterface: start");
  HRESULT hr;
  // create video capture filter
  IBaseFilter *pVideoCaptureFilter = NULL;

  // Always query the filter on the current thread to avoid cross-thread COM issues
  hr = QueryIBaseFilter(wszName, index, &pVideoCaptureFilter);
  if (FAILED(hr) || !pVideoCaptureFilter)
  {
    DebugLog("QueryAllInterface: QueryIBaseFilter failed, hr=" + HResultToHex(hr));
    return E_FAIL;
  }

  // get IAMCameraControl interface
  hr = pVideoCaptureFilter->QueryInterface(IID_IAMCameraControl, (void **)ppCameraControl);
  if (FAILED(hr))
  {
    DebugLog("QueryAllInterface: QueryInterface(IID_IAMCameraControl) failed, hr=" + HResultToHex(hr));
    // pVideoCaptureFilter->Release();
    return E_FAIL;
  }

  // get IAMVideoProcAmp interface
  hr = pVideoCaptureFilter->QueryInterface(IID_IAMVideoProcAmp, (void **)ppProcAmp);
  if (FAILED(hr))
  {
    DebugLog("QueryAllInterface: QueryInterface(IID_IAMVideoProcAmp) failed, hr=" + HResultToHex(hr));
    pVideoCaptureFilter->Release();
    return E_FAIL;
  }

  // We no longer keep the filter cached here; release our reference
  pVideoCaptureFilter->Release();

  DebugLog("QueryAllInterface: success");
  return S_OK;
}

std::vector<CameraSetting> GetCameraSettings(const wchar_t *wszName, int index)
{
  bool wasInitialized = false;
  if (!SafeCoInitialize(&wasInitialized))
  {
    DebugLog("GetCameraSettings: COM init failed");
    throw std::runtime_error("Failed to initialize COM");
  }

  DebugLog("GetCameraSettings: start");

  IAMVideoProcAmp *pProcAmp = NULL;
  IAMCameraControl *pCameraControl = NULL;

  std::vector<CameraSetting> settings;

  HRESULT hr = QueryAllInterface(wszName, index, &pProcAmp, &pCameraControl);
  if (FAILED(hr))
  {
    DebugLog("GetCameraSettings: QueryAllInterface failed");
    SafeCoUninitialize(wasInitialized);
    throw std::runtime_error("Failed to query device");
  }
  else
  {
    for (int i = 0; i <= VideoProcAmp_Gain; i++)
    {
      long min, max, val, step, def, range_flags, flags;
      hr = pProcAmp->GetRange(i, &min, &max, &step, &def, &range_flags);
      if (FAILED(hr))
      {
        DebugLog("GetCameraSettings: VideoProcAmp GetRange failed for index=" + std::to_string(i) +
                 ", hr=" + HResultToHex(hr));
        continue;
      }

      hr = pProcAmp->Get(i, &val, &flags);
      if (FAILED(hr))
      {
        DebugLog("GetCameraSettings: VideoProcAmp Get failed for index=" + std::to_string(i) +
                 ", hr=" + HResultToHex(hr));
        continue;
      }

      CameraSetting setting;
      setting.prop = propMapVideo[i];
      setting.min = min;
      setting.max = max;
      setting.val = val;
      setting.step = step;
      setting.def = def;
      setting.rangeFlags = range_flags;
      setting.isAuto = flags == 1;
      setting.ctrlType = "video";
      settings.push_back(setting);
    }

    for (int i = 0; i <= CameraControl_Focus; i++)
    {
      long min, max, val, step, def, range_flags, flags;
      hr = pCameraControl->GetRange(i, &min, &max, &step, &def, &range_flags);
      if (FAILED(hr))
      {
        DebugLog("GetCameraSettings: CameraControl GetRange failed for index=" + std::to_string(i) +
                 ", hr=" + HResultToHex(hr));
        continue;
      }

      hr = pCameraControl->Get(i, &val, &flags);
      if (FAILED(hr))
      {
        DebugLog("GetCameraSettings: CameraControl Get failed for index=" + std::to_string(i) +
                 ", hr=" + HResultToHex(hr));
        continue;
      }

      CameraSetting setting;
      setting.prop = propMapCamera[i];
      setting.min = min;
      setting.max = max;
      setting.val = val;
      setting.step = step;
      setting.def = def;
      setting.rangeFlags = range_flags;
      setting.isAuto = flags == 1;
      setting.ctrlType = "camera";
      settings.push_back(setting);
    }
    pProcAmp->Release();
    pCameraControl->Release();
  }

  SafeCoUninitialize(wasInitialized);

  DebugLog("GetCameraSettings: success, settings count=" + std::to_string(settings.size()));
  return settings;
}

void SetCameraSettings(const wchar_t *wszName, int index, const std::vector<CameraSettingSetter> &settings)
{
  bool wasInitialized = false;
  if (!SafeCoInitialize(&wasInitialized))
  {
    DebugLog("SetCameraSettings: COM init failed");
    throw std::runtime_error("Failed to initialize COM");
  }

  DebugLog("SetCameraSettings: start, settings size=" + std::to_string(settings.size()));

  IAMVideoProcAmp *pProcAmp = NULL;
  IAMCameraControl *pCameraControl = NULL;
  HRESULT hr = QueryAllInterface(wszName, index, &pProcAmp, &pCameraControl);
  if (FAILED(hr))
  {
    DebugLog("SetCameraSettings: QueryAllInterface failed");
    SafeCoUninitialize(wasInitialized);
    throw std::runtime_error("Failed to query device");
  }
  else
  {
    for (auto setting : settings)
    {
      long flags = setting.isAuto ? 1 : 2;
      long prop;
      if (propMapVideoReverse.count(setting.prop))
      {
        prop = propMapVideoReverse[setting.prop];
        hr = pProcAmp->Set(prop, setting.val, flags);
        if (FAILED(hr))
        {
          DebugLog("SetCameraSettings: pProcAmp->Set failed for prop=" + setting.prop +
                   ", hr=" + HResultToHex(hr));
          continue;
        }
      }
      else if (propMapCameraReverse.count(setting.prop))
      {
        prop = propMapCameraReverse[setting.prop];
        hr = pCameraControl->Set(prop, setting.val, flags);
        if (FAILED(hr))
        {
          DebugLog("SetCameraSettings: pCameraControl->Set failed for prop=" + setting.prop +
                   ", hr=" + HResultToHex(hr));
          continue;
        }
      }
      else
      {
        DebugLog("SetCameraSettings: invalid prop=" + setting.prop);
        pProcAmp->Release();
        pCameraControl->Release();
        SafeCoUninitialize(wasInitialized);
        throw std::runtime_error("Invalid prop");
      }
    }
    pProcAmp->Release();
    pCameraControl->Release();
  }

  SafeCoUninitialize(wasInitialized);
  DebugLog("SetCameraSettings: success");
}

std::vector<Resolution> GetCameraResolutions(const wchar_t *wszName, int index)
{
  bool wasInitialized = false;
  if (!SafeCoInitialize(&wasInitialized))
  {
    DebugLog("GetCameraResolutions: COM init failed");
    throw std::runtime_error("Failed to initialize COM");
  }

  DebugLog("GetCameraResolutions: start");

  HRESULT hr;
  // create video capture filter
  IBaseFilter *pVideoCaptureFilter = NULL;
  // Always query the filter on the current thread instead of using a cached one
  hr = QueryIBaseFilter(wszName, index, &pVideoCaptureFilter);
  if (FAILED(hr) || !pVideoCaptureFilter)
  {
    DebugLog("GetCameraResolutions: QueryIBaseFilter failed, hr=" + HResultToHex(hr));
    SafeCoUninitialize(wasInitialized);
    throw std::runtime_error("Failed to query device");
  }

  std::vector<Resolution> resolutions;

  IEnumPins *pEnum = NULL;
  IPin *pPin = NULL;
  hr = pVideoCaptureFilter->EnumPins(&pEnum);
  if (FAILED(hr))
  {
    pVideoCaptureFilter->Release();
    DebugLog("GetCameraResolutions: EnumPins failed, hr=" + HResultToHex(hr));
    SafeCoUninitialize(wasInitialized);
    throw std::runtime_error("Failed to enumerate pins");
  }

  while (pEnum->Next(1, &pPin, NULL) == S_OK)
  {
    IAMStreamConfig *pConfig = NULL;
    hr = pPin->QueryInterface(IID_PPV_ARGS(&pConfig));
    if (SUCCEEDED(hr))
    {
      int iCount = 0, iSize = 0;
      hr = pConfig->GetNumberOfCapabilities(&iCount, &iSize);
      if (SUCCEEDED(hr))
      {
        for (int i = 0; i < iCount; i++)
        {
          VIDEO_STREAM_CONFIG_CAPS caps;
          AM_MEDIA_TYPE *pmtConfig = NULL;
          hr = pConfig->GetStreamCaps(i, &pmtConfig, (BYTE *)&caps);
          if (SUCCEEDED(hr))
          {
            if (pmtConfig->formattype == FORMAT_VideoInfo)
            {
              VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *)pmtConfig->pbFormat;

              std::string type = "unkown";
              if (IsEqualGUID(pmtConfig->subtype, MEDIASUBTYPE_YUY2))
              {
                type = "yuy2";
              }
              else if (IsEqualGUID(pmtConfig->subtype, MEDIASUBTYPE_MJPG))
              {
                type = "mjpg";
              }
              else if (IsEqualGUID(pmtConfig->subtype, MEDIASUBTYPE_RGB24))
              {
                type = "rgb24";
              }

              Resolution resolution = {pVih->bmiHeader.biWidth, pVih->bmiHeader.biHeight, type};
              resolutions.push_back(resolution);
            }
            CoTaskMemFree(pmtConfig);
          }
        }
      }
      pConfig->Release();
    }
    pPin->Release();
  }

  pEnum->Release();
  pVideoCaptureFilter->Release();

  SafeCoUninitialize(wasInitialized);

  DebugLog("GetCameraResolutions: success, resolutions count=" + std::to_string(resolutions.size()));
  return resolutions;
}

#else
#endif