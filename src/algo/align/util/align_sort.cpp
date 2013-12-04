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
 * Authors:  Mike DiCuccio
 *
 * File Description:
 *
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbifile.hpp>
#include <corelib/ncbi_system.hpp>

#include <objmgr/util/sequence.hpp>

#include <objects/seq/MolInfo.hpp>
#include <objects/seqalign/Score.hpp>
#include <objects/general/Object_id.hpp>

#include <algo/align/util/align_sort.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////

bool CAlignSort::SSortKey_Less::operator()(const TAlignment& k1,
                                            const TAlignment& k2) const
{
    const SSortKey& key1 = k1.first;
    const SSortKey& key2 = k2.first;

    for (size_t i = 0;
         i < key1.items.size()  &&  i < key2.items.size();
         ++i) {
        ESortDir dir = eAscending;
        if (i < sort_dirs.size()) {
            dir = sort_dirs[i];
        }

        switch (dir) {
        case eAscending:
            if (key1.items[i] < key2.items[i]) {
                return true;
            }
            if (key2.items[i] < key1.items[i]) {
                return false;
            }
            break;

        case eDescending:
            if (key2.items[i] < key1.items[i]) {
                return true;
            }
            if (key1.items[i] < key2.items[i]) {
                return false;
            }
            break;
        }
    }

    return key1.items.size() < key2.items.size();
}


CAlignSort::SSortKey
CAlignSort::SAlignExtractor::operator()(const CSeq_align& align)
{
    SSortKey key;
    ITERATE (vector<string>, iter, key_toks) {
        SSortKey::TItem item;

        if (NStr::EqualNocase(*iter, "query")) {
            CSeq_id_Handle idh =
                CSeq_id_Handle::GetHandle(align.GetSeq_id(0));
            idh = sequence::GetId(idh, *scope,
                                  sequence::eGetId_Canonical);
            item.first = idh.GetSeqId()->AsFastaString();
        }
        else if (NStr::EqualNocase(*iter, "subject")) {
            CSeq_id_Handle idh =
                CSeq_id_Handle::GetHandle(align.GetSeq_id(1));
            idh = sequence::GetId(idh, *scope,
                                  sequence::eGetId_Canonical);
            item.first = idh.GetSeqId()->AsFastaString();
        }

        else if (NStr::EqualNocase(*iter, "query_start")) {
            item.second = align.GetSeqStart(0);
        }
        else if (NStr::EqualNocase(*iter, "subject_start")) {
            item.second = align.GetSeqStart(1);
        }

        else if (NStr::EqualNocase(*iter, "query_end")) {
            item.second = align.GetSeqStop(0);
        }
        else if (NStr::EqualNocase(*iter, "subject_end")) {
            item.second = align.GetSeqStop(1);
        }

        else if (NStr::EqualNocase(*iter, "query_strand")) {
            item.second = align.GetSeqStrand(0);
        }
        else if (NStr::EqualNocase(*iter, "subject_strand")) {
            item.second = align.GetSeqStrand(1);
        }

        else if (NStr::EqualNocase(*iter, "query_align_len")) {
            item.second = align.GetSeqRange(0).GetLength();
        }
        else if (NStr::EqualNocase(*iter, "subject_align_len")) {
            item.second = align.GetSeqRange(1).GetLength();
        }
        else if (NStr::EqualNocase(*iter, "query_traceback")) {
            CScoreBuilder builder;
            item.first = builder.GetTraceback(*scope, align, 0);
        }
        else if (NStr::EqualNocase(*iter, "subject_traceback")) {
            CScoreBuilder builder;
            item.first = builder.GetTraceback(*scope, align, 1);
        }

        else {
            /// assume it is a score
            double score = 0;
            if (align.IsSetScore()) {
                ITERATE (CSeq_align::TScore, it, align.GetScore()) {
                    const CScore& s = **it;
                    if (s.IsSetId()  &&  s.GetId().IsStr()  &&
                        NStr::EqualNocase(*iter, s.GetId().GetStr())) {
                        if (s.GetValue().IsInt()) {
                            score = s.GetValue().GetInt();
                        } else if (s.GetValue().IsReal()) {
                            score = s.GetValue().GetReal();
                        } else {
                            NCBI_THROW(CException, eUnknown,
                                       "invalid score type");
                        }
                    }
                }
            }

            item.second = score;
        }
        key.items.push_back(item);
    }

    ++count;
    if (count % 100000 == 0) {
        double e = sw.Elapsed();
        LOG_POST(Error << "  processed " << count
                 << " alignments ("
                 << count / e << " alignments/sec)");
    }

    return key;
}


/////////////////////////////////////////////////////////////////////////////

CAlignSort::CAlignSort(CScope &scope,
                       string sorting_keys,
                       CRef<CAlignFilter> filter,
                       const string &tmp_path,
                       size_t memory_limit,
                       size_t count_limit)
: m_Filter(filter)
, m_MemoryLimit(memory_limit)
, m_CountLimit(count_limit)
, m_DataSizeLimit(0)
, m_Extractor(scope)
{
    NStr::Tokenize(sorting_keys, ", \t\r\n", m_Extractor.key_toks, NStr::eMergeDelims);
    NON_CONST_ITERATE (vector<string>, iter, m_Extractor.key_toks) {
        *iter = NStr::TruncateSpaces(*iter);

        if ((*iter)[0] == '-') {
            m_Predicate.sort_dirs.push_back(eDescending);
            iter->erase(0, 1);
        } else if ((*iter)[0] == '+') {
            m_Predicate.sort_dirs.push_back(eAscending);
            iter->erase(0, 1);
        } else {
            m_Predicate.sort_dirs.push_back(eAscending);
        }
    }

    if ( !m_MemoryLimit  &&  !m_CountLimit ) {
        /// default is to use 50% of available RAM for sorting
        m_MemoryLimit = GetPhysicalMemorySize() / 2;
        LOG_POST(Info << "default physical memory size = " << m_MemoryLimit);
    }

    m_TmpPath = CDirEntry::NormalizePath(
        CDirEntry::CreateAbsolutePath(tmp_path), eFollowLinks);
    CDir d(m_TmpPath);
    if ( !d.Exists()  &&  !d.CreatePath() ) {
         NCBI_THROW(CException, eUnknown,
                    "failed to create temporary path");
    }

    m_TmpPath = CDir::GetTmpNameEx(m_TmpPath, "align_sort_");
    m_TmpPath += ".";
}

void CAlignSort::SortAlignments(IAlignSource &align_source,
                                IAlignSortedOutput &sorted_output)
{
    /// temporary array of seq-aligns
    /// when this exhausts our ability to manage
    TAlignments aligns;

    ///
    /// loop on our input stream
    /// if we hit the limit, we dump a temporary file and merge at the end
    ///
    LOG_POST(Error << "pass 1: extracting alignments");

    size_t last_flush_point = 0;
    vector<string> tmp_volumes;

    try {
        while (!align_source.EndOfData()) {
            CRef<CSeq_align> align = align_source.GetNext();
            if (m_Filter  &&  !m_Filter->Match(*align)) {
                continue;
            }

            TAlignments::value_type val;
            val.first = m_Extractor(*align);
            val.second = align;
            aligns.push_back(val);

            if (m_MemoryLimit && !m_DataSizeLimit &&
                m_Extractor.count % 10000 == 0)
            {
                /// check to see if we've exceeded memory limits
                size_t total_mem = 0;
                size_t resident_mem = 0;
                size_t shared_mem = 0;
                if (GetMemoryUsage(&total_mem,
                                   &resident_mem,
                                   &shared_mem)) {
                    if (total_mem > m_MemoryLimit) {
                        m_DataSizeLimit = align_source.DataSizeSoFar();
                    }
                }
            }

            if ((m_CountLimit  &&  aligns.size() >= m_CountLimit)  ||
               (m_DataSizeLimit  &&
                align_source.DataSizeSoFar() - last_flush_point
                    >= m_DataSizeLimit))
            {
                last_flush_point = align_source.DataSizeSoFar();
                std::sort(aligns.begin(), aligns.end(), m_Predicate);

                string fname = m_TmpPath;
                fname += NStr::NumericToString(tmp_volumes.size() + 1);
                tmp_volumes.push_back(fname);

                LOG_POST(Error << "  tmp volume: " << fname
                         << ": " << aligns.size() << " alignments");
                CNcbiOfstream tmp_ostr(fname.c_str(), ios::binary | ios::out);
                auto_ptr<CObjectOStream> tmp_os
                    (CObjectOStream::Open(eSerial_AsnBinary, tmp_ostr));
                ITERATE (TAlignments, it, aligns) {
                    if ( !tmp_ostr ) {
                        NCBI_THROW(CException, eUnknown,
                                   "output stream error");
                    }

                    *tmp_os << *it->second;
                }
                aligns.clear();
            }
        }

        if (tmp_volumes.size()  &&  aligns.size()) {
            ///
            /// we need to do a merge sort
            /// for purposes of algorithmic uniformity, write any spare alignments
            /// to their own volume
            ///
            if (aligns.size()) {
                std::sort(aligns.begin(), aligns.end(), m_Predicate);

                string fname = m_TmpPath;
                fname += NStr::NumericToString(tmp_volumes.size() + 1);
                tmp_volumes.push_back(fname);

                LOG_POST(Error << "  tmp volume: " << fname
                         << ": " << aligns.size() << " alignments");
                CNcbiOfstream tmp_ostr(fname.c_str(), ios::binary | ios::out);
                auto_ptr<CObjectOStream> tmp_os
                    (CObjectOStream::Open(eSerial_AsnBinary, tmp_ostr));
                ITERATE (TAlignments, it, aligns) {
                    if ( !tmp_ostr ) {
                        NCBI_THROW(CException, eUnknown,
                                   "output stream error");
                    }

                    *tmp_os << *it->second;
                }
                aligns.clear();
            }
        }

        LOG_POST(Error << "pass 2: sorting");
        if (tmp_volumes.size()) {
            MergeSortedFiles(tmp_volumes, sorted_output, true, true);
        } else {
            ///
            /// this side is much simpler - all alignments fit into RAM
            /// sort and dump
            ///
            std::sort(aligns.begin(), aligns.end(), m_Predicate);

            ITERATE (TAlignments, it, aligns) {
                sorted_output.Write(*it);
            }
        }

        // lastly, call Flush(); we're done
        sorted_output.Flush();
    }
    catch (CException&) {
        ITERATE (vector<string>, it, tmp_volumes) {
            LOG_POST(Error << "removing tmp volume: " << *it);
            CFile(*it).Remove();
        }

        throw;
    }
}

void CAlignSort::MergeSortedFiles(const vector<string> &input_files,
                                  IAlignSortedOutput &sorted_output,
                                  bool remove_input_files,
                                  bool filtered)
{
    LOG_POST(Error << "...performing merge sort...");

    ///
    /// Open each volume
    /// NB: there is a hole here - if we have more than, say, 8k volumes,
    /// the open may fail because we will run out of file descriptors
    /// the solution to this is to do several partial merges
    ///
    typedef vector< AutoPtr<CObjectIStream> > TFiles;
    TFiles files;
    files.reserve(input_files.size());
    ITERATE (vector<string>, it, input_files) {
        AutoPtr<CObjectIStream> is
            (CObjectIStream::Open(eSerial_AsnBinary, *it));
        files.push_back(is);
    }

    ///
    /// seet a priority queue
    /// the priority queue keeps track of which alignment is the next for
    /// us to process, and provides overall an O(ln n) algorithmic
    /// complexity
    ///
    typedef priority_queue<SKeyAndFile,
                           vector<SKeyAndFile>,
                           SPQSort> TQueue;
    m_Extractor.sw.Restart();
    m_Extractor.count = 0;
    SPQSort pqs(m_Predicate);
    TQueue q(pqs);
    size_t count = 0;
    ITERATE (TFiles, it, files) {
        if ( !(*it)->EndOfData() ) {
            /// NB: this is expensive
            /// we are re-extracting our key information from our alignment
            /// we should try and make this less heavy
            /// one solution is to serialize the key from above and then
            /// only extract the binary strip of data corresponding to the
            /// actual alignment.
            CRef<CSeq_align> sa(new CSeq_align);
            (**it) >> *sa;

            SKeyAndFile kf;
            kf.first.first = m_Extractor(*sa);
            kf.first.second = sa;
            kf.second = count;
            q.push(kf);
        }
        ++count;
    }

    while ( !q.empty() ) {
        SKeyAndFile kf = q.top();
        q.pop();
        if (filtered || !m_Filter  ||  m_Filter->Match(*kf.first.second)) {
            sorted_output.Write(kf.first);
        }

        if ( !(*files[kf.second]).EndOfData() ) {
            /// NB: as above, this is expensive
            CRef<CSeq_align> sa(new CSeq_align);
            (*files[kf.second]) >> *sa;

            kf.first.first = m_Extractor(*sa);
            kf.first.second = sa;
            q.push(kf);
        }
        else {
            /// close our file once done
            files[kf.second].reset();
            LOG_POST(Error << "  freeing volume: "
                     << input_files[kf.second]);
            if ( remove_input_files &&
                 !CFile(input_files[kf.second]).Remove() )
            {
                LOG_POST(Error << "    failed to remove temp file: "
                         << input_files[kf.second]);
            }
        }
    }
}

END_NCBI_SCOPE
