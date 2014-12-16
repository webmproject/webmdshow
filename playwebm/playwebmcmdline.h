// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef WEBMDSHOW_PLAYWEBM_PLAYWEBMCMDLINE_H_
#define WEBMDSHOW_PLAYWEBM_PLAYWEBMCMDLINE_H_

#include <string>

class CmdLine {
  CmdLine(const CmdLine&);
  CmdLine& operator=(const CmdLine&);

 public:
  CmdLine();

  int Parse(int argc, wchar_t* argv[]);

  const wchar_t* GetInputFileName() const;
  const CLSID* GetSplitter() const;
  const CLSID* GetSource() const;
  bool GetList() const;
  bool GetVerbose() const;

 private:
  const wchar_t* const* m_argv;  // unpermutated
  bool m_bUsage;
  bool m_bList;
  bool m_bVerbose;
  bool m_bVersion;
  const wchar_t* m_input;
  const CLSID* m_pSplitter;
  const CLSID* m_pSource;

  int Parse(wchar_t**);
  int ParseWindows(wchar_t**);
  int ParseShort(wchar_t**);
  int ParseLong(wchar_t**);
  void PrintUsage() const;
  void PrintVersion() const;
  void ListArgs() const;
  static std::wstring GetPath(const wchar_t*);
  static bool IsSwitch(const wchar_t*);
};

#endif  // WEBMDSHOW_PLAYWEBM_PLAYWEBMCMDLINE_H_
