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
 * Authors:  Josh Cherry
 *
 * File Description:  Data loader for user features in tabular form
 *
 */

#include <ncbi_pch.hpp>
#include <objtools/data_loaders/table/user_feature_dload.hpp>
#include <sqlite/sqlite.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objects/general/Object_id.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_id.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objmgr/scope.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/impl/synonyms.hpp>
#include <objmgr/impl/handle_range_map.hpp>

#include <serial/serial.hpp>
#include <serial/objostr.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


bool CUsrFeatDataLoader::SIdHandleByContent::operator()
    (const CSeq_id_Handle& h1, const CSeq_id_Handle& h2) const
{
    CConstRef<CSeq_id> id1 = h1.GetSeqId();
    CConstRef<CSeq_id> id2 = h2.GetSeqId();
    return (*id1 < *id2);
}


CUsrFeatDataLoader::TRegisterLoaderInfo
CUsrFeatDataLoader::RegisterInObjectManager(
    objects::CObjectManager& om,
    const string& input_file,
    const string& temp_file,
    bool delete_file,
    EOffset offset,
    const string& type,
    const objects::CSeq_id* given_id,
    objects::CObjectManager::EIsDefault is_default,
    objects::CObjectManager::TPriority priority)
{
    TRegisterLoaderInfo info;
    string name = GetLoaderNameFromArgs(input_file,
                                        temp_file,
                                        delete_file,
                                        offset,
                                        type,
                                        given_id);
    CDataLoader* loader = om.FindDataLoader(name);
    if ( loader ) {
        info.Set(loader, false);
        return info;
    }
    loader = new CUsrFeatDataLoader(name,
                                    input_file,
                                    temp_file,
                                    delete_file,
                                    offset,
                                    type,
                                    given_id);
    CObjectManager::TRegisterLoaderInfo base_info =
        CDataLoader::RegisterInObjectManager(om, name, *loader,
                                             is_default, priority);
    info.Set(base_info.GetLoader(), base_info.IsCreated());
    return info;
}


string CUsrFeatDataLoader::GetLoaderNameFromArgs(
    const string& input_file,
    const string& temp_file,
    bool delete_file,
    EOffset offset,
    const string& type,
    const objects::CSeq_id* given_id)
{
    return input_file;
}


CUsrFeatDataLoader::CUsrFeatDataLoader(const string& loader_name,
                                       const string& input_file,
                                       const string& temp_file,
                                       bool delete_file,
                                       EOffset offset,
                                       const string& type,
                                       const CSeq_id* given_id)
    : CDataLoader(loader_name), m_Offset(offset), m_Type(type)
{
    //
    // create our SQLite DB
    //
    m_Table.Reset(new CSQLiteTable(input_file, temp_file, delete_file));

    //
    // now, store some precalculated info about our table
    //
    {{
        // extract the column names
        list<string> cols;
        m_Table->GetColumnTitles(cols);
        m_Cols.reserve(cols.size());
        std::copy(cols.begin(), cols.end(), back_inserter(m_Cols));
    }}

    // determine our column mapping
    int i = 0;
    m_ColAssign.resize(m_Cols.size(), eUnknown);
    fill(m_ColIdx, m_ColIdx + eMaxKnownCol, -1);

    ITERATE(vector<string>, iter, m_Cols) {
        string str(*iter);
        NStr::ToLower(str);
        if (str == "contig"  ||
            str == "contig_accession"  ||
            str == "accession" ||
            str == "id") {
            m_ColAssign[i] = eAccession;
            m_ColIdx[eAccession] = i;
        } else if (str == "from") {
            m_ColAssign[i] = eFrom;
            m_ColIdx[eFrom] = i;
        } else if (str == "to") {
            m_ColAssign[i] = eTo;
            m_ColIdx[eTo] = i;
        } else if (str == "type") {
            m_ColAssign[i] = eType;
            m_ColIdx[eType] = i;
        } else if (str == "strand"  ||
                   str == "orientation") {
            m_ColAssign[i] = eStrand;
            m_ColIdx[eStrand] = i;
        }

        ++i;
    }

    if (given_id) {
        m_SeqId.Reset(given_id);
    }

    if (!m_SeqId && m_ColIdx[eAccession] == -1) {
        LOG_POST(Info << "CUsrFeatDataLoader: no id column in file, "
                 "and no id given as parameter");
        throw runtime_error("no id column in file, "
                            "and no id given as parameter");
    }

    CSQLite& sqlite = m_Table->SetDB();

    if (!m_SeqId) {
        string acc_col = "col" + NStr::IntToString(m_ColIdx[eAccession]);

        // create an index on accession
        try {
            sqlite.Execute("create index IDX_accession "
                           "on TableData (" + acc_col + ")");
        }
        catch (...) {
            // index already exists - ignored
        }

        // extract a list of the accessions we have
        CRef<CSQLiteQuery> q
            (sqlite.Compile("select distinct " + acc_col +
                            " from TableData order by " + acc_col));

        int count;
        const char** data = NULL;
        const char** cols = NULL;
        while (q->NextRow(count, data, cols)) {
            CRef<CSeq_id> id(new CSeq_id(data[0]));
            if (id->Which() == CSeq_id::e_not_set) {
                LOG_POST(Error << "failed to index id = " << data[0]);
                continue;
            }

            CSeq_id_Handle handle = CSeq_id_Handle::GetHandle(*id);
            m_Ids.insert(TIdMap::value_type(handle, data[0]));
            _TRACE("  id = " << data[0]);
        }

        LOG_POST(Info << "CUsrFeatDataLoader: " 
                 << m_Ids.size() << " distinct ids");
    } else {
        LOG_POST(Info << "CUsrFeatDataLoader: using single "
                 "specified id");
    }
}


// Request from a datasource using handles and ranges instead of seq-loc
// The TSEs loaded in this call will be added to the tse_set.
void CUsrFeatDataLoader::GetRecords(const CSeq_id_Handle& idh,
                                    EChoice choice)
{
    //
    // find out if we've already loaded annotations for this seq-id
    //
    TEntries::iterator iter = m_Entries.find(idh);
    if (iter != m_Entries.end()) {
        return;
    }

    CRef<CSeq_annot> annot = GetAnnot(idh);
    if (!annot) {
        return;
    }

    CRef<CSeq_entry> entry;

    // we then add the object to the data loader
    // we need to create a dummy TSE for it first
    entry.Reset(new CSeq_entry());
    entry->SetSet().SetSeq_set();
    entry->SetSet().SetAnnot().push_back(annot);
    GetDataSource()->AddTSE(*entry);
    
    _TRACE("CUsrFeatDataLoader(): loaded "
           << annot->GetData().GetFtable().size()
           << " features for " << idh.AsString());
    

    // we always save an entry here.  If the entry is empty,
    // we have no information about this sequence, but we at
    // least don't need to repeat an expensive search
    m_Entries[idh] = entry;
}


CRef<CSeq_annot> CUsrFeatDataLoader::GetAnnot(const CSeq_id_Handle& idh)
{

    CRef<CSeq_annot> annot;

    CSQLiteTable::TIterator row_iter;
    if (!m_SeqId) {
        //
        // find out if this ID is in our list of ids
        //
        pair<TIdMap::iterator, TIdMap::iterator> id_iter
            = m_Ids.equal_range(idh);
        if (id_iter.first == id_iter.second) {
            return annot;  // null CRef
        }
        // select just the rows with that match the id
        string acc_col = "col" + NStr::IntToString(m_ColIdx[eAccession]);
        string sql("select * from TableData where " + acc_col +
                   " in (");
        string tmp;
        for ( ;  id_iter.first != id_iter.second;  ++id_iter.first) {
            TIdMap::iterator iter = id_iter.first;
            
            if ( !tmp.empty() ) {
                tmp += ", ";
            }
            tmp += "'" + iter->second + "'";
        }
        sql += tmp + ")";
        row_iter = m_Table->Begin(sql);
        
    } else {
        // check that this is the right id
        if (!idh.GetSeqId()->Match(*m_SeqId)) {
            return annot;  // null CRef
        }
        // select 'em all
        row_iter = m_Table->Begin("select * from TableData");
    }


    annot.Reset(new CSeq_annot());
    vector<string> data;

    for ( ;  *row_iter;  ++(*row_iter)) {
        list<string> temp;
        (*row_iter).GetRow(temp);
        data.resize(temp.size());
        std::copy(temp.begin(), temp.end(), data.begin());


        // create a new feature
        CRef<CSeq_feat> feat(new CSeq_feat());
        CSeq_loc& loc = feat->SetLocation();
        loc.SetInt().SetId().Assign(*idh.GetSeqId());
        CUser_object& user = feat->SetData().SetUser();

        // fill in our columns
        TSeqPos from;
        TSeqPos to;
        string strand_str;
        for (unsigned int i = 0;  i < data.size();  ++i) {
            switch (m_ColAssign[i]) {
            case eAccession:
                // already handled as ID...
                break;

            case eStrand:
                strand_str = NStr::ToLower(data[i]);
                if (strand_str == "+" || strand_str == "plus" ||
                    strand_str == "positive" || strand_str == "forward") {
                    loc.SetInt().SetStrand(eNa_strand_plus);
                } else if (strand_str == "-" || strand_str == "minus" ||
                           strand_str == "negative" || 
                           strand_str == "reverse") {
                    loc.SetInt().SetStrand(eNa_strand_minus);
                } else if (strand_str == "b" || strand_str == "+/-" ||
                           strand_str == "both") {
                    loc.SetInt().SetStrand(eNa_strand_both);
                } else {
                    throw runtime_error(string("Invalid strand designation: ")
                                        + data[i]);
                }
                break;

            case eFrom:
                from = NStr::StringToInt(data[i]);
                break;

            case eTo:
                to = NStr::StringToInt(data[i]);
                break;

            case eType:
                user.SetType().SetStr(data[i]);
                break;

            case eUnknown:
            default:
                // add a user field, unless column title
                // starts with '.'
                if (!NStr::StartsWith(m_Cols[i], ".")) {
                    user.AddField(m_Cols[i], data[i]);
                }
                break;
            }
        }

        loc.SetInt().SetFrom(from - m_Offset);
        loc.SetInt().SetTo  (to - m_Offset);
        if (!m_Type.empty() || m_ColIdx[eType] == -1) {
            user.SetType().SetStr(m_Type);
        }
        annot->SetData().SetFtable().push_back(feat);
    }
    return annot;
}

END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.4  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.3  2004/05/21 21:42:53  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.2  2003/11/28 13:41:10  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.1  2003/11/14 19:09:18  jcherry
 * Initial version
 *
 * ===========================================================================
 */
