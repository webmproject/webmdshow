// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "makewebmapp.hpp"
#include <cassert>
#include <iostream>
#include <iomanip>
#include <uuids.h>
#include <evcode.h>
#include "hrtext.hpp"
#include "mediatypeutil.hpp"
#include "webmtypes.hpp"
#include "vorbistypes.hpp"
#include "registry.hpp"
#include "vp8encoderidl.h"
//#include <vector>
#include <sstream>
#include <iomanip>
using std::hex;
using std::dec;
using std::wcout;
using std::setw;
using std::setfill;
using std::endl;
using std::flush;
using std::wostringstream;
using GraphUtil::IPinPtr;
using GraphUtil::IBaseFilterPtr;
using GraphUtil::FindOutpinVideo;
using GraphUtil::FindOutpinAudio;
using GraphUtil::FindInpinVideo;
using GraphUtil::FindInpinAudio;


App::App(HANDLE hQuit) :
    m_hQuit(hQuit)
{
    assert(m_hQuit);
}


int App::operator()(int argc, wchar_t* argv[])
{
    const bool bVerbose = m_cmdline.GetVerbose();

    int status = m_cmdline.Parse(argc, argv);
    
    if (status)
        return status;
        
    assert(!bool(m_pGraph));
    
    HRESULT hr = m_pGraph.CreateInstance(CLSID_FilterGraphNoThread);
    
    if (FAILED(hr))
    {
        wcout << L"Unable to create filter graph instance.\n"
              << hrtext(hr)
              << " (0x" << hex << hr << dec << ")"
              << endl;
              
        return 1;  //error
    }
    
    assert(bool(m_pGraph));
    
    const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
    assert(bool(pBuilder));
        
    const GraphUtil::IMediaFilterPtr pGraphFilter(m_pGraph);
    assert(bool(pGraphFilter));
    
    hr = pGraphFilter->SetSyncSource(0);  //process as quickly as possible
    //TODO: are we setting this too early?
    
#ifdef _DEBUG
    if (FAILED(hr))
    {
        wcout << L"IMediaFilter::SetSyncSource failed.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
    }
#endif

    IBaseFilterPtr pReader;

    hr = pBuilder->AddSourceFilter(m_cmdline.GetInputFileName(), L"source", &pReader);

    if (FAILED(hr))
    {
        wcout << "Unable to add source filter to graph.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
              
        return 1;
    }
        
    assert(bool(pReader));
    
    if (GraphUtil::PinCount(pReader) == 0)
    {
        wcout << "Source filter does not have any output pins.\n" << endl;
        return 1;
    }
    
    const GraphUtil::IBaseFilterPtr pDemux = AddDemuxFilter(pReader);
    
    if (!bool(pDemux))
        return 1;        
    
    const GraphUtil::IPinPtr pDemuxOutpinVideo = FindOutpinVideo(pDemux);
    //TODO: we need to do better here: we check for the 0 case,
    //but we must also check for the 1+ case.
    
    if (!bool(pDemuxOutpinVideo))
    {
        if (bVerbose)
            wcout << "Demuxer does not expose video output pin." << endl;
            
        //TODO: we need to provide a command-line option that says to
        //quit immediately if no video stream.
    }
    else
    {
        if (bVerbose)
            DumpPreferredMediaTypes(
                pDemuxOutpinVideo,
                L"demuxer video outpin",
                &App::DumpVideoMediaType);
    }

    const GraphUtil::IPinPtr pDemuxOutpinAudio = FindOutpinAudio(pDemux);
    //TODO: we need to do better here: we check for the 0 case,
    //but we must also check for the 1+ case.
    
    if (!bool(pDemuxOutpinAudio))
    {
        if (bVerbose)
            wcout << "Demuxer does not expose audio output pin." << endl;

        //TODO: we need to provide a command-line option that says to
        //quit immediately if no audio stream.
    }
    else
    {
        if (bVerbose)
            DumpPreferredMediaTypes(
                pDemuxOutpinAudio,
                L"demuxer audio outpin",
                &App::DumpAudioMediaType);
    }
    
    IRunningObjectTablePtr rot;
    
    hr = GetRunningObjectTable(0, &rot);
    assert(SUCCEEDED(hr));
    assert(bool(rot));
    
    IFilterGraph* const pGraph = m_pGraph;
    
    std::wostringstream os;
    os << "FilterGraph " 
       << std::hex
       << static_cast<const void*>(pGraph)
       << " pid "
       << GetCurrentProcessId();
       
    IMonikerPtr mon;
       
    hr = CreateItemMoniker(L"!", os.str().c_str(), &mon);
    assert(SUCCEEDED(hr));
    assert(bool(mon));
    
    //one-pass:    
    //source -> video -> vp8enc -->  webmmux -> writer
    //       \-> audio -> vorbisenc /

    //two-pass first
    //source -> video -> vp8enc -->  webmmux
    //                          \-> stat pkts

    if (m_cmdline.GetTwoPass() >= 0)
    {
        wcout << "Two-pass VP8 encoding mode not yet implemented." << endl;
        status = 1;
    }
    else
    {
        IBaseFilterPtr pMux;
        
        HRESULT hr = pMux.CreateInstance(WebmTypes::CLSID_WebmMux);
        
        if (FAILED(hr))
        {
            wcout << "Unable to create WebmMux filter instance.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return 1;
        }
        
        assert(GraphUtil::InpinCount(pMux) == 2);  //TODO: liberalize
        assert(GraphUtil::OutpinCount(pMux) == 1);
        
        hr = m_pGraph->AddFilter(pMux, L"webmmux");
        assert(SUCCEEDED(hr));
    
        status = CreateMuxerGraph(pDemuxOutpinVideo, pDemuxOutpinAudio, pMux);
        
        if (status == 0)
        {        
            DWORD dw;
            
            hr = rot->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pGraph, mon, &dw);
            assert(SUCCEEDED(hr));
        
            status = RunMuxerGraph(pMux);
            
            hr = rot->Revoke(dw);
            assert(SUCCEEDED(hr));
        }    
    }
        
    m_pGraph.Detach();

    const ULONG n = pGraph->Release();
    n;

    return status;
}


int App::CreateMuxerGraph(
    IPin* pDemuxOutpinVideo,
    IPin* pDemuxOutpinAudio,
    IBaseFilter* pMux)
{
    assert(bool(m_pGraph));

    const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
    assert(bool(pBuilder));

    const bool bVerbose = m_cmdline.GetVerbose();

    int nConnections = 0;
    HRESULT hr;
    
    if (pDemuxOutpinVideo)
    {
        const IPinPtr pMuxInpinVideo(FindInpinVideo(pMux));
        assert(bool(pMuxInpinVideo));
        
        if (IsVP8(pDemuxOutpinVideo))
        {    
            hr = m_pGraph->ConnectDirect(pDemuxOutpinVideo, pMuxInpinVideo, 0);
            
            if (FAILED(hr))
            {
                wcout << "Unable to (directly) connect splitter video outpin to muxer video inpin.\n"
                      << hrtext(hr)
                      << L" (0x" << hex << hr << dec << L")"
                      << endl;
                              
                return 1;
            }
        }
        else
        {
            IBaseFilterPtr pCompressor;
            
            hr = pCompressor.CreateInstance(CLSID_VP8Encoder);
            
            if (FAILED(hr))
            {
                wcout << "Unable to create VP8 encoder filter instance.\n"
                
                      << hrtext(hr)
                      << L" (0x" << hex << hr << dec << L")"
                      << endl;
                              
                return 1;
            }
            
            assert(bool(pCompressor));
            
            _COM_SMARTPTR_TYPEDEF(IVP8Encoder, __uuidof(IVP8Encoder));

            const IVP8EncoderPtr pVP8(pCompressor);
            assert(bool(pVP8));
            
            hr = SetVP8Options(pVP8);
            
            if (FAILED(hr))
                return 1;
            
            hr = m_pGraph->AddFilter(pCompressor, L"vp8enc");
            assert(SUCCEEDED(hr));
            
            IPinPtr pVP8Inpin;
            
            hr = pCompressor->FindPin(L"input", &pVP8Inpin);
            assert(SUCCEEDED(hr));
            assert(bool(pVP8Inpin));
            
            hr = pBuilder->Connect(pDemuxOutpinVideo, pVP8Inpin);
            //hr = ConnectVideo(pDemuxOutpinVideo, pVP8Inpin);
            
            if (FAILED(hr))
            {
                wcout << "Unable to connect demux outpin to VP8 encoder filter inpin.\n"                
                      << hrtext(hr)
                      << L" (0x" << hex << hr << dec << L")"
                      << endl;
                              
                return 1;
            }
                
            IPinPtr pVP8Outpin;
            
            hr = pCompressor->FindPin(L"output", &pVP8Outpin);
            assert(SUCCEEDED(hr));
            assert(bool(pVP8Outpin));
            
            hr = m_pGraph->ConnectDirect(pVP8Outpin, pMuxInpinVideo, 0);
            
            if (FAILED(hr))
            {
                wcout << "Unable to connect VP8 encoder outpin to muxer video inpin.\n"
                      << hrtext(hr)
                      << L" (0x" << hex << hr << dec << L")"
                      << endl;
                              
                return 1;
            }
        }    
        
        ++nConnections;

        if (bVerbose)
            DumpConnectionMediaType(
                pMuxInpinVideo, 
                L"muxer video inpin", 
                &App::DumpVideoMediaType);
    }
    
    if (pDemuxOutpinAudio)
    {
        const IPinPtr pMuxInpinAudio(FindInpinAudio(pMux));
        assert(bool(pMuxInpinAudio));
        
        if (!ConnectAudio(pDemuxOutpinAudio, pMuxInpinAudio))
        {
            if (m_cmdline.GetRequireAudio())
            {
                wcout << "Source has audio, but no audio stream connected to muxer." << endl;            
                return 1;
            }
        }
        else
        {        
            ++nConnections;

            if (bVerbose)
                DumpConnectionMediaType(
                    pMuxInpinAudio, 
                    L"muxer audio inpin", 
                    &App::DumpAudioMediaType);
        }
    }
    
    if (nConnections <= 0)
    {
        wcout << L"No splitter outpins are connected to muxer inpins." << endl;
        return 1;
    }
        
    IBaseFilterPtr pWriter;
    
    hr = pWriter.CreateInstance(CLSID_FileWriter);
    
    if (FAILED(hr))
    {
        wcout << "Unable to create writer filter instance.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
              
        return 1;
    }
    
    assert(bool(pWriter));
    assert(GraphUtil::InpinCount(pWriter) == 1);
    
    m_pGraph->AddFilter(pWriter, L"writer");
    assert(SUCCEEDED(hr));
    
    const GraphUtil::IFileSinkFilterPtr pSink(pWriter);
    assert(bool(pSink));
    
    hr = pSink->SetFileName(m_cmdline.GetOutputFileName(), 0);
    
    if (FAILED(hr))
    {
        wcout << "Unable to set output filename of file writer filter.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
        
        return 1;
    }
    
    hr = GraphUtil::ConnectDirect(m_pGraph, pMux, pWriter, 0);
    
    if (FAILED(hr))
    {
        wcout << "Unable to connect muxer to writer.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
              
        return 1;
    }
    
    //TODO: this needs to be a dedicated switch, e.g. --dry-run or --build-only
    //if (bList)
    //    return 1;  //soft error
    
    return 0;  //success   
}



int App::RunMuxerGraph(IBaseFilter* pMux)
{
    assert(bool(m_pGraph));
    assert(pMux);
    
    const GraphUtil::IMediaSeekingPtr pSeek(pMux);
    assert(bool(pSeek));
    
    const GraphUtil::IMediaEventPtr pEvent(m_pGraph);
    assert(bool(pEvent));

    HANDLE h;
    
    HRESULT hr = pEvent->GetEventHandle((OAEVENT*)&h);
    assert(hr == S_OK);
    assert(h);
    
    //DWORD dw = WaitForSingleObject(h, 0);
    
    enum { nh = 2 };
    const HANDLE ha[nh] = { m_hQuit, h };
    
    const GraphUtil::IMediaControlPtr pControl(m_pGraph);
    assert(bool(pControl));
    
    hr = pControl->Run();
    
    if (FAILED(hr))
    {
        wcout << "Unable to run graph.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
              
        return 1;
    }
    
    //int n = 1;
    
    m_progress = 0;
    
    for (;;)
    {
        MSG msg;
        
        while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            assert(msg.message != WM_QUIT);
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        const DWORD dw = MsgWaitForMultipleObjects(
                            nh,
                            ha,
                            0,
                            100,  //timeout (ms)
                            QS_ALLINPUT);
                    
        if (dw == WAIT_TIMEOUT)
        {
            DisplayProgress(pSeek, false);
            continue;
        }

        assert(dw >= WAIT_OBJECT_0);
        assert(dw <= (WAIT_OBJECT_0 + nh));
        
        if (dw == WAIT_OBJECT_0)  //quit
        {
            //wcout << "CTRL+C detected" << endl;
            break;
        }
            
        if (dw == (WAIT_OBJECT_0 + nh))  //window message
            continue;
            
        //media event
        
        long code, param1, param2;
        
        HRESULT hr = pEvent->GetEvent(&code, &param1, &param2, 0);
        assert(hr == S_OK);
        
        hr = pEvent->FreeEventParams(code, param1, param2);
        assert(hr == S_OK);
        
        if (code == EC_USERABORT) //window closed
        {
            //wcout << "EC_USERABORT" << endl;
            break;
        }
            
        if (code == EC_COMPLETE)
        {
            //wcout << "EC_COMPLETE" << endl;
            break;
        }
            
        //if (code == (EC_USER + 0x100))  //done muxing
        //    break;
    }
    
    DisplayProgress(pSeek, true);

    if (!m_cmdline.ScriptMode())
        wcout << endl;
    
    hr = pControl->Stop();
    assert(SUCCEEDED(hr));
    
    return 0;
}


void App::DisplayProgress(IMediaSeeking* pSeek, bool last)
{
    assert(pSeek);
    
    //TODO: display this (or give option to) in HH:MM:SS.sss
    //TODO: attempt to query upstream filter for duration.
    
    __int64 curr;

    HRESULT hr = pSeek->GetCurrentPosition(&curr);

    if (FAILED(hr))
    {
#ifdef _DEBUG
        wcout << L"IMediaSeeking::GetCurrPos failed: "
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
#endif
        return;                    
    }
        
    assert(curr >= 0);
    
    double val = double(curr) / 10000000;

    wcout << std::fixed << std::setprecision(1);

    if (m_cmdline.ScriptMode())
        wcout << "TIME=" << val;
    else
        wcout << "\rtime[sec]=" << val;
        
    __int64 d;
    
    //TODO: it's not clear whether we're allowed to call
    //IMediaSeeking::GetDuration directly on the mkvmux
    //filter, or whether we must call the graph object.

#if 1
    hr = pSeek->GetDuration(&d);
#else
    const GraphUtil::IMediaSeekingPtr pSeek(m_pGraph);
    assert(bool(pSeek));
    
    hr = pSeek->GetDuration(&d);
#endif
    
    if (SUCCEEDED(hr))  //have duration
    {
        val = double(d) / 10000000;
        
        if (m_cmdline.ScriptMode())
            wcout << " DURATION=" << val;
        else
            wcout << L'/' << val;
    }
    
    if (m_cmdline.ScriptMode())
    {
        wcout << endl;
        return;
    }
    
    if (last)
        m_progress = 0;

    //TODO: there's probably a slicker way to do this:
    
    assert(m_progress >= 0);
    assert(m_progress <= 10);
    
    const int space = 10 - m_progress;
    
    for (int i = 0; i < m_progress; ++i)
        wcout << L'.';
        
    for (int i = 0; i < space; ++i)
        wcout << L' ';
        
    ++m_progress;
    
    if (m_progress > 10)
        m_progress = 0;
        
    wcout << flush;
}
            


void App::DumpVideoMediaType(const AM_MEDIA_TYPE& mt)
{
    wcout << "mt.subtype=";
    
    if (mt.subtype == MEDIASUBTYPE_MPEG2_VIDEO)
        wcout << "MPEG2_VIDEO";
    else
        wcout << GraphUtil::ToString(mt.subtype);
        
    wcout << L'\n';

    wcout << "mt.cbFormat=" << mt.cbFormat << L'\n';
    
    if (mt.formattype == FORMAT_VideoInfo)
    {
        wcout << "mt.format=VideoInfo\n";
        
        assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER));
        assert(mt.pbFormat);
        
        const VIDEOINFOHEADER& vih = (VIDEOINFOHEADER&)(*mt.pbFormat);
        DumpVideoInfoHeader(vih);
    }
    else if (mt.formattype == FORMAT_VideoInfo2)
    {
        wcout << "mt.format=VideoInfo2\n";        

        assert(mt.cbFormat >= sizeof(VIDEOINFOHEADER2));
        assert(mt.pbFormat);
        
        const VIDEOINFOHEADER2& vih = (VIDEOINFOHEADER2&)(*mt.pbFormat);
        DumpVideoInfoHeader2(vih);
    }
    else if (mt.formattype == FORMAT_MPEG2_VIDEO)
    {
        wcout << "mt.format=MPEG2_VIDEO\n";

        const MPEG2VIDEOINFO& mpg = (MPEG2VIDEOINFO&)(*mt.pbFormat);
        assert(mt.cbFormat >= SIZE_MPEG2VIDEOINFO(&mpg));

#if 0
        const DWORD cb = mpg.cbSequenceHeader;
        cb;
        assert(cb >= 4);
    
        const BYTE* const h = MPEG2_SEQUENCE_INFO(&mpg);
        h;

        //There doesn't seem to be agreement among filters about
        //whether this 4-byte header should exist.  These assertions 
        //fail with the Haali splitter, for example.

        assert(h[0] == 0);
        assert(h[1] == 0);
        assert(h[2] == 1);
        assert(h[3] == 0xB3);
#endif
        
        const VIDEOINFOHEADER2& vih = mpg.hdr;
        DumpVideoInfoHeader2(vih);
    }
    else
    {
        wcout << "mt.format=" << GraphUtil::ToString(mt.formattype) << L'\n';
    }
}


void App::DumpAudioMediaType(const AM_MEDIA_TYPE& mt)
{
    using namespace VorbisTypes;
    
    wcout << "mt.subtype=";
    
    if (mt.subtype == MEDIASUBTYPE_Vorbis2)
        wcout << "Vorbis2 (Matroska)";
        
    else if (mt.subtype == MEDIASUBTYPE_Vorbis)
        wcout << "Vorbis (Xiph)";
        
    else if (mt.subtype == MEDIASUBTYPE_DOLBY_AC3)
        wcout << "DOLBY_AC3";
        
    else
        wcout << GraphUtil::ToString(mt.subtype);
        
    wcout << L'\n';
    
    wcout << "mt.cbFormat=" << mt.cbFormat << L'\n';
    
    if (mt.formattype == FORMAT_WaveFormatEx)
    {
        wcout << "mt.format=WaveFormatEx\n";
        
        assert(mt.cbFormat >= sizeof(WAVEFORMATEX));
        assert(mt.pbFormat);
        
        const WAVEFORMATEX& wfx = (WAVEFORMATEX&)(*mt.pbFormat);
        
        wcout << "wfx.wFormatTag=0x"
              << setw(4)
              << setfill(L'0')
              << hex
              << wfx.wFormatTag
              << dec
              << L'\n';
              
        wcout << "wfx.nChannels="
              << wfx.nChannels
              << L'\n';
              
        wcout << "wfx.nSamplesPerSec="
              << wfx.nSamplesPerSec
              << L'\n';
              
        wcout << "wfx.nAvgBytesPerSec="
              << wfx.nAvgBytesPerSec
              << L'\n';
              
        wcout << "wfx.nBlockAlign="
              << wfx.nBlockAlign
              << L'\n';
              
        wcout << "wfx.wBitsPerSample="
              << wfx.wBitsPerSample
              << L'\n';
              
        wcout << "wfx.cbSize="
              << wfx.cbSize
              << L'\n';
    }
    else if (mt.formattype == FORMAT_Vorbis2)
    {
        wcout << "mt.format=Vorbis2 (Matroska)\n";
        
        assert(mt.cbFormat >= sizeof(VORBISFORMAT2));
        assert(mt.pbFormat);
        
        const VORBISFORMAT2& fmt = (VORBISFORMAT2&)(*mt.pbFormat);
        
        wcout << "fmt.channels="
              << fmt.channels
              << L'\n';
              
        wcout << "fmt.samplesPerSec="
              << fmt.samplesPerSec
              << L'\n';
              
        wcout << "fmt.bitsPerSample="
              << fmt.bitsPerSample
              << L'\n';
              
        wcout << "fmt.headerSize[0=\"ident\"]="
              << fmt.headerSize[0]
              << L'\n';
              
        wcout << "fmt.headerSize[1=\"comment\"]="
              << fmt.headerSize[1]
              << L'\n';
              
        wcout << "fmt.headerSize[2=\"setup\"]="
              << fmt.headerSize[2]
              << L'\n';
    }
    else if (mt.formattype == FORMAT_Vorbis)
    {
        wcout << "mt.format=Vorbis (Xiph)\n";
        
        assert(mt.cbFormat >= sizeof(VORBISFORMAT));
        assert(mt.pbFormat);
        
        const VORBISFORMAT& fmt = (VORBISFORMAT&)(*mt.pbFormat);
        
        wcout << "fmt.vorbisVersion="
              << fmt.vorbisVersion
              << L'\n';

        wcout << "fmt.samplesPerSec="
              << fmt.samplesPerSec
              << L'\n';
              
        wcout << "fmt.minBitsPerSec="
              << fmt.minBitsPerSec 
              << L'\n';
              
        wcout << "fmt.avgBitsPerSec="
              << fmt.avgBitsPerSec
              << L'\n';
              
        wcout << "fmt.maxBitsPerSec="
              << fmt.maxBitsPerSec
              << L'\n';
              
        wcout << "fmt.channels="
              << fmt.numChannels
              << L'\n';
    }
    else
    {
        wcout << "mt.format=" << GraphUtil::ToString(mt.formattype) << L'\n';
    }
    
    wcout << flush;
}


void App::DumpVideoInfoHeader(const VIDEOINFOHEADER& vih)
{
    //TODO: print vih stuff
    
    const BITMAPINFOHEADER& bmih = vih.bmiHeader;
    DumpBitMapInfoHeader(bmih);
}


void App::DumpVideoInfoHeader2(const VIDEOINFOHEADER2& vih)
{
    //TODO: print vih2 stuff

    const BITMAPINFOHEADER& bmih = vih.bmiHeader;
    DumpBitMapInfoHeader(bmih);
}


void App::DumpBitMapInfoHeader(const BITMAPINFOHEADER& bmih)
{
    wcout << L"bmih.biSize=" << bmih.biSize << L'\n'
          << L"bmih.biWidth=" << bmih.biWidth << L'\n'
          << L"bmih.biHeight=" << bmih.biHeight << L'\n'
          << L"bmih.biPlanes=" << bmih.biPlanes << L'\n'
          << L"bmih.biBitCount=" << bmih.biBitCount << L'\n'
          << L"bmih.biCompression=0x"
          << setfill(L'0')
          << setw(8)
          << bmih.biCompression;
    
    switch (bmih.biCompression)
    {
        case BI_RGB:
            wcout << L" [RGB]\n";
            break;
            
        case BI_RLE8:
            wcout << L" [RLE8]\n";
            break;
            
        case BI_RLE4:
            wcout << L" [RLE4]\n";
            break;
            
        case BI_BITFIELDS:
            wcout << L" [BITFIELDS]\n";
            break;
            
        case BI_JPEG:
            wcout << L" [JPEG]\n";
            break;
            
        case BI_PNG:
            wcout << L" [PNG]\n";
            break;
            
        default:
            wcout << L" [" << std::flush;
            std::cout.write((const char*)&bmih.biCompression, 4);
            std::cout << std::flush;
            wcout << L"]\n";            
            break;
    }
}


GraphUtil::IBaseFilterPtr App::AddDemuxFilter(IBaseFilter* pReader) const
{
    assert(bool(m_pGraph));
    assert(pReader);
    
    if (GraphUtil::OutpinCount(pReader) > 1)  //assume source, not merely reader
        return pReader;
        
    const IPinPtr pPin = GraphUtil::FindOutpin(pReader);
    assert(bool(pPin));
    
    GraphUtil::IEnumMediaTypesPtr e;
    
    HRESULT hr = pPin->EnumMediaTypes(&e);
    
    if (hr != S_OK)
    {
        wcout << "Unable to enumerate source filter's media types.\n"
              << hrtext(hr)
              << L" (0x" << hex << hr << dec << L")"
              << endl;
   
        return 0;
    }
    
    assert(bool(e));
    
    for (;;)
    {
        AM_MEDIA_TYPE* pmt;
        
        hr = e->Next(1, &pmt, 0);
        
        if (hr != S_OK)
        {
            wcout << "No acceptable media types found on source filter's outpins." << endl;
            return 0;
        }
        
        assert(pmt);

        const GUID g = pmt->majortype;
        
        MediaTypeUtil::Free(pmt);
        
        if (g == MEDIATYPE_Stream)
            return FindDemuxFilter(pReader);
            
        if (g == MEDIATYPE_Video)
            return pReader;
            
        if (g == MEDIATYPE_Audio)
            return pReader;
            
        if (g == MEDIATYPE_NULL)  //assume stream
            return FindDemuxFilter(pReader);
    }
}


GraphUtil::IBaseFilterPtr App::FindDemuxFilter(IBaseFilter* pReader) const
{
    assert(bool(m_pGraph));
    assert(pReader);

    const IPinPtr pOutputPin = GraphUtil::FindOutpin(pReader);
    assert(bool(pOutputPin));

    IBaseFilterPtr f;
    
    HRESULT hr = f.CreateInstance(WebmTypes::CLSID_WebmSplit);
    
    if (FAILED(hr))
        return EnumDemuxFilters(pOutputPin);
        
    assert(bool(f));
    
    const IPinPtr pInputPin = GraphUtil::FindInpin(f);
    assert(bool(pInputPin));
    
    hr = m_pGraph->AddFilter(f, L"demux");
    assert(SUCCEEDED(hr));
    
    hr = m_pGraph->ConnectDirect(pOutputPin, pInputPin, 0);
    
    if (SUCCEEDED(hr))
    {
        assert(GraphUtil::OutpinCount(f) > 0);
        return f;
    }

    hr = m_pGraph->RemoveFilter(f);
    assert(SUCCEEDED(hr));
    
    return EnumDemuxFilters(pOutputPin);
}


GraphUtil::IBaseFilterPtr App::EnumDemuxFilters(IPin* pOutputPin) const
{    
    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));
    
    IEnumMonikerPtr e;
    
    enum { cInputTypes = 1 };
    const GUID inputTypes[2 * cInputTypes] =
    {
        MEDIATYPE_Stream, MEDIASUBTYPE_NULL
    };
    
    enum { cOutputTypes = 2 };
    const GUID outputTypes[2 * cOutputTypes] =
    {
        MEDIATYPE_Video, MEDIASUBTYPE_NULL,
        MEDIATYPE_Audio, MEDIASUBTYPE_NULL
    };
    
    HRESULT hr = pMapper->EnumMatchingFilters(
                    &e, 
                    0,  //flags
                    FALSE,  //no, we don't require exact match
                    MERIT_DO_NOT_USE + 1,
                    TRUE,   //input needed
                    cInputTypes,
                    inputTypes,
                    0,  //input medium
                    0,  //input category
                    FALSE,  //bRender
                    FALSE,  //no output needed
                    cOutputTypes,
                    outputTypes,
                    0,  //output medium
                    0); //output category
                    
    if (FAILED(hr))
    {
        wcout << "Unable to enumerate filters that can demux the source filter." << endl;
        return 0; 
    }
                    
    for (;;)
    {
        IMonikerPtr m;
        
        hr = e->Next(1, &m, 0);
        
        if (hr != S_OK)
        {
            wcout << "No filters found that can demux the source filter." << endl;
            return 0;
        }
        
        IBaseFilterPtr f;
        
        hr = m->BindToObject(0, 0, __uuidof(IBaseFilter), (void**)&f);
        
        if (FAILED(hr))
            continue;
            
        assert(bool(f));
        
        const IPinPtr pInputPin = GraphUtil::FindInpin(f);
        
        if (!bool(pInputPin))
            continue;
            
        hr = m_pGraph->AddFilter(f, L"demux");
        assert(SUCCEEDED(hr));
        
        hr = m_pGraph->ConnectDirect(pOutputPin, pInputPin, 0);
        
        if (FAILED(hr))
        {
            hr = m_pGraph->RemoveFilter(f);
            assert(SUCCEEDED(hr));
            
            continue;
        }

        IPinPtr pPin = GraphUtil::FindOutpinVideo(f);
        
        if (!bool(pPin))
            pPin = GraphUtil::FindOutpinAudio(f);
            
        if (!bool(pPin))
        {
            //TODO: do we need to disconnect here?
            
            hr = m_pGraph->RemoveFilter(f);
            assert(SUCCEEDED(hr));
            
            continue;
        }
        
        return f;
    }
}


bool App::IsVP8(IPin* pPin)
{
    assert(pPin);
    
    GraphUtil::IEnumMediaTypesPtr e;
    
    HRESULT hr = pPin->EnumMediaTypes(&e);
    
    if (FAILED(hr))
        return false;  //?
        
    for (;;)
    {
        AM_MEDIA_TYPE* pmt;
        
        hr = e->Next(1, &pmt, 0);
        
        if (hr != S_OK)
            return false;  //?
            
        const GUID major = pmt->majortype;
        const GUID minor = pmt->subtype;
        
        MediaTypeUtil::Free(pmt);
        pmt = 0;

        if (major != MEDIATYPE_Video)
            continue;
            
        if (minor != WebmTypes::MEDIASUBTYPE_VP80)
            continue;
            
        return true;        
    }
}


GUID App::GetSubtype(IPin* pPin)
{
    assert(pPin);
    
    GraphUtil::IEnumMediaTypesPtr e;
    
    HRESULT hr = pPin->EnumMediaTypes(&e);
    
    if (FAILED(hr))
        return GUID_NULL;
        
    for (;;)
    {
        AM_MEDIA_TYPE* pmt;
        
        hr = e->Next(1, &pmt, 0);
        
        if (hr != S_OK)
            return GUID_NULL;
            
        const GUID major = pmt->majortype;
        major;
        
        const GUID minor = pmt->subtype;
        
        MediaTypeUtil::Free(pmt);
        pmt = 0;
        
        if (minor != GUID_NULL)
            return minor;
    }
}


#if 0
bool App::ConnectVideo(IPin* pDemuxOutpin, IPin* pVP8Inpin) const
{
    const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
    assert(bool(pBuilder));
    
    HRESULT hr = pBuilder->Connect(pDemuxOutpin, pVP8Inpin);
    
    if (SUCCEEDED(hr))
        return true;  //connected
        
    hr = ConnectVideoConverter(pDemuxOutpin, pVP8Inpin);
    
    if (SUCCEEDED(hr))
        return true;
        
    return false;
}


HRESULT App::ConnectVideoConverter(IPin* pDemuxOutpin, IPin* pVP8Inpin) const
{
    typedef std::vector<GUID> guids_t;
    guids_t input_guids;
    
    {
        GraphUtil::IEnumMediaTypesPtr e;
        
        HRESULT hr = pDemuxOutpin->EnumMediaTypes(&e);
        
        if (FAILED(hr))
            return hr;
            
        for (;;)
        {
            AM_MEDIA_TYPE* pmt;
            
            hr = e->Next(1, &pmt, 0);
            
            if (hr != S_OK)
                break;
                
            assert(pmt);
            assert(pmt->majortype == MEDIATYPE_Video);
            assert(pmt->subtype != MEDIASUBTYPE_NULL);  //?
            
            input_guids.push_back(pmt->majortype);
            input_guids.push_back(pmt->subtype);
            
            MediaTypeUtil::Free(pmt);
            pmt = 0;
        }
    
        if (input_guids.empty())  //weird
            return E_FAIL;
    }    

    const guids_t::size_type cInputTypes_ = input_guids.size() / 2;        
    const DWORD cInputTypes = static_cast<DWORD>(cInputTypes_);
    
    const GUID* const inputTypes = &input_guids[0];
    
    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));
    
    //TODO: for now, hard-code the VP8 input pin media types.
    //At some point we should query for that filter's 
    //preferred input pin media types.
    
    enum { cOutputTypes = 4 };
    const GUID outputTypes[2 * cOutputTypes] =
    {
        MEDIATYPE_Video, MEDIASUBTYPE_YV12,
        MEDIATYPE_Video, WebmTypes::MEDIASUBTYPE_I420,
        MEDIATYPE_Video, MEDIASUBTYPE_YUY2,
        MEDIATYPE_Video, MEDIASUBTYPE_YUYV
    };
    
    IEnumMonikerPtr e;
        
    HRESULT hr = pMapper->EnumMatchingFilters(
                    &e, 
                    0,  //flags (reserved -- must be 0)
                    FALSE, //TRUE,  //yes, require exact match  //?
                    MERIT_DO_NOT_USE,
                    TRUE,   //input needed
                    cInputTypes,
                    inputTypes,
                    0,  //input medium
                    0,  //input category
                    FALSE,  //bRender
                    TRUE,  //yes, output needed
                    cOutputTypes,
                    outputTypes,
                    0,  //output medium
                    0); //output category
                    
    if (FAILED(hr))
        return hr;
        
    const GraphUtil::IGraphConfigPtr pConfig(m_pGraph);
    assert(bool(pConfig));
    
    const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
    assert(bool(pBuilder));
        
    for (;;)
    {
        IMonikerPtr m;
        
        hr = e->Next(1, &m, 0);
        
        if (hr != S_OK)
            return VFW_E_NOT_CONNECTED;
            
        assert(bool(m));
        
        IBaseFilterPtr f;  //video converter filter
        
        hr = m->BindToObject(0, 0, __uuidof(IBaseFilter), (void**)&f);
        
        if (FAILED(hr))
            continue;
            
        assert(bool(f));
        
#if 0
        hr = pConfig->AddFilterToCache(f);
        assert(SUCCEEDED(hr));
        
        const HRESULT hrConnect = pBuilder->Connect(pDemuxOutpin, pMuxInpin);
        
        hr = pConfig->RemoveFilterFromCache(f);
        assert(SUCCEEDED(hr));
        
        if (SUCCEEDED(hrConnect))
            return S_OK;
#else
        hr = m_pGraph->AddFilter(f, L"video converter");
        assert(SUCCEEDED(hr));
        
        const IPinPtr pConverterInpin = GraphUtil::FindInpin(f);
        assert(bool(pConverterInpin));
        
        hr = m_pGraph->ConnectDirect(pDemuxOutpin, pConverterInpin, 0);
        
        if (FAILED(hr))       
            hr = pBuilder->Connect(pDemuxOutpin, pConverterInpin);
        
        if (FAILED(hr))
        {
            hr = m_pGraph->RemoveFilter(f);
            assert(SUCCEEDED(hr));
            
            continue;
        }
        
        const IPinPtr pConverterOutpin = GraphUtil::FindOutpin(f);
        assert(bool(pConverterOutpin));
        
        hr = m_pGraph->ConnectDirect(pConverterOutpin, pVP8Inpin, 0);
        
        if (SUCCEEDED(hr))
            return S_OK;
            
        hr = m_pGraph->RemoveFilter(f);
        assert(SUCCEEDED(hr));
#endif
    }  //end for
}
#endif



bool App::ConnectAudio(IPin* pDemuxOutpin, IPin* pMuxInpin) const
{
    HRESULT hr = m_pGraph->ConnectDirect(pDemuxOutpin, pMuxInpin, 0);
    
    if (SUCCEEDED(hr))
        return true;  //connected
        
    const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
    assert(bool(pBuilder));
    
    hr = pBuilder->Connect(pDemuxOutpin, pMuxInpin);
    
    if (SUCCEEDED(hr))
        return true;

    hr = ConnectVorbisEncoder(pDemuxOutpin, pMuxInpin);
    
    if (SUCCEEDED(hr))
        return true;  //connected
        
    wcout << "Unable to connect audio stream to muxer." << endl;
    return false;
}


HRESULT App::ConnectVorbisEncoder(IPin* pDemuxOutpin, IPin* pMuxInpin) const
{
    const GraphUtil::IFilterMapper2Ptr pMapper(CLSID_FilterMapper2);
    assert(bool(pMapper));
    
    IEnumMonikerPtr e;
    
    enum { cInputTypes = 1 };
    const GUID inputTypes[2 * cInputTypes] =
    {
        MEDIATYPE_Audio, MEDIASUBTYPE_PCM
    };
    
    enum { cOutputTypes = 2 };
    const GUID outputTypes[2 * cOutputTypes] =
    {
        MEDIATYPE_Audio, VorbisTypes::MEDIASUBTYPE_Vorbis2,
        MEDIATYPE_Audio, VorbisTypes::MEDIASUBTYPE_Vorbis
    };
    
    HRESULT hr = pMapper->EnumMatchingFilters(
                    &e, 
                    0,  //flags (reserved -- must be 0)
                    TRUE,  //yes, require exact match
                    MERIT_DO_NOT_USE,
                    TRUE,   //input needed
                    cInputTypes,
                    inputTypes,
                    0,  //input medium
                    0,  //input category
                    FALSE,  //bRender
                    TRUE,  //yes, output needed
                    cOutputTypes,
                    outputTypes,
                    0,  //output medium
                    0); //output category
                    
    if (FAILED(hr))
        return hr;
        
    const GraphUtil::IGraphConfigPtr pConfig(m_pGraph);
    assert(bool(pConfig));
    
    const GraphUtil::IGraphBuilderPtr pBuilder(m_pGraph);
    assert(bool(pBuilder));
        
    for (;;)
    {
        IMonikerPtr m;
        
        hr = e->Next(1, &m, 0);
        
        if (hr != S_OK)
            return VFW_E_NOT_CONNECTED;
            
        assert(bool(m));
        
        IBaseFilterPtr f;
        
        hr = m->BindToObject(0, 0, __uuidof(IBaseFilter), (void**)&f);
        
        if (FAILED(hr))
            continue;
            
        assert(bool(f));
        
#if 0
        hr = pConfig->AddFilterToCache(f);
        assert(SUCCEEDED(hr));
        
        const HRESULT hrConnect = pBuilder->Connect(pDemuxOutpin, pMuxInpin);
        
        hr = pConfig->RemoveFilterFromCache(f);
        assert(SUCCEEDED(hr));
        
        if (SUCCEEDED(hrConnect))
            return S_OK;
#else
        hr = m_pGraph->AddFilter(f, L"vorbis encoder");
        assert(SUCCEEDED(hr));
        
        const IPinPtr pEncoderInpin = GraphUtil::FindInpin(f);
        assert(bool(pEncoderInpin));
        
        hr = pBuilder->Connect(pDemuxOutpin, pEncoderInpin);
        
        if (FAILED(hr))
        {
            hr = m_pGraph->RemoveFilter(f);
            assert(SUCCEEDED(hr));
            
            continue;
        }
        
        const IPinPtr pEncoderOutpin = GraphUtil::FindOutpin(f);
        assert(bool(pEncoderOutpin));
        
        hr = m_pGraph->ConnectDirect(pEncoderOutpin, pMuxInpin, 0);
        
        if (SUCCEEDED(hr))
            return S_OK;
            
        hr = m_pGraph->RemoveFilter(f);
        assert(SUCCEEDED(hr));
#endif
    }
}



void App::DumpPreferredMediaTypes(
    IPin* pPin,
    const wchar_t* id,
    void (*pfn)(const AM_MEDIA_TYPE&))
{
    GraphUtil::IEnumMediaTypesPtr e;
    
    HRESULT hr = pPin->EnumMediaTypes(&e);
    
    if (FAILED(hr))
    {
        wcout << L"Unable to enumerate preferred media types for " << id << "." << endl;
        return;
    }

    wcout << "Preferred media types for " << id << ":\n";

    for (;;)
    {
        AM_MEDIA_TYPE* pmt;
        
        hr = e->Next(1, &pmt, 0);
        
        if (hr != S_OK)
            break;
            
        (*pfn)(*pmt);
        wcout << L'\n';
        MediaTypeUtil::Free(pmt);
    }
    
    wcout << endl;
}


void App::DumpConnectionMediaType(
    IPin* pPin, 
    const wchar_t* id,
    void (*pfn)(const AM_MEDIA_TYPE&))
{
    AM_MEDIA_TYPE mt;
    
    const HRESULT hr = pPin->ConnectionMediaType(&mt);
    
    if (FAILED(hr))
    {
        wcout << L"No connection media type for " << id << "." << endl;
        return;
    }
    
    wcout << "Media type for " << id << ":\n";
    (*pfn)(mt);
    wcout << endl;
    MediaTypeUtil::Destroy(mt);
}


HRESULT App::SetVP8Options(IVP8Encoder* pVP8) const
{
    assert(pVP8);
    
    const int deadline = m_cmdline.GetDeadline();

    if (deadline >= 0)
    {            
        const HRESULT hr = pVP8->SetDeadline(deadline);

        if (FAILED(hr))
        {        
            wcout << "Unable to set VP8 encoder deadline.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int target_bitrate = m_cmdline.GetTargetBitrate();
    
    if (target_bitrate >= 0)
    {
        const HRESULT hr = pVP8->SetTargetBitrate(target_bitrate);
        
        if (FAILED(hr))
        {        
            wcout << "Unable to set VP8 encoder target bitrate.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int min_quantizer = m_cmdline.GetMinQuantizer();
    
    if (min_quantizer >= 0)
    {
        const HRESULT hr = pVP8->SetMinQuantizer(min_quantizer);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder min quantizer.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int max_quantizer = m_cmdline.GetMaxQuantizer();
    
    if (max_quantizer >= 0)
    {
        const HRESULT hr = pVP8->SetMaxQuantizer(max_quantizer);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder max quantizer.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    const int undershoot_pct = m_cmdline.GetUndershootPct();
    
    if (undershoot_pct >= 0)
    {
        const HRESULT hr = pVP8->SetUndershootPct(undershoot_pct);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder undershoot pct.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
        
    const int overshoot_pct = m_cmdline.GetOvershootPct();
    
    if (overshoot_pct >= 0)
    {
        const HRESULT hr = pVP8->SetOvershootPct(overshoot_pct);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder overshoot pct.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    const int decoder_buffer_size = m_cmdline.GetDecoderBufferSize();
    
    if (decoder_buffer_size >= 0)
    {
        const HRESULT hr = pVP8->SetDecoderBufferSize(decoder_buffer_size);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder decoder buffer size.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
        
    const int decoder_buffer_initial_size = m_cmdline.GetDecoderBufferInitialSize();
    
    if (decoder_buffer_initial_size >= 0)
    {
        const HRESULT hr = pVP8->SetDecoderBufferInitialSize(decoder_buffer_initial_size);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder decoder buffer initial size.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    const int decoder_buffer_optimal_size = m_cmdline.GetDecoderBufferOptimalSize();
    
    if (decoder_buffer_optimal_size >= 0)
    {
        const HRESULT hr = pVP8->SetDecoderBufferOptimalSize(decoder_buffer_optimal_size);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder decoder buffer optimal size.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int keyframe_mode = m_cmdline.GetKeyframeMode();
    
    if (keyframe_mode >= kKeyframeModeDefault)
    {
        const VP8KeyframeMode m = static_cast<VP8KeyframeMode>(keyframe_mode);
        const HRESULT hr = pVP8->SetKeyframeMode(m);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder keyframe mode.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
        
    const int keyframe_min_interval = m_cmdline.GetKeyframeMinInterval();
    
    if (keyframe_min_interval >= 0)
    {
        const HRESULT hr = pVP8->SetKeyframeMinInterval(keyframe_min_interval);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder keyframe min interval.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
        
    const int keyframe_max_interval = m_cmdline.GetKeyframeMaxInterval();
    
    if (keyframe_max_interval >= 0)
    {
        const HRESULT hr = pVP8->SetKeyframeMaxInterval(keyframe_max_interval);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder keyframe max interval.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
                        
    const int thread_count = m_cmdline.GetThreadCount();
    
    if (thread_count >= 0)
    {
        const HRESULT hr = pVP8->SetThreadCount(thread_count);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder thread count.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int error_resilient = m_cmdline.GetErrorResilient();
    
    if (error_resilient >= 0)
    {
        const boolean b = static_cast<boolean>(error_resilient);
        const HRESULT hr = pVP8->SetErrorResilient(b);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder error resilient value.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
        
    const int end_usage = m_cmdline.GetEndUsage();
    
    if (end_usage >= 0)
    {
        const VP8EndUsage val = static_cast<VP8EndUsage>(end_usage);        
        const HRESULT hr = pVP8->SetEndUsage(val);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder end usage.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int lag_in_frames = m_cmdline.GetLagInFrames();
    
    if (lag_in_frames >= 0)
    {
        const HRESULT hr = pVP8->SetLagInFrames(lag_in_frames);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder lag in frames.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int token_partitions = m_cmdline.GetTokenPartitions();
    
    if (token_partitions >= 0)
    {
        const HRESULT hr = pVP8->SetTokenPartitions(token_partitions);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder token partitions.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }
    
    const int dropframe_thresh = m_cmdline.GetDropframeThreshold();
    
    if (dropframe_thresh >= 0)
    {
        const HRESULT hr = pVP8->SetDropframeThreshold(dropframe_thresh);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder dropframe threshold.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    const int resize_allowed = m_cmdline.GetResizeAllowed();
    
    if (resize_allowed >= 0)
    {
        const HRESULT hr = pVP8->SetResizeAllowed(resize_allowed);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder resize allowed.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    const int resize_up_thresh = m_cmdline.GetResizeUpThreshold();
    
    if (resize_up_thresh >= 0)
    {
        const HRESULT hr = pVP8->SetResizeUpThreshold(resize_up_thresh);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder resize up threshold.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    const int resize_down_thresh = m_cmdline.GetResizeDownThreshold();
    
    if (resize_down_thresh >= 0)
    {
        const HRESULT hr = pVP8->SetResizeDownThreshold(resize_down_thresh);
        
        if (FAILED(hr))
        {
            wcout << "Unable to set VP8 encoder resize down threshold.\n"
                  << hrtext(hr)
                  << L" (0x" << hex << hr << dec << L")"
                  << endl;
                  
            return hr;
        }
    }

    return S_OK;    
}
