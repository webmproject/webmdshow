// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#ifndef __WEBMDSHOW_COMMON_MFUTIL_HPP__
#define __WEBMDSHOW_COMMON_MFUTIL_HPP__

namespace WebmMfUtil
{
// forward declarations
class MfByteStreamHandlerWrapper;
class MfTransformWrapper;
// basic MediaFoundation utility functions
HRESULT copy_media_type(IMFMediaType* ptr_src, IMFMediaType** ptr_dest);
HRESULT get_event_iunk_ptr(IMFMediaEvent* ptr_event, IUnknown** ptr_iunk);
HRESULT get_major_type(IMFStreamDescriptor* ptr_desc, GUID* ptr_type);
HRESULT get_media_type(IMFStreamDescriptor* ptr_desc,
                       IMFMediaType** ptr_type);
HRESULT get_sub_type(IMFStreamDescriptor* ptr_desc, GUID* ptr_type);
HRESULT mf_startup();
HRESULT mf_shutdown();
// WebM MediaFoundation Component specific utility functions
HRESULT get_webm_vorbis_sample(MfByteStreamHandlerWrapper* ptr_source,
                               IMFSample** ptr_sample);
HRESULT get_webm_vp8_sample(MfByteStreamHandlerWrapper* ptr_source,
                            IMFSample** ptr_sample);
HRESULT open_webm_source(const std::wstring& dll_path, const std::wstring& url,
                         MfByteStreamHandlerWrapper** ptr_wrapper_instance);
HRESULT open_webm_decoder(const std::wstring& dll_path, const GUID& clsid,
                          MfTransformWrapper** ptr_decoder_instance);
HRESULT setup_webm_decode(MfByteStreamHandlerWrapper* ptr_source,
                          MfTransformWrapper* ptr_decoder,
                          const GUID& major_type);
HRESULT setup_webm_vorbis_decoder(const std::wstring& url,
                                  MfByteStreamHandlerWrapper** ptr_source,
                                  MfTransformWrapper** ptr_decoder);
HRESULT setup_webm_vp8_decoder(const std::wstring& url,
                               MfByteStreamHandlerWrapper** ptr_source,
                               MfTransformWrapper** ptr_decoder);
} // WebmMfUtil namespace

#endif // __WEBMDSHOW_COMMON_MFUTIL_HPP__
