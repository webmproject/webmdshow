// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifdef _DEBUG
#include <cassert>
#include <vector>
#ifdef _WIN32
#include "mferror.h"
#include "windows.h"
#include "mmsystem.h"
#include "mmreg.h"
#endif
#include "wfxrawwriter.hpp"

namespace WebmMfVorbisDecLib
{

WfxRawWriter::WfxRawWriter() : closed_(true)
{
  ::memset(&wave_format_, 0, sizeof WAVEFORMATEX);
}

WfxRawWriter::~WfxRawWriter()
{
  Close();
}

int WfxRawWriter::Open(const WAVEFORMATEX* ptr_wave_format)
{
  if (!ptr_wave_format)
    return E_POINTER;

  file_name_ = "out.wfxpcm";
  using std::ios;
  file_stream_.open(file_name_.c_str(), ios::binary);

  int result = E_FAIL;
  closed_ = !file_stream_.is_open();
  if (!closed_)
  {
    file_stream_ << 'W' << 'F' << 'X';
    const UINT32 num_bytes_to_write =
      sizeof WAVEFORMATEX + ptr_wave_format->cbSize;
    const char* const ptr_wfx_data = (char*)ptr_wave_format;
    file_stream_.write(ptr_wfx_data, num_bytes_to_write);
    result = Close();
  }
  return result;
}

int WfxRawWriter::Write(const BYTE* const ptr_byte_buffer, UINT32 byte_count)
{
  assert(ptr_byte_buffer);
  assert(byte_count > 0);
  if (!ptr_byte_buffer)
    return E_POINTER;
  if (byte_count <= 0)
    return E_INVALIDARG;
  if (Reopen_() == S_OK)
  {
    const char* const ptr_char_buffer = (const char*)ptr_byte_buffer;
    file_stream_.write(ptr_char_buffer, byte_count);
    Close();
  }
  return S_OK;
}

int WfxRawWriter::Reopen_()
{
  using std::ios;
  file_stream_.open(file_name_.c_str(), ios::binary | ios::ate | ios::app);
  closed_ = !file_stream_.is_open();
  assert(!closed_);
  if (closed_)
    return E_FAIL;
  return S_OK;
}

int WfxRawWriter::Close()
{
  int result = S_FALSE;
  if (!closed_)
  {
    file_stream_.flush();
    file_stream_.close();
    closed_ = true;
    result = S_OK;
  }
  return result;
}

} // WebmMfVorbisDecLib

#endif // _DEBUG
