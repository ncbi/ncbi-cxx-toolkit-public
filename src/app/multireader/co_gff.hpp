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
* File Description: Front-end GFF reader class
*
*
*/

#ifndef _HUGE_ASN_CO_GFF_READER_HPP_INCLUDED_
#define _HUGE_ASN_CO_GFF_READER_HPP_INCLUDED_

#include <objtools/readers/co_generator.hpp>
#include <vector>

BEGIN_SCOPE(ncbi::objects::edit)
class CHugeFile;
END_SCOPE(ncbi::objects::edit)

BEGIN_SCOPE(ncbi::objtools::hugeasn)

class CGffSourceImpl;

/// @file co_gff.hpp
/// Frond-end GFF-familty files readers, including GFF2, GFF3, GTF
///

/////////////////////////////////////////////////////////////////////////////
///
/// CGffSource --
///
/// Front-end class to read GFF-like files
///
/// - Read and pre-parse GFF lines
/// - Sort the lines by seqid (first column) and record parent
/// - Issue sorted block as C++ coroutine generator
///
/// Typical use should look like:
///
/// {
///     using namespace ncbi::objtools::hugeasn;
///     auto gff_gen = CGffSource::ReadBlobs(filename);
///     size_t total_lines = 0;
///     for (int j = 0; gff_gen; ++j) {
///         auto blob = gff_gen();
///         total_lines += blob->m_data.size();
///         std::cout << "Blob: " << (int)blob->m_blob_type << ":" << blob->m_data.size() << "\n";
///     }
/// }
///
/// ReadBlobs methods return C++-20 coroutine generator instance. Its major functions are 'bool' operator
//  that signals if more data present and 'call' operator that returns CGffSource::TGffBlob instances.
/// After obtaining the blob from the generator, it can be processing independently from other blobs.
/// It is completely thread safe. Each blob contains lines for only one seqid (column 1).
/// Lines are sorted by Parent and ID properties for optimal parsing by calling party.
///
/// CGffSource opens and reads file via CHugeFile class that does memory mapped IO.
/// Each blob hold a reference to the source CHugeFile instance using shared_ptr,
/// maintaining its lifecycle. The calling party not need to retain the CHugeFile instance, the mapped
/// memory will be free when last usage is deferenced.
/// The m_lines are string_view to original memory backed by the input file and valid as long as it owning
/// TGffBlob alive. The calling party should retain TGffBlob reference until m_lines are completely parsed
/// and integrated into ASN.1 structures.

class CGffSource
{
public:
    using TFile = std::shared_ptr<objects::edit::CHugeFile>;
    using TView = std::string_view;

    enum class blob_type { unknown, comments, lines, fasta };

    struct TGffBlob
    {
        TGffBlob() = default;
        TGffBlob(std::shared_ptr<CGffSourceImpl> impl) : m_impl{impl} {}
        TView  m_seqid;
        blob_type m_blob_type = blob_type::unknown;
        std::vector<TView> m_lines;
    private:
        std::shared_ptr<CGffSourceImpl> m_impl;
    };

    using TGenerator = Generator<std::shared_ptr<TGffBlob>>;
    TGenerator ReadBlobs() const;

    /// Constructor. Open file in memory mapping mode
    ///
    /// @param filename
    ///   Filename

    explicit CGffSource(const std::string& filename);
    explicit CGffSource(TFile file);
    explicit CGffSource(TView blob);

    CGffSource() = default;

    /// Static method. Open file in memory mapping mode and return generator
    ///
    /// @param filename
    ///   Filename

    static TGenerator ReadBlobs(const std::string& filename);
    static TGenerator ReadBlobs(TFile file);
    static TGenerator ReadBlobs(TView blob);

protected:
    std::shared_ptr<CGffSourceImpl> m_impl;
};


END_SCOPE(ncbi::objtools::hugeasn)

#endif

