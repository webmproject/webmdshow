// Copyright (c) 2011 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

#include <windows.h>

#include <cassert>
#include <memory>
#include <string>
#include <sstream>

#include "omahautil.h"
#include "debugutil.h"
//#include "memutil.h"
#include "registry.h"

namespace WebmUtil
{

class OmahaStats
{
public:
    OmahaStats();
    ~OmahaStats();
    HRESULT SetUsageFlags(const GUID& app_id);
private:
    bool UserHasEnabledStats_();
    HRESULT CreateHkcuClientStateSubKey_();
    HRESULT MakeAppKeyStr_();
    HRESULT SetUsageFlags_();
    bool stats_enabled_;
    GUID app_id_guid_;
    Registry::Key app_key_;  // App's key (might be in HKLM)
    Registry::Key user_key_; // App's key in HKCU
    std::wstring app_key_str_;
    DISALLOW_COPY_AND_ASSIGN(OmahaStats);
};

} // WebmUtil namespace

HRESULT WebmUtil::set_omaha_usage_flags(const GUID& app_id)
{
    OmahaStats stats;
    return stats.SetUsageFlags(app_id);
}

namespace WebmUtil
{

const int kOmahaAppIdStrLen = 40;

OmahaStats::OmahaStats():
  app_id_guid_(GUID_NULL),
  stats_enabled_(false)
{
}

OmahaStats::~OmahaStats()
{
}

HRESULT OmahaStats::SetUsageFlags(const GUID& app_id)
{
    HRESULT hr;
    // store the user guid
    app_id_guid_ = app_id;
    // let's make our app sub key string...
    CHK(hr, MakeAppKeyStr_());
    if (FAILED(hr))
    {
        return hr;
    }
    // survived putting the sub key string in |app_key_str_|, let's see if
    // the user enabled stats at install time...
    if (UserHasEnabledStats_() == false)
    {
        // nope! Everything's ok, nothing to see here...
        return S_OK;
    }
    // stats enabled!  Let's set the usage flags...
    CHK(hr, SetUsageFlags_());
    return hr;
}

bool OmahaStats::UserHasEnabledStats_()
{
    if (GUID_NULL != app_id_guid_ && app_key_str_.length())
    {
        // We support Omaha client installs that live in HKLM and HKCU...
        const int num_keys =  2;
        HKEY keys_to_check[num_keys] = {HKEY_CURRENT_USER, HKEY_LOCAL_MACHINE};
        for (int i = 0; i < num_keys; ++i)
        {
            // loop through |keys_to_check| and try each with the generated
            // key string in |app_key_str_|
            LONG reg_error = app_key_.open(keys_to_check[i], app_key_str_);
            if (ERROR_SUCCESS == reg_error)
            {
                assert(app_key_.is_open());
                // key is open, let's read (query) the usagestats val...
                DWORD usage_stats_value = 0;
                reg_error = app_key_.query("usagestats", usage_stats_value);
                if (ERROR_SUCCESS == reg_error && usage_stats_value)
                {
                    // the user enabled usage tracking at install time
                    stats_enabled_ = true;
                }
            }
        }
    }
    return stats_enabled_;
}

HRESULT OmahaStats::CreateHkcuClientStateSubKey_()
{
    // .. and set the "dr" flag
    // TODO(tomfinegan): flag setting should probably move elsewhere, and
    //                   wrapping the subkey creation is sort of silly given
    //                   how easy matthewjheaney's Registry class is to use...
    LONG reg_error = user_key_.create(HKEY_CURRENT_USER, app_key_str_);
    if (ERROR_SUCCESS == reg_error)
    {
        reg_error = user_key_.set(L"dr", L"1", REG_SZ);
    }
    return ERROR_SUCCESS == reg_error ? S_OK : E_FAIL;
}

// Build the subkey path for the Omaha application we're working with using
// the GUID passed to |WebmUtil::set_omaha_usage_flag|
HRESULT OmahaStats::MakeAppKeyStr_()
{
    if (GUID_NULL == app_id_guid_)
    {
        DBGLOG("need a valid guid.");
        return E_INVALIDARG;
    }
    wchar_t app_id_wchars[kOmahaAppIdStrLen] = {0};
    int wchars_converted = StringFromGUID2(app_id_guid_, &app_id_wchars[0],
                                           kOmahaAppIdStrLen);
    assert(wchars_converted > 0);
    if (!wchars_converted)
    {
        DBGLOG("conversion of app_id to string failed.");
        return E_FAIL;
    }

    std::wostringstream key_stream;
    key_stream << "Software\\";

#ifdef _WIN64
    // At present we use a 32 bit NSIS installer, and it writes to the tree
    // under Wow6432Node when the NSIS registry utility functions are used.
    // Omaha reads the values from there as well, so let's keep it simple and
    // write there from our 64bit builds as well.
    key_stream << L"Wow6432Node\\";
#endif

    key_stream << L"Google\\Update\\ClientState\\" << app_id_wchars;
    app_key_str_ = key_stream.str();
    return S_OK;
}

// Note: SetUsageFlags_ is a bit of a misnomer-- we're setting only the "dr"
//       flag, which tells Omaha that the tracked application has been run
//       since it was last updated.
HRESULT OmahaStats::SetUsageFlags_()
{
    REGSAM access_flags = KEY_QUERY_VALUE | KEY_SET_VALUE;
    LONG reg_error = user_key_.open(HKEY_CURRENT_USER, app_key_str_,
                                    access_flags);
    if (ERROR_FILE_NOT_FOUND == reg_error)
    {
        HRESULT hr;
        // Note: |CreateHkcuClientStateSubKey_| sets the "dr" flag
        CHK(hr, CreateHkcuClientStateSubKey_());
        return hr;
    }
    else if (ERROR_SUCCESS == reg_error)
    {
        // Set only if not present. This should help prevent repeated nag
        // dialogs for users running registry monitoring software that notifies
        // on all registry writes.
        std::wstring dr_value;
        reg_error = user_key_.query(L"dr", dr_value);
        if (ERROR_FILE_NOT_FOUND == reg_error)
        {
            reg_error = user_key_.set(L"dr", L"1", REG_SZ);
        }
    }
    return ERROR_SUCCESS == reg_error ? S_OK : E_FAIL;
}

} // WebmUtil namespace
