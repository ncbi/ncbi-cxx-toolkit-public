#ifndef GPIPE_COMMON___ALIGN_SORT__HPP
#define GPIPE_COMMON___ALIGN_SORT__HPP

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
 * Authors:  Eyal Mozes
 *
 * File Description:
 *
 */

#include <corelib/ncbitime.hpp>

#include <objects/seqalign/Seq_align.hpp>
#include <objects/seq/seq_id_handle.hpp>

#include <algo/align/util/align_filter.hpp>
#include <algo/align/util/align_source.hpp>

#include <objmgr/scope.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class NCBI_XALGOALIGN_EXPORT CAlignSort
{
public:
    CAlignSort(CScope &scope,
               string sorting_keys,
               CRef<CAlignFilter> filter = CRef<CAlignFilter>(),
               const string &tmp_path = "/tmp",
               size_t memory_limit = 0,
               size_t count_limit = 0);

    struct SSortKey
    {
        typedef pair<string, double> TItem;
        vector<TItem> items;
    };
    typedef pair<SSortKey, CRef<CSeq_align> > TAlignment;

    class IAlignSortedOutput
    {
    public:
        virtual ~IAlignSortedOutput() {}
        virtual void Write(const TAlignment &aln) = 0;
    };

    void SortAlignments(IAlignSource &align_source, 
                        IAlignSortedOutput &sorted_output);

    void MergeSortedFiles(const vector<string> &input_files,
                          IAlignSortedOutput &sorted_output,
                          bool remove_input_files = false,
                          bool filtered = false);

    size_t NumProcessed() const
    { return m_Extractor.count; }

private:
    typedef deque<TAlignment> TAlignments;
    typedef pair<TAlignment, size_t> SKeyAndFile;

    enum ESortDir
    {
        eAscending,
        eDescending
    };

    struct SSortKey_Less
    {
        vector<ESortDir> sort_dirs;

        bool operator()(const TAlignment &k1, const TAlignment &k2) const;
    };

    struct SPQSort
    {
        SSortKey_Less predicate;

        SPQSort() {}
        SPQSort(const SSortKey_Less& sk)
            : predicate(sk) {}

        bool operator() (const SKeyAndFile& k1,
                         const SKeyAndFile& k2) const
        {
            // NB: intentionally reversed!
            // priority_queue<> expects sort by greater<>, not less<>
            return predicate(k2.first, k1.first);
        }
    };

    struct SAlignExtractor
    {
        vector<string> key_toks;
        size_t count;
        CStopWatch sw;
        CRef<CScope> scope;

        SAlignExtractor(CScope& s)
            : count(0)
        {
            scope.Reset(&s);
            sw.Start();
        }

        SSortKey operator()(const CSeq_align& align);
    };

    CRef<CAlignFilter> m_Filter;
    string m_TmpPath;

    size_t m_MemoryLimit;
    size_t m_CountLimit;
    size_t m_DataSizeLimit;

    SAlignExtractor m_Extractor;
    SSortKey_Less m_Predicate;
};

END_NCBI_SCOPE


#endif  // GPIPE_COMMON___ALIGN_SORT__HPP
