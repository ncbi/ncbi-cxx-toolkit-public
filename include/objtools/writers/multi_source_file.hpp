#ifndef __NCBI_MULTI_SOURCE_FILE_HPP__
#define __NCBI_MULTI_SOURCE_FILE_HPP__
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
* Authors:  Sergiy Gotvyanskyy
*
* File Description:
*   Asynchronous multi-source file writer
*
*/

#include <corelib/ncbistl.hpp>

#include <memory>
#include <ostream>

BEGIN_NCBI_SCOPE

// forward declarations

class CMultiSourceOStreamBuf;
class CMultiSourceOStream;
class CMultiSourceWriter;
class CMultiSourceWriterImpl;


/*
    CMultiSourceWriter concatenates outputs from multiple substreams into one
    Substreams are supposed to be used in different threads
    The substreams have a buffer and blocking+waiting only happens on buffer overflow
    The substream that is currently at the top of stack writes to output stream non-blocking
    Closing the top substream removes it from the stack and allows next substream to write non-blocking
    See usage example below
*/

/*
    {
        ncbi::objects::CMultiSourceWriter writer;
        writer.Open(std::cerr);

        {
            // creating two substreams, the output will be concatenated in the order NewStream is invoked
            // regardless of the writing order
            auto ostr1 = writer.NewStream();
            auto ostr2 = writer.NewStream();

            // note passing streams as a value
            // despite task 1 starts writing after task 2 because of delay of 5 seconds
            // task1's output will appear first in the stderr
            auto task1 = std::async(std::launch::async, [](ncbi::objects::CMultiSourceOStream ostr)
                {
                    std::this_thread::sleep_for(5s);
                    for (size_t i=0; i<100; ++i) {
                        ostr << "Hello " << i << "\n";
                    }
                    // ostr destructor will be invoked at the thread end
                }, std::move(ostr1));

            //std::this_thread::sleep_for(1s);

            auto task2 = std::async(std::launch::async, [](ncbi::objects::CMultiSourceOStream ostr)
                {
                    ostr << "Last hello\n";
                    // ostr destructor will be invoked at the thread end
                }, std::move(ostr2));
        }
    }
*/

// all methods are thread safe
class NCBI_XOBJWRITE_EXPORT CMultiSourceWriter
{
public:
    CMultiSourceWriter();
    CMultiSourceWriter(CMultiSourceWriter&&) = default;
    CMultiSourceWriter& operator=(CMultiSourceWriter&&) = default;

    CMultiSourceWriter(const CMultiSourceWriter&) = delete;
    CMultiSourceWriter& operator=(const CMultiSourceWriter&) = delete;
    ~CMultiSourceWriter();

    void Open(std::ostream& o_stream);
    void Open(const std::string& filename);
    void OpenDelayed(const std::string& filename);
    const std::string& GetFilename() const;
    bool IsOpen() const;
    void Close();
    [[nodiscard]] CMultiSourceOStream NewStream();
    [[nodiscard]] std::unique_ptr<CMultiSourceOStream> NewStreamPtr();
    void Flush();
    void SetMaxWriters(size_t num);
private:
    void x_ConstructImpl();
    std::unique_ptr<CMultiSourceWriterImpl> m_impl;
};

// CMultiSourceOStream is moveable but not copiable
// methods are not thread-safe as all std::ostream
class NCBI_XOBJWRITE_EXPORT CMultiSourceOStream: public std::ostream
{
public:
    using _MyBase = std::ostream;
    CMultiSourceOStream() = default;
    // move constructor and assignment allowed
    CMultiSourceOStream(CMultiSourceOStream&&);
    CMultiSourceOStream& operator=(CMultiSourceOStream&&);
    // copy constructor and assignment is deleted in parent class
    ~CMultiSourceOStream();
    CMultiSourceOStream(std::shared_ptr<CMultiSourceOStreamBuf> buf);
    void close();
    bool is_open() const {
        return static_cast<bool>(m_buf);
    }

private:
    std::shared_ptr<CMultiSourceOStreamBuf> m_buf;
};

END_NCBI_SCOPE

#endif
