// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "playwebmapp.h"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <uuids.h>
#include <evcode.h>
#include "hrtext.h"
#include "mediatypeutil.h"
#include <string>
#include <sstream>
using std::hex;
using std::dec;
using std::wcout;
using std::endl;
using std::wstring;
using GraphUtil::IPinPtr;
using GraphUtil::IBaseFilterPtr;
// using GraphUtil::IFileSourceFilterPtr;
using GraphUtil::FindOutpinVideo;
using GraphUtil::FindOutpinAudio;
using GraphUtil::FindInpinVideo;
using GraphUtil::FindInpinAudio;

App::App(HANDLE hQuit) : m_hQuit(hQuit) {
  assert(m_hQuit);
}

int App::operator()(int argc, wchar_t* argv[]) {
  int status = m_cmdline.Parse(argc, argv);

  if (status)
    return status;

  status = BuildGraph();

  if (status)
    return status;

  IRunningObjectTablePtr rot;

  HRESULT hr = GetRunningObjectTable(0, &rot);
  assert(SUCCEEDED(hr));
  assert(bool(rot));

  IFilterGraph* const pGraph = m_pGraph;

  std::wostringstream os;
  os << "FilterGraph " << std::hex << static_cast<const void*>(pGraph)
     << " pid " << GetCurrentProcessId();

  IMonikerPtr mon;

  hr = CreateItemMoniker(L"!", os.str().c_str(), &mon);
  assert(SUCCEEDED(hr));
  assert(bool(mon));

  DWORD dw;
  hr = rot->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pGraph, mon, &dw);
  assert(SUCCEEDED(hr));

  status = RunGraph();

  hr = rot->Revoke(dw);
  assert(SUCCEEDED(hr));

  DestroyGraph();

  return status;
}

int App::BuildGraph() {
  const bool bList = m_cmdline.GetList();
  // const bool bVerbose = m_cmdline.GetVerbose();

  HRESULT hr = m_pGraph.CreateInstance(CLSID_FilterGraphNoThread);
  assert(SUCCEEDED(hr));
  assert(bool(m_pGraph));

  const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
  assert(bool(pBuilder));

#if 0  // console output in unicode, but console in ascii??
    const HANDLE hFile = CreateFile(
                            L"CONOUT$",  //see also GetStdHandle
                            GENERIC_READ | GENERIC_WRITE,
                            FILE_SHARE_WRITE,
                            0,  //security
                            OPEN_EXISTING,
                            0,   //ignored
                            0);  //ignored
#elif 0
  const HANDLE hFile = CreateFile(L"rendermkv_logfile.txt", GENERIC_WRITE,
                                  0,  // no sharing
                                  0,  // security
                                  CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

  assert(hFile != INVALID_HANDLE_VALUE);
  assert(hFile != 0);

  hr = pBuilder->SetLogFile(DWORD_PTR(hFile));
  assert(SUCCEEDED(hr));
#endif

  if (const CLSID* pclsid = m_cmdline.GetSource()) {
    IBaseFilterPtr pSource;

    hr = pSource.CreateInstance(*pclsid);

    if (FAILED(hr)) {
      wcout << "Unable to create WebmSource filter instance.\n" << hrtext(hr)
            << L" (0x" << hex << hr << dec << L")" << endl;

      return 1;
    }

    assert(bool(pSource));

    hr = m_pGraph->AddFilter(pSource, L"source");
    assert(SUCCEEDED(hr));

    const GraphUtil::IFileSourceFilterPtr pFileSource(pSource);
    assert(bool(pFileSource));

    hr = pFileSource->Load(m_cmdline.GetInputFileName(), 0);

    if (FAILED(hr)) {
      wcout << "Source filter is unable to load input file \""
            << m_cmdline.GetInputFileName() << "\".\n" << hrtext(hr) << L" (0x"
            << hex << hr << dec << L")" << endl;

      return 1;
    }

    if (GraphUtil::OutpinCount(pSource) == 0) {
      wcout << "Source filter does not advertise any output pins." << endl;
      return 1;
    }

    GraphUtil::IEnumPinsPtr e;

    hr = pSource->EnumPins(&e);
    assert(SUCCEEDED(hr));

    int n = 0;

    for (;;) {
      IPinPtr pin;

      hr = e->Next(1, &pin, 0);

      if (hr != S_OK)
        break;

      hr = pBuilder->Render(pin);

      if (SUCCEEDED(hr))  // TODO: check for partial success
        ++n;
      else
        RenderFailed(pin, hr);
    }

    if (n <= 0) {
      wcout << "No source filter output pins rendered successfully." << endl;
      return 1;
    }
  } else if (const CLSID* pclsid = m_cmdline.GetSplitter()) {
    // TODO: parameterize this explicitly: asyncreader vs. url source
    const IBaseFilterPtr pReader(CLSID_AsyncReader);
    assert(bool(pReader));

    hr = m_pGraph->AddFilter(pReader, L"reader");
    assert(SUCCEEDED(hr));

    const GraphUtil::IFileSourceFilterPtr pFileSource(pReader);
    assert(bool(pFileSource));

    hr = pFileSource->Load(m_cmdline.GetInputFileName(), 0);

    if (FAILED(hr)) {
      wcout << "AsyncReader filter is unable to load input file \""
            << m_cmdline.GetInputFileName() << "\".\n" << hrtext(hr) << L" (0x"
            << hex << hr << dec << L")" << endl;

      return 1;
    }

    assert(GraphUtil::OutpinCount(pReader) == 1);

    IBaseFilterPtr pSplitter;

    hr = pSplitter.CreateInstance(*pclsid);

    if (FAILED(hr)) {
      wcout << "Unable to create WebmSplit filter instance.\n" << hrtext(hr)
            << L" (0x" << hex << hr << dec << L")" << endl;

      return 1;
    }

    hr = m_pGraph->AddFilter(pSplitter, L"splitter");
    assert(SUCCEEDED(hr));

    hr = GraphUtil::ConnectDirect(m_pGraph, pReader, pSplitter);

    if (FAILED(hr)) {
      wcout << "Unable to create connect reader filter to splitter.\n"
            << hrtext(hr) << L" (0x" << hex << hr << dec << L")" << endl;

      return 1;
    }

    if (GraphUtil::OutpinCount(pSplitter) == 0) {
      wcout << "Splitter filter does not advertise any output pins." << endl;
      return 1;
    }

    GraphUtil::IEnumPinsPtr e;

    hr = pSplitter->EnumPins(&e);
    assert(SUCCEEDED(hr));

    int n = 0;

    for (;;) {
      IPinPtr pin;

      hr = e->Next(1, &pin, 0);

      if (hr != S_OK)
        break;

      PIN_DIRECTION dir;

      hr = pin->QueryDirection(&dir);
      assert(SUCCEEDED(hr));  // TODO

      if (dir != PINDIR_OUTPUT)
        continue;

      hr = pBuilder->Render(pin);

      if (SUCCEEDED(hr))  // TODO: check for partial success
        ++n;
      else
        RenderFailed(pin, hr);
    }

    if (n <= 0) {
      wcout << "No splitter filter output pins rendered successfully." << endl;
      return 1;
    }
  } else {
    hr = pBuilder->RenderFile(m_cmdline.GetInputFileName(), 0);

    if (FAILED(hr)) {
      wcout << "RenderFile failed to load input file \""
            << m_cmdline.GetInputFileName() << "\".\n" << hrtext(hr) << L" (0x"
            << hex << hr << dec << L")" << endl;

      return 1;
    } else if (hr != S_OK) {
      wcout << "RenderFile only partially succeeded.\n" << hrtext(hr) << L" (0x"
            << hex << hr << dec << L")" << endl;
    }
  }

#if 0
    hr = pBuilder->SetLogFile(0);
    assert(SUCCEEDED(hr));

    const BOOL b = CloseHandle(hFile);
    assert(b);
#endif

  if (bList)
    return 1;  // soft error

  return 0;  // success
}

void App::DestroyGraph() {
  if (IFilterGraph* pGraph = m_pGraph.Detach()) {
    const ULONG n = pGraph->Release();
    n;
    assert(n == 0);
  }
}

int App::RunGraph() {
  assert(bool(m_pGraph));

  const GraphUtil::IMediaEventPtr pEvent(m_pGraph);
  assert(bool(pEvent));

  HANDLE h;

  HRESULT hr = pEvent->GetEventHandle((OAEVENT*)&h);
  assert(hr == S_OK);
  assert(h);

  // DWORD dw = WaitForSingleObject(h, 0);

  enum { nh = 2 };
  const HANDLE ha[nh] = {m_hQuit, h};

  const GraphUtil::IMediaControlPtr pControl(m_pGraph);
  assert(bool(pControl));

  hr = pControl->Run();
  assert(SUCCEEDED(hr));

  for (;;) {
    MSG msg;

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      assert(msg.message != WM_QUIT);

      // wcout << "msg.message=" << msg.message << endl;

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    const DWORD dw = MsgWaitForMultipleObjects(nh, ha, 0,
                                               INFINITE,  // timeout (ms)
                                               QS_ALLINPUT);

    assert(dw >= WAIT_OBJECT_0);
    assert(dw <= (WAIT_OBJECT_0 + nh));

    if (dw == WAIT_OBJECT_0)  // quit
      break;

    if (dw == (WAIT_OBJECT_0 + nh))  // window message
      continue;

    // media event

    long code, param1, param2;

    HRESULT hr = pEvent->GetEvent(&code, &param1, &param2, 0);
    assert(hr == S_OK);

    hr = pEvent->FreeEventParams(code, param1, param2);
    assert(hr == S_OK);

    if (code == EC_USERABORT)  // window closed
      break;

    if (code == EC_COMPLETE)
      break;
  }

  wcout << endl;

  hr = pControl->Stop();
  assert(SUCCEEDED(hr));

  return 0;
}

void App::RenderFailed(IPin* pin, HRESULT hrRender) {
  assert(pin);

  wstring name;

  PIN_INFO info;

  HRESULT hr = pin->QueryPinInfo(&info);

  if (SUCCEEDED(hr)) {
    if (info.pFilter)
      info.pFilter->Release();

    name = info.achName;
  }

  if (name.empty()) {
    wchar_t* id;

    hr = pin->QueryId(&id);

    if (SUCCEEDED(hr) && (id != 0)) {
      name = id;
      CoTaskMemFree(id);
    }
  }

  if (name.empty()) {
    AM_MEDIA_TYPE mt;

    hr = pin->ConnectionMediaType(&mt);

    if (SUCCEEDED(hr)) {
      if (mt.majortype == MEDIATYPE_Video)
        name = L"video";

      else if (mt.subtype == MEDIATYPE_Audio)
        name = L"audio";

      MediaTypeUtil::Destroy(mt);
    }
  }

  wcout << L"Pin";

  if (!name.empty())
    wcout << L"[" << name << L"]";

  wcout << L" failed to render.\n" << hrtext(hrRender) << L" (0x" << hex
        << hrRender << dec << L")\n" << endl;
}
