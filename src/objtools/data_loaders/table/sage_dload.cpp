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
#include <objtools/data_loaders/table/sage_dload.hpp>
#include <sqlite/sqlite.hpp>

#include <objects/general/User_object.hpp>
#include <objects/general/User_field.hpp>
#include <objtools/manip/sage_manip.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objects/seqloc/Seq_loc.hpp>
#include <objects/seqloc/Seq_interval.hpp>
#include <objects/seqset/Seq_entry.hpp>
#include <objects/seqset/Bioseq_set.hpp>

#include <objmgr/impl/tse_loadlock.hpp>
#include <objmgr/impl/data_source.hpp>
#include <objmgr/data_loader_factory.hpp>
#include <corelib/plugin_manager_impl.hpp>
#include <corelib/plugin_manager_store.hpp>


BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


bool CSageDataLoader::SIdHandleByContent::operator()(const CSeq_id_Handle& h1,
                                                     const CSeq_id_Handle& h2) const
{
    CConstRef<CSeq_id> id1 = h1.GetSeqId();
    CConstRef<CSeq_id> id2 = h2.GetSeqId();
    return (*id1 < *id2);
}


CSageDataLoader::TRegisterLoaderInfo CSageDataLoader::RegisterInObjectManager(
    CObjectManager& om,
    const string& input_file,
    const string& temp_file,
    bool delete_file,
    CObjectManager::EIsDefault is_default,
    CObjectManager::TPriority priority)
{
    SSageParam param(input_file, temp_file, delete_file);
    TMaker maker(param);
    CDataLoader::RegisterInObjectManager(om, maker, is_default, priority);
    return maker.GetRegisterInfo();
}


string CSageDataLoader::GetLoaderNameFromArgs(const SSageParam& param)
{
    // This may cause conflicts if some other loader uses the same
    // approach. Probably needs to add some prefix.
    return param.m_InputFile;
}


CSageDataLoader::CSageDataLoader(const string& loader_name,
                                 const SSageParam& param)
    : CDataLoader(loader_name)
{
    //
    // create our SQLite DB
    //
    m_Table.Reset(new CSQLiteTable(param.m_InputFile,
                                   param.m_TempFile,
                                   param.m_DeleteFile));

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
            str == "accession") {
            m_ColAssign[i] = eAccession;
            m_ColIdx[eAccession] = i;
        } else if (str == "tag_position_on_contig"  ||
                   str == "pos"  ||
                   str == "contig_pos") {
            m_ColAssign[i] = eFrom;
            m_ColIdx[eFrom] = i;
        } else if (str == "tag"  ||
                   str == "catgtag") {
            m_ColAssign[i] = eTag;
            m_ColIdx[eTag] = i;
        } else if (str == "method") {
            m_ColAssign[i] = eMethod;
            m_ColIdx[eMethod] = i;
        } else if (str == "tag_orientation"  ||
                   str == "strand"  ||
                   str == "orientation") {
            m_ColAssign[i] = eStrand;
            m_ColIdx[eStrand] = i;
        }

        ++i;
    }

    string acc_col = "col" + NStr::IntToString(m_ColIdx[eAccession]);

    CSQLite& sqlite = m_Table->SetDB();
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

    LOG_POST(Info << "CSageDataLoader: " << m_Ids.size() << " distinct ids");

}


// Request from a datasource using handles and ranges instead of seq-loc
// The TSEs loaded in this call will be added to the tse_set.
CDataLoader::TTSE_LockSet
CSageDataLoader::GetRecords(const CSeq_id_Handle& idh,
                            EChoice choice)
{
    TTSE_LockSet locks;
    if ( choice < eOrphanAnnot ) {
        // orphan annotations only - no Bioseqs in this DB
        return locks;
    }
    string acc_col = "col" + NStr::IntToString(m_ColIdx[eAccession]);

    //
    // find out if we've already loaded annotations for this seq-id
    //
    TEntries::iterator iter = m_Entries.find(idh);
    if (iter != m_Entries.end()) {
        CConstRef<CObject> blob_id(&*iter->second);
        CTSE_LoadLock load_lock =
            GetDataSource()->GetTSE_LoadLock(blob_id);
        if ( !load_lock.IsLoaded() ) {
            locks.insert(GetDataSource()->AddTSE(*iter->second));
            load_lock.SetLoaded();
        }
        return locks;
    }

    //
    // now, find out if this ID is in our list of ids
    //
    pair<TIdMap::iterator, TIdMap::iterator> id_iter = m_Ids.equal_range(idh);
    if (id_iter.first == id_iter.second) {
        return locks;
    }

    //
    // okay, this ID is correct, so load all features with this id
    //

    string sql("select * from TableData where " + acc_col +
               " in (");
    string tmp;
    for ( ;  id_iter.first != id_iter.second;  ++id_iter.first) {
        TIdMap::iterator it = id_iter.first;

        if ( !tmp.empty() ) {
            tmp += ", ";
        }
        tmp += "'" + it->second + "'";
    }
    sql += tmp + ")";

    CSQLiteTable::TIterator row_iter = m_Table->Begin(sql);

    CRef<CSeq_annot> annot(new CSeq_annot());
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

        CUser_object& user = feat->SetData().SetUser()
            .SetCategory(CUser_object::eCategory_Experiment);
        CUser_object& experiment =
            user.SetExperiment(CUser_object::eExperiment_Sage);
        CSageData manip(experiment);

        // fill in our columns
        bool neg_strand = false;
        TSeqPos from = 0;
        TSeqPos len  = 0;
        for (size_t i = 0;  i < data.size();  ++i) {
            switch (m_ColAssign[i]) {
            case eAccession:
                // already handled as ID...
                break;

            case eTag:
                manip.SetTag(data[i]);
                len = data[i].length();
                break;

            case eStrand:
                if (NStr::Compare(data[i], "-") == 0) {
                    neg_strand = true;
                }
                break;

            case eFrom:
                from = NStr::StringToInt(data[i]);
                break;

            case eMethod:
                manip.SetMethod(data[i]);
                break;

            case eUnknown:
            default:
                manip.SetField(m_Cols[i], data[i]);
                break;
            }
        }

        size_t to = from + len;
        if (neg_strand) {
            loc.SetInt().SetStrand(eNa_strand_minus);

            size_t offs = to - from;
            from -= offs;
            to   -= offs;
        } else {
            loc.SetInt().SetStrand(eNa_strand_plus);
        }

        loc.SetInt().SetFrom(from);
        loc.SetInt().SetTo  (to);

        annot->SetData().SetFtable().push_back(feat);
    }

    CRef<CSeq_entry> entry;
    if (annot) {
        // we then add the object to the data loader
        // we need to create a dummy TSE for it first
        entry.Reset(new CSeq_entry());
        entry->SetSet().SetSeq_set();
        entry->SetSet().SetAnnot().push_back(annot);

        CConstRef<CObject> blob_id(&*entry);
        CTSE_LoadLock load_lock = GetDataSource()->GetTSE_LoadLock(blob_id);
        _ASSERT(!load_lock.IsLoaded());
        locks.insert(GetDataSource()->AddTSE(*entry));
        load_lock.SetLoaded();

        _TRACE("CSageDataLoader(): loaded "
            << annot->GetData().GetFtable().size()
            << " features for " << idh.AsString());
    }

    // we always save an entry here.  If the entry is empty,
    // we have no information about this sequence, but we at
    // least don't need to repeat an expensive search
    m_Entries[idh] = entry;
    return locks;
}


// ===========================================================================

USING_SCOPE(objects);

void DataLoaders_Register_Sage(void)
{
    RegisterEntryPoint<CDataLoader>(NCBI_EntryPoint_DataLoader_Sage);
}


const string kDataLoader_Sage_DriverName("sage");

class CSage_DataLoaderCF : public CDataLoaderFactory
{
public:
    CSage_DataLoaderCF(void)
        : CDataLoaderFactory(kDataLoader_Sage_DriverName) {}
    virtual ~CSage_DataLoaderCF(void) {}

protected:
    virtual CDataLoader* CreateAndRegister(
        CObjectManager& om,
        const TPluginManagerParamTree* params) const;
};


CDataLoader* CSage_DataLoaderCF::CreateAndRegister(
    CObjectManager& om,
    const TPluginManagerParamTree* params) const
{
    if ( !ValidParams(params) ) {
        // Can not create loader without arguments
        return 0;
    }
    // Parse params
    const string& input_file =
        GetParam(GetDriverName(), params,
        kCFParam_Sage_InputFile, true, kEmptyStr);
    const string& temp_file =
        GetParam(GetDriverName(), params,
        kCFParam_Sage_TempFile, true, kEmptyStr);
    const string& delete_file_str =
        GetParam(GetDriverName(), params,
        kCFParam_Sage_TempFile, false, "1");
    bool delete_file = (delete_file_str == "1");

    return CSageDataLoader::RegisterInObjectManager(
        om,
        input_file,
        temp_file,
        delete_file,
        GetIsDefault(params),
        GetPriority(params)).GetLoader();
}


void NCBI_EntryPoint_DataLoader_Sage(
    CPluginManager<CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<CDataLoader>::EEntryPointRequest method)
{
    CHostEntryPointImpl<CSage_DataLoaderCF>::NCBI_EntryPointImpl(info_list, method);
}


void NCBI_EntryPoint_xloader_sage(
    CPluginManager<objects::CDataLoader>::TDriverInfoList&   info_list,
    CPluginManager<objects::CDataLoader>::EEntryPointRequest method)
{
    NCBI_EntryPoint_DataLoader_Sage(info_list, method);
}


END_NCBI_SCOPE


/*
 * ===========================================================================
 * $Log$
 * Revision 1.14  2005/02/02 19:49:55  grichenk
 * Fixed more warnings
 *
 * Revision 1.13  2004/12/22 20:42:53  grichenk
 * Added entry points registration funcitons
 *
 * Revision 1.12  2004/10/25 16:53:52  vasilche
 * Sage features are orphan, not external.
 *
 * Revision 1.11  2004/08/10 16:56:11  grichenk
 * Fixed dll export declarations, moved entry points to cpp.
 *
 * Revision 1.10  2004/08/04 14:56:35  vasilche
 * Updated to changes in TSE locking scheme.
 *
 * Revision 1.9  2004/08/02 17:34:44  grichenk
 * Added data_loader_factory.cpp.
 * Renamed xloader_cdd to ncbi_xloader_cdd.
 * Implemented data loader factories for all loaders.
 *
 * Revision 1.8  2004/07/28 14:02:57  grichenk
 * Improved MT-safety of RegisterInObjectManager(), simplified the code.
 *
 * Revision 1.7  2004/07/26 14:13:32  grichenk
 * RegisterInObjectManager() return structure instead of pointer.
 * Added CObjectManager methods to manipuilate loaders.
 *
 * Revision 1.6  2004/07/21 15:51:26  grichenk
 * CObjectManager made singleton, GetInstance() added.
 * CXXXXDataLoader constructors made private, added
 * static RegisterInObjectManager() and GetLoaderNameFromArgs()
 * methods.
 *
 * Revision 1.5  2004/05/21 21:42:53  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.4  2003/11/28 13:41:10  dicuccio
 * Fixed to match new API in CDataLoader
 *
 * Revision 1.3  2003/11/13 21:01:03  dicuccio
 * Altered to support finding multiple accession strings matching a single handle
 *
 * Revision 1.2  2003/10/30 21:43:08  jcherry
 * Fixed typo
 *
 * Revision 1.1  2003/10/02 17:40:17  dicuccio
 * Initial revision
 *
 * ===========================================================================
 */
