#ifndef DATA_LOADER__HPP
#define DATA_LOADER__HPP

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
* Author: Aleksey Grichenko, Michael Kimelman
*
* File Description:
*   Data loader base class for object manager
*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.6  2002/03/20 04:50:34  kimelman
* GB loader added
*
* Revision 1.5  2002/03/18 23:05:18  kimelman
* comments
*
* Revision 1.4  2002/03/18 17:26:32  grichenk
* +CDataLoader::x_GetSeq_id(), x_GetSeq_id_Key(), x_GetSeq_id_Handle()
*
* Revision 1.3  2002/03/11 21:10:11  grichenk
* +CDataLoader::ResolveConflict()
*
* Revision 1.2  2002/02/21 19:27:00  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.1  2002/01/11 19:04:00  gouriano
* restructured objmgr
*
*
* ===========================================================================
*/


#include <corelib/ncbiobj.hpp>
#include <objects/objmgr1/data_loader_factory.hpp>
#include <objects/objmgr1/seq_id_handle.hpp>
#include <set>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


// fwd decl
class CSeq_loc;
class CSeq_entry;
class CDataSource;
class CHandleRangeMap;
class CTSE_Info;


////////////////////////////////////////////////////////////////////
//
//  CDataLoader::
//


class CDataLoader : public CObject
{
protected:
    CDataLoader(void);
public:
    CDataLoader(const string& loader_name);
    virtual ~CDataLoader(void);

public:
    enum EChoice {
        eBlob,        // whole seqentry
        eBioseq,      // whole bioseq
        eCore,        // everything except bioseqs & annotations
        eBioseqCore,  // bioseq without seqdata and annotations
        eSequence,    // seq data 
        eFeatures,    // SeqFeatures
        eGraph,       // SeqGraph 
        eAll          // whatever fits location
    };
    
    // Request from a datasource for data specified in "choice".
    // The data loaded will be sent back to the datasource through
    // CDataSource::AppendXXX() methods.
    //### virtual bool GetRecords(const CSeq_loc& loc, EChoice choice) = 0;
    
    // Request from a datasource using handles and ranges instead of seq-loc
    virtual bool GetRecords(const CHandleRangeMap& hrmap,
                            const EChoice choice) = 0;
    
    // 
    virtual bool DropTSE(const CSeq_entry *sep) = 0 ;
    
    // Specify datasource to send loaded data to.
    void SetTargetDataSource(CDataSource& data_source);
    
    string GetName(void) const;
    
    // Resolve TSE conflict
    // *select the best TSE from the set of dead TSEs.
    // *select the live TSE from the list of live TSEs
    //  and mark the others one as dead.
    typedef set< CRef<CTSE_Info> > TTSESet;
    virtual CTSE_Info*
      ResolveConflict(const CSeq_id_Handle&,
                      const TTSESet&) { return 0; } //### = 0;
    
protected:
    void SetName(const string& loader_name);
    CDataSource* GetDataSource(void);
    
    TSeq_id_Key    x_GetSeq_id_Key(const CSeq_id_Handle& handle);
    CSeq_id_Handle x_GetSeq_id_Handle(TSeq_id_Key key);
    const CSeq_id* x_GetSeq_id(const CSeq_id_Handle& handle) const;

private:
    string       m_Name;
    CDataSource* m_DataSource;
    
    CRef<CSeq_id_Mapper> m_Mapper;
    friend class CObjectManager;
};


inline
const CSeq_id* CDataLoader::x_GetSeq_id(const CSeq_id_Handle& handle) const
{
    return handle.x_GetSeqId();
}


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // DATA_LOADER__HPP
