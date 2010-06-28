// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <objbase.h>
#include "makewebmcmdline.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <sstream>
#include <windows.h>
#include <uuids.h>
#include "versionhandling.hpp"
#include "vp8encoderidl.h"
using std::wcout;
using std::endl;
using std::boolalpha;
using std::wstring;
using std::dec;

CmdLine::CmdLine() :
    m_input(0),
    m_output(0),
    m_usage(false),
    m_list(false),
    m_version(false),
    m_script(false),
    m_verbose(false),
    m_require_audio(false),
    m_deadline(-1),
    m_target_bitrate(-1),
    m_min_quantizer(-1),
    m_max_quantizer(-1),
    m_undershoot_pct(-1),
    m_overshoot_pct(-1),
    m_decoder_buffer_size(-1),
    m_decoder_buffer_initial_size(-1),
    m_decoder_buffer_optimal_size(-1),
    m_keyframe_frequency(-1),
    m_keyframe_mode(-2),
    m_keyframe_min_interval(-1),
    m_keyframe_max_interval(-1),
    m_thread_count(-1),
    m_error_resilient(-1),
    m_end_usage(-1),
    m_lag_in_frames(-1),
    m_token_partitions(-1),
    m_two_pass(-1),
    m_dropframe_thresh(-1),
    m_resize_allowed(-1),
    m_resize_up_thresh(-1),
    m_resize_down_thresh(-1),
    m_two_pass_vbr_bias_pct(-1),
    m_two_pass_vbr_minsection_pct(-1),
    m_two_pass_vbr_maxsection_pct(-1),
    m_save_graph_file_ptr(0)
{
}


int CmdLine::Parse(int argc, wchar_t* argv[])
{
    m_argv = argv;

    if (argc <= 1)  //program-name only
    {
        PrintUsage();
        return 1;  //soft error
    }

    //more ideas:
    //require video outpin
    //require audio outpin
    //make video connection optional
    //make audio connection optional
    //verbose mode
    //terse mode
    //quiet
    //very quite
    //warnings vs. errors
    //treat warnings as errors

    wchar_t** const argv_end = argv + argc;
    assert(*argv_end == 0);

    --argc;  //throw away program name
    assert(argc >= 1);

    wchar_t** i = argv + 1;
    assert(i < argv_end);

    for (;;)
    {
        const int n = Parse(i);

        if (n < 0)  //error
            return n;

        if (n == 0)  //arg, not switch
        {
            --argc;
            assert(argc >= 0);

            ++i;
            assert(i <= argv_end);
        }
        else  //switch
        {
            argc -= n;
            assert(argc >= 0);
            assert((i + n) <= argv_end);

            std::rotate(i, i + n, argv_end + 1);
        }

        if (argc <= 0)
            break;
    }

    assert(*i == 0);

    wchar_t** const j = i;  //args end
    i = argv + 1;           //args begin

    if (m_usage)
    {
        PrintUsage();
        return 1;  //soft error
    }

    if (m_version)
    {
        PrintVersion();
        return 1;  //soft error
    }

    if (m_input == 0)  //not specified as switch
    {
        if (i >= j)  //no args remain
        {
            if (m_list)
                ListArgs();
            else
                wcout << "No input filename specified." << endl;

            return 1;  //error
        }

        m_input = *i++;
        assert(m_input);
    }

    if (m_output == 0)  //not specified as switch
    {
#if 0
        if (i >= j)  //no args remain
        {
            if (m_list)
                ListArgs();
            else
                wcout << "No output filename specified." << endl;

            return 1;  //error
        }

        m_output = *i++;
        assert(m_output);
#else
        if (i >= j)  //no args remain
            SynthesizeOutput();
        else
            m_output = *i++;

        assert(m_output);
#endif
    }

    if (m_save_graph_file_ptr)  //had a request
    {
        if (m_two_pass >= 1)  //two-pass requested
        {
            wcout << L"Unable to save GraphEdit storage file"
                  << L" in two-pass mode."
                  << endl;

            return 1;
        }

        if (wcslen(m_save_graph_file_ptr) == 0)  //request, but no filename
            SynthesizeSaveGraph();
    }

    if (i < j)  //not all args consumed
    {
        if (m_list)
            ListArgs();
        else
            wcout << L"Too many command-line arguments." << endl;

        return 1;
    }

    if (m_list)
    {
        ListArgs();

        //TODO: we should have a dedicated arg to say, "build the graph,
        //but don't run it", something like --build-only or --dry-run
        //if (!m_verbose)
        //    return 1;  //soft error

        return 1;
    }

    return 0;
}


bool CmdLine::IsSwitch(const wchar_t* arg)
{
    assert(arg);

    switch (*arg)
    {
        case L'/':  //windows-style switch
        case L'-':  //unix-style switch
            return true;

        default:
            return false;  //this is an arg, not a switch
    }
}


int CmdLine::Parse(wchar_t** i)
{
    assert(i);

    const wchar_t* arg = *i;
    assert(arg);

    switch (*arg)
    {
        case L'/':  //windows-style switch
            return ParseWindows(i);

        case L'-':  //unix-style switch
            if (*++arg == L'-')  //long-form
                return ParseLong(i);
            else
                return ParseShort(i);

        default:
            return 0;  //this is an arg, not a switch
    }
}


int CmdLine::ParseWindows(wchar_t** i)
{
    assert(i);

    const wchar_t* arg = *i;
    assert(arg);
    assert(*arg == L'/');

    ++arg;

    if (*arg == L'\0')
    {
        wcout << L"Slash cannot stand alone as switch indicator."
              << endl;

        return -1;  //error
    }

    const wchar_t* end = arg;

    while ((*end != L'\0') && (*end != L':'))
        ++end;

    const size_t len = end - arg;

    //TODO: we special-case the following switches.
    //Is this the best approach here?

    if (len != 1)
        return ParseLongPost(i, arg, len);

    const bool has_value = (arg[len] == L':');

    if (*arg == L'V')
    {
        if (has_value)
        {
            wcout << "Version option does not accept a value." << endl;
            return -1;  //error
        }

        m_version = true;
        return 1;
    }

    if (*arg == L'v')  //verbose
    {
        if (has_value)
        {
            wcout << "Verbose option does not accept a value." << endl;
            return -1;  //error
        }

        m_verbose = true;
        return 1;
    }

    return ParseLongPost(i, arg, len);
}


int CmdLine::ParseShort(wchar_t** i)
{
    assert(i);

    const wchar_t* arg = *i;
    assert(arg);
    assert(*arg == L'-');

    const wchar_t c = *++arg;
    assert(c != L'-');

    if (c == L'\0')
    {
        wcout << L"Hyphen cannot stand alone as switch indicator."
              << endl;

        return -1;
    }

    switch (c)
    {
        case L'i':
        case L'I':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"input", len) != 0)
                {
                    wcout << L"Unknown switch: " << *i
                          << L"\nUse -i or --input to specify input filename."
                          << endl;

                    return -1;  //error
                }
            }

            m_input = *++i;

            if (m_input == 0)
            {
                wcout << L"No value specified for input filename switch."
                      << endl;

                return -1;  //error
            }

            return 2;

        case L'o':
        case L'O':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"output", len) != 0)
                {
                    wcout << L"Unknown switch: " << *i
                          << L"\nUse -o or --output to specify output filename."
                          << endl;

                    return -1;  //error
                }
            }

            m_output = *++i;

            if (m_output == 0)
            {
                wcout << L"No value specified for output filename switch."
                      << endl;

                return -1;  //error
            }

            return 2;

        case L'?':
            ++arg;  //throw away '?'

            if (*arg == L'?')  //-??
            {
                m_verbose = true;
                ++arg;
            }

            if (*arg != L'\0')
            {
                wcout << L"Unknown switch: " << *i
                      << L"\nUse -? by itself to get usage info."
                      << endl;

                return -1;
            }

            m_usage = true;
            return 1;

        case L'h':
        case L'H':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"help", len) == 0)
                    __noop;
                else if (_wcsnicmp(arg, L"hh", len) == 0)
                    m_verbose = true;
                else
                {
                    wcout << L"Unknown switch: " << *i
                          << L"\nIf help info was desired, specify the -h or --help switches."
                          << endl;

                    return -1;
                }
            }

            m_usage = true;
            return 1;

        case L'u':
        case L'U':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"usage", len) != 0)
                {
                    wcout << L"Unknown switch: " << *i
                          << L"\nIf usage info was desired, specify the -u or --usage switches."
                          << endl;

                    return -1;
                }
            }

            m_usage = true;
            return 1;

        case L'l':
        case L'L':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"list", len) != 0)
                {
                    wcout << L"Unknown switch: " << *i
                          << L"\nIf list info was desired, specify the -l or --list switches."
                          << endl;

                    return -1;  //error
                }
            }

            m_list = true;
            return 1;

        case L'V':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"version", len) != 0)
                {
                    wcout << "Unknown switch: " << *i
                          << L"\nIf version info was desired, specify the -v or --version switches."
                          << L"\nIf verbosity was desired, specify the -V or --verbose switches."
                          << endl;

                    return -1;  //error
                }
            }

            m_version = true;
            return 1;

        case L'v':
            if (*(arg + 1) != L'\0')
            {
                const size_t len = wcslen(arg);

                if (_wcsnicmp(arg, L"verbose", len) != 0)
                {
                    wcout << "Unknown switch: " << *i
                          << L"\nIf verbosity was desired, specify the -V or --verbose switches."
                          << L"\nIf version info was desired, specify the -v or --version switches."
                          << endl;

                    return -1;  //error
                }
            }

            m_verbose = true;
            return 1;

        default:
            wcout << L"Unknown switch: " << *i
                  << L"\nUse -h to get usage info."
                  << endl;

            return -1;
    }
}


int CmdLine::ParseLong(wchar_t** i)
{
    assert(i);

    const wchar_t* arg = *i;
    assert(arg);
    assert(*arg == L'-');

    ++arg;
    assert(arg);
    assert(*arg == L'-');

    const wchar_t* end = ++arg;

    while ((*end != L'\0') && (*end != L'='))
        ++end;

    const size_t len = end - arg;

    if (len == 0)
    {
        wcout << L"Double-hyphen cannot stand alone as switch indicator."
              << endl;

        return -1;  //error
    }

    return ParseLongPost(i, arg, len);
}


int CmdLine::ParseLongPost(
    wchar_t** i,
    const wchar_t* arg,
    size_t len)
{
    const bool has_value = (arg[len] != L'\0');  //L':' or L'="

    //  -h help
    //  -hh verbose help
    //  /h help
    //  /hh verbose help
    //  --h help
    //  --help help
    //  --hh verbose help

    const wchar_t* const param = *i;

    if ((_wcsnicmp(arg, L"help", len) == 0) ||
        (_wcsnicmp(arg, L"hh", len) == 0) ||
        (_wcsnicmp(arg, L"?", len) == 0) ||
        (_wcsnicmp(arg, L"??", len) == 0))
    {
        if (has_value)
        {
            wcout << "Help switch does not accept a value." << endl;
            return -1;  //error
        }

        m_usage = true;

        if (_wcsicmp(arg, L"hh") == 0)
            m_verbose = true;

        else if (wcscmp(arg, L"??") == 0)
            m_verbose = true;

        else if (wcscmp(arg, L"?") == 0)
        {
            if (*param == L'-')  //long-form
                m_verbose = true;
        }

        return 1;
    }

    //  -? help
    //  /? help
    //  --? verbose help
    //  -?? verbose help
    //  /?? verbose help
    //  --?? verbose help

    if (wcsncmp(arg, L"?", len) == 0)
    {
        if (has_value)
        {
            wcout << "Help switch does not accept a value." << endl;
            return -1;  //error
        }

        m_usage = true;
        m_verbose = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"list", len) == 0)
    {
        if (has_value)
        {
            wcout << L"List switch does not accept a value." << endl;
            return -1;  //error
        }

        m_list = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"require-audio", len) == 0)
    {
        if (has_value)
        {
            wcout << "Require-audio option does not accept a value." << endl;
            return -1;  //error
        }

        m_require_audio = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"script-mode", len) == 0)
    {
        if (has_value)
        {
            wcout << "Script-mode switch does not accept a value." << endl;
            return -1;  //error
        }

        m_script = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"usage", len) == 0)
    {
        if (has_value)
        {
            wcout << "Usage switch does not accept a value." << endl;
            return -1;  //error
        }

        m_usage = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"verbose", len) == 0)
    {
        if (has_value)
        {
            wcout << L"Verbose switch does not accept a value." << endl;
            return -1;  //error
        }

        m_verbose = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"version", len) == 0)
    {
        if (has_value)
        {
            wcout << L"Version switch does not accept a value." << endl;
            return -1;  //error
        }

        m_version = true;
        return 1;
    }

    if (_wcsnicmp(arg, L"input", len) == 0)
    {
        if (has_value)
        {
            m_input = arg + len + 1;

            if (wcslen(m_input) == 0)
            {
                wcout << "Empty value specified for input filename switch." << endl;
                return -1;  //error
            }

            return 1;
        }

        m_input = *++i;

        if (m_input == 0)
        {
            wcout << "No filename specified for input switch." << endl;
            return -1;  //error
        }

        return 2;
    }

    if (_wcsnicmp(arg, L"output", len) == 0)
    {
        if (has_value)
        {
            m_output = arg + len + 1;

            if (wcslen(m_output) == 0)
            {
                wcout << "Empty value specified for output filename switch." << endl;
                return -1;  //error
            }

            return 1;
        }

        m_output = *++i;

        if (m_output == 0)
        {
            wcout << "No filename specified for output switch." << endl;
            return -1;  //error
        }

        return 2;
    }

    if (_wcsnicmp(arg, L"deadline", len) == 0)
    {
        int n;
        const wchar_t* value;
        size_t value_length;

        if (has_value)
        {
            value = arg + len + 1;
            value_length = wcslen(value);

            if (value_length == 0)
            {
                wcout << "Empty value specified for deadline switch." << endl;
                return -1;  //error
            }

            n = 1;
        }
        else
        {
            value = *++i;

            if (value == 0)
            {
                wcout << "No value specified for deadline switch." << endl;
                return -1;  //error
            }

            value_length = wcslen(value);

            n = 2;
        }

        if ((_wcsnicmp(value, L"infinite", value_length) == 0) ||
            (_wcsnicmp(value, L"best", value_length) == 0))
        {
            m_deadline = kDeadlineBestQuality;
        }
        else if ((_wcsnicmp(value, L"realtime", value_length) == 0) ||
                 (_wcsnicmp(value, L"real-time", value_length) == 0))
        {
            m_deadline = kDeadlineRealtime;
        }
        else if (_wcsnicmp(value, L"good", value_length) == 0)
            m_deadline = kDeadlineGoodQuality;

        else
        {
            std::wistringstream is(value);

            if (!(is >> m_deadline) || !is.eof())
            {
                wcout << "Bad value specified for deadline switch." << endl;
                return -1;  //error
            }

            if (m_deadline < 0)
            {
                wcout << "Value specified for deadline out-of-range (too small)." << endl;
                return -1;  //error
            }
        }

        return n;
    }

    int status = ParseOpt(i, arg, len, L"decoder-buffer-size", m_decoder_buffer_size, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"decoder-buffer-initial-size", m_decoder_buffer_initial_size, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"decoder-buffer-optimal-size", m_decoder_buffer_optimal_size, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"dropframe-threshold", m_dropframe_thresh, 0, 100);

    if (status)
        return status;

    if (_wcsnicmp(arg, L"end-usage", len) == 0)
    {
        int n;
        const wchar_t* value;
        size_t value_length;

        if (has_value)
        {
            value = arg + len + 1;
            value_length = wcslen(value);

            if (value_length == 0)
            {
                wcout << "Empty value specified for end-usage switch." << endl;
                return -1;  //error
            }

            n = 1;
        }
        else
        {
            value = *++i;

            if (value == 0)
            {
                wcout << "No value specified for end-usage switch." << endl;
                return -1;  //error
            }

            value_length = wcslen(value);

            n = 2;
        }

        if ((_wcsnicmp(value, L"VBR", value_length) == 0) ||
            (_wcsnicmp(value, L"variable", value_length) == 0))
        {
            m_end_usage = kEndUsageVBR;
        }
        else if ((_wcsnicmp(value, L"CBR", value_length) == 0) ||
                 (_wcsnicmp(value, L"constant", value_length) == 0))
        {
            m_end_usage = kEndUsageCBR;
        }
        else
        {
            std::wistringstream is(value);

            if (!(is >> m_end_usage) || !is.eof())
            {
                wcout << "Bad value specified for end-usage switch." << endl;
                return -1;  //error
            }

            if (m_end_usage < 0)
            {
                wcout << "Value specified for end-usage switch out-of-range (too small)." << endl;
                return -1;  //error
            }

            if (m_end_usage > 1)
            {
                wcout << "Value specified for end-usage switch out-of-range (too large)." << endl;
                return -1;  //error
            }
        }

        return n;
    }

    if (_wcsnicmp(arg, L"error-resilient", len) == 0)
    {
        int n;
        const wchar_t* value;
        size_t value_length;

        if (has_value)
        {
            value = arg + len + 1;
            value_length = wcslen(value);

            if (value_length == 0)
            {
                wcout << "Empty value specified for error-resilient switch." << endl;
                return -1;  //error
            }

            n = 1;
        }
        else
        {
            value = *++i;

            if (value == 0)
            {
                wcout << "No value specified for error-resilient switch." << endl;
                return -1;  //error
            }

            value_length = wcslen(value);

            n = 2;
        }

        if ((_wcsnicmp(value, L"true", value_length) == 0) ||
            (_wcsnicmp(value, L"enabled", value_length) == 0))
        {
            m_error_resilient = 1;
        }
        else if ((_wcsnicmp(value, L"false", value_length) == 0) ||
                 (_wcsnicmp(value, L"disabled", value_length) == 0))
        {
            m_error_resilient = 0;
        }
        else
        {
            std::wistringstream is(value);

            if (!(is >> m_error_resilient) || !is.eof())
            {
                wcout << "Bad value specified for error-resilient switch." << endl;
                return -1;  //error
            }

            if (m_error_resilient < 0)
            {
                wcout << "Value specified for error-resilient switch out-of-range (too small)." << endl;
                return -1;  //error
            }
        }

        return n;
    }

    if (_wcsnicmp(arg, L"keyframe-frequency", len) == 0)
    {
        int n;
        const wchar_t* value;
        size_t value_length;

        if (has_value)
        {
            value = arg + len + 1;
            value_length = wcslen(value);

            if (value_length == 0)
            {
                wcout << "Empty value specified for keyframe-frequency switch." << endl;
                return -1;  //error
            }

            n = 1;
        }
        else
        {
            value = *++i;

            if (value == 0)
            {
                wcout << "No value specified for keyframe-frequency switch." << endl;
                return -1;  //error
            }

            value_length = wcslen(value);

            n = 2;
        }

        std::wistringstream is(value);

        if (!(is >> m_keyframe_frequency) || !is.eof())
        {
            wcout << "Bad value specified for keyframe-frequency switch." << endl;
            return -1;  //error
        }

        if (m_keyframe_frequency < 0)
        {
            wcout << "Value for keyframe-frequency is out-of-range (too small)." << endl;
            return -1;  //error
        }

        return n;
    }

    if (_wcsnicmp(arg, L"keyframe-mode", len) == 0)
    {
        int n;
        const wchar_t* value;
        size_t value_length;

        if (has_value)
        {
            value = arg + len + 1;
            value_length = wcslen(value);

            if (value_length == 0)
            {
                wcout << "Empty value specified for keyframe-mode switch." << endl;
                return -1;  //error
            }

            n = 1;
        }
        else
        {
            value = *++i;

            if (value == 0)
            {
                wcout << "No value specified for keyframe-mode switch." << endl;
                return -1;  //error
            }

            value_length = wcslen(value);

            n = 2;
        }

        if (_wcsnicmp(value, L"auto", value_length) == 0)
        {
            m_keyframe_mode = kKeyframeModeAuto;
        }
        else if ((_wcsnicmp(value, L"disabled", value_length) == 0) ||
                 (_wcsnicmp(value, L"fixed", value_length) == 0))
        {
            m_keyframe_mode = kKeyframeModeDisabled;
        }
        else if (_wcsnicmp(value, L"default", value_length) == 0)
        {
            m_keyframe_mode = kKeyframeModeDefault;
        }
        else
        {
            std::wistringstream is(value);

            if (!(is >> m_keyframe_mode) || !is.eof())
            {
                wcout << "Bad value specified for keyframe-mode switch." << endl;
                return -1;  //error
            }

            if (m_keyframe_mode < -1)
            {
                wcout << "Value for keyframe-mode is out-of-range (too small)." << endl;
                return -1;  //error
            }

            if (m_keyframe_mode > 1)
            {
                wcout << "Value for keyframe-mode is out-of-range (too large)." << endl;
                return -1;  //error
            }
        }

        return n;
    }

    status = ParseOpt(i, arg, len, L"keyframe-min-interval", m_keyframe_min_interval, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"keyframe-max-interval", m_keyframe_max_interval, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"lag-in-frames", m_lag_in_frames, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"min-quantizer", m_min_quantizer, 0, 63);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"max-quantizer", m_max_quantizer, 0, 63);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"resize-allowed", m_resize_allowed, 0, 1, 1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"resize-up-threshold", m_resize_up_thresh, 0, 100);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"resize-down-threshold", m_resize_down_thresh, 0, 100);

    if (status)
        return status;

    if (_wcsnicmp(arg, L"save-graph", len) == 0)
    {
        const wchar_t*& f = m_save_graph_file_ptr;

        if (has_value)
        {
            f = arg + len + 1;
            return 1;
        }

        f = *++i;

        if ((f == 0) || IsSwitch(f)) //no value specified
        {
            f = L"";
            return 1;
        }

        return 2;
    }

    status = ParseOpt(i, arg, len, L"target-bitrate", m_target_bitrate, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"thread-count", m_thread_count, 0, -1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"token-partitions", m_token_partitions, 0, 3);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"two-pass", m_two_pass, 0, 1, 1);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"two-pass-vbr-bias-pct", m_two_pass_vbr_bias_pct, 0, 100);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"two-pass-vbr-minsection-pct", m_two_pass_vbr_minsection_pct, 0, 1000);  //?

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"two-pass-vbr-maxsection-pct", m_two_pass_vbr_maxsection_pct, 0, 1000);  //?

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"undershoot-pct", m_undershoot_pct, 0, 100);

    if (status)
        return status;

    status = ParseOpt(i, arg, len, L"overshoot-pct", m_overshoot_pct, 0, 100);

    if (status)
        return status;

    wcout << "Unknown switch: " << *i
          << "\nUse /help or --help to get usage info."
          << endl;

    return -1;  //error
}


const wchar_t* CmdLine::GetInputFileName() const
{
    return m_input;
}


const wchar_t* CmdLine::GetOutputFileName() const
{
    return m_output;
}


const wchar_t* CmdLine::GetSaveGraphFile() const
{
    return m_save_graph_file_ptr;
}


bool CmdLine::ScriptMode() const
{
    return m_script;
}


bool CmdLine::GetList() const
{
    return m_list;
}


bool CmdLine::GetVerbose() const
{
    return m_verbose;
}


bool CmdLine::GetRequireAudio() const
{
    return m_require_audio;
}


int CmdLine::GetDeadline() const
{
    return m_deadline;
}


int CmdLine::GetTargetBitrate() const
{
    return m_target_bitrate;
}


int CmdLine::GetMinQuantizer() const
{
    return m_min_quantizer;
}


int CmdLine::GetMaxQuantizer() const
{
    return m_max_quantizer;
}


int CmdLine::GetUndershootPct() const
{
    return m_undershoot_pct;
}


int CmdLine::GetOvershootPct() const
{
    return m_overshoot_pct;
}


int CmdLine::GetDecoderBufferSize() const
{
    return m_decoder_buffer_size;
}


int CmdLine::GetDecoderBufferInitialSize() const
{
    return m_decoder_buffer_initial_size;
}


int CmdLine::GetDecoderBufferOptimalSize() const
{
    return m_decoder_buffer_optimal_size;
}


int CmdLine::GetKeyframeMode() const
{
    return m_keyframe_mode;
}


int CmdLine::GetKeyframeMinInterval() const
{
    return m_keyframe_min_interval;
}


int CmdLine::GetKeyframeMaxInterval() const
{
    return m_keyframe_max_interval;
}


double CmdLine::GetKeyframeFrequency() const
{
    return m_keyframe_frequency;
}


int CmdLine::GetThreadCount() const
{
    return m_thread_count;
}


int CmdLine::GetErrorResilient() const
{
    return m_error_resilient;
}


int CmdLine::GetDropframeThreshold() const
{
    return m_dropframe_thresh;
}


int CmdLine::GetResizeAllowed() const
{
    return m_resize_allowed;
}


int CmdLine::GetResizeUpThreshold() const
{
    return m_resize_up_thresh;
}


int CmdLine::GetResizeDownThreshold() const
{
    return m_resize_down_thresh;
}


int CmdLine::GetEndUsage() const
{
    return m_end_usage;
}


int CmdLine::GetLagInFrames() const
{
    return m_lag_in_frames;
}


int CmdLine::GetTokenPartitions() const
{
    return m_token_partitions;
}


int CmdLine::GetTwoPass() const
{
    return m_two_pass;
}


int CmdLine::GetTwoPassVbrBiasPct() const
{
    return m_two_pass_vbr_bias_pct;
}


int CmdLine::GetTwoPassVbrMinsectionPct() const
{
    return m_two_pass_vbr_minsection_pct;
}


int CmdLine::GetTwoPassVbrMaxsectionPct() const
{
    return m_two_pass_vbr_maxsection_pct;
}


void CmdLine::PrintVersion() const
{
    wcout << "makewebm ";

    wchar_t* fname;

    const errno_t e = _get_wpgmptr(&fname);
    e;
    assert(e == 0);
    assert(fname);

    VersionHandling::GetVersion(fname, wcout);

    wcout << endl;
}


void CmdLine::PrintUsage() const
{
    wcout << L"usage: makewebm <opts> <args>\n";

    wcout << L"  -i, --input                     input filename\n"
          << L"  -o, --output                    output filename\n"
          << L"  --deadline                      max time for frame encode (in microseconds)\n"
          << L"  --decoder-buffer-size           buffer size (in milliseconds)\n"
          << L"  --decoder-buffer-initial-size   before playback (in milliseconds)\n"
          << L"  --decoder-buffer-optimal-size   desired size (in milliseconds)\n"
          << L"  --dropframe-threshold           temporal resampling\n"
          << L"  --end-usage                     {\"VBR\"|\"CBR\"}\n"
          << L"  --error-resilient               defend against lossy or noisy links\n"
          << L"  --keyframe-frequency            time (in sec) between keyframes\n"
          << L"  --keyframe-mode                 {\"disabled\"|\"auto\"}\n"
          << L"  --keyframe-min-interval         min distance between keyframes\n"
          << L"  --keyframe-max-interval         max distable between keyframes\n"
          << L"  --lag-in-frames                 consume frames before producing\n"
          << L"  --min-quantizer                 min (best quality) quantizer\n"
          << L"  --max-quantizer                 max (worst quality) quantizer\n"
          << L"  --require-audio                 quit if no audio encoder available\n"
          << L"  --resize-allowed                spatial resampling\n"
          << L"  --resize-up-threshold           spatial resampling up threshold\n"
          << L"  --resize-down-threshold         spatial resampling down threshold\n"
          << L"  --script-mode                   print progress in script-friendly way\n"
          << L"  --save-graph                    save graph as GraphEdit storage file (*.grf)\n"
          << L"  --target-bitrate                target bandwidth (in kilobits/second)\n"
          << L"  --thread-count                  number of threads to use for VP8 encoding\n"
          << L"  --token-partitions              number of sub-streams\n"
          << L"  --two-pass                      two-pass encoding\n"
          << L"  --two-pass-vbr-bias-pct         CBR/VBR bias\n"
          << L"  --two-pass-vbr-minsection-pct   minimum bitrate\n"
          << L"  --two-pass-vbr-maxsection-pct   maximum bitrate\n"
          << L"  --undershoot-pct                percent of target bitrate for easier frames\n"
          << L"  --overshoot-pct                 percent of target bitrate for harder frames\n"
          << L"  -l, --list                      print switch values, but do not run app\n"
          << L"  -v, --verbose                   print verbose list or usage info\n"
          << L"  -V, --version                   print version information\n"
          << L"  -?, -h, --help                  print usage\n"
          << L"  -??, -hh, --?                   print verbose usage\n";

    if (!m_verbose)
    {
        wcout << endl;
        return;
    }

    wcout << L'\n'
          << L"The order of appearance of switches and arguments\n"
          << L"on the command line does not matter.\n"
          << L'\n'
          << L"Long-form switches may be abbreviated, and are case-insensitive.\n"
          << L"They may also be specified using Windows-style syntax, using a\n"
          << L"forward slash for the switch.\n";

    wcout << L'\n'
          << L"The input filename must be specified, as either a switch\n"
          << L"value or as a command-line argument.\n"
          << L'\n'
          << L"The output filename may be specified as either a switch\n"
          << L"value or command-line argument, but it may also be omitted.\n"
          << L"If omitted, its value is synthesized from the input filename.\n";

    wcout << L'\n'
          << L"The deadline value specifies the maximum amount of time\n"
          << L"(in microseconds) allowed for VP8 encoding of a video frame.\n"
          << L"The following distinguished values are also defined:\n"
          << L"  0 (or \"best\", or \"infinite\") means take as long as\n"
          << L"    necessary to achieve best quality\n"
          << L"  1 (or \"realtime\") means real-time encoding\n"
          << L"  1000000 (or \"good\") means good quality (the default)\n";

    wcout << '\n'
          << "TODO: MORE PARAMS TO BE DESCRIBED HERE\n";

    wcout << endl;
}


void CmdLine::ListArgs() const
{
    wcout << L"input      : ";

    if (m_input == 0)
        wcout << "(no input specified)\n";
    else
    {
        wcout << L"\"";

        if (m_verbose)
            wcout << GetPath(m_input);
        else
            wcout << m_input;

        wcout << L"\"\n";
    }

    wcout << L"output     : ";

    if (m_output == 0)
        wcout << "(no output specified)\n";
    else
    {
        wcout << L"\"";

        if (m_verbose)
            wcout << GetPath(m_output);
        else
            wcout << m_output;

        wcout << L"\"\n";
    }

    wcout << L"save-graph : ";

    if (m_save_graph_file_ptr == 0)
        wcout << "(no save graph file requested)\n";

    else if (wcslen(m_save_graph_file_ptr) == 0)
        wcout << "(will be synthesized from output filename)\n";

    else
        wcout << m_save_graph_file_ptr << L'\n';

    wcout << L"script-mode: " << boolalpha << m_script << L'\n';
    wcout << L"verbose    : " << boolalpha << m_verbose << L'\n';

    if (m_deadline >= 0)
    {
        wcout << L"deadline   : " << m_deadline;

        //if (m_verbose)
        {
            if (m_deadline == kDeadlineBestQuality)
                wcout << " (best quality)";

            else if (m_deadline == kDeadlineRealtime)
                wcout << " (real-time)";

            else if (m_deadline == kDeadlineGoodQuality)
                wcout << " (good quality)";
        }

        wcout << L'\n';  //complete output of deadline value
    }

    if (m_decoder_buffer_size >= 0)
    {
        wcout << L"decoder-buffer-size: " << m_decoder_buffer_size;

        if (m_decoder_buffer_size == 0)
            wcout << " (use encoder default)";

        wcout << L'\n';
    }

    if (m_decoder_buffer_initial_size >= 0)
    {
        wcout << L"decoder-buffer-initial-size: " << m_decoder_buffer_initial_size;

        if (m_decoder_buffer_initial_size == 0)
            wcout << " (use encoder default)";

        wcout << L'\n';
    }

    if (m_decoder_buffer_optimal_size >= 0)
    {
        wcout << L"decoder-buffer-optimal-size: " << m_decoder_buffer_optimal_size;

        if (m_decoder_buffer_optimal_size == 0)
            wcout << " (use encoder default)";

        wcout << L'\n';
    }

    if (m_end_usage >= 0)
    {
        wcout << L"end-usage: ";

        switch (m_end_usage)
        {
            case kEndUsageVBR:
            default:
                wcout << "VBR";
                break;

            case kEndUsageCBR:
                wcout << "CBR";
                break;
        }

        wcout << L'\n';
    }

    if (m_error_resilient >= 0)
    {
        wcout << L"error-resilient: " << m_error_resilient;

        if (m_error_resilient == 0)
            wcout << " (disabled)";
        else
            wcout << " (enabled)";

        wcout << L'\n';
    }

    if (m_lag_in_frames >= 0)
    {
        wcout << L"lag-in-frames: " << m_lag_in_frames;

        if (m_lag_in_frames == 0)
            wcout << " (disabled)";

        wcout << L'\n';
    }

    if (m_min_quantizer >= 0)
        wcout << L"min-quantizer: " << m_min_quantizer << L'\n';

    if (m_max_quantizer >= 0)
        wcout << L"max-quantizer: " << m_max_quantizer << L'\n';

    if (m_target_bitrate >= 0)
    {
        wcout << L"target-bitrate: " << m_target_bitrate;

        if (m_target_bitrate == 0)
            wcout << " (use encoder default)";

        wcout << L'\n';
    }

    if (m_thread_count >= 0)
    {
        wcout << L"thread-count: " << m_thread_count;

        if (m_thread_count == 0)
            wcout << L" (use encoder default)";

        wcout << L'\n';
    }

    if (m_token_partitions >= 0)
        wcout << L"token-partitions: " << m_token_partitions << L'\n';

    if (m_two_pass >= 0)
        wcout << L"two-pass: " << m_two_pass << L'\n';

    if (m_two_pass_vbr_bias_pct >= 0)
        wcout << L"two-pass-vbr-bias-pct: " << m_two_pass_vbr_bias_pct << L'\n';

    if (m_two_pass_vbr_minsection_pct >= 0)
        wcout << L"two-pass-vbr-minsection-pct: " << m_two_pass_vbr_minsection_pct << L'\n';

    if (m_two_pass_vbr_maxsection_pct >= 0)
        wcout << L"two-pass-vbr-maxsection-pct: " << m_two_pass_vbr_maxsection_pct << L'\n';

    if (m_undershoot_pct >= 0)
        wcout << L"undershoot-pct: " << m_undershoot_pct << L'\n';

    if (m_overshoot_pct >= 0)
        wcout << L"overshoot-pct : " << m_overshoot_pct << L'\n';

    if (m_keyframe_frequency >= 0)
    {
        wcout << L"keyframe-frequency: "
              << std::fixed
              << std::setprecision(3)
              << m_keyframe_frequency
              << L'\n';
    }

    if (m_keyframe_mode >= kKeyframeModeDefault)
    {
        wcout << L"keyframe-mode: " << m_keyframe_mode;

        switch (m_keyframe_mode)
        {
            case kKeyframeModeDefault:
            default:
                wcout << L" (use encoder default)";
                break;

            case kKeyframeModeDisabled:
                wcout << L" (disabled)";
                break;

            case kKeyframeModeAuto:
                wcout << L" (auto)";
                break;
        }

        wcout << L'\n';
    }
    else if (m_keyframe_frequency >= 0)
    {
        wcout << L"keyframe-mode: 1 (auto implied by keyframe-frequency)" << endl;
    }

    if (m_keyframe_min_interval >= 0)
        wcout << L"keyframe-min-interval: " << m_keyframe_min_interval << L'\n';
    else if (m_keyframe_frequency >= 0)
        wcout << L"keyframe-min-interval: (determined from framerate)" << L'\n';

    if (m_keyframe_max_interval >= 0)
        wcout << L"keyframe-max-interval: " << m_keyframe_max_interval << L'\n';
    else
        wcout << L"keyframe-max-interval: (determined from framerate)" << L'\n';

    wcout << endl;
}


std::wstring CmdLine::GetPath(const wchar_t* filename)
{
    assert(filename);

    //using std::wstring;
    //wstring path;
    //wstring::size_type filepos;

    DWORD buflen = _MAX_PATH + 1;

    for (;;)
    {
        const DWORD cb = buflen * sizeof(wchar_t);
        wchar_t* const buf = (wchar_t*)_malloca(cb);
        wchar_t* ptr;

        const DWORD n = GetFullPathName(filename, buflen, buf, &ptr);

        if (n == 0)  //error
        {
            const DWORD e = GetLastError();
            e;
            return filename;  //best we can do
        }

        if (n < buflen)
        {
            //path = buf;
            //filepos = ptr - buf;
            //break;
            return buf;
        }

        buflen = 2 * buflen + 1;
    }

    //return path;
}


void CmdLine::SynthesizeOutput()
{
    wstring& path = m_synthesized_output;
    path = GetPath(m_input);

    const wstring::size_type pos = path.rfind(L'.');
    const wstring::size_type len = path.length();

    if (pos == wstring::npos)  //no ext
        path.append(L".webm");

    else if (_wcsicmp(path.c_str() + pos, L".webm") == 0)  //match
        path.replace(pos, len, L"_remux.webm");

    else
        path.replace(pos, len, L".webm");

    m_output = path.c_str();
}


void CmdLine::SynthesizeSaveGraph()
{
    wstring& path = m_save_graph_file_str;
    path = GetPath(m_output);

    const wstring::size_type pos = path.rfind(L'.');
    const wstring::size_type len = path.length();

    if (pos == wstring::npos)  //weird: no ext
        path.append(L".grf");

    else if (_wcsicmp(path.c_str() + pos, L".grf") == 0)  //weird: match
        path.replace(pos, len, L"_graph.grf");

    else
        path.replace(pos, len, L".grf");  //typical

    m_save_graph_file_ptr = m_save_graph_file_str.c_str();
}


int CmdLine::ParseOpt(
    wchar_t** i,
    const wchar_t* arg,
    size_t len,
    const wchar_t* name,
    int& value,
    int min,
    int max,
    int optional) const
{
    if (_wcsnicmp(arg, name, len) != 0)  //not a match
        return 0;  //keep parsing

    const bool has_value = (arg[len] != L'\0');  //L':' or L'="
    const bool is_required = (optional == kValueIsRequired);

    int n;
    const wchar_t* str_value;
    size_t str_value_length;

    if (has_value)
    {
        str_value = arg + len + 1;
        str_value_length = wcslen(str_value);

        if (str_value_length == 0)
        {
            if (is_required)
            {
                wcout << "Empty value specified for " << name << " switch." << endl;
                return -1;  //error
            }

            str_value = 0;  //means "use optional value"
        }

        n = 1;
    }
    else
    {
        str_value = *++i;

        if (str_value)
        {
            if (IsSwitch(str_value) || !iswdigit(*str_value))
            {
                //potential optional found

                if (is_required)
                {
                    wcout << "No value specified for " << name << " switch." << endl;
                    return -1;  //error
                }

                str_value = 0;  //yes, optional was found
                n = 1;
            }
            else
            {
                str_value_length = wcslen(str_value);
                n = 2;
            }
        }
        else if (is_required)
        {
            wcout << "No value specified for " << name << " switch." << endl;
            return -1;  //error
        }
        else
            n = 1;  //optional value will be used
    }

    if (str_value == 0)
    {
        assert(!is_required);
        value = optional;

        return n;
    }

    std::wistringstream is(str_value);

    if (!(is >> value) || !is.eof())
    {
        wcout << "Bad value specified for " << name << " switch." << endl;
        return -1;  //error
    }

    if (value < min)
    {
        wcout << "Value for " << name
              << " is out-of-range (too small)."
              << endl;

        return -1;  //error
    }

    if ((max >= min) && (value > max))
    {
        wcout << "Value for " << name
              << " is out-of-range (too large)."
              << endl;

        return -1;  //error
    }

    return n;  //success
}
