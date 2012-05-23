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
 * Author:  Kevin Bealer
 *
 */

/// @file writedb_impl.cpp
/// Implementation for the CWriteDB_Impl class.
/// class for WriteDB.

#ifndef SKIP_DOXYGEN_PROCESSING
static char const rcsid[] = "$Id$";
#endif /* SKIP_DOXYGEN_PROCESSING */

#include <ncbi_pch.hpp>
#include <objtools/blast/seqdb_writer/writedb_error.hpp>
#include <objtools/blast/seqdb_reader/seqdbexpert.hpp>
#include <objtools/blast/seqdb_reader/seqdbcommon.hpp>
#include <objects/general/general__.hpp>
#include <objects/seqfeat/seqfeat__.hpp>
#include <util/sequtil/sequtil_convert.hpp>
#include <objects/blastdb/defline_extra.hpp>    // for kAsnDeflineObjLabel
#include <serial/typeinfo.hpp>
#include <corelib/ncbi_bswap.hpp>

#include "writedb_impl.hpp"
#include <objtools/blast/seqdb_writer/writedb_convert.hpp>

#include <iostream>
#include <sstream>

BEGIN_NCBI_SCOPE

/// Import C++ std namespace.
USING_SCOPE(std);

CWriteDB_Impl::CWriteDB_Impl(const string & dbname,
                             bool           protein,
                             const string & title,
                             EIndexType     indices,
                             bool           parse_ids,
                             bool           use_gi_mask)
    : m_Dbname           (dbname),
      m_Protein          (protein),
      m_Title            (title),
      m_MaxFileSize      (0),
      m_MaxVolumeLetters (0),
      m_Indices          (indices),
      m_Closed           (false),
      m_MaskDataColumn   (-1),
      m_ParseIDs         (parse_ids),
      m_UseGiMask        (use_gi_mask),
      m_Pig              (0),
      m_Hash             (0),
      m_SeqLength        (0),
      m_HaveSequence     (false)
{
    CTime now(CTime::eCurrent);
    
    m_Date = now.AsString("b d, Y  ");
    string t = now.AsString("H:m P");
    
    if (t[0] == '0') {
        t.assign(t, 1, t.size() - 1);
    }
    
    m_Date += t;
}

CWriteDB_Impl::~CWriteDB_Impl()
{
    Close();
}

void CWriteDB_Impl::x_ResetSequenceData()
{
    m_Bioseq.Reset();
    m_SeqVector = CSeqVector();
    m_Deflines.Reset();
    m_Ids.clear();
    m_Linkouts.clear();
    m_Memberships.clear();
    m_Pig = 0;
    m_Hash = 0;
    m_SeqLength = 0;
    
    m_Sequence.erase();
    m_Ambig.erase();
    m_BinHdr.erase();
    
    NON_CONST_ITERATE(vector<int>, iter, m_HaveBlob) {
        *iter = 0;
    }
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
    NON_CONST_ITERATE(vector< CRef<CBlastDbBlob> >, iter, m_Blobs) {
        (**iter).Clear();
    }
#endif
}

void CWriteDB_Impl::AddSequence(const CTempString & seq,
                                const CTempString & ambig)
{
    // Publish previous sequence (if any)
    x_Publish();
    
    // Blank slate for new sequence.
    x_ResetSequenceData();
    
    m_Sequence.assign(seq.data(), seq.length());
    m_Ambig.assign(ambig.data(), ambig.length());
    
    if (m_Indices & CWriteDB::eAddHash) {
        x_ComputeHash(seq, ambig);
    }
    
    x_SetHaveSequence();
}

void CWriteDB_Impl::AddSequence(const CBioseq & bs)
{
    // Publish previous sequence
    x_Publish();
    
    // Blank slate for new sequence.
    x_ResetSequenceData();
    
    m_Bioseq.Reset(& bs);
    
    if (m_Indices & CWriteDB::eAddHash) {
        x_ComputeHash(bs);
    }
    
    x_SetHaveSequence();
}

void CWriteDB_Impl::AddSequence(const CBioseq & bs, CSeqVector & sv)
{
    AddSequence(bs);
    m_SeqVector = sv;
}

void CWriteDB_Impl::AddSequence(const CBioseq_Handle & bsh)
{
    CSeqVector sv(bsh);
    AddSequence(*bsh.GetCompleteBioseq(), sv);
}


/// class to support searching for duplicate isam keys
template <class T>
class CWriteDB_IsamKey {

    public:
    // data member
    T              key;
    CNcbiIfstream * source;

    // constructor
    CWriteDB_IsamKey(const string &fn) { 
        source = new CNcbiIfstream(fn.c_str(), 
                                   IOS_BASE::in | IOS_BASE::binary);
        key = x_GetNextKey();
    };

    ~CWriteDB_IsamKey() {
        delete source;
    };

    // advance key to catch up other
    bool AdvanceKey(const CWriteDB_IsamKey & other) {
        while (!source->eof()) {
            T next_key = x_GetNextKey();
            if (next_key >= other.key) {
                key = next_key;
                return true;
            }
        }
        return false;
    };

    // less_than, used for sorting
    bool operator <(const CWriteDB_IsamKey &other) const {
        return (key < other.key);
    };

    private:
    // read in the next key, for numeric id
    T x_GetNextKey() {
#define INT4_SIZE 4
        char s[INT4_SIZE] = { '\0' };
        source->read(s, INT4_SIZE);
        if ((source->gcount() != INT4_SIZE) || source->eof()) {
            return T();
        }
        source->seekg(INT4_SIZE, ios_base::cur);
#ifdef WORDS_BIGENDIAN
        Int4 next_key = (Int4) *((Int4 *) s);
#else
        Int4 next_key = CByteSwap::GetInt4((const unsigned char *)s);
#endif
        return next_key;
    };
};

// customized string file reading
template <> inline string
CWriteDB_IsamKey<string>::x_GetNextKey() {
#define CHAR_BUFFER_SIZE 256
    char s[CHAR_BUFFER_SIZE] = { '\0' };
    source->getline(s, CHAR_BUFFER_SIZE);
    if ((source->gcount() == 0) || source->eof()) {
        return kEmptyStr;
    }
    char * p = s;
    while (*p != 0x02) ++p;
    string in(s, p);

    // check if the current key is PDB-like, 
    // if so, advance for the next
    // PDB key must be [0-9]...
    if ( (in.size() == 4) 
      && ((in[0] - '0') * (in[0] - '9') <= 0) ) { 

        // probing the next key to make sure this is pdb id
        char next_token[4];
        source->read(next_token, 4);
        source->seekg(-4, ios_base::cur);
        string next_key(next_token, 4);

        if (next_key == in) {
           // automatically advance to next key
           return x_GetNextKey();
        }
    }
    return in;
};

/// Comparison function for set<CWriteDB_IsamKey<T> *>                                                     
template <class T>                                                                                         
struct CWriteDB_IsamKey_Compare {                                                                          
    bool operator() (const CWriteDB_IsamKey<T> * lhs,                                                      
                     const CWriteDB_IsamKey<T> * rhs) const {                                              
        return (*lhs < *rhs);                                                                              
    }                                                                                                      
};                                                                                                         
          
/// Check for duplicate ids across volumes                                                                 
template <class T>                                                                                         
static void s_CheckDuplicateIds(set<CWriteDB_IsamKey<T> *, 
                                    CWriteDB_IsamKey_Compare<T> > & keys) {                                       
    while (!keys.empty()) {                                                                                
        // pick the smallest key                                                                           
        CWriteDB_IsamKey<T> * key = *(keys.begin());                                                          
                                                                                                           
        keys.erase(key);                                                                                   
                                                                                                           
        if (keys.empty()) {                                                                                
            delete key;                                                                                    
            return;                                                                                        
        }                                                                                                  
                                                                                                           
        const CWriteDB_IsamKey<T> * next = *(keys.begin());                                                   
        if (key->AdvanceKey(*next)) {                                                                      
            if (keys.find(key) != keys.end()) {
                CNcbiOstrstream msg;
                msg << "Error: Duplicate seq_id <"
                    << key->key
                    << "> is found multiple times across volumes.";
                NCBI_THROW(CWriteDBException, eArgErr, CNcbiOstrstreamToString(msg));
            } 
            keys.insert(key); 
        } else {                                                                                           
            delete key;                                                                                    
        }                                                                                                  
    }                                                                                                      
};                                                                                                         

void CWriteDB_Impl::Close()
{
    if (m_Closed)
        return;
    
    m_Closed = true;
    
    x_Publish();
    m_Sequence.erase();
    m_Ambig.erase();
    
    if (! m_Volume.Empty()) {
        m_Volume->Close();
        
        if (m_UseGiMask) {
            for (unsigned int i=0; i<m_GiMasks.size(); ++i) {
                m_GiMasks[i]->Close();
            }
        }

        if (m_VolumeList.size() == 1) {
            m_Volume->RenameSingle();
        } 

        // disable the check for duplicate ids across volumes
        /* 
        else if (m_Indices != CWriteDB::eNoIndex) {
            set<CWriteDB_IsamKey<string> *, CWriteDB_IsamKey_Compare<string> > sids;
            ITERATE(vector< CRef<CWriteDB_Volume> >, iter, m_VolumeList) {
                string fn = (*iter)->GetVolumeName() + (m_Protein ? ".psd" : ".nsd");
                if (CFile(fn).Exists()) {
                    sids.insert(new CWriteDB_IsamKey<string>(fn));
                }
            }
            s_CheckDuplicateIds(sids);

            set<CWriteDB_IsamKey<Int4> *, CWriteDB_IsamKey_Compare<Int4> > nids;
            ITERATE(vector< CRef<CWriteDB_Volume> >, iter, m_VolumeList) {
                string fn = (*iter)->GetVolumeName() + (m_Protein ? ".pnd" : ".nnd");
                if (CFile(fn).Exists()) {
                    nids.insert(new CWriteDB_IsamKey<Int4>(fn));
                }
            }
            s_CheckDuplicateIds(nids);
        } */

        if (m_VolumeList.size() > 1 || m_UseGiMask) {
            x_MakeAlias();
        }
        
        m_Volume.Reset();
    }
}

string CWriteDB_Impl::x_MakeAliasName()
{
    return m_Dbname + (m_Protein ? ".pal" : ".nal");
}

void CWriteDB_Impl::x_MakeAlias()
{
    string dblist;
    if (m_VolumeList.size() > 1) {
        for(unsigned i = 0; i < m_VolumeList.size(); i++) {
            if (dblist.size())
                dblist += " ";
        
            dblist += CWriteDB_File::MakeShortName(m_Dbname, i);
        }
    } else {
        dblist = m_Dbname;
    }
    
    string masklist("");
    if (m_UseGiMask) {
        for (unsigned i = 0; i < m_GiMasks.size(); i++) {
            const string & x = m_GiMasks[i]->GetName();
            if (x != "") {
                masklist += x + " ";
            }
        }
    }

    string nm = x_MakeAliasName();
    
    ofstream alias(nm.c_str());
    
    alias << "#\n# Alias file created: " << m_Date  << "\n#\n"
          << "TITLE "        << m_Title << "\n"
          << "DBLIST "       << dblist  << "\n";

    if (masklist != "") {
        alias << "MASKLIST " << masklist << "\n";
    }
}

void CWriteDB_Impl::x_GetBioseqBinaryHeader(const CBioseq & bioseq,
                                            string        & bin_hdr)
{
    if (! bin_hdr.empty()) {
        return;
    }
    
    if (! bioseq.CanGetDescr()) {
        return;
    }
    
    // Getting the binary headers, when they exist, is probably faster
    // than building new deflines from the 'visible' CBioseq parts.
    
    vector< vector< char >* > bindata;
    
    ITERATE(list< CRef< CSeqdesc > >, iter, bioseq.GetDescr().Get()) {
        if ((**iter).IsUser()) {
            const CUser_object & uo = (**iter).GetUser();
            const CObject_id & oi = uo.GetType();
            
            if (oi.IsStr() && oi.GetStr() == kAsnDeflineObjLabel) {
                if (uo.CanGetData()) {
                    const vector< CRef< CUser_field > > & D = uo.GetData();
                    
                    if (D.size() &&
                        D[0].NotEmpty() &&
                        D[0]->CanGetLabel() &&
                        D[0]->GetLabel().IsStr() &&
                        D[0]->GetLabel().GetStr() == kAsnDeflineObjLabel &&
                        D[0]->CanGetData() &&
                        D[0]->GetData().IsOss()) {
                        
                        bindata = D[0]->GetData().GetOss();
                        break;
                    }
                }
            }
        }
    }
    
    if (! bindata.empty()) {
        if (bindata[0] && (! bindata[0]->empty())) {
            vector<char> & b = *bindata[0];
            
            bin_hdr.assign(& b[0], b.size());
        }
    }
}

static void
s_CheckEmptyLists(CRef<CBlast_def_line_set> & deflines, bool owner);

static CRef<CBlast_def_line_set>
s_EditDeflineSet(CConstRef<CBlast_def_line_set> & deflines)
{
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    SerialAssign(*bdls, *deflines);
    s_CheckEmptyLists(bdls, true);
    return bdls;
}

static void
s_CheckEmptyLists(CRef<CBlast_def_line_set> & deflines, bool owner)
{
    CBlast_def_line_set * bdls = 0;
    CConstRef<CBlast_def_line_set> here(&*deflines);
    
    if (! owner) {
        here = s_EditDeflineSet(here);
        return;
    }
    
    bdls = const_cast<CBlast_def_line_set*>(here.GetPointer());
    
    NON_CONST_ITERATE(list< CRef< CBlast_def_line > >, iter, bdls->Set()) {
        CRef<CBlast_def_line> defline = *iter;
        if (defline->CanGetMemberships() &&
            defline->GetMemberships().size() == 0) {
            
            defline->ResetMemberships();
        }
        
        if (defline->CanGetLinks() &&
            defline->GetLinks().size() == 0) {
            
            defline->ResetLinks();
        }
    }
    
    deflines.Reset(bdls);
}

void
CWriteDB_Impl::x_BuildDeflinesFromBioseq(const CBioseq                  & bioseq,
                                         CConstRef<CBlast_def_line_set> & deflines,
                                         const vector< vector<int> >    & membbits,
                                         const vector< vector<int> >    & linkouts,
                                         int                              pig)
{
    if (! (bioseq.CanGetDescr() && bioseq.CanGetId())) {
        return;
    }
    
    vector<int> taxids;
    string titles;
    
    // Scan the CBioseq for taxids and the title string.
    
    ITERATE(list< CRef< CSeqdesc > >, iter, bioseq.GetDescr().Get()) {
        const CSeqdesc & desc = **iter;
        
        if (desc.IsTitle()) {
            //defline->SetTitle((**iter)->GetTitle());
            titles = (**iter).GetTitle();
        }
        else {
        	const COrg_ref * org_pt = NULL;
        	if (desc.IsSource()) {
        		org_pt = &(desc.GetSource().GetOrg());
        	}
        	else if( desc.IsOrg()) {
        		org_pt = &(desc.GetOrg());
        	}

        	if((NULL != org_pt) && org_pt->CanGetDb()) {
                ITERATE(vector< CRef< CDbtag > >,
                        dbiter,
                        org_pt->GetDb()) {
                    
                    if ((**dbiter).CanGetDb() &&
                        (**dbiter).GetDb() == "taxon") {
                        
                        const CObject_id & oi = (**dbiter).GetTag();
                        
                        if (oi.IsId()) {
                            //defline->SetTaxid(oi.GetId());
                            taxids.push_back(oi.GetId());
                        }
                    }
                }
            }
        }
    }
    
    // The bioseq has a field contianing the ids for the first
    // defline.  The title string contains the title for the first
    // defline, plus all the other defline titles and ids.  This code
    // unpacks them and builds a normal blast defline set.
    
    list< CRef<CSeq_id> > ids = bioseq.GetId();
    
    unsigned taxid_i(0), mship_i(0), links_i(0);
    bool used_pig(false);
    
    // Build the deflines.
    
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    CRef<CBlast_def_line> defline;
    
    while(! ids.empty()) {
        defline.Reset(new CBlast_def_line);
        
        defline->SetSeqid() = ids;
        ids.clear();
        
        /*
        size_t pos = titles.find(" >");
        string T;
        
        if (pos != titles.npos) {
            T.assign(titles, 0, pos);
            titles.erase(0, pos + 2);
            
            pos = titles.find(" ");
            string nextid;
            
            if (pos != titles.npos) {
                nextid.assign(titles, 0, pos);
                titles.erase(0, pos + 1);
            } else {
                nextid.swap(titles);
            }
            
            // Parse '|' seperated ids.
            if ( nextid.find('|') == NPOS 
              || !isalpha((unsigned char)(nextid[0]))) {
                 ids.push_back(CRef<CSeq_id> (new CSeq_id(CSeq_id::e_Local, nextid)));
            } else {
                 CSeq_id::ParseFastaIds(ids, nextid);
            }
        } else {
            T = titles;
        }

        */
        defline->SetTitle(titles);

        if (taxid_i < taxids.size()) {
            defline->SetTaxid(taxids[taxid_i++]);
        }
        
        if (mship_i < membbits.size()) {
            const vector<int> & V = membbits[mship_i++];
            defline->SetMemberships().assign(V.begin(), V.end());
        }
        
        if (links_i < linkouts.size()) {
            const vector<int> & V = linkouts[mship_i++];
            defline->SetLinks().assign(V.begin(), V.end());
        }
        
        if ((! used_pig) && pig) {
            defline->SetOther_info().push_back(pig);
            used_pig = true;
        }
        
        bdls->Set().push_back(defline);
    }
    
    s_CheckEmptyLists(bdls, true);
    deflines = bdls;
}

void CWriteDB_Impl::
x_SetDeflinesFromBinary(const string                   & bin_hdr,
                        CConstRef<CBlast_def_line_set> & deflines)
{
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    
    istringstream iss(bin_hdr);
    iss >> MSerial_AsnBinary >> *bdls;
    
    s_CheckEmptyLists(bdls, true);
    deflines.Reset(&* bdls);
}

void
CWriteDB_Impl::x_ExtractDeflines(CConstRef<CBioseq>             & bioseq,
                                 CConstRef<CBlast_def_line_set> & deflines,
                                 string                         & bin_hdr,
                                 const vector< vector<int> >    & membbits,
                                 const vector< vector<int> >    & linkouts,
                                 int                              pig,
                                 int                              OID,
                                 bool                             parse_ids)
{
    bool use_bin = (deflines.Empty() && pig == 0);
    
    if (! bin_hdr.empty() && OID<0) {
        return;
    }
    
    if (deflines.Empty()) {
        // Use bioseq if deflines are not provided.
        
        if (bioseq.Empty()) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Error: Cannot find CBioseq or deflines.");
        }
        
        // CBioseq objects from SeqDB have binary headers embedded in
        // them.  If these are found, we try to use them.  However,
        // using binary headers may not help us much if we also want
        // lists of sequence identifiers (for building ISAM files).
        
        if (use_bin) {
            x_GetBioseqBinaryHeader(*bioseq, bin_hdr);
        }
        
        if (bin_hdr.empty()) {
            x_GetFastaReaderDeflines(*bioseq,
                                     deflines,
                                     membbits,
                                     linkouts,
                                     pig,
                                     false,
                                     parse_ids);
        }
        
        if (bin_hdr.empty() && deflines.Empty()) {
            x_BuildDeflinesFromBioseq(*bioseq,
                                      deflines,
                                      membbits,
                                      linkouts,
                                      pig);
        }
    }
    
    if (bin_hdr.empty() &&
        (deflines.Empty() || deflines->Get().empty())) {
        
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Error: No deflines provided.");
    }
    
    if (pig != 0) {
        const list<int> * L = 0;
        
        if (deflines->Get().front()->CanGetOther_info()) {
            L = & deflines->Get().front()->GetOther_info();
        }
        
        // If the pig does not agree with the current value, set the
        // new value and force a rebuild of the binary headers.  If
        // there is more than one value in the list, leave the others
        // in place.
        
        if ((L == 0) || L->empty()) {
            CRef<CBlast_def_line_set> bdls = s_EditDeflineSet(deflines);
            bdls->Set().front()->SetOther_info().push_back(pig);
            
            deflines.Reset(&* bdls);
            bin_hdr.erase();
        } else if (L->front() != pig) {
            CRef<CBlast_def_line_set> bdls = s_EditDeflineSet(deflines);
            bdls->Set().front()->SetOther_info().front() = pig;
            
            deflines.Reset(&* bdls);
            bin_hdr.erase();
        }
    }

    if (OID>=0) {
        // Re-inject the BL_ORD_ID
        CRef<CSeq_id> gnl_id(new CSeq_id);
        gnl_id->SetGeneral().SetDb("BL_ORD_ID");
        gnl_id->SetGeneral().SetTag().SetId(OID);
        CRef<CBlast_def_line_set> bdls = s_EditDeflineSet(deflines);
        bdls->Set().front()->SetSeqid().front() = gnl_id;
            
        deflines.Reset(&* bdls);
    }
    
    if (bin_hdr.empty() || OID>=0) {
        // Compress the deflines to binary.
        
        ostringstream oss;
        oss << MSerial_AsnBinary << *deflines;
        bin_hdr = oss.str();
    }
    
    if (deflines.Empty() && (! bin_hdr.empty())) {
        // Uncompress the deflines from binary.
        
        x_SetDeflinesFromBinary(bin_hdr, deflines);
    }
}

void CWriteDB_Impl::x_CookHeader()
{
    int OID = -1;
    if (! m_ParseIDs) {
        OID = (m_Volume ) ? m_Volume->GetOID() : 0;
    }
    x_ExtractDeflines(m_Bioseq,
                      m_Deflines,
                      m_BinHdr,
                      m_Memberships,
                      m_Linkouts,
                      m_Pig,
                      OID,
                      m_ParseIDs);
}

void CWriteDB_Impl::x_CookIds()
{
    if (! m_Ids.empty()) {
        return;
    }
    
    if (m_Deflines.Empty()) {
        if (m_BinHdr.empty()) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Error: Cannot find IDs or deflines.");
        }
        
        x_SetDeflinesFromBinary(m_BinHdr, m_Deflines);
    }
    
    ITERATE(list< CRef<CBlast_def_line> >, iter, m_Deflines->Get()) {
        const list< CRef<CSeq_id> > & ids = (**iter).GetSeqid();
        // m_Ids.insert(m_Ids.end(), ids.begin(), ids.end());
        // Spelled out for WorkShop. :-/
        m_Ids.reserve(m_Ids.size() + ids.size());
        ITERATE (list<CRef<CSeq_id> >, it, ids) {
            m_Ids.push_back(*it);
        }
    }
}

void CWriteDB_Impl::x_MaskSequence()
{
    // Scan and mask the sequence itself.
    for(unsigned i = 0; i < m_Sequence.size(); i++) {
        if (m_MaskLookup[m_Sequence[i] & 0xFF] != 0) {
            m_Sequence[i] = m_MaskByte[0];
        }
    }
}

int CWriteDB_Impl::x_ComputeSeqLength()
{
    if (! m_SeqLength) {
        if (! m_Sequence.empty()) {
            m_SeqLength = WriteDB_FindSequenceLength(m_Protein, m_Sequence);
        } else if (m_SeqVector.size()) {
            m_SeqLength = m_SeqVector.size();
        } else if (! (m_Bioseq &&
                      m_Bioseq->CanGetInst() &&
                      m_Bioseq->GetInst().GetLength())) {
            
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Need sequence data.");
        }
        
        if (m_Bioseq.NotEmpty()) {
            const CSeq_inst & si = m_Bioseq->GetInst();
            m_SeqLength = si.GetLength();
        }
    }
    
    return m_SeqLength;
}

void CWriteDB_Impl::x_CookSequence()
{
    if (! m_Sequence.empty())
        return;
    
    if (! (m_Bioseq.NotEmpty() && m_Bioseq->CanGetInst())) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Need sequence data.");
    }
    
    const CSeq_inst & si = m_Bioseq->GetInst();
    
    if (m_Bioseq->GetInst().CanGetSeq_data()) {
        const CSeq_data & sd = si.GetSeq_data();
        
        string msg;
        
        switch(sd.Which()) {
        case CSeq_data::e_Ncbistdaa:
            WriteDB_StdaaToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Ncbieaa:
            WriteDB_EaaToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Iupacaa:
            WriteDB_IupacaaToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Ncbi2na:
            WriteDB_Ncbi2naToBinary(si, m_Sequence);
            break;
            
        case CSeq_data::e_Ncbi4na:
            WriteDB_Ncbi4naToBinary(si, m_Sequence, m_Ambig);
            break;
            
        default:
            msg = "Need to write conversion for data type [";
            msg += NStr::IntToString((int) sd.Which());
            msg += "].";
        }
        
        if (! msg.empty()) {
            NCBI_THROW(CWriteDBException, eArgErr, msg);
        }
    } else {
        int sz = m_SeqVector.size();
        
        if (sz == 0) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "No sequence data in Bioseq, "
                       "and no Bioseq_Handle available.");
        }
        
        if (m_Protein) {
            // I add one to the string length to allow the "i+1" in
            // the loop to be done safely.
            
            m_Sequence.reserve(sz);
            m_SeqVector.GetSeqData(0, sz, m_Sequence);
        } else {
            // I add one to the string length to allow the "i+1" in the
            // loop to be done safely.
        
            string na8;
            na8.reserve(sz + 1);
            m_SeqVector.GetSeqData(0, sz, na8);
            na8.resize(sz + 1);
        
            string na4;
            na4.resize((sz + 1) / 2);
        
            for(int i = 0; i < sz; i += 2) {
                na4[i/2] = (na8[i] << 4) + na8[i+1];
            }
        
            WriteDB_Ncbi4naToBinary(na4.data(),
                                    (int) na4.size(),
                                    (int) si.GetLength(),
                                    m_Sequence,
                                    m_Ambig);
        }
    }
}

void CWriteDB_Impl::x_CookColumns()
{
}

// The CPU should be kept at 190 degrees for 10 minutes.
void CWriteDB_Impl::x_CookData()
{
    // We need sequence, ambiguity, and binary deflines.  If any of
    // these is missing, it is created from other data if possible.
    
    // For now I am disabling binary headers, because in normal usage
    // I would expect to see sequences from ID1 or similar, and the
    // non-binary case is slightly more complex.
    
    x_CookHeader();
    x_CookIds();
    x_CookSequence();
    x_CookColumns();
    
    if (m_Protein && m_MaskedLetters.size()) {
        x_MaskSequence();
    }
}

bool CWriteDB_Impl::x_HaveSequence() const
{
    return m_HaveSequence;
}

void CWriteDB_Impl::x_SetHaveSequence()
{
    _ASSERT(! m_HaveSequence);
    m_HaveSequence = true;
}

void CWriteDB_Impl::x_ClearHaveSequence()
{
    _ASSERT(m_HaveSequence);
    m_HaveSequence = false;
}

void CWriteDB_Impl::x_Publish()
{
    // This test should fail only on the first call, or if an
    // exception was thrown.
    
    if (x_HaveSequence()) {
        _ASSERT(! (m_Bioseq.Empty() && m_Sequence.empty()));
        
        x_ClearHaveSequence();
    } else {
        return;
    }
    
    x_CookData();

    bool done = false;

    if (! m_Volume.Empty()) {
        done = m_Volume->WriteSequence(m_Sequence,
                                       m_Ambig,
                                       m_BinHdr,
                                       m_Ids,
                                       m_Pig,
                                       m_Hash,
                                       m_Blobs,
                                       m_MaskDataColumn);
    }
    
    if (! done) {
        int index = (int) m_VolumeList.size();
        
        if (m_Volume.NotEmpty()) {
            m_Volume->Close();
        }
        
        {
            m_Volume.Reset(new CWriteDB_Volume(m_Dbname,
                                               m_Protein,
                                               m_Title,
                                               m_Date,
                                               index,
                                               m_MaxFileSize,
                                               m_MaxVolumeLetters,
                                               m_Indices));
            
            m_VolumeList.push_back(m_Volume);
            
#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
            _ASSERT(m_Blobs.size() == m_ColumnTitles.size() * 2);
            _ASSERT(m_Blobs.size() == m_ColumnMetas.size() * 2);
            _ASSERT(m_Blobs.size() == m_HaveBlob.size() * 2);
            
            for(size_t i = 0; i < m_ColumnTitles.size(); i++) {
                m_Volume->CreateColumn(m_ColumnTitles[i],
                                       m_ColumnMetas[i],
                                       m_MaxFileSize);
            }
#endif
        }

        // need to reset OID,  hense recalculate the header and id
        x_CookHeader();
        x_CookIds();
        
        done = m_Volume->WriteSequence(m_Sequence,
                                       m_Ambig,
                                       m_BinHdr,
                                       m_Ids,
                                       m_Pig,
                                       m_Hash,
                                       m_Blobs,
                                       m_MaskDataColumn);
        
        if (! done) {
            NCBI_THROW(CWriteDBException,
                       eArgErr,
                       "Cannot write sequence to volume.");
        }
    }
}

void CWriteDB_Impl::SetDeflines(const CBlast_def_line_set & deflines)
{
    CRef<CBlast_def_line_set>
        bdls(const_cast<CBlast_def_line_set*>(& deflines));
    
    s_CheckEmptyLists(bdls, true);
    m_Deflines = bdls;
}

inline int s_AbsMax(int a, int b)
{
    return std::max(((a < 0) ? -a : a),
                    ((b < 0) ? -b : b));
}

// Filtering data format on disk:
// 
// Size of integer type for this blob (1, 2, or 4) (4 bytes).
//
// Array of filtering types:
//     Filter-type (enumeration)
//     Array of offsets:
//         Start Offset
//         End Offset
// 
// The isize is one of 1, 2, or 4, written in the first byte, and
// followed by 0, 1, or 3 NUL bytes to align the data offset to a
// multiple of `isize'.
// 
// All other integer values in this array use isize bytes, including
// array counts and the `type' enumerations.  After all the offset is
// written, the blob is aligned to a multiple of 4 using the `eSimple'
// method.
// 
// Each array is an element count followed by that many elements.

#if 0

// I think this is a better approach; but it needs more testing,
// particularly with regard to platform portability.

struct SWriteInt1 {
    static void WriteInt(CBlastDbBlob & blob, int value)
    {
        blob.WriteInt1(value);
    }
};

struct SWriteInt2 {
    static void WriteInt(CBlastDbBlob & blob, int value)
    {
        blob.WriteInt2(value);
    }
};

struct SWriteInt4 {
    static void WriteInt(CBlastDbBlob & blob, int value)
    {
        blob.WriteInt4(value);
    }
};

template<class TWriteSize, class TRanges>
void s_WriteRanges(CBlastDbBlob  & blob,
                   int             count,
                   const TRanges & ranges)
{
    typedef vector< pair<TSeqPos, TSeqPos> > TPairVector;
    
    Int4 num_written = 0;
    TWriteSize::WriteInt(blob, count);
    
    for ( typename TRanges::const_iterator r1 = (ranges).begin(),
              r1_end = (ranges).end();
          r1 != r1_end;
          ++r1 ) {
        
        if (r1->offsets.size()) {
            num_written ++;
            TWriteSize::WriteInt(blob, r1->algorithm_id);
            TWriteSize::WriteInt(blob, r1->offsets.size());
            
            ITERATE(TPairVector, r2, r1->offsets) {
                TWriteSize::WriteInt(blob, r2->first);
                TWriteSize::WriteInt(blob, r2->second);
            }
        }
    }
    
    _ASSERT(num_written == count);
}

#endif

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
void CWriteDB_Impl::SetMaskData(const CMaskedRangesVector & ranges,
                                const vector <int>        & gis)
{
    // No GI is found for the sequence 
    // TODO should we generate a warning?
    if (m_UseGiMask && !gis.size()) {
        return;
    }

    TSeqPos seq_length = x_ComputeSeqLength();
    
    // Check validity of data and determine maximum integer value
    // stored here before writing anything.  The best numeric_size
    // will be selected; this numeric size is applied uniformly to all
    // integers in this blob (except for the first one, which is the
    // integer size itself, and which is always a single byte.)
    
    typedef vector< pair<TSeqPos, TSeqPos> > TPairVector;
    
    int range_list_count = 0;
    int offset_pairs_count = 0;
    
    
    ITERATE(CMaskedRangesVector, r1, ranges) {
        if (r1->empty()) {
            continue;
        }
        
        range_list_count ++;
        offset_pairs_count += r1->offsets.size();
        
        if ( !m_MaskAlgoRegistry.IsRegistered(r1->algorithm_id) ) {
            string msg("Error: Algorithm IDs must be registered before use.");
            msg += " Unknown algorithm ID = " +
                NStr::IntToString((int)r1->algorithm_id);
            NCBI_THROW(CWriteDBException, eArgErr, msg);
        }
        
        
        ITERATE(TPairVector, r2, r1->offsets) {
            if ((r2->first  > r2->second) ||
                (r2->second > seq_length)) {
                
                NCBI_THROW(CWriteDBException,
                           eArgErr,
                           "Error: Masked data offsets out of bounds.");
            }
        }
    }
    
    
    // We may be passed an empty list of ranges, or we might be passed
    // several ranges whose lists of offsets are themself empty.  No
    // matter what is passed in, we should not emit empty lists and we
    // should not emit any bytes at all if there are no elements.
    
    if (offset_pairs_count == 0) {
        return;
    }
    
    // Gi-based masks
    if (m_UseGiMask) {
        ITERATE(CMaskedRangesVector, r1, ranges) {
            if (r1->offsets.size()) {
                m_GiMasks[m_MaskAlgoMap[r1->algorithm_id]]
                    ->AddGiMask(gis, r1->offsets);
            }
        }
        return;
    }  

    // OID-based masks
    const int col_id = x_GetMaskDataColumnId();
    CBlastDbBlob & blob = SetBlobData(col_id);
    blob.Clear();
    blob.WriteInt4(range_list_count);
    
    CBlastDbBlob & blob2 = SetBlobData(col_id);
    blob2.Clear();
    blob2.WriteInt4(range_list_count);
            
    ITERATE(CMaskedRangesVector, r1, ranges) {
        if (r1->offsets.size()) {
            blob.WriteInt4(r1->algorithm_id);
            blob.WriteInt4(r1->offsets.size());
            blob2.WriteInt4(r1->algorithm_id);
            blob2.WriteInt4(r1->offsets.size());
             
            ITERATE(TPairVector, r2, r1->offsets) {
                blob.WriteInt4(r2->first);
                blob.WriteInt4(r2->second);
                blob2.WriteInt4_LE(r2->first);
                blob2.WriteInt4_LE(r2->second);
            }
        }
    }
    
    blob.WritePadBytes(4, CBlastDbBlob::eSimple);
    blob2.WritePadBytes(4, CBlastDbBlob::eSimple); 
}

int CWriteDB_Impl::
RegisterMaskAlgorithm(EBlast_filter_program   program, 
                      const string          & options,
                      const string          & name)
{
    int algorithm_id = m_MaskAlgoRegistry.Add(program, options);
    
    string key = NStr::IntToString(algorithm_id);
    string value = NStr::IntToString((int)program) + ":" + options;

    if (m_UseGiMask) {
        m_MaskAlgoMap[algorithm_id] = m_GiMasks.size();
        m_GiMasks.push_back(CRef<CWriteDB_GiMask> 
            (new CWriteDB_GiMask(name, value, m_MaxFileSize)));
    } else {
        m_ColumnMetas[x_GetMaskDataColumnId()][key] = value;
    }

    return algorithm_id;
}

int CWriteDB_Impl::FindColumn(const string & title) const
{
    for(int i = 0; i < (int) m_ColumnTitles.size(); i++) {
        if (title == m_ColumnTitles[i]) {
            return i;
        }
    }
    
    return -1;
}

int CWriteDB_Impl::CreateColumn(const string & title, bool mbo)
{
    _ASSERT(FindColumn(title) == -1);
    
    size_t col_id = m_Blobs.size() / 2;
    
    _ASSERT(m_HaveBlob.size()     == col_id);
    _ASSERT(m_ColumnTitles.size() == col_id);
    _ASSERT(m_ColumnMetas.size()  == col_id);
    
    CRef<CBlastDbBlob> new_blob(new CBlastDbBlob);
    CRef<CBlastDbBlob> new_blob2(new CBlastDbBlob);
    
    m_Blobs       .push_back(new_blob);
    m_Blobs       .push_back(new_blob2);
    m_HaveBlob    .push_back(0);
    m_ColumnTitles.push_back(title);
    m_ColumnMetas .push_back(TColumnMeta());
    
    if (m_Volume.NotEmpty()) {
        size_t id2 = m_Volume->CreateColumn(title, m_ColumnMetas.back(), mbo);
        _ASSERT(id2 == col_id);
        (void)id2;  // get rid of compiler warning
    }
    
    return col_id;
}

void CWriteDB_Impl::AddColumnMetaData(int            col_id,
                                      const string & key,
                                      const string & value)
{
    if ((col_id < 0) || (col_id >= (int) m_ColumnMetas.size())) {
        NCBI_THROW(CWriteDBException, eArgErr,
                   "Error: provided column ID is not valid");
    }
    
    m_ColumnMetas[col_id][key] = value;
    
    if (m_Volume.NotEmpty()) {
        m_Volume->AddColumnMetaData(col_id, key, value);
    }
}

CBlastDbBlob & CWriteDB_Impl::SetBlobData(int col_id)
{
    typedef CBlastDbBlob TBlob;
    
    if ((col_id < 0) || (col_id * 2 >= (int) m_Blobs.size())) {
        NCBI_THROW(CWriteDBException, eArgErr,
                   "Error: provided column ID is not valid");
    }
    
    if (m_HaveBlob[col_id] > 1) {
        NCBI_THROW(CWriteDBException, eArgErr,
                   "Error: Already have blob for this sequence and column");
    }
    
    ++m_HaveBlob[col_id];
    
    // Blobs are reused to reduce buffer reallocation; a missing blob
    // means the corresponding column does not exist.
    
    return *m_Blobs[col_id * 2 + m_HaveBlob[col_id] - 1];
}
#endif

void CWriteDB_Impl::SetPig(int pig)
{
    m_Pig = pig;
}

void CWriteDB_Impl::SetMaxFileSize(Uint8 sz)
{
    m_MaxFileSize = sz;
}

void CWriteDB_Impl::SetMaxVolumeLetters(Uint8 sz)
{
    m_MaxVolumeLetters = sz;
}

CRef<CBlast_def_line_set>
CWriteDB_Impl::ExtractBioseqDeflines(const CBioseq & bs, bool parse_ids)
{
    // Get information
    
    CConstRef<CBlast_def_line_set> deflines;
    string binary_header;
    vector< vector<int> > v1, v2;
    
    CConstRef<CBioseq> bsref(& bs);
    x_ExtractDeflines(bsref, deflines, binary_header, v2, v2, 0, -1, parse_ids);
    
    // Convert to return type
    
    CRef<CBlast_def_line_set> bdls;
    bdls.Reset(const_cast<CBlast_def_line_set*>(&*deflines));
    
    return bdls;
}

void CWriteDB_Impl::SetMaskedLetters(const string & masked)
{
    // Only supported for protein.
    
    if (! m_Protein) {
        NCBI_THROW(CWriteDBException,
                   eArgErr,
                   "Error: Nucleotide masking not supported.");
    }
    
    m_MaskedLetters = masked;
    
    if (masked.empty()) {
        vector<char> none;
        m_MaskLookup.swap(none);
        return;
    }
    
    // Convert set of masked letters to stdaa, use the result to build
    // a lookup table.
    
    string mask_bytes;
    CSeqConvert::Convert(m_MaskedLetters,
                         CSeqUtil::e_Iupacaa,
                         0,
                         (int) m_MaskedLetters.size(),
                         mask_bytes,
                         CSeqUtil::e_Ncbistdaa);
    
    _ASSERT(mask_bytes.size() == m_MaskedLetters.size());
    
    // Build a table of character-to-bool.
    // (Bool is represented by char 0 and 1.)
    
    m_MaskLookup.resize(256, (char)0);
    
    for (unsigned i = 0; i < mask_bytes.size(); i++) {
        int ch = ((int) mask_bytes[i]) & 0xFF;
        m_MaskLookup[ch] = (char)1;
    }
    
    // Convert the masking character - always 'X' - to stdaa.
    
    if (m_MaskByte.empty()) {
        string mask_byte = "X";
        
        CSeqConvert::Convert(mask_byte,
                             CSeqUtil::e_Iupacaa,
                             0,
                             1,
                             m_MaskByte,
                             CSeqUtil::e_Ncbistdaa);
        
        _ASSERT(m_MaskByte.size() == 1);
    }
}

void CWriteDB_Impl::ListVolumes(vector<string> & vols)
{
    vols.clear();
    
    ITERATE(vector< CRef<CWriteDB_Volume> >, iter, m_VolumeList) {
        vols.push_back((**iter).GetVolumeName());
    }
}

void CWriteDB_Impl::ListFiles(vector<string> & files)
{
    files.clear();
    
    ITERATE(vector< CRef<CWriteDB_Volume> >, iter, m_VolumeList) {
        (**iter).ListFiles(files);
    }
    
    if (m_VolumeList.size() > 1) {
        files.push_back(x_MakeAliasName());
    }
}

/// Compute the hash of a (raw) sequence.
///
/// The hash of the provided sequence will be computed and assigned to
/// the m_Hash field.  For protein, the sequence is in the Ncbistdaa
/// format.  For nucleotide, the sequence and optional ambiguities are
/// in 'raw' format, meaning they are packed just as sequences are
/// packed in nsq files.
///
/// @param sequence The sequence data. [in]
/// @param ambiguities Nucleotide ambiguities are provided here. [in]
void CWriteDB_Impl::x_ComputeHash(const CTempString & sequence,
                                  const CTempString & ambig)
{
    if (m_Protein) {
        m_Hash = SeqDB_SequenceHash(sequence.data(), sequence.size());
    } else {
        string na8;
        SeqDB_UnpackAmbiguities(sequence, ambig, na8);
        m_Hash = SeqDB_SequenceHash(na8.data(), na8.size());
    }
}

/// Compute the hash of a (Bioseq) sequence.
///
/// The hash of the provided sequence will be computed and
/// assigned to the m_Hash member.  The sequence is packed as a
/// CBioseq.
///
/// @param sequence The sequence as a CBioseq. [in]
void CWriteDB_Impl::x_ComputeHash(const CBioseq & sequence)
{
    m_Hash = SeqDB_SequenceHash(sequence);
}

void CWriteDB_Impl::
x_GetFastaReaderDeflines(const CBioseq                  & bioseq,
                         CConstRef<CBlast_def_line_set> & deflines,
                         const vector< vector<int> >    & membits,
                         const vector< vector<int> >    & linkout,
                         int                              pig,
                         bool                             accept_gt,
                         bool                             parse_ids)
{
    if (! bioseq.CanGetDescr()) {
        return;
    }
    
    string fasta;
    
    // Scan the CBioseq for the CFastaReader user object.
    
    ITERATE(list< CRef< CSeqdesc > >, iter, bioseq.GetDescr().Get()) {
        const CSeqdesc & desc = **iter;
        
        if (desc.IsUser() &&
            desc.GetUser().CanGetType() &&
            desc.GetUser().GetType().IsStr() &&
            desc.GetUser().GetType().GetStr() == "CFastaReader" &&
            desc.GetUser().CanGetData()) {
            
            const vector< CRef< CUser_field > > & D = desc.GetUser().GetData();
            
            ITERATE(vector< CRef< CUser_field > >, iter, D) {
                const CUser_field & f = **iter;
                
                if (f.CanGetLabel() &&
                    f.GetLabel().IsStr() &&
                    f.GetLabel().GetStr() == "DefLine" &&
                    f.CanGetData() &&
                    f.GetData().IsStr()) {
                    
                    fasta = NStr::ParseEscapes(f.GetData().GetStr());
                    break;
                }
            }
        }
    }
    
    if (fasta.empty())
        return;
    
    // The bioseq has a field contianing the ids for the first
    // defline.  The title string contains the title for the first
    // defline, plus all the other defline titles and ids.  This code
    // unpacks them and builds a normal blast defline set.
    
    unsigned mship_i(0), links_i(0);
    bool used_pig(false);
    
    // Build the deflines.
    
    CRef<CBlast_def_line_set> bdls(new CBlast_def_line_set);
    CRef<CBlast_def_line> defline;

    if (!parse_ids) {

        // Generate an BL_ORD_ID in case no parse is needed
        CRef<CSeq_id> gnl_id(new CSeq_id());
        gnl_id->SetGeneral().SetDb("BL_ORD_ID");
        gnl_id->SetGeneral().SetTag().SetId(0);  // will be filled later
     
        // Build the local defline.
        defline.Reset(new CBlast_def_line);
        defline->SetSeqid().push_back(gnl_id);

        string title(fasta, 1, fasta.size());
        // Replace ^A with space
        for(size_t pos=0; pos < title.size(); ++pos) {
            if (title[pos] == '\001') title[pos] = ' ';
        }
        defline->SetTitle(title);

        if (mship_i < membits.size()) {
            const vector<int> & V = membits[mship_i++];
            defline->SetMemberships().assign(V.begin(), V.end());
        }
        
        if (links_i < linkout.size()) {
            const vector<int> & V = linkout[mship_i++];
            defline->SetLinks().assign(V.begin(), V.end());
        }
        
        if ((! used_pig) && pig) {
            defline->SetOther_info().push_back(pig);
            used_pig = true;
        }

        bdls->Set().push_back(defline);
        
    } else {

        int skip = 1;
        while(fasta.size()) {
            size_t id_start = skip;
            size_t pos_title = fasta.find(" ", skip);
            size_t pos_next = fasta.find("\001", skip);
            skip = 1;
        
            if (pos_next == fasta.npos) {
                if (accept_gt) {
                    pos_next = fasta.find(" >");
                    skip = 2;
                }
            } else {
                // If there is a ^A, turn off GT checking.
                accept_gt = false;
            }
        
            if (pos_next == fasta.npos) {
                pos_next = fasta.size();
                skip = 0;
            }

            if (pos_title == fasta.npos || pos_title >= pos_next) {
                // title field is missing
                pos_title = pos_next;
            }
        
            string ids(fasta, id_start, pos_title - id_start);
            if (pos_title == pos_next) pos_title--;
            string title(fasta, pos_title + 1, pos_next-pos_title - 1);
            string remaining(fasta, pos_next, fasta.size() - pos_next);
            fasta.swap(remaining);
        
            // Parse '|' seperated ids.
            list< CRef<CSeq_id> > seqids;
            if ( ids.find('|') == NPOS 
              || !isalpha((unsigned char)(ids[0]))) {
                 seqids.push_back(CRef<CSeq_id> (new CSeq_id(CSeq_id::e_Local, ids)));
            } else {
                 CSeq_id::ParseFastaIds(seqids, ids);
            }
        
            // Build the actual defline.
        
            defline.Reset(new CBlast_def_line);
            defline->SetSeqid().swap(seqids);
            defline->SetTitle(title);
        
            if (mship_i < membits.size()) {
                const vector<int> & V = membits[mship_i++];
                defline->SetMemberships().assign(V.begin(), V.end());
            }
        
            if (links_i < linkout.size()) {
                const vector<int> & V = linkout[mship_i++];
                defline->SetLinks().assign(V.begin(), V.end());
            }
        
            if ((! used_pig) && pig) {
                defline->SetOther_info().push_back(pig);
                used_pig = true;
            }
        
            bdls->Set().push_back(defline);
        }
    }
    s_CheckEmptyLists(bdls, true);
    deflines = bdls;
}

#if ((!defined(NCBI_COMPILER_WORKSHOP) || (NCBI_COMPILER_VERSION  > 550)) && \
     (!defined(NCBI_COMPILER_MIPSPRO)) )
int CWriteDB_Impl::x_GetMaskDataColumnId()
{
    if (m_MaskDataColumn == -1) {
        m_MaskDataColumn = CreateColumn("BlastDb/MaskData", true);
    }
    return m_MaskDataColumn;
}
#endif

END_NCBI_SCOPE


