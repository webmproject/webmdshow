// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license and patent
// grant that can be found in the LICENSE file in the root of the source
// tree. All contributing project authors may be found in the AUTHORS
// file in the root of the source tree.

#pragma once
#include <objbase.h>

class CFactory : public IClassFactory
{
public:

    typedef HRESULT (*create_t)(IClassFactory*, IUnknown*, const IID&, void**);

    CFactory(ULONG*, create_t);
    virtual ~CFactory();

    HRESULT STDMETHODCALLTYPE QueryInterface(const IID&, void**);
    ULONG STDMETHODCALLTYPE AddRef();
    ULONG STDMETHODCALLTYPE Release();    

    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown*, const IID&, void**);    
    HRESULT STDMETHODCALLTYPE LockServer(BOOL);
    
private:

    const create_t m_create;
    
    ULONG* const m_pcLock;
    ULONG m_cRef;
    
    CFactory(const CFactory&);
    CFactory& operator=(const CFactory&);

};    
