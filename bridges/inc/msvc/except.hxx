/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <sal/config.h>
#include <rtl/ustrbuf.hxx>
#include <osl/mutex.hxx>

#include <unordered_map>

class type_info;
typedef struct _uno_Any uno_Any;
typedef struct _uno_Mapping uno_Mapping;
typedef std::unordered_map<OUString, void*> t_string2PtrMap;

constexpr long MSVC_magic_number = 0x19930520L;
constexpr DWORD MSVC_ExceptionCode = 0xe06d7363;

#if defined(_M_IX86)
#define EH_EXCEPTION_PARAMETERS 3 // Number of parameters in exception record for x86
#elif defined(_M_AMD64) || defined(_M_ARM64)
#define EH_EXCEPTION_PARAMETERS 4 // Number of parameters in exception record for AMD64
#else
#error "Unsupported machine type"
#endif

class ExceptionTypeDescriptor;
struct RaiseInfo;

int msvc_filterCppException(EXCEPTION_POINTERS*, uno_Any*, uno_Mapping*);
void msvc_raiseException(uno_Any*, uno_Mapping*);

class RTTInfos
{
    osl::Mutex m_aMutex;
    t_string2PtrMap m_allRTTI;

    static OUString toRawName(OUString const& rUNOname) throw();

public:
    type_info* getRTTI(OUString const& rUNOname) throw();
    int getRTTI_len(OUString const& rUNOname) throw();
    ExceptionTypeDescriptor* insert_new_exception_type_descriptor(OUString const& rUNOname);

    RTTInfos();
    ~RTTInfos();
};

type_info* msvc_getRTTI(OUString const& rUNOname);

class ExceptionTypeInfo
{
    friend type_info* RTTInfos::getRTTI(OUString const&) throw();
    friend int msvc_filterCppException(EXCEPTION_POINTERS*, uno_Any*, uno_Mapping*);

    void* m_data;
    char m_d_name[1];

public:
    virtual ~ExceptionTypeInfo() throw();

    inline ExceptionTypeInfo(void* data, const char* d_name) throw()
        : m_data(data)
    {
        ::strcpy(m_d_name, d_name);
    } // #100211# - checked
};

class ExceptionTypeDescriptor
{
    int type_info_size;
    ExceptionTypeInfo info;

public:
    ExceptionTypeDescriptor(void* m_data, const char* m_d_name) throw()
        : info(m_data, m_d_name)
    {
        type_info_size = sizeof(ExceptionTypeInfo) + strlen(m_d_name);
    }

    type_info* get_type_info() { return reinterpret_cast<type_info*>(&info); }
    int get_type_info_size() { return type_info_size; }
};

class ExceptionInfos
{
    osl::Mutex m_aMutex;
    t_string2PtrMap m_allRaiseInfos;

public:
    static RaiseInfo* getRaiseInfo(typelib_TypeDescription* pTD) throw();

    static DWORD allocationGranularity;

    ExceptionInfos() throw();
    ~ExceptionInfos() throw();
};

inline OUString toUNOname(OUString const& rRTTIname) throw()
{
    OUStringBuffer aRet(64);
    OUString aStr(rRTTIname.copy(4, rRTTIname.getLength() - 4 - 2)); // filter .?AUzzz@yyy@xxx@@
    sal_Int32 nPos = aStr.getLength();
    while (nPos > 0)
    {
        sal_Int32 n = aStr.lastIndexOf('@', nPos);
        aRet.append(aStr.copy(n + 1, nPos - n - 1));
        if (n >= 0)
        {
            aRet.append('.');
        }
        nPos = n;
    }
    return aRet.makeStringAndClear();
}

inline OUString toRTTIname(OUString const& rUNOname) throw()
{
    OUStringBuffer aRet(64);
    aRet.appendAscii(".?AV"); // class ".?AV"; struct ".?AU"
    sal_Int32 nPos = rUNOname.getLength();
    while (nPos > 0)
    {
        sal_Int32 n = rUNOname.lastIndexOf('.', nPos);
        aRet.append(rUNOname.copy(n + 1, nPos - n - 1));
        aRet.append('@');
        nPos = n;
    }
    aRet.append('@');
    return aRet.makeStringAndClear();
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
