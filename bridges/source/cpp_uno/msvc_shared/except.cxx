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

#include <sal/config.h>

#include <memory>

#include <malloc.h>
#include <new.h>
#include <typeinfo>
#include <signal.h>

#include <rtl/alloc.h>
#include <rtl/strbuf.hxx>
#include <rtl/ustrbuf.hxx>
#include <sal/log.hxx>
#include <osl/mutex.hxx>

#include <com/sun/star/uno/Any.hxx>
#include <unordered_map>
#include <except.hxx>
#include <msvc/except.hxx>

#if defined(_M_IX86)
#include <msvc/x86.hxx>
#elif defined(_M_AMD64)
#include <msvc/amd64.hxx>
#elif defined(_M_ARM64)
#include <msvc/arm64.hxx>
#else
#error "Unsupported machine type"
#endif

#pragma pack(push, 8)

using namespace ::com::sun::star;

ExceptionTypeInfo::~ExceptionTypeInfo() throw() { (void)m_data; }

ExceptionTypeDescriptor* RTTInfos::insert_new_exception_type_descriptor(OUString const& rUNOname)
{
    // insert new type_info
    OString aRawName(OUStringToOString(toRTTIname(rUNOname), RTL_TEXTENCODING_ASCII_US));
    ExceptionTypeDescriptor* pRTTI
        = new (std::malloc(sizeof(ExceptionTypeDescriptor) + aRawName.getLength()))
            ExceptionTypeDescriptor(nullptr, aRawName.getStr());

    // put into map
    std::pair<t_string2PtrMap::iterator, bool> insertion(
        m_allRTTI.insert(t_string2PtrMap::value_type(rUNOname, pRTTI)));
    assert(insertion.second && "### rtti insertion failed?!");

    return pRTTI;
}

type_info* RTTInfos::getRTTI(OUString const& rUNOname) throw()
{
    // a must be
    static_assert(sizeof(ExceptionTypeInfo) == sizeof(type_info),
                  "### type info structure size differ!");

    osl::MutexGuard aGuard(m_aMutex);
    t_string2PtrMap::const_iterator const iFind(m_allRTTI.find(rUNOname));

    // check if type is already available
    if (iFind == m_allRTTI.end())
    {
        // Wrap new ExceptionTypeInfo in ExceptionTypeDescriptor to preserve length info
        ExceptionTypeDescriptor* pRTTI = insert_new_exception_type_descriptor(rUNOname);
        return pRTTI->get_type_info();
    }
    else
    {
        return static_cast<ExceptionTypeDescriptor*>(iFind->second)->get_type_info();
    }
}

int RTTInfos::getRTTI_len(OUString const& rUNOname) throw()
{
    osl::MutexGuard aGuard(m_aMutex);
    t_string2PtrMap::const_iterator const iFind(m_allRTTI.find(rUNOname));

    // Wrap new ExceptionTypeInfo in ExceptionTypeDescriptor to preserve length info
    // check if type is already available
    if (iFind == m_allRTTI.end())
    {
        // Wrap new ExceptionTypeInfo in ExceptionTypeDescriptor to preserve length info
        ExceptionTypeDescriptor* pRTTI = insert_new_exception_type_descriptor(rUNOname);
        return pRTTI->get_type_info_size();
    }
    else
    {
        return static_cast<ExceptionTypeDescriptor*>(iFind->second)->get_type_info_size();
    }
}

RTTInfos::RTTInfos() throw() {}

type_info* msvc_getRTTI(OUString const& rUNOname)
{
    static RTTInfos* s_pRTTIs = new RTTInfos();
    return s_pRTTIs->getRTTI(rUNOname);
}

DWORD ExceptionInfos::allocationGranularity = 0;

ExceptionInfos::ExceptionInfos() throw() {}

ExceptionInfos::~ExceptionInfos() throw()
{
    SAL_INFO("bridges", "> freeing exception infos... <");

    osl::MutexGuard aGuard(m_aMutex);
    for (auto& rEntry : m_allRaiseInfos)
        delete reinterpret_cast<RaiseInfo*>(rEntry.second);
}

RaiseInfo* ExceptionInfos::getRaiseInfo(typelib_TypeDescription* pTD) throw()
{
    static ExceptionInfos* s_pInfos = []() {
#ifdef _M_AMD64
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        allocationGranularity = systemInfo.dwAllocationGranularity;
#endif
        return new ExceptionInfos();
    }();

    assert(pTD
           && (pTD->eTypeClass == typelib_TypeClass_STRUCT
               || pTD->eTypeClass == typelib_TypeClass_EXCEPTION));

    RaiseInfo* pRaiseInfo;

    OUString const& rTypeName = OUString::unacquired(&pTD->pTypeName);
    osl::MutexGuard aGuard(s_pInfos->m_aMutex);
    t_string2PtrMap::const_iterator const iFind(s_pInfos->m_allRaiseInfos.find(rTypeName));
    if (iFind == s_pInfos->m_allRaiseInfos.end())
    {
        pRaiseInfo = new RaiseInfo(pTD);

        // Put into map
        std::pair<t_string2PtrMap::iterator, bool> insertion(s_pInfos->m_allRaiseInfos.insert(
            t_string2PtrMap::value_type(rTypeName, static_cast<void*>(pRaiseInfo))));
        assert(insertion.second && "### raise info insertion failed?!");
    }
    else
    {
        // Reuse existing info
        pRaiseInfo = static_cast<RaiseInfo*>(iFind->second);
    }

    return pRaiseInfo;
}

void msvc_raiseException(uno_Any* pUnoExc, uno_Mapping* pUno2Cpp)
{
    // no ctor/dtor in here: this leads to dtors called twice upon RaiseException()!
    // thus this obj file will be compiled without opt, so no inlining of
    // ExceptionInfos::getRaiseInfo()

    // construct cpp exception object
    typelib_TypeDescription* pTD = nullptr;
    TYPELIB_DANGER_GET(&pTD, pUnoExc->pType);

    void* pCppExc = alloca(pTD->nSize);
    ::uno_copyAndConvertData(pCppExc, pUnoExc->pData, pTD, pUno2Cpp);

    ULONG_PTR arFilterArgs[EH_EXCEPTION_PARAMETERS];
    arFilterArgs[0] = MSVC_magic_number;
    arFilterArgs[1] = reinterpret_cast<ULONG_PTR>(pCppExc);
    arFilterArgs[2] = reinterpret_cast<ULONG_PTR>(ExceptionInfos::getRaiseInfo(pTD));
#if EH_EXCEPTION_PARAMETERS == 4
    arFilterArgs[3] = reinterpret_cast<RaiseInfo*>(arFilterArgs[2])->_codeBase;
#endif

    // Destruct uno exception
    ::uno_any_destruct(pUnoExc, nullptr);
    TYPELIB_DANGER_RELEASE(pTD);

    // last point to release anything not affected by stack unwinding
    RaiseException(MSVC_ExceptionCode, EXCEPTION_NONCONTINUABLE, 4, arFilterArgs);
}

// This function does the same check as __CxxDetectRethrow from msvcrt (see its
// crt/src/vcruntime/mgdframe.cpp). But it does not alter the global state, i.e. it does not
// increment __ProcessingThrow, and so does not break following exception handling. We rely on the
// definition of EHExceptionRecord, PER_IS_MSVC_EH and PER_PTHROW, that are current as of msvcrt
// 2017 (14.14.26428).
static bool DetectRethrow(void* ppExcept)
{
    struct EHExceptionRecord
    {
        DWORD ExceptionCode;
        DWORD ExceptionFlags;
        struct _EXCEPTION_RECORD* ExceptionRecord;
        PVOID ExceptionAddress;
        DWORD NumberParameters;
#if EH_EXCEPTION_PARAMETERS == 3
        struct EHParameters
#else
        struct alignas(8)
#endif
        {
            DWORD magicNumber;
            PVOID pExceptionObject;
            PVOID pThrowInfo;
#if EH_EXCEPTION_PARAMETERS == 4
            PVOID pThrowImageBase;
#endif
        } params;
    };

    constexpr auto PER_IS_MSVC_EH = [](EHExceptionRecord* p) {
        constexpr DWORD EH_EXCEPTION_NUMBER
            = 0xE06D7363; // The NT Exception # that msvcrt uses ('msc' | 0xE0000000)
        constexpr DWORD EH_MAGIC_NUMBER1 = 0x19930520; // latest magic # in thrown object
        constexpr DWORD EH_MAGIC_NUMBER2
            = 0x19930521; // latest magic # in func info for exception specs
        constexpr DWORD EH_MAGIC_NUMBER3 = 0x19930522; // latest magic #

        return p->ExceptionCode == EH_EXCEPTION_NUMBER
               && p->NumberParameters == EH_EXCEPTION_PARAMETERS
               && (p->params.magicNumber == EH_MAGIC_NUMBER1
                   || p->params.magicNumber == EH_MAGIC_NUMBER2
                   || p->params.magicNumber == EH_MAGIC_NUMBER3);
    };

    constexpr auto PER_PTHROW = [](EHExceptionRecord* p) { return p->params.pThrowInfo; };

    EHExceptionRecord* pExcept;
    if (!ppExcept)
        return false;
    pExcept = *static_cast<EHExceptionRecord**>(ppExcept);
    if (PER_IS_MSVC_EH(pExcept) && PER_PTHROW(pExcept) == nullptr)
    {
        return true;
    }
    return false;
}

int msvc_filterCppException(EXCEPTION_POINTERS* pPointers, uno_Any* pUnoExc, uno_Mapping* pCpp2Uno)
{
    if (pPointers == nullptr)
        return EXCEPTION_CONTINUE_SEARCH;

    EXCEPTION_RECORD* pRecord = pPointers->ExceptionRecord;

    // Handle only C++ exceptions:
    if (pRecord == nullptr || pRecord->ExceptionCode != MSVC_ExceptionCode)
        return EXCEPTION_CONTINUE_SEARCH;

    const bool rethrow = DetectRethrow(&pRecord);
    assert(pRecord == pPointers->ExceptionRecord);

    if (rethrow && pRecord == pPointers->ExceptionRecord)
    {
        pRecord = *reinterpret_cast<EXCEPTION_RECORD**>(__current_exception());
    }

    // Rethrow: handle only C++ exceptions:
    if (pRecord == nullptr || pRecord->ExceptionCode != MSVC_ExceptionCode)
        return EXCEPTION_CONTINUE_SEARCH;

    if (pRecord->NumberParameters == EH_EXCEPTION_PARAMETERS
#if EH_EXCEPTION_PARAMETERS == 4
        && pRecord->ExceptionInformation[0] == MSVC_magic_number
#endif
        && pRecord->ExceptionInformation[1] != 0 && pRecord->ExceptionInformation[2] != 0
#if EH_EXCEPTION_PARAMETERS == 4
        && pRecord->ExceptionInformation[3] != 0
#endif
    )
    {
        // ExceptionInformation[1] is the address of the thrown object
        // (or the address of a pointer to it, in most cases when it
        // is a C++ class, obviously).

        // [2] is the pThrowInfo pointer
        RaiseInfo* pInfo = reinterpret_cast<RaiseInfo*>(pRecord->ExceptionInformation[2]);

#if EH_EXCEPTION_PARAMETERS == 3
        ULONG_PTR base = 0;
#else
        // [3] is the image base address which is added the 32-bit
        // rva_t fields in throwinfo to get actual 64-bit addresses
        ULONG_PTR base = pRecord->ExceptionInformation[3];
#endif
        DWORD* types = base + reinterpret_cast<DWORD*>(pInfo->_types);
        if (types != nullptr && types[0] != 0 && types[1] != 0)
        {
            ExceptionType* et = reinterpret_cast<ExceptionType*>(base + types[1]);
            if (et->_pTypeInfo != 0)
            {
                OUString aRTTIname(OStringToOUString(
                    reinterpret_cast<ExceptionTypeInfo*>(base + et->_pTypeInfo)->m_d_name,
                    RTL_TEXTENCODING_ASCII_US));
                OUString aUNOname(toUNOname(aRTTIname));

                typelib_TypeDescription* pExcTD = nullptr;
                typelib_typedescription_getByName(&pExcTD, aUNOname.pData);
                if (pExcTD == nullptr)
                {
                    OUString sMsg = "[mscx_uno bridge error] UNO type of C++ exception unknown: \""
                                    + aUNOname + "\", RTTI-name=\"" + aRTTIname + "\"!";

                    uno::RuntimeException exc(sMsg);
                    uno_type_any_constructAndConvert(
                        pUnoExc, &exc, cppu::UnoType<decltype(exc)>::get().getTypeLibType(),
                        pCpp2Uno);
                }
                else
                {
                    // construct uno exception any
                    uno_any_constructAndConvert(
                        pUnoExc, reinterpret_cast<void*>(pRecord->ExceptionInformation[1]), pExcTD,
                        pCpp2Uno);
                    typelib_typedescription_release(pExcTD);
                }

                return EXCEPTION_EXECUTE_HANDLER;
            }
        }
    }

    // though this unknown exception leaks now, no user-defined exception
    // is ever thrown through the binary C-UNO dispatcher call stack.
    uno::RuntimeException exc("[mscx_uno bridge error] unexpected C++ exception occurred!");
    uno_type_any_constructAndConvert(
        pUnoExc, &exc, cppu::UnoType<decltype(exc)>::get().getTypeLibType(), pCpp2Uno);
    return EXCEPTION_EXECUTE_HANDLER;
}

#pragma pack(pop)
