/* $Id$
* ===========================================================================
*
*                            public DOMAIN NOTICE                          
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
* Author:  Yuri Kapustin
*
* File Description:
*   CSeqLoader class
*
*/

#include <ncbi_pch.hpp>
#include "seq_loader.hpp"
#include "splign_app_exception.hpp"

#include <objtools/readers/fasta.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seq/Bioseq.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seq/Seq_inst.hpp>
#include <objects/seq/Seq_data.hpp>
#include <objects/seq/IUPACna.hpp>
#include <objects/seq/NCBI2na.hpp>
#include <objects/seq/NCBI4na.hpp>

#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <objmgr/seq_vector.hpp>

#include <string>
#include <algorithm>
#include <memory>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


static const size_t g_sizeof_endl = strlen(Endl());

void CSeqLoader::Open(const string& filename_index)
{
    CNcbiIfstream idxstream (filename_index.c_str());

    if(idxstream) {
        
        // first read file names
        char buf [1024];
        bool found_start = false;
        while(idxstream.good()) {

            buf[0] = 0;
            idxstream.getline(buf, sizeof buf, '\n');
            if(buf[0] == '#') continue;
            if(idxstream.fail()) {
                goto throw_file_corrupt;
            }
            if(strcmp(buf,"$$$FI") == 0) {
                found_start = true;
                break;
            }
        }

        if(!found_start) {
            goto throw_file_corrupt;
        }

        vector<string> filenames;
        vector<size_t> indices;
        found_start = false;
        m_min_idx = kMax_UInt;
        while(idxstream.good()) {
            buf[0] = 0;
            idxstream.getline(buf, sizeof buf, '\n');

            if(strcmp("$$$SI", buf) == 0) {
                found_start = true;
                break;
            }
            if(buf[0] == '#' || buf[0] == 0) {
                continue;
            }
            CNcbiIstrstream iss (buf);
            size_t idx = kMax_UInt;
            string name;
            iss >> name >> idx;
            if(idx == kMax_UInt) {
                goto throw_file_corrupt;
            }
            filenames.push_back(name);
            indices.push_back(idx);
            if(idx < m_min_idx) {
                m_min_idx = idx;
            }
        }
        if(!found_start) {
            goto throw_file_corrupt;
        }

        const size_t fndim = filenames.size();
        m_filenames.resize(fndim);
        for(size_t i = 0; i < fndim; ++i) {
            m_filenames[indices[i] - m_min_idx] = filenames[i];
        }
        
        m_idx.clear();
        while(idxstream.good()) {
            
            buf[0] = 0;
            idxstream.getline(buf, sizeof buf, '\n');
            
            if(buf[0] == '#') continue; // skip comments
            if(idxstream.eof()) break;
            
            if(idxstream.fail()) {
                goto throw_file_corrupt;
            }
            
            CNcbiIstrstream iss (buf);
            string id;
            SIdxTarget s;
            iss >> id >> s.m_filename_idx >> s.m_offset;
            if(s.m_offset == kMax_UInt) {
                goto throw_file_corrupt;
            }
            m_idx[id] = s;
        }
        
        return;
    }
    else {      
        
        NCBI_THROW( CSplignAppException,
                    eCannotOpenFile,
                    filename_index.c_str());
    }

 throw_file_corrupt: {
        
        NCBI_THROW( CSplignAppException,
                    eErrorReadingIndexFile,
                    "File is corrupt");
    }
}


void CSeqLoader::Load(const string& id, vector<char>* seq,
		      size_t from, size_t to)
{
    if(seq == 0) {
        NCBI_THROW( CSplignAppException,
                    eInternal,
                    "Null pointer passed to CSeqLoader::Load()");
        
    }
    
    auto_ptr<CNcbiIstream> input (0);
    
    map<string, SIdxTarget>::const_iterator im = m_idx.find(id);
    if(im == m_idx.end()) {
        string msg ("Unable to locate ");
        msg += id;
        NCBI_THROW( CSplignAppException,
                    eInternal,
                    msg.c_str());
    }
    else {
        const string& filename = m_filenames[im->second.m_filename_idx-m_min_idx];
        CNcbiIstream* istr = new ifstream (filename.c_str());
        if(!istr || !*istr) {
            NCBI_THROW( CSplignAppException,
                        eCannotOpenFile,
                        filename.c_str());
        }
        input.reset(istr);
        input->seekg(im->second.m_offset);
    }
  
    char buf [1024];
    input->getline(buf, sizeof buf, '\n'); // info line
    if(input->fail()) {
        NCBI_THROW( CSplignAppException,
                    eCannotReadFile,
                    "Unable to read sequence data");
    }
    
    seq->clear();
    
    if(from == 0 && to == kMax_UInt) {

        // read entire sequence until the next one or eof
        while(*input) {

            CT_POS_TYPE i0 = input->tellg();
            input->getline(buf, sizeof buf, '\n');
            if(!*input) {
                break;
            }
            CT_POS_TYPE i1 = input->tellg();
            if(i1 - i0 > 1) {
                CT_OFF_TYPE line_size = i1 - i0;
                line_size -= g_sizeof_endl;
                if(buf[0] == '>') break;
                size_t size_old = seq->size();
                seq->resize(size_old + line_size);
                transform(buf, buf + line_size, seq->begin() + size_old,
                          (int(*)(int))toupper);
            }
            else if (i0 == i1) {
                break; // GCC hack
            }
        }
    }
    else {

        // read only a portion of a sequence
        const size_t dst_seq_len = to - from + 1;
        seq->resize(dst_seq_len + sizeof buf);
        CT_POS_TYPE i0 = input->tellg(), i1;
        CT_OFF_TYPE dst_read = 0, src_read = 0;
        while(*input) {

            input->getline(buf, sizeof buf, '\n');
            if(buf[0] == '>' || !*input) {
                seq->resize(dst_read);
                return;
            }
            i1 = input->tellg();
            
            CT_OFF_TYPE off = i1 - i0;
            if (off > g_sizeof_endl) {
                src_read += off - g_sizeof_endl;
            }
            else if (off == g_sizeof_endl) {
                continue;
            }
            else { 
                break; // GCC hack
            }
            
            if(src_read > CT_OFF_TYPE(from)) {

                CT_OFF_TYPE line_size = i1 - i0;
                line_size = line_size - g_sizeof_endl;
                size_t start  = dst_read? 0: (line_size - (src_read - from));
                size_t finish = (src_read > CT_OFF_TYPE(to))?
                    (line_size - (src_read - to) + 1):
                    (size_t)line_size;
                transform(buf + start, buf + finish, seq->begin() + dst_read,
                          (int(*)(int))toupper);
                dst_read += finish - start;
                if(dst_read >= CT_OFF_TYPE(dst_seq_len)) {
                    seq->resize(dst_seq_len);
                    return;
                }
            }
            i0 = i1;
        }
        seq->resize(dst_read);
    }
}


CSeqLoaderPairwise::CSeqLoaderPairwise(const string& query_filename,
                                       const string& subj_filename)
{
    CRef<CObjectManager> objmgr = CObjectManager::GetInstance();

    // load query (the spliced sequence)
    CRef<CScope> scope_query (new CScope(*objmgr));
    scope_query->AddDefaults();
    {{
    CNcbiIfstream istream (query_filename.c_str());
    CRef<CSeq_entry> se = ReadFasta(istream, fReadFasta_OneSeq);
    scope_query->AddTopLevelSeqEntry(*se);
    const CSeq_entry::TSeq& bioseq = se->GetSeq();    
    const CSeq_entry::TSeq::TId& seqid = bioseq.GetId();
    m_QueryId = seqid.front()->GetSeqIdString(true);    
    m_se_query = se;

    CBioseq_Handle bh = scope_query->GetBioseqHandle(*seqid.front());
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    size_t dim = sv.size();
    m_Query.resize(dim);
    for(size_t i = 0; i < dim; ++i) {
        m_Query[i] = sv[i];
    }
    }}

    // load subj (the genomic sequence)
    CRef<CScope> scope_subj (new CScope(*objmgr));
    scope_subj->AddDefaults();    
    {{
    CNcbiIfstream istream (subj_filename.c_str());
    CRef<CSeq_entry> se = ReadFasta(istream, fReadFasta_OneSeq);
    scope_subj->AddTopLevelSeqEntry(*se);
    const CSeq_entry::TSeq& bioseq = se->GetSeq();    
    const CSeq_entry::TSeq::TId& seqid = bioseq.GetId();
    m_SubjId = seqid.front()->GetSeqIdString(true);    
    m_se_subj = se;

    CBioseq_Handle bh = scope_subj->GetBioseqHandle(*seqid.front());
    CSeqVector sv = bh.GetSeqVector(CBioseq_Handle::eCoding_Iupac);
    size_t dim = sv.size();
    m_Subj.resize(dim);
    for(size_t i = 0; i < dim; ++i) {
        m_Subj[i] = sv[i];
    }
    }}
}


void CSeqLoaderPairwise::Load(const string& id, vector<char> *seq,
                              size_t start, size_t finish)
{
    if(seq == 0) {
        NCBI_THROW( CSplignAppException,
                    eInternal,
                    "Null pointer passed to CSeqLoader::Load()");
    }

    if(start > finish) {
        NCBI_THROW( CSplignAppException,
                    eBadParameter,
                    "Inconsistent parameters passed to passed to "
                    "CSeqLoaderPairwise::Load()");
    }

    seq->clear();

    vector<char>::const_iterator i0, i1;

    if(id == m_QueryId) {
        const size_t dim = m_Query.size();
        if(start >= dim) {
            return;
        }
        i0 = m_Query.begin() + start;
        i1 = m_Query.begin() + (finish < dim? finish + 1: dim);
    }
    else if(id == m_SubjId) {
        const size_t dim = m_Subj.size();
        if(start >= dim) {
            return;
        }
        i0 = m_Subj.begin() + start;
        i1 = m_Subj.begin() + (finish < dim? finish + 1: dim);
    }
    else {
        NCBI_THROW( CSplignAppException,
                    eGeneral,
                    "Unknolwn sequence requested from "
                    "CSeqLoaderPairwise::Load()");
    }
    seq->resize(i1 - i0);
    copy(i0, i1, seq->begin());
}


END_NCBI_SCOPE

/*
 * ===========================================================================
 * $Log$
 * Revision 1.20  2004/07/21 15:51:24  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.19  2004/06/23 21:44:25  kapustin
 * Use Endl() to figure out EOL length
 *
 * Revision 1.18  2004/06/23 19:31:10  kapustin
 * Minor cleanup
 *
 * Revision 1.17  2004/05/21 21:41:02  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.16  2004/05/10 21:50:03  kapustin
 * Fixed some MSVC6.0 incompatibility issues
 *
 * Revision 1.15  2004/05/10 16:39:56  kapustin
 * Add a pairwise mode sequence loader
 *
 * Revision 1.14  2004/04/23 14:33:32  kapustin
 * *** empty log message ***
 *
 * Revision 1.12  2004/02/20 00:26:18  ucko
 * Tweak previous fix for portability.
 *
 * Revision 1.11  2004/02/19 22:57:54  ucko
 * Accommodate stricter implementations of CT_POS_TYPE.
 *
 * Revision 1.10  2004/02/18 16:04:16  kapustin
 * Support lower case input sequences
 *
 * Revision 1.9  2003/12/09 13:20:50  ucko
 * +<memory> for auto_ptr
 *
 * Revision 1.8  2003/12/03 19:45:33  kapustin
 * Keep min index value to support non-zero based index
 *
 * Revision 1.7  2003/11/20 17:58:20  kapustin
 * Make the code msvc6.0-friendly
 *
 * Revision 1.6  2003/11/14 13:13:29  ucko
 * Fix #include directives: +<memory> for auto_ptr; -<fstream>
 * (redundant, and seq_loader.hpp should be the first anyway)
 *
 * Revision 1.5  2003/11/06 02:07:41  kapustin
 * Fix NCB_THROW usage
 *
 * Revision 1.4  2003/11/05 20:32:10  kapustin
 * Include source information into the index
 *
 * Revision 1.2  2003/10/31 19:43:15  kapustin
 * Format and compatibility update
 *
 * ===========================================================================
 */
