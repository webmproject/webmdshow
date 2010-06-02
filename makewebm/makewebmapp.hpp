// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license and patent
// grant that can be found in the LICENSE file in the root of the source
// tree. All contributing project authors may be found in the AUTHORS
// file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include <control.h>
#include <uuids.h>
#include "graphutil.hpp"
#include "makewebmcmdline.hpp"
#include <amvideo.h>
#include <dvdmedia.h>
#include <list>

interface IVP8Encoder;

class App
{
    App(const App&);
    App& operator=(const App&);
    
public:

    explicit App(HANDLE);
    int operator()(int, wchar_t*[]);
    
private:

    const HANDLE m_hQuit;
    CmdLine m_cmdline;    
    GraphUtil::IFilterGraphPtr m_pGraph;
    GraphUtil::IMediaSeekingPtr m_pSeek;
    int m_progress;
    
    int CreateGraph();
    void DestroyGraph();
    int RunGraph();

    static bool IsVP8(IPin*);
    static GUID GetSubtype(IPin*);

    GraphUtil::IBaseFilterPtr AddDemuxFilter(IBaseFilter*) const;
    GraphUtil::IBaseFilterPtr FindDemuxFilter(IBaseFilter*) const;
    GraphUtil::IBaseFilterPtr EnumDemuxFilters(IPin*) const;

#if 0    
    bool ConnectVideo(IPin*, IPin*) const;
    HRESULT ConnectVideoConverter(IPin*, IPin*) const;
#endif

    bool ConnectAudio(IPin*, IPin*) const;        
    HRESULT ConnectVorbisEncoder(IPin*, IPin*) const;

    static void DumpPreferredMediaTypes(IPin*, const wchar_t*, void (*)(const AM_MEDIA_TYPE&));
    static void DumpConnectionMediaType(IPin*, const wchar_t*, void (*)(const AM_MEDIA_TYPE&));
    void DisplayProgress(bool);

    static void DumpVideoMediaType(const AM_MEDIA_TYPE&);
    static void DumpVideoInfoHeader(const VIDEOINFOHEADER&);
    static void DumpVideoInfoHeader2(const VIDEOINFOHEADER2&);
    static void DumpBitMapInfoHeader(const BITMAPINFOHEADER&);
    static void DumpAudioMediaType(const AM_MEDIA_TYPE&);
    
    HRESULT SetVP8Options(IVP8Encoder*) const;

};
