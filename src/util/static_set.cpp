/*  $Id$
 * ===========================================================================
 *
 *                            PUBLIC DOMAIN NOTICE
 *               National Center for Biotechnology Information
 *
 *  This software/database is a "United States Government Work" under the
 *  terms of the United States Copyright Act.  It was written as part of
 *  the author's official duties as a United States Government employee and
 *  thus cannot be copyrighted.  This software/database is freely available
 *  to the public for use. The National Library of Medicine and the U.S.
 *  Government have not placed any restriction on its use or reproduction.
 *
 *  Although all reasonable efforts have been taken to ensure the accuracy
 *  and reliability of the software and data, the NLM and the U.S.
 *  Government do not and cannot warrant the performance or results that
 *  may be obtained by using this software or data. The NLM and the U.S.
 *  Government disclaim all warranties, express or implied, including
 *  warranties of performance, merchantability or fitness for any particular
 *  purpose.
 *
 *  Please cite the author in any work or product based on this material.
 *
 * ===========================================================================
 *
 * Author: Eugene Vasilchenko
 *
 * File Description:  Implementation of CStaticSet/CStaticMap
 *
 */

#include <ncbi_pch.hpp>
#include <util/static_set.hpp>
#include <corelib/ncbi_param.hpp>
#include <corelib/ncbi_stack.hpp>

BEGIN_NCBI_NAMESPACE;


NCBI_PARAM_DEF_EX(bool, NCBI, STATIC_ARRAY_COPY_WARNING, false,
                  eParam_NoThread, NCBI_STATIC_ARRAY_COPY_WARNING);


NCBI_PARAM_DEF_EX(bool, NCBI, STATIC_ARRAY_UNSAFE_TYPE_WARNING, true,
                  eParam_NoThread, NCBI_STATIC_ARRAY_UNSAFE_TYPE_WARNING);


BEGIN_NAMESPACE(NStaticArray);


DEFINE_CLASS_STATIC_FAST_MUTEX(IObjectConverter::sx_InitMutex);


IObjectConverter::~IObjectConverter(void) THROWS_NONE
{
}


CArrayHolder::CArrayHolder(IObjectConverter* converter) THROWS_NONE
    : m_Converter(converter),
      m_ArrayPtr(0),
      m_ElementCount(0)
{
}


CArrayHolder::~CArrayHolder(void) THROWS_NONE
{
    if ( m_ArrayPtr ) {
        size_t dst_size = m_Converter->GetDstTypeSize();
        for ( size_t i = GetElementCount(); i--; ) { // delete in reverse order
            m_Converter->Destroy(static_cast<char*>(GetArrayPtr())+i*dst_size);
        }
        free(GetArrayPtr());
    }
}


void CArrayHolder::Convert(const void* src_array,
                           size_t size,
                           const char* file,
                           int line)
{
    if ( TParamStaticArrayCopyWarning::GetDefault() ) {
        // report incorrect usage
        CDiagCompileInfo diag_compile_info
            (file? file: __FILE__,
             file? line: __LINE__,
             NCBI_CURRENT_FUNCTION,
             NCBI_MAKE_MODULE(NCBI_MODULE));
        CNcbiDiag diag(diag_compile_info, eDiag_Warning,
                       eDPF_Default|eDPF_File|eDPF_LongFilename|eDPF_Line);
        diag.GetRef()
            << ErrCode(NCBI_ERRCODE_X_NAME(Util_StaticArray), 3)
            << ": converting static array from "
            << m_Converter->GetSrcTypeInfo().name() << "[] to "
            << m_Converter->GetDstTypeInfo().name() << "[]";
        if ( !file ) {
            diag.GetRef() << CStackTrace();
        }
        diag.GetRef() << Endm;
    }

    size_t src_size = m_Converter->GetSrcTypeSize();
    size_t dst_size = m_Converter->GetDstTypeSize();
    m_ArrayPtr = malloc(size*dst_size);
    for ( size_t i = 0; i < size; ++i ) {
        m_Converter->Convert(static_cast<char*>(GetArrayPtr())+i*dst_size,
                             static_cast<const char*>(src_array)+i*src_size);
        m_ElementCount = i+1;
    }
}


void ReportUnsafeStaticType(const char* type_name,
                            const char* file,
                            int line)
{
    if ( TParamStaticArrayUnsafeTypeWarning::GetDefault() ) {
        // report incorrect usage
        CDiagCompileInfo diag_compile_info
            (file? file: __FILE__,
             file? line: __LINE__,
             NCBI_CURRENT_FUNCTION,
             NCBI_MAKE_MODULE(NCBI_MODULE));
        CNcbiDiag diag(diag_compile_info, eDiag_Warning,
                       eDPF_Default|eDPF_File|eDPF_LongFilename|eDPF_Line);
        diag.GetRef()
            << ErrCode(NCBI_ERRCODE_X_NAME(Util_StaticArray), 2)
            << ": static array type is not MT-safe: " << type_name << "[]";
        if ( !file ) {
            diag.GetRef() << CStackTrace();
        }
        diag.GetRef() << Endm;
    }
}


void ReportIncorrectOrder(size_t curr_index,
                          const char* file,
                          int line)
{
    { // report incorrect usage
        CDiagCompileInfo diag_compile_info
            (file? file: __FILE__,
             file? line: __LINE__,
             NCBI_CURRENT_FUNCTION,
             NCBI_MAKE_MODULE(NCBI_MODULE));
        CNcbiDiag diag(diag_compile_info, eDiag_Fatal,
                       eDPF_Default|eDPF_File|eDPF_LongFilename|eDPF_Line);
        diag.GetRef()
            << ErrCode(NCBI_ERRCODE_X_NAME(Util_StaticArray), 1)
            << "keys are out of order: "
            << "key["<<curr_index<<"] < key["<<(curr_index-1)<<"]";
        if ( !file ) {
            diag.GetRef() << CStackTrace();
        }
        diag.GetRef() << Endm;
    }
}


END_NAMESPACE(NStaticArray);

END_NCBI_NAMESPACE;
