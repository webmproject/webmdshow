// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include "webmmuxfilter.hpp"
#include "webmmuxstream.hpp"
#include "graphutil.hpp"
#include <vfwmsgs.h>
#include <cassert>
#ifdef _DEBUG
#include "odbgstream.hpp"
using std::endl;
using std::hex;
using std::dec;
#endif

//#define DEBUG_RECEIVE
//#define DEBUG_WAIT
//#define DEBUG_RECEIVE_MULTIPLE
    

namespace WebmMux
{


Inpin::Inpin(Filter* p, const wchar_t* id) : 
    Pin(p, id, PINDIR_INPUT),
    m_pStream(0)
{
    m_hSample = CreateEvent(0, 0, 0, 0);
    assert(m_hSample);  //TODO
    
    m_hStateChangeOrFlush = CreateEvent(0, 0, 0, 0);
    assert(m_hStateChangeOrFlush);  //TODO
}


Inpin::~Inpin()
{
    BOOL b = CloseHandle(m_hStateChangeOrFlush);
    assert(b);
    
    b = CloseHandle(m_hSample);
    assert(b);
}


void Inpin::Init()
{
   m_bFlush = false;
   m_bEndOfStream = false;
   
   BOOL b = ResetEvent(m_hSample);
   assert(b);
   
   b = ResetEvent(m_hStateChangeOrFlush);
   assert(b);

   assert(m_pStream == 0);

   if (!bool(m_pPinConnection))
      return;  //not connected, so nothing else to do

   const HRESULT hr = OnInit();
   hr;
   assert(SUCCEEDED(hr));
   assert(m_pStream);
}


//void Inpin::Stop()
//{
//    if (m_pStream)
//        m_pStream->Stop();
//}


void Inpin::Final()
{
    OnFinal();

    delete m_pStream;
    m_pStream = 0;
   
    const BOOL b = SetEvent(m_hStateChangeOrFlush);  //transition to stopped
    assert(b);
}


void Inpin::Run()
{
    const BOOL b = SetEvent(m_hStateChangeOrFlush);
    assert(b);
}


HRESULT Inpin::QueryInterface(const IID& iid, void** ppv)
{
    if (ppv == 0)
        return E_POINTER;
        
    IUnknown*& pUnk = reinterpret_cast<IUnknown*&>(*ppv);
    
    if (iid == __uuidof(IUnknown))
        pUnk = static_cast<IPin*>(this);
        
    else if (iid == __uuidof(IPin))
        pUnk = static_cast<IPin*>(this);
        
    else if (iid == __uuidof(IMemInputPin))
        pUnk = static_cast<IMemInputPin*>(this);
        
    else
    {
        pUnk = 0;
        return E_NOINTERFACE;
    }
    
    pUnk->AddRef();
    return S_OK;
}
    

ULONG Inpin::AddRef()
{
    return m_pFilter->AddRef();
}


ULONG Inpin::Release()
{
    return m_pFilter->Release();
}


HRESULT Inpin::Connect(IPin*, const AM_MEDIA_TYPE*)
{
    return E_UNEXPECTED;  //for output pins only
}


HRESULT Inpin::QueryInternalConnections( 
    IPin** pa,
    ULONG* pn)
{
    if (pn == 0)
        return E_POINTER;
        
    if (*pn == 0)
    {
        if (pa == 0)  //query for required number
        {
            *pn = 1;
            return S_OK;
        }
        
        return S_FALSE;  //means "insufficient number of array elements"
    }
    
    if (pa == 0)
    {
        *pn = 0;
        return E_POINTER;
    }
    
    IPin*& pin = pa[0];
    
    pin = &m_pFilter->m_outpin;
    pin->AddRef();
    
    *pn = 1;
    return S_OK;        
}


HRESULT Inpin::ReceiveConnection( 
    IPin* pin,
    const AM_MEDIA_TYPE* pmt)
{
    if ((pin == 0) || (pmt == 0))
        return E_POINTER;
        
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (m_pFilter->m_state != State_Stopped)
        return VFW_E_NOT_STOPPED;

    if (bool(m_pPinConnection))
        return VFW_E_ALREADY_CONNECTED;

    m_connection_mtv.Clear();

    hr = QueryAccept(pmt);
    
    if (hr != S_OK)
        return VFW_E_TYPE_NOT_ACCEPTED;
        
    const AM_MEDIA_TYPE& mt = *pmt;
        
    hr = OnReceiveConnection(pin, mt);  //dispatch to subclass
    
    if (FAILED(hr))
        return hr;

    hr = m_connection_mtv.Add(mt);
    
    if (FAILED(hr))
        return hr;
                    
    m_pPinConnection = pin;
    
    return S_OK;
}


HRESULT Inpin::EndOfStream()
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
        
    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_WRONG_STATE;  //?

    if (m_bEndOfStream)
        return S_FALSE;

    m_bEndOfStream = true;

    return OnEndOfStream();
}
    

HRESULT Inpin::BeginFlush()
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
        
    //TODO: check state?
    
    //I think flush is supposed to flush EOS indicators
    //too (they can be queued, just like sammples).
    
    if (m_bFlush)
        return S_FALSE;
        
    if (m_pStream)
        m_pStream->Flush();
    //TODO: destroy stream, then re-create it during EndFlush
        
    m_bFlush = true;
    
    const BOOL b = SetEvent(m_hStateChangeOrFlush);
    assert(b);
    
    return S_OK;
}
    

HRESULT Inpin::EndFlush()
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
        
    const BOOL b = ResetEvent(m_hStateChangeOrFlush);
    assert(b);

    if (!m_bFlush)
        return S_FALSE;  //?

    m_bFlush = false;
    
    //TODO: it's not clear whether this should also clear
    //the EOS status.
    
    //TODO: re-create stream object

    return S_OK;
}


HRESULT Inpin::NewSegment( 
    REFERENCE_TIME,
    REFERENCE_TIME,
    double)
{
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
        
    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
        
    //TODO: we could probably tell file object to render its
    //current cluster (and start a new one)
        
    return S_OK;  //TODO
}


HRESULT Inpin::GetAllocator(IMemAllocator** p)
{
    if (p)
        *p = 0;
        
    return VFW_E_NO_ALLOCATOR;  //?
}


HRESULT Inpin::NotifyAllocator( 
    IMemAllocator* pAllocator,
    BOOL)
{
    if (pAllocator == 0)
        return E_INVALIDARG;
        
    ALLOCATOR_PROPERTIES props;
    
    const HRESULT hr = pAllocator->GetProperties(&props);
    hr;
    assert(SUCCEEDED(hr));
    
#ifdef _DEBUG    
    wodbgstream os;
    os << "mkvmux::inpin[" 
       << m_id 
       << "]::NotifyAllocator: props.cBuffers="
       << props.cBuffers
       << " cbBuffer="
       << props.cbBuffer
       << " cbAlign="
       << props.cbAlign
       << " cbPrefix="
       << props.cbPrefix
       << endl;
#endif    
    
    return S_OK;
}


HRESULT Inpin::Receive(IMediaSample* pSample)
{
    if (pSample == 0)
        return E_INVALIDARG;

#ifdef DEBUG_RECEIVE
    {
        __int64 start_reftime, stop_reftime;
        const HRESULT hr = pSample->GetTime(&start_reftime, &stop_reftime);

        odbgstream os;
        os << "mkvmux::inpin::receive: ";
        
        if (hr == S_OK)
            os << "start=" << double(start_reftime) / 10000000
               << "; stop=" << double(stop_reftime) / 10000000;

        else if (hr == VFW_S_NO_STOP_TIME)
            os << "start=" << double(start_reftime) / 10000000;

        os << endl;
    }
#endif

    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
    
#ifdef DEBUG_RECEIVE
    wodbgstream os;
    os << L"inpin[" << m_id << "]::Receive: THREAD=0x"
       << hex << GetCurrentThreadId() << dec
       << endl;
#endif

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
    
    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_NOT_RUNNING;

    if (m_bEndOfStream)
        return VFW_E_SAMPLE_REJECTED_EOS;

    if (m_bFlush)
        return S_FALSE;
      
    hr = m_pStream->Receive(pSample);
    
    if (hr != S_OK)
        return hr;
    
    const BOOL b = SetEvent(m_hSample);  //notify other pin
    assert(b);
    
    return Wait(lock);
}


HRESULT Inpin::Wait(CLockable::Lock& lock)
{
    //TODO:
    //If pStream->Wait returns true, it implies that there's
    //another stream.  However, even without another pin
    //(that is, pStream->Wait returns false), we must still
    //wait if we're paused. 
    
    //If video stream is ahead of audio, then wait for audio.
    //Audio stream will signal when new sample is delivered.
    
    //If there's no audio stream connection, then don't wait.
    
    //If audio stream is EOS, then don't wait.
    
    //While we're waiting, we must wake up because:
    //  receipt of audio sample
    //  flush
    //  transition to stopped
    //  any other transition?
    
    //We must signal receipt of this video sample, so audio
    //streaming thread will wake up.
    
    //How do we detect whether video stream is ahead of audio?
    
    //If we wait, we must release the filter lock.
    
    //If we transition to stopped, then wake up and release caller.
    
    //If we're paused, then write frame, and block caller.
    //If we transition from paused, then wake up and release caller.
    
    const HANDLE hOther = GetOtherHandle();
    
    enum { cHandles = 2 };
    HANDLE hh[cHandles] = { m_hStateChangeOrFlush, hOther };
    
#ifdef DEBUG_WAIT
    wodbgstream os;
#endif
    
    for (;;)
    {
        if (m_bFlush)
        {
#ifdef DEBUG_WAIT
            os << L"inpin[" << m_id << "]::wait: FLUSH; EXITING" << endl;
#endif

            return S_FALSE;
        }
            
        const FILTER_STATE state = m_pFilter->m_state;
        
        if (state == State_Stopped)
        {
#ifdef DEBUG_WAIT
            os << L"inpin[" << m_id << "]::wait: STATE=STOPPED; EXITING" << endl;
#endif

            return VFW_E_NOT_RUNNING;
        }

        if ((state == State_Running) && !m_pStream->Wait())
        {
#ifdef DEBUG_WAIT
            os << L"inpin[" 
               << m_id 
               << "]::wait: STATE=RUNNING and !WAIT; EXITING; THREAD=0x"
               << hex << GetCurrentThreadId() << dec
               << endl;
#endif

            return S_OK;
        }

#ifdef DEBUG_WAIT
        os << L"inpin[" << m_id << "]::wait: state=" 
           << state
           << "; WAITING; THREAD=0x" 
           << hex << GetCurrentThreadId() << dec
           << endl;
#endif
        
        assert(bool(m_pPinConnection));
        assert(!m_bEndOfStream);
            
        lock.Release();
    
        DWORD index;
            
        HRESULT hr = CoWaitForMultipleHandles(
                        0,  //wait flags
                        INFINITE,
                        cHandles,
                        hh,
                        &index);
                
        assert(hr == S_OK);
        
        hr = lock.Seize(m_pFilter);
        
        if (FAILED(hr))       //should never happen
            return S_FALSE;   //what else can we do?
    }
}


HRESULT Inpin::ReceiveMultiple(   //TODO: handle wait here, instead of in Receive?
    IMediaSample** pSamples,
    long n,    //in
    long* pm)  //out
{
    if (pm == 0)
        return E_POINTER;
        
    long& m = *pm;    //out
    m = 0;
    
    if (n <= 0)
        return S_OK;  //weird
    
    if (pSamples == 0)
        return E_INVALIDARG;
        
    Filter::Lock lock;
    
    HRESULT hr = lock.Seize(m_pFilter);
    
    if (FAILED(hr))
        return hr;
    
#ifdef DEBUG_RECEIVE_MULTIPLE
    wodbgstream os;
    os << L"inpin[" << m_id << "]::ReceiveMultiple(begin): THREAD=0x"
       << hex << GetCurrentThreadId() << dec
       << endl;
#endif

    if (!bool(m_pPinConnection))
        return VFW_E_NOT_CONNECTED;
    
    if (m_pFilter->m_state == State_Stopped)
        return VFW_E_WRONG_STATE;  //?

    if (m_bEndOfStream)
        return VFW_E_SAMPLE_REJECTED_EOS;

    if (m_bFlush)
        return S_FALSE;
      
    for (long i = 0; i < n; ++i)
    {
        IMediaSample* const pSample = pSamples[i];
        assert(pSample);
        
#ifdef DEBUG_RECEIVE_MULTIPLE
        {
            os << L"inpin[" << m_id << "]::ReceiveMultiple(cont'd):";
            
            __int64 st, sp;
            
            HRESULT hr = pSample->GetTime(&st, &sp);
            
            if (hr == S_OK)
                os << " st=" << st << " sp=" << sp;
            else if (SUCCEEDED(hr))
                os << " st=" << st;
            else
                os << " (NO TIME SET)";
                
            os << "; PREROLL=" 
               << ((pSample->IsPreroll() == S_OK) ? "TRUE" : "FALSE");
               
            os << endl;
        }
#endif

        const HRESULT hr = m_pStream->Receive(pSample);
        
        if (hr != S_OK)
        {
            if (m > 0)
                break;
                
            return hr;
        }
        
        ++m;
    }

    const BOOL b = SetEvent(m_hSample);  //notify other pin
    assert(b);
    
    return Wait(lock);
}


HRESULT Inpin::ReceiveCanBlock()
{
    return S_OK;  //yes, we block caller
}



HRESULT Inpin::OnReceiveConnection(IPin*, const AM_MEDIA_TYPE&)
{
    return S_OK;
}


HRESULT Inpin::OnEndOfStream()
{
#if 0
    wodbgstream os;
    os << L"inpin[" << m_id << L"]::EndOfStream (before calling stream)" << endl;
#endif

    const int result = m_pStream->EndOfStream();
    
#if 0
    os << L"inpin[" << m_id << L"]::EndOfStream (after calling stream); result="
       << result
       << endl;
#endif

    const BOOL b = SetEvent(m_hSample);  //notify other pin
    assert(b);

    if (result <= 0)
        return S_OK;
        
    const HRESULT hr = m_pFilter->OnEndOfStream();
    hr;
    assert(SUCCEEDED(hr));
    
    return S_OK;
}

HRESULT Inpin::ResetPosition()
{
    //Filter locked by caller
    
    if (!bool(m_pPinConnection))
        return S_FALSE;
        
    const GraphUtil::IMediaSeekingPtr pSeek(m_pPinConnection);
    
    if (!bool(pSeek))
        return S_FALSE;
        
    LONGLONG tCurr = 0;
    
    const HRESULT hr = pSeek->SetPositions(
                        &tCurr, 
                        AM_SEEKING_AbsolutePositioning,
                        0,
                        AM_SEEKING_NoPositioning);
    
    return hr;
}

}  //end namespace WebmMux
