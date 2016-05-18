#ifndef UTIL___BUFFER_WRITER__HPP
#define UTIL___BUFFER_WRITER__HPP

#if NCBI_PRODUCTION_VER > 20110000
#  error This is a sub file intended for SC-6.0 compatibility; please use <util/buffer_writer.hpp> instead
#endif

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
 * Authors:  Mike DiCuccio, Sergey Satskiy
 *
 */

/// @file buffer_writer.hpp
///  Adaptor for writing to a resizable buffer using IWriter interface.


#include <corelib/ncbistd.hpp>
#include <corelib/reader_writer.hpp>
#include <util/simple_buffer.hpp>

BEGIN_NCBI_SCOPE


// Supported buffer types and other auxiliary implementation details
namespace impl {
    template <typename T>
    struct SBufferWriter_SupportedType { typedef void TUnsupportedType; };
    template<typename T>
    struct SBufferWriter_Write {
        static void Write(T& buf, const void* src, size_t count); };
}


/// Flag indicating if the lower level container should be cleared
/// (of the pre-existing data) in the CBufferWriter constructor.
/// @sa CBufferWriter
enum EBufferWriter_CreateMode {
    eCreateMode_Truncate,
    eCreateMode_Add
};



/// Adaptor for writing to a resizable buffer using IWriter interface.
/// The buffer type (TBuffer) can be one of the following:
///  - std::string
///  - std::vector<signed char>
///  - std::vector<unsigned char>
///  - ncbi::CSimpleBufferT<signed char>
///  - ncbi::CSimpleBufferT<unsigned char>
///  - ncbi::CSimpleBuffer
/// For any other buffer type a compilation error will be generated.
///
/// You can create a C++ output stream on this writer to write to your buffer:
/// @example
///  string str;
///  CBufferWriter<string> buffer_writer(str, eCreateMode_Add);
///  CWStream os(buffer_writer);
///  os << "Hello NCBI!";

template <typename TBuffer>
class CBufferWriter : public IWriter
{
public:
    /// @param buf
    ///   Reference to the lower level buffer where data are stored
    /// @param create_mode
    ///   If eCreateMode_Truncate then the container will be cleared
    CBufferWriter(TBuffer& buf, EBufferWriter_CreateMode create_mode)
        : m_Buffer(buf)
    {
        if (create_mode == eCreateMode_Truncate)
            m_Buffer.clear();
    }

    ERW_Result Write(const void* src, size_t count, size_t* bytes_written)
    {
        // Lower level containers have unique ways to append data without
        // resizing (i.e. without zeroing overheads) so a templatized function
        // is used here.
        impl::SBufferWriter_Write<TBuffer>::Write(m_Buffer, src, count);

        if (bytes_written)
            *bytes_written = count;

        return eRW_Success;
    }

    ERW_Result Flush(void)
    {
        return eRW_Success;
    }

private:
    TBuffer& m_Buffer;

    /// Private -- to prohibit copying and assigning
    CBufferWriter(const CBufferWriter&);
    CBufferWriter& operator= (const CBufferWriter&);

public:
    /// @param buf
    ///   Reference to the lower level buffer where data are stored
    /// @param clear_buffer
    ///   If true then the container will be cleared. Default: true
    /// @deprecated
    ///   Please use another constructor
    NCBI_DEPRECATED_CTOR(CBufferWriter(TBuffer& buf,
                                       bool     clear_buffer = true))
        : m_Buffer(buf)
    {
        if (clear_buffer)
            m_Buffer.clear();
    }

private:
    // Auxiliary code to enforce the template type restrictions.
    // It will generate a compile time error for unsupported buffer types.
    typename impl::SBufferWriter_SupportedType<TBuffer>::TUnsupportedType
    CBufferWriter_Type_Is_Not_Supported;
};





// Supported buffer types and other auxiliary implementation details.
// NOTE:  Do not use it in your code, it can be changed without notice!
namespace impl {
    template<>
    struct SBufferWriter_SupportedType<std::vector<unsigned char> >
    { typedef int TUnsupportedType; };
    template<>
    struct SBufferWriter_SupportedType<std::vector<signed char> >
    { typedef int TUnsupportedType; };
    template<>
    struct SBufferWriter_SupportedType<std::vector<char> >
    { typedef int TUnsupportedType; };
    template<>
    struct SBufferWriter_SupportedType<std::string >
    { typedef int TUnsupportedType; };
    template<>
    struct SBufferWriter_SupportedType<CSimpleBufferT<unsigned char> >
    { typedef int TUnsupportedType; };
    template<>
    struct SBufferWriter_SupportedType<CSimpleBufferT<signed char> >
    { typedef int TUnsupportedType; };
    template<>
    struct SBufferWriter_SupportedType<CSimpleBufferT<char> >
    { typedef int TUnsupportedType; };

    template<typename T>
    void SBufferWriter_Write<T>::Write(T& buf, const void* src, size_t count)
    {
        buf.insert(buf.size(), static_cast<const char *>(src), count);
    }

    template<typename T>
    struct SBufferWriter_Write< std::vector<T> >
    {
        static void Write(std::vector<T>& buf, const void* src, size_t count)
        {
            buf.insert(buf.end(), static_cast<const T*>(src),
                       static_cast<const T*>(src) + count);
        }
    };

    template<typename T>
    struct SBufferWriter_Write< ncbi::CSimpleBufferT<T> >
    {
        static void Write(ncbi::CSimpleBufferT<T>& buf,
                          const void* src, size_t count)
        {
            buf.append(static_cast<const T*>(src), count);
        }
    };
}

END_NCBI_SCOPE

#endif  // UTIL___BUFFER_WRITER__HPP

