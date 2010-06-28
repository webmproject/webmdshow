// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <strmif.h>
#include <comdef.h>
#include <control.h>
#include <uuids.h>
#include "graphutil.hpp"
#include "makewebmcmdline.hpp"
#include "memfile.hpp"
#include <amvideo.h>
#include <dvdmedia.h>
#include <list>

interface IVP8Encoder;

class App
{
    App(const App&);
    App& operator=(const App&);

public:

    App();
    int operator()(int, wchar_t*[]);

private:

    CmdLine m_cmdline;
    GraphUtil::IFilterGraphPtr m_pGraph;

    int LoadGraph();
    int SaveGraph();

    int m_progress;

    int CreateMuxerGraph(
            bool bTwoPass,
            IPin* pDemuxVideo,
            IPin* pDemuxAudio,
            IBaseFilter** pMux);

    int CreateFirstPassGraph(IPin* pDemuxVideo, IPin** pEncoderOutpin);

    int RunGraph(IMediaSeeking* pSeek);

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

    static void DumpPreferredMediaTypes(
                    IPin*,
                    const wchar_t*,
                    void (*)(const AM_MEDIA_TYPE&));

    static void DumpConnectionMediaType(
                    IPin*,
                    const wchar_t*,
                    void (*)(const AM_MEDIA_TYPE&));

    void DisplayProgress(IMediaSeeking*, bool);

    static void DumpVideoMediaType(const AM_MEDIA_TYPE&);
    static void DumpVideoInfoHeader(const VIDEOINFOHEADER&);
    static void DumpVideoInfoHeader2(const VIDEOINFOHEADER2&);
    static void DumpBitMapInfoHeader(const BITMAPINFOHEADER&);
    static void DumpAudioMediaType(const AM_MEDIA_TYPE&);
    static double GetFramerate(const AM_MEDIA_TYPE&);

    HRESULT SetVP8Options(IVP8Encoder*, const AM_MEDIA_TYPE*) const;

    const wchar_t* GetStatsFileName();
    std::wstring m_stats_filename;
    MemFile m_stats_file;

};
