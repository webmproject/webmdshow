// Copyright (c) 2010 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#pragma once

#ifndef NO_SHLWAPI_REG
#ifndef _INC_SHLWAPI
#include <shlwapi.h>
#endif
#pragma comment(lib, "shlwapi.lib")
#endif

namespace Registry
{
    //struct create_t
    //{
    //    const HKEY m_hKey;
    //    create_t(HKEY h) : m_hKey(h) {}
    //private:
    //    create_t(const create_t& c);
    //    create_t& operator=(const create_t&);
    //};

    class Key
    {
    public:

        Key();

        //open
        template<typename char_t>
        Key(HKEY, const char_t*, REGSAM = KEY_QUERY_VALUE);

        //open
        template<typename char_t>
        Key(HKEY, const std::basic_string<char_t>&, REGSAM = KEY_QUERY_VALUE);

        //template<typename char_t>
        //Key(const create_t&, const char_t*);  //create
        //
        //template<typename char_t>
        //Key(const create_t&, const std::basic_string<char_t>&);  //create

        ~Key();

        operator HKEY() const;

        template<typename char_t>
        LONG open(HKEY, const char_t*, REGSAM = KEY_QUERY_VALUE);

        template<typename char_t>
        LONG open(
                HKEY,
                const std::basic_string<char_t>&,
                REGSAM = KEY_QUERY_VALUE);

        template<typename char_t>
        LONG create(
                HKEY parent,
                const char_t* subkey,
                DWORD* disposition = 0);

        template<typename char_t>
        LONG create(
                HKEY parent,
                const std::basic_string<char_t>& subkey,
                DWORD* disposition = 0);

        template<typename char_t>
        LONG create(
                HKEY parent,
                const char_t* subkey,
                const char_t* object_type,
                DWORD options,
                REGSAM samDesired,
                const SECURITY_ATTRIBUTES*,
                DWORD* disposition = 0);  //out

        bool is_open() const;

        LONG close();

        template<typename char_t>
        LONG query(const char_t*, DWORD&) const;

        template<typename char_t>
        inline bool operator()(const char_t* name, DWORD& value) const
        {
            return (query<char_t>(name, value) == ERROR_SUCCESS);
        }

        template<typename char_t>
        LONG query(
                const char_t*,
                std::basic_string<char_t>&,
                DWORD = REG_SZ) const;

        template<typename char_t>
        inline bool operator()(
            const char_t* val,
            std::basic_string<char_t>& buf,
            DWORD t = REG_SZ) const
        {
            return (query<char_t>(val, buf, t) == ERROR_SUCCESS);
        }

        template<typename char_t>
        inline LONG query(
            std::basic_string<char_t>& buf,
            DWORD t = REG_SZ) const
        {
            return query<char_t>((const char_t*)0, buf, t);
        }

        template<typename char_t>
        inline bool operator()(
            std::basic_string<char_t>& buf,
            DWORD t = REG_SZ) const
        {
            return (query<char_t>(buf, t) == ERROR_SUCCESS);
        }

        template<typename char_t>
        LONG set(
            const char_t* name,
            DWORD value,
            DWORD type = REG_DWORD) const;

        template<typename char_t>
        LONG set(
            const char_t* name,
            const char_t* value,
            DWORD type = REG_SZ) const;

        template<typename char_t>
        inline LONG set(const char_t* value) const
        {
            const char_t* const default_name = 0;
            return set<char_t>(default_name, value, REG_SZ);
        }

        template<typename char_t>
        LONG setn(
            const char_t* name,
            const char_t* value,
            DWORD length_including_null,
            DWORD type = REG_SZ) const;

        template<typename char_t>
        inline LONG setn(const char_t* val, DWORD len) const
        {
            const char_t* const default_name = 0;
            return setn<char_t>(default_name, val, len, REG_SZ);
        }

        template<typename char_t>
        LONG set(
            const char_t* name,
            const std::basic_string<char_t>&,
            DWORD type = REG_SZ) const;

        //for binary data:
        //LONG set(
        //    const TCHAR* name,
        //    const void* data,
        //    DWORD size);

    private:

        Key(const Key&);
        Key& operator=(const Key&);

        HKEY m_hKey;

    };

    template<typename char_t>
    LONG DeleteKey(HKEY, const std::basic_string<char_t>&);

    template<>
    inline LONG DeleteKey(HKEY h, const std::string& k)
    {
        return ::RegDeleteKeyA(h, k.c_str());
    }

    template<>
    inline LONG DeleteKey(HKEY h, const std::wstring& k)
    {
        return ::RegDeleteKeyW(h, k.c_str());
    }

    template<typename char_t>
    LONG OpenKey(HKEY, const char_t*, REGSAM, HKEY&);

    template<>
    inline LONG OpenKey(HKEY h, const char* k, REGSAM sam, HKEY& hh)
    {
        return RegOpenKeyExA(h, k, 0, sam, &hh);
    }

    template<>
    inline LONG OpenKey(HKEY h, const wchar_t* k, REGSAM sam, HKEY& hh)
    {
        return RegOpenKeyExW(h, k, 0, sam, &hh);
    }

    template<typename char_t>
    LONG CreateKey(
            HKEY,
            const char_t*,
            char_t*,
            DWORD,
            REGSAM,
            LPSECURITY_ATTRIBUTES,
            HKEY&,
            DWORD*);

    template<>
    inline LONG CreateKey(
            HKEY h,
            const char* k,
            char* t,
            DWORD opts,
            REGSAM sam,
            LPSECURITY_ATTRIBUTES p,
            HKEY& hh,
            DWORD* dw)
    {
        return RegCreateKeyExA(h, k, 0, t, opts, sam, p, &hh, dw);
    }

    template<>
    inline LONG CreateKey(
            HKEY h,
            const wchar_t* k,
            wchar_t* t,
            DWORD opts,
            REGSAM sam,
            LPSECURITY_ATTRIBUTES p,
            HKEY& hh,
            DWORD* dw)
    {
        return RegCreateKeyExW(h, k, 0, t, opts, sam, p, &hh, dw);
    }


    template<typename char_t>
    LONG QueryValue(HKEY, const char_t*, DWORD&, BYTE*, DWORD&);

    template<>
    inline LONG QueryValue(
        HKEY h,
        const char* n,
        DWORD& t,
        BYTE* p,
        DWORD& cb)
    {
        return RegQueryValueExA(h, n, 0, &t, p, &cb);
    }

    template<>
    inline LONG QueryValue(
        HKEY h,
        const wchar_t* n,
        DWORD& t,
        BYTE* p,
        DWORD& cb)
    {
        return RegQueryValueExW(h, n, 0, &t, p, &cb);
    }


    template<typename char_t>
    LONG SetValue(HKEY, const char_t*, DWORD, const BYTE*, DWORD);

    template<>
    inline LONG SetValue(
        HKEY h,
        const char* n,
        DWORD t,
        const BYTE* p,
        DWORD cb)
    {
        return RegSetValueExA(h, n, 0, t, p, cb);
    }

    template<>
    inline LONG SetValue(
        HKEY h,
        const wchar_t* n,
        DWORD t,
        const BYTE* p,
        DWORD cb)
    {
        return RegSetValueExW(h, n, 0, t, p, cb);
    }

    template<typename char_t>
    LONG DeleteKey(HKEY, const char_t*);

    template<>
    inline LONG DeleteKey(HKEY h, const char* k)
    {
        return ::RegDeleteKeyA(h, k);
    }

    template<>
    inline LONG DeleteKey(HKEY h, const wchar_t* k)
    {
        return ::RegDeleteKeyW(h, k);
    }

    template<typename char_t>
    LONG DeleteKey(HKEY, const std::basic_string<char_t>&);


#ifndef NO_SHLWAPI_REG

    //template<typename char_t>
    //DWORD DeleteKeyAll(HKEY, const char_t*);
    //
    //template<>
    //inline DWORD DeleteKeyAll(HKEY h, const char* k)
    //{
    //    return ::SHDeleteKeyA(h, k);
    //}

    //template<>
    //inline DWORD DeleteKeyAll(HKEY h, const wchar_t* k)
    //{
    //    return ::SHDeleteKeyW(h, k);
    //}

    //template<typename char_t>
    //DWORD DeleteKeyAll(HKEY, const std::basic_string<char_t>&);

    //template<>
    //inline DWORD DeleteKeyAll(HKEY h, const std::string& k)
    //{
    //    return ::SHDeleteKeyA(h, k.c_str());
    //}

    //template<>
    //inline DWORD DeleteKeyAll(HKEY h, const std::wstring& k)
    //{
    //    return ::SHDeleteKeyW(h, k.c_str());
    //}

    inline DWORD SHDeleteKey(HKEY h, const std::string& k)
    {
        return ::SHDeleteKeyA(h, k.c_str());
    }

    inline DWORD SHDeleteKey(HKEY h, const std::wstring& k)
    {
        return ::SHDeleteKeyW(h, k.c_str());
    }

#endif  // NO_SHLWAPI_REG
}


inline Registry::Key::Key()
    : m_hKey(0)
{
}

template<typename char_t>
inline Registry::Key::Key(HKEY hKey, const char_t* subkey, REGSAM sam)
    : m_hKey(0)
{
    OpenKey<char_t>(hKey, subkey, sam, m_hKey);
}


template<typename char_t>
inline Registry::Key::Key(
    HKEY h,
    const std::basic_string<char_t>& k,
    REGSAM sam)
    : m_hKey(0)
{
    OpenKey<char_t>(h, k.c_str(), sam, m_hKey);
}


template<typename char_t>
inline LONG Registry::Key::open(
    HKEY h,
    const char_t* k,
    REGSAM sam)
{
    close();
    return OpenKey<char_t>(h, k, sam, m_hKey);
}


inline Registry::Key::~Key()
{
    close();
}


inline LONG Registry::Key::close()
{
    if (m_hKey == 0)
        return 0;

    const LONG status = RegCloseKey(m_hKey);

    m_hKey = 0;

    return status;
}


inline bool Registry::Key::is_open() const
{
    return (m_hKey != 0);
}


inline Registry::Key::operator HKEY() const
{
    return m_hKey;
}


template<typename char_t>
inline LONG Registry::Key::create(
    HKEY h,
    const char_t* k,
    DWORD* disposition)
{
    close();

    return CreateKey<char_t>(
            h,
            k,
            0, //class (object type)
            REG_OPTION_NON_VOLATILE,  //options
            KEY_ALL_ACCESS,           //samDesired
            0,                        //security attributes
            m_hKey,
            disposition);
}


template<typename char_t>
inline LONG Registry::Key::create(
    HKEY parent,
    const std::basic_string<char_t>& subkey,
    DWORD* disposition)
{
    return create(parent, subkey.c_str(), disposition);
}


template<typename char_t>
inline LONG Registry::Key::create(
    HKEY hKey,
    const char_t* subkey,
    const char_t* const_object_type,
    DWORD options,
    REGSAM samDesired,
    const SECURITY_ATTRIBUTES* const_security_attributes,
    DWORD* disposition)
{
    close();

    char_t* const object_type = const_cast<char_t*>(const_object_type);

    SECURITY_ATTRIBUTES* const security_attributes =
        const_cast<SECURITY_ATTRIBUTES*>(const_security_attributes);

    return CreateKey<char_t>(
            hKey,
            subkey,
            object_type,
            options,
            samDesired,
            security_attributes,
            m_hKey,
            disposition);
}


template<typename char_t>
inline LONG Registry::Key::set(
    const char_t* name,
    const std::basic_string<char_t>& str,
    DWORD type) const
{
    const char_t* const val = str.c_str();
    const std::basic_string<char_t>::size_type len_ = str.length() + 1;
    const DWORD len = static_cast<DWORD>(len_);

    return setn<char_t>(name, val, len, type);
}


template<typename char_t>
inline LONG Registry::Key::open(
    HKEY h,
    const std::basic_string<char_t>& k,
    REGSAM sam)
{
    return open<char_t>(h, k.c_str(), sam);
}

//inline DWORD Registry::DeleteSubkey(
//    HKEY hKey,
//    const std::string& name)
//{
//    return SH
//}



template<typename char_t>
inline LONG Registry::Key::query(const char_t* n, DWORD& data) const
{
    DWORD type;

    BYTE* const p = reinterpret_cast<BYTE*>(&data);
    DWORD cb = sizeof data;

    const LONG status = QueryValue<char_t>(m_hKey, n, type, p, cb);

    if (status != ERROR_SUCCESS)
        return status;

    if (type != REG_DWORD)
        return ERROR_FILE_NOT_FOUND;

    //assert(cb == sizeof data);

    return 0;
}


template<typename char_t>
inline LONG Registry::Key::query(
    const char_t* name,
    std::basic_string<char_t>& str,
    DWORD type_) const
{
    DWORD cb = 64;

    for (;;)
    {
        //void* const buf_ = _alloca(cb);
        //BYTE* const buf = static_cast<BYTE*>(buf_);

        BYTE* const buf = new (std::nothrow) BYTE[cb];

        if (buf == 0)
            return ERROR_NOT_ENOUGH_MEMORY;

        DWORD type;

        const LONG status = QueryValue<char_t>(
                                m_hKey,
                                name,
                                type,
                                buf,
                                cb);

        if (status == ERROR_SUCCESS)
        {
            if (type != type_)
            {
                delete[] buf;
                return ERROR_FILE_NOT_FOUND;
            }

            if (cb % sizeof(char_t))
            {
                delete[] buf;
                return ERROR_FILE_NOT_FOUND;
            }

            if (cb == 0)
            {
                str.clear();
                delete[] buf;

                return 0;
            }

            cb /= sizeof(char_t);  //convert from bytes to chars
            --cb;                //convert from buflen to strlen

            char_t* const val = (char_t*)buf;

            if (val[cb])  //verify terminating null is present
            {
                delete[] buf;
                return ERROR_FILE_NOT_FOUND;
            }

            str.assign(val, cb);
            delete[] buf;

            return 0;
        }

        delete[] buf;

        if (status == ERROR_MORE_DATA)
            continue;

        return status;
    }
}


template<typename char_t>
inline LONG Registry::Key::set(
    const char_t* name,
    DWORD value,
    DWORD type) const
{
    const BYTE* const p = reinterpret_cast<const BYTE*>(&value);
    const DWORD cb = sizeof value;

    return SetValue<char_t>(m_hKey, name, type, p, cb);
}


template<>
inline LONG Registry::Key::set(
    const char* name,
    const char* val,
    DWORD type) const
{
    const BYTE* const p = reinterpret_cast<const BYTE*>(val);
    const size_t cb = val ? strlen(val) + 1 : 0;

    return SetValue<char>(m_hKey, name, type, p, DWORD(cb));
}


template<>
inline LONG Registry::Key::set(
    const wchar_t* name,
    const wchar_t* val,
    DWORD type) const
{
    const BYTE* const p = reinterpret_cast<const BYTE*>(val);

    const size_t len = val ? wcslen(val) + 1 : 0;
    const size_t cb = len * sizeof(wchar_t);

    return SetValue<wchar_t>(m_hKey, name, type, p, DWORD(cb));
}


template<>
inline LONG Registry::Key::setn(
    const char* name,
    const char* val,
    DWORD cb,
    DWORD type) const
{
    const BYTE* const p = reinterpret_cast<const BYTE*>(val);

    return SetValue<char>(m_hKey, name, type, p, cb);
}


template<>
inline LONG Registry::Key::setn(
    const wchar_t* name,
    const wchar_t* val,
    DWORD length_including_null,
    DWORD type) const
{
    const BYTE* const p = reinterpret_cast<const BYTE*>(val);
    const DWORD cb = length_including_null * sizeof(wchar_t);

    return SetValue<wchar_t>(m_hKey, name, type, p, cb);
}
