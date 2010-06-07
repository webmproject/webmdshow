// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once
#include "webmmuxpin.hpp"
#include "clockable.hpp"

namespace WebmMux
{

class Stream;

class Inpin : public Pin, public IMemInputPin
{
    Inpin(const Inpin&);
    Inpin& operator=(const Inpin&);
    
protected:
    
    Inpin(Filter*, const wchar_t*);
    ~Inpin();
    
public:

    void Init();
    //void Stop();
    void Final();
    void Run();

    //IUnknown interface:
    
    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();
    
    //IPin interface:

    HRESULT STDMETHODCALLTYPE Connect(IPin*, const AM_MEDIA_TYPE*);

    //HRESULT STDMETHODCALLTYPE Disconnect();

    HRESULT STDMETHODCALLTYPE ReceiveConnection( 
        IPin*,
        const AM_MEDIA_TYPE*);
        
    HRESULT STDMETHODCALLTYPE QueryInternalConnections( 
        IPin**,
        ULONG*);
        
    HRESULT STDMETHODCALLTYPE EndOfStream();

    HRESULT STDMETHODCALLTYPE BeginFlush();    

    HRESULT STDMETHODCALLTYPE EndFlush();
    
    HRESULT STDMETHODCALLTYPE NewSegment( 
        REFERENCE_TIME,
        REFERENCE_TIME,
        double);
        
    //IMemInputPin
    
    HRESULT STDMETHODCALLTYPE GetAllocator(
        IMemAllocator**);
    
    HRESULT STDMETHODCALLTYPE NotifyAllocator( 
        IMemAllocator*,
        BOOL);
    
    //HRESULT STDMETHODCALLTYPE GetAllocatorRequirements( 
    //    ALLOCATOR_PROPERTIES*);

    HRESULT STDMETHODCALLTYPE Receive(IMediaSample*);    
    
    HRESULT STDMETHODCALLTYPE ReceiveMultiple( 
        IMediaSample**,
        long,
        long*);
    
    HRESULT STDMETHODCALLTYPE ReceiveCanBlock();
    
    //local members
    
    HRESULT ResetPosition();

    HANDLE m_hSample;

protected:

   virtual HRESULT OnReceiveConnection(IPin*, const AM_MEDIA_TYPE&);
   virtual HRESULT OnEndOfStream();
    
private:

   bool m_bEndOfStream;
   bool m_bFlush;
    
protected:

    Stream* m_pStream;
    virtual HRESULT OnInit() = 0;
    virtual void OnFinal() = 0;
   
    HRESULT Wait(CLockable::Lock&);   
    virtual HANDLE GetOtherHandle() const = 0;
   
    HANDLE m_hStateChangeOrFlush;

};
    
}  //end namespace WebmMux

