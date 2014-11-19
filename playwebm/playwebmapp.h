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
#include "playwebmcmdline.hpp"

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

    int BuildGraph();
    int RunGraph();
    void DestroyGraph();
    static void RenderFailed(IPin*, HRESULT);

};
