// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <objbase.h>
#include "playwebmcmdline.h"
#include "versionhandling.h"
#include "webmtypes.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <cassert>
#include <cstring>
#include <windows.h>
#include <uuids.h>
using std::wcout;
using std::endl;
using std::boolalpha;
using std::wstring;
using std::dec;

CmdLine::CmdLine()
    : m_input(0),
      m_bUsage(false),
      m_bList(false),
      m_bVersion(false),
      m_pSplitter(0),
      m_pSource(0),
      m_bVerbose(false) {}

int CmdLine::Parse(int argc, wchar_t* argv[]) {
  m_argv = argv;

  if (argc <= 1)  // program-name only
  {
    PrintUsage();
    return 1;  // soft error
  }

  // TODO:
  // start time
  // stop time
  //
  // IGraphBuilder::RenderFile
  // select splitter
  // select source
  // verbose list can dump graph and then exit
  //
  // more ideas:
  // require video outpin
  // require audio outpin
  // make video connection optional
  // make audio connection optional
  // verbose mode
  // terse mode
  // quiet
  // very quite
  // warnings vs. errors
  // treat warnings as errors

  wchar_t** const argv_end = argv + argc;
  assert(*argv_end == 0);

  --argc;  // throw away program name
  assert(argc >= 1);

  wchar_t** i = argv + 1;
  assert(i < argv_end);

  for (;;) {
    const int n = Parse(i);

    if (n < 0)  // error
      return n;

    if (n == 0)  // arg, not switch
    {
      --argc;
      assert(argc >= 0);

      ++i;
      assert(i <= argv_end);
    } else  // switch
    {
      argc -= n;
      assert(argc >= 0);
      assert((i + n) <= argv_end);

      std::rotate(i, i + n, argv_end + 1);
    }

    if (argc <= 0)
      break;
  }

  assert(i);
  assert(*i == 0);

  wchar_t** const j = i;  // args end
  i = argv + 1;  // args begin

  if (m_bUsage) {
    PrintUsage();
    return 1;  // soft error
  }

  if (m_bVersion) {
    PrintVersion();
    return 1;  // soft error
  }

  if (m_input == 0)  // not specified as switch
  {
    if (i >= j)  // no args remain
    {
      wcout << "No input filename specified." << endl;
      return 1;  // error
    }

    m_input = *i++;
    assert(m_input);
  }

  if (i < j)  // not all args consumed
  {
    wcout << L"Too many command-line arguments." << endl;
    return 1;
  }

  if (m_bList) {
    ListArgs();

    if (!m_bVerbose)
      return 1;  // soft error
  }

  return 0;
}

bool CmdLine::IsSwitch(const wchar_t* str) {
  if (str == 0)
    return false;

  const wchar_t c = *str;

  switch (c) {
    case L'/':
    case L'-':
      return true;

    default:
      return false;
  }
}

int CmdLine::Parse(wchar_t** i) {
  assert(i);

  const wchar_t* arg = *i;
  assert(arg);

  switch (*arg) {
    case L'/':  // windows-style switch
      return ParseWindows(i);

    case L'-':  // unix-style switch
      if (*++arg == L'-')  // long-form
        return ParseLong(i);
      else
        return ParseShort(i);

    default:
      return 0;  // this is an arg, not a switch
  }
}

int CmdLine::ParseWindows(wchar_t** i) {
  assert(i);

  const wchar_t* arg = *i;
  assert(arg);
  assert(*arg == L'/');

  ++arg;

  if (*arg == L'\0') {
    wcout << L"Slash cannot stand alone as switch indicator." << endl;

    return -1;  // error
  }

  const wchar_t* end = arg;

  while ((*end != L'\0') && (*end != L':'))
    ++end;

  ptrdiff_t len = end - arg;

  if (_wcsnicmp(arg, L"input", len) == 0) {
    if (*end == L':') {
      m_input = ++end;

      if (wcslen(m_input) == 0) {
        wcout << "Empty value specified for input filename switch." << endl;
        return -1;  // error
      }

      return 1;
    }

    m_input = *++i;

    if (m_input == 0) {
      wcout << "No filename specified for input switch." << endl;
      return -1;  // error
    }

    return 2;
  }

  if (_wcsnicmp(arg, L"source", len) == 0) {
    int n;
    const wchar_t* str;

    if (*end == L':') {
      n = 1;
      str = ++end;

      if (wcslen(str) == 0)
        str = 0;
    } else {
      str = *++i;

      if ((str != 0) && !IsSwitch(str)) {
        n = 2;
        len = wcslen(str);
      } else {
        n = 1;
        str = 0;
      }
    }

    if ((str == 0) || (_wcsnicmp(str, L"WebmSource", len) == 0) ||
        (_wcsnicmp(str, L"Webm.Source", len) == 0)) {
      m_pSource = &WebmTypes::CLSID_WebmSource;
    } else {
      wcout << L"Unknown source value: " << str << endl;
      return -1;  // error
    }

    return n;
  }

  if (_wcsnicmp(arg, L"splitter", len) == 0) {
    int n;
    const wchar_t* str;

    if (*end == L':') {
      n = 1;
      str = ++end;

      if (wcslen(str) == 0)
        str = 0;
    } else {
      str = *++i;

      if ((str != 0) && !IsSwitch(str)) {
        n = 2;
        len = wcslen(str);
      } else {
        n = 1;
        str = 0;
      }
    }

    if ((str == 0) || (_wcsnicmp(str, L"WebmSplitter", len) == 0) ||
        (_wcsnicmp(str, L"Webm.Splitter", len) == 0)) {
      m_pSplitter = &WebmTypes::CLSID_WebmSplit;
    } else {
      wcout << L"Unknown splitter value: " << str << endl;
      return -1;  // error
    }

    return n;
  }

  if (_wcsnicmp(arg, L"list", len) == 0) {
    if (*end == L':') {
      wcout << "List option does not accept a value." << endl;
      return -1;  // error
    }

    m_bList = true;
    return 1;
  }

  if ((wcsncmp(arg, L"?", len) == 0) || (_wcsnicmp(arg, L"help", len) == 0)) {
    if (*end == L':') {
      wcout << "Help option does not accept a value." << endl;
      return -1;  // error
    }

    m_bUsage = true;
    return 1;
  }

  if ((wcsncmp(arg, L"??", len) == 0) || (_wcsnicmp(arg, L"hh", len) == 0)) {
    if (*end == L':') {
      wcout << "Help option does not accept a value." << endl;
      return -1;  // error
    }

    m_bUsage = true;
    m_bVerbose = true;
    return 1;
  }

  if (_wcsnicmp(arg, L"usage", len) == 0) {
    if (*end == L':') {
      wcout << "Usage option does not accept a value." << endl;
      return -1;  // error
    }

    m_bUsage = true;
    return 1;
  }

  if (*arg == L'V')  // V[ersion]
  {
    if (_wcsnicmp(arg, L"version", len) != 0) {
      wcout << L"Unknown switch: " << *i << L'\n'
            << L"If version info was desired, specify the /V[ersion] switch.\n"
            << L"If verbosity was desired, specify the /v[erbose] switch."
            << endl;

      return -1;  // error
    }

    if (*end == L':') {
      wcout << "Version option does not accept a value." << endl;
      return -1;  // error
    }

    m_bVersion = true;
    return 1;
  }

  if (*arg == L'v')  // v[erbose]
  {
    if (_wcsnicmp(arg, L"verbose", len) != 0) {
      wcout << L"Unknown switch: " << *i << L'\n'
            << L"If verbosity was desired, specify the /v[erbose] switch.\n"
            << L"If version info was desired, specify the /V[ersion] switch."
            << endl;

      return -1;  // error
    }

    if (*end == L':') {
      wcout << "Verbose option does not accept a value." << endl;
      return -1;  // error
    }

    m_bVerbose = true;
    return 1;
  }

  wcout << "Unknown switch: " << *i << "\nUse /help to get usage info." << endl;

  return -1;  // error
}

int CmdLine::ParseShort(wchar_t** i) {
  assert(i);

  const wchar_t* arg = *i;
  assert(arg);
  assert(*arg == L'-');

  const wchar_t c = *++arg;
  assert(c != L'-');

  if (c == L'\0') {
    wcout << L"Hyphen cannot stand alone as switch indicator." << endl;

    return -1;
  }

  switch (c) {
    case L'i':
    case L'I':
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"input", len) != 0) {
          wcout << L"Unknown switch: " << *i
                << L"\nUse -i or --input to specify input filename." << endl;

          return -1;  // error
        }
      }

      m_input = *++i;

      if (m_input == 0) {
        wcout << L"No value specified for input filename switch." << endl;

        return -1;  // error
      }

      return 2;

    case L's':  // source filter switch
    {
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"source", len) != 0) {
          wcout << L"Unknown switch: " << *i << L'\n'
                << L"Use -s or --source to specify source filter.\n"
                << L"Use -S or --splitter to specify splitter filter.\n"
                << endl;

          return -1;  // error
        }
      }

      int n;
      const wchar_t* str = *++i;
      size_t len;

      if ((str != 0) && !IsSwitch(str)) {
        n = 2;
        len = wcslen(str);
      } else {
        n = 1;
        str = 0;
        len = 0;
      }

      if ((str == 0) || (_wcsnicmp(str, L"WebmSource", len) == 0) ||
          (_wcsnicmp(str, L"Webm.Source", len) == 0)) {
        m_pSource = &WebmTypes::CLSID_WebmSource;
      } else {
        wcout << L"Unknown source value: " << str << endl;
        return -1;  // error
      }

      return n;
    }
    case L'S':  // splitter filter switch
    {
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"splitter", len) != 0) {
          wcout << L"Unknown switch: " << *i << L'\n'
                << L"Use -S or --splitter to specify splitter filter."
                << L"Use -s or --source to specify source filter." << endl;

          return -1;  // error
        }
      }

      int n;
      const wchar_t* str = *++i;
      size_t len;

      if ((str != 0) && !IsSwitch(str)) {
        n = 2;
        len = wcslen(str);
      } else {
        n = 1;
        str = 0;
        len = 0;
      }

      if ((str == 0) || (_wcsnicmp(str, L"WebmSplitter", len) == 0) ||
          (_wcsnicmp(str, L"Webm.Splitter", len) == 0)) {
        m_pSplitter = &WebmTypes::CLSID_WebmSplit;
      } else {
        wcout << L"Unknown splitter value: " << str << endl;
        return -1;  // error
      }

      return n;
    }
    case L'?':
      ++arg;  // throw away '?'

      if (*arg == L'?')  //-??
      {
        m_bVerbose = true;
        ++arg;
      }

      if (*arg != L'\0') {
        wcout << L"Unknown switch: " << *i
              << L"\nUse -? by itself to get usage info." << endl;

        return -1;
      }

      m_bUsage = true;
      return 1;

    case L'h':
    case L'H':
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"help", len) == 0)
          __noop;
        else if (_wcsnicmp(arg, L"hh", len) == 0)
          m_bVerbose = true;
        else {
          wcout << L"Unknown switch: " << *i << L"\nIf help info was desired, "
                                                L"specify the -h or --help "
                                                L"switches." << endl;

          return -1;
        }
      }

      m_bUsage = true;
      return 1;

    case L'u':
    case L'U':
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"usage", len) != 0) {
          wcout << L"Unknown switch: " << *i << L"\nIf usage info was desired, "
                                                L"specify the -u or --usage "
                                                L"switches." << endl;

          return -1;
        }
      }

      m_bUsage = true;
      return 1;

    case L'l':
    case L'L':
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"list", len) != 0) {
          wcout << L"Unknown switch: " << *i << L"\nIf list info was desired, "
                                                L"specify the -l or --list "
                                                L"switches." << endl;

          return -1;  // error
        }
      }

      m_bList = true;
      return 1;

    case L'V':
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"version", len) != 0) {
          wcout << "Unknown switch: " << *i
                << L"\nIf version info was desired, specify the -V or "
                   L"--version switches."
                << L"\nIf verbosity was desired, specify the -v or --verbose "
                   L"switches." << endl;

          return -1;  // error
        }
      }

      m_bVersion = true;
      return 1;

    case L'v':
      if (*(arg + 1) != L'\0') {
        const size_t len = wcslen(arg);

        if (_wcsnicmp(arg, L"verbose", len) != 0) {
          wcout << "Unknown switch: " << *i
                << L"\nIf verbosity was desired, specify the -v or --verbose "
                   L"switches." << L"\nIf version info was desired, specify "
                                   L"the -V or --version switches." << endl;

          return -1;  // error
        }
      }

      m_bVerbose = true;
      return 1;

    default:
      wcout << L"Unknown switch: " << *i << L"\nUse -h to get usage info."
            << endl;

      return -1;
  }
}

int CmdLine::ParseLong(wchar_t** i) {
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

  size_t len = end - arg;

  if (len == 0) {
    wcout << L"Double-hyphen cannot stand alone as switch indicator." << endl;

    return -1;  // error
  }

  if (_wcsnicmp(arg, L"input", len) == 0) {
    if (*end) {
      assert(*end == L'=');
      m_input = ++end;

      if (wcslen(m_input) == 0) {
        wcout << "Empty value specified for input filename switch." << endl;
        return -1;  // error
      }

      return 1;
    }

    m_input = *++i;

    if (m_input == 0) {
      wcout << "No filename specified for input switch." << endl;
      return -1;  // error
    }

    return 2;
  }

  if (_wcsnicmp(arg, L"source", len) == 0) {
    int n;
    const wchar_t* str;

    if (*end)  // found '=' separator
    {
      n = 1;
      str = ++end;

      if (wcslen(str) == 0)
        str = 0;
    } else {
      str = *++i;

      if ((str != 0) && !IsSwitch(str)) {
        n = 2;
        len = wcslen(str);
      } else {
        n = 1;
        str = 0;
      }
    }

    if ((str == 0) || (_wcsnicmp(str, L"WebmSource", len) == 0) ||
        (_wcsnicmp(str, L"Webm.Source", len) == 0)) {
      m_pSource = &WebmTypes::CLSID_WebmSource;
    } else {
      wcout << L"Unknown source value: " << str << endl;
      return -1;  // error
    }

    return n;
  }

  if (_wcsnicmp(arg, L"splitter", len) == 0) {
    int n;
    const wchar_t* str;

    if (*end)  // found '=' separator
    {
      n = 1;
      str = ++end;

      if (wcslen(str) == 0)
        str = 0;
    } else {
      str = *++i;

      if ((str != 0) && !IsSwitch(str)) {
        n = 2;
        len = wcslen(str);
      } else {
        n = 1;
        str = 0;
      }
    }

    if ((str == 0) || (_wcsnicmp(str, L"WebmSplitter", len) == 0) ||
        (_wcsnicmp(str, L"Webm.Splitter", len) == 0)) {
      m_pSplitter = &WebmTypes::CLSID_WebmSplit;
    } else {
      wcout << L"Unknown splitter value: " << str << endl;
      return -1;  // error
    }

    return n;
  }

  if ((_wcsnicmp(arg, L"help", len) == 0) || (_wcsicmp(arg, L"hh") == 0)) {
    if (*end == L'=') {
      wcout << "Help switch does not accept a value." << endl;
      return -1;  // error
    }

    m_bUsage = true;

    if (_wcsicmp(arg, L"hh") == 0)
      m_bVerbose = true;

    return 1;
  }

  if (wcsncmp(arg, L"?", len) == 0) {
    if (*end == L'=') {
      wcout << "Help switch does not accept a value." << endl;
      return -1;  // error
    }

    m_bUsage = true;
    m_bVerbose = true;
    return 1;
  }

  if (_wcsnicmp(arg, L"usage", len) == 0) {
    if (*end == L'=') {
      wcout << "Usage switch does not accept a value." << endl;
      return -1;  // error
    }

    m_bUsage = true;
    return 1;
  }

  if (_wcsnicmp(arg, L"list", len) == 0) {
    if (*end == L'=') {
      wcout << L"List switch does not accept a value." << endl;
      return -1;  // error
    }

    m_bList = true;
    return 1;
  }

  if (_wcsnicmp(arg, L"verbose", len) == 0) {
    if (*end == L'=') {
      wcout << L"Verbose switch does not accept a value." << endl;
      return -1;  // error
    }

    m_bVerbose = true;
    return 1;
  }

  if (_wcsnicmp(arg, L"version", len) == 0) {
    if (*end == L'=') {
      wcout << L"Version switch does not accept a value." << endl;
      return -1;  // error
    }

    m_bVersion = true;
    return 1;
  }

  wcout << "Unknown switch: " << *i << "\nUse --help to get usage info."
        << endl;

  return -1;  // error
}

const wchar_t* CmdLine::GetInputFileName() const { return m_input; }

const CLSID* CmdLine::GetSplitter() const { return m_pSplitter; }

const CLSID* CmdLine::GetSource() const { return m_pSource; }

bool CmdLine::GetList() const { return m_bList; }

bool CmdLine::GetVerbose() const { return m_bVerbose; }

void CmdLine::PrintVersion() const {
  wcout << "playwebm ";

  wchar_t* fname;

  const errno_t e = _get_wpgmptr(&fname);
  assert(e == 0);
  assert(fname);

  VersionHandling::GetVersion(fname, wcout);

  wcout << endl;
}

void CmdLine::PrintUsage() const {
  wcout << L"usage: playwebm <opts> <args>\n";

  wcout << L"  -i, --input       input filename\n"
        << L"  -s, --source      use source filter\n"
        << L"  -S, --splitter    use splitter filter\n"
        << L"  -l, --list        print switch values, but do not run app\n"
        << L"  -v, --verbose     print verbose list or usage info\n"
        << L"  -V, --version     print version information\n"
        << L"  -?, -h, --help    print usage\n"
        << L"  -??, -hh, --?     print verbose usage\n";

  if (m_bVerbose) {
    wcout << L'\n' << L"The possible value for --source is:\n"
          << L" WebmSource (default)\n" << L'\n'
          << L"The possible value for --splitter is:\n"
          << L" WebmSplitter (default)\n" << L'\n'
          << L"The values listed above are case-insensitive, and may\n"
          << L"be abbreviated or even completely omitted.\n" << L'\n'
          << L"If neither the source switch nor the splitter switch is\n"
          << L"specified, then the graph is constructed by calling\n"
          << L"IGraphBuilder::RenderFile.\n" << L'\n'
          << L"The input filename must be specified, as either\n"
          << L"a switch value or as a command-line argument.\n" << L'\n'
          << L"Note that the order of appearance of switches and arguments\n"
          << L"on the command line does not matter.\n" << L'\n'
          << L"Long-form options may also be specified using Windows-style\n"
          << L"syntax, with a forward slash to indicate the switch, and a\n"
          << L"colon to indicate its value.\n";
  }

  wcout << endl;
}

void CmdLine::ListArgs() const {
  wcout << L"input      : \"";

  if (m_bVerbose)
    wcout << GetPath(m_input);
  else
    wcout << m_input;

  wcout << L"\"\n";

  if (m_pSource) {
    wcout << L"source     : WebmSource";

    // if (m_bVerbose)
    //    wcout << L' ' << ToString(CLSID_MkvSource);

    wcout << L'\n';
  }

  if (m_pSplitter) {
    wcout << L"splitter   : WebmSplitter";

    // if (m_bVerbose)
    //    wcout << L' ' << ToString(m_splitter);

    if (m_pSource)
      wcout << L" (will be ignored)";

    wcout << L'\n';
  }

  wcout << endl;
}

std::wstring CmdLine::GetPath(const wchar_t* filename) {
  assert(filename);

  // using std::wstring;
  // wstring path;
  // wstring::size_type filepos;

  DWORD buflen = _MAX_PATH + 1;

  for (;;) {
    const DWORD cb = buflen * sizeof(wchar_t);
    wchar_t* const buf = (wchar_t*)_malloca(cb);
    wchar_t* ptr;

    const DWORD n = GetFullPathName(filename, buflen, buf, &ptr);

    if (n == 0)  // error
    {
      const DWORD e = GetLastError();
      e;
      return filename;  // best we can do
    }

    if (n < buflen) {
      // path = buf;
      // filepos = ptr - buf;
      // break;
      return buf;
    }

    buflen = 2 * buflen + 1;
  }

  // return path;
}
