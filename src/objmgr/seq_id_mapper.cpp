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
* Author: Aleksey Grichenko, Eugene Vasilchenko
*
* File Description:
*   Seq-id mapper for Object Manager
*
*/

#include <ncbi_pch.hpp>
#include <objmgr/seq_id_mapper.hpp>
#include <objmgr/impl/seq_id_tree.hpp>
#include <objmgr/objmgr_exception.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)

////////////////////////////////////////////////////////////////////
//
//  CSeq_id_Mapper::
//

    
static CSeq_id_Mapper* s_Seq_id_Mapper = 0;

CRef<CSeq_id_Mapper> CSeq_id_Mapper::GetSeq_id_Mapper(void)
{
    if (!s_Seq_id_Mapper) {
        s_Seq_id_Mapper = new CSeq_id_Mapper;
    }
    return Ref(s_Seq_id_Mapper);
}

CSeq_id_Mapper::CSeq_id_Mapper(void)
{
    CSeq_id_Which_Tree::Initialize(m_Trees);
}


CSeq_id_Mapper::~CSeq_id_Mapper(void)
{
#ifdef _DEBUG
    CSeq_id_Handle::DumpRegister("~CSeq_id_Mapper");
#endif

    ITERATE ( TTrees, it, m_Trees ) {
        _ASSERT((*it)->Empty());
    }
    _ASSERT(s_Seq_id_Mapper == this);
    s_Seq_id_Mapper = 0;
}


inline
CSeq_id_Which_Tree& CSeq_id_Mapper::x_GetTree(const CSeq_id& id)
{
    CSeq_id::E_Choice type = id.Which();
    _ASSERT(size_t(type) < m_Trees.size());
    return *m_Trees[type];
}


inline
CSeq_id_Which_Tree& CSeq_id_Mapper::x_GetTree(const CSeq_id_Handle& idh)
{
    return idh? *idh.m_Info->GetTree(): *m_Trees[CSeq_id::e_not_set];
}


CSeq_id_Handle CSeq_id_Mapper::GetGiHandle(int gi)
{
    _ASSERT(size_t(CSeq_id::e_Gi) < m_Trees.size());
    return m_Trees[CSeq_id::e_Gi]->GetGiHandle(gi);
}


CSeq_id_Handle CSeq_id_Mapper::GetHandle(const CSeq_id& id, bool do_not_create)
{
    CSeq_id_Which_Tree& tree = x_GetTree(id);
    return do_not_create? tree.FindInfo(id): tree.FindOrCreate(id);
}


bool CSeq_id_Mapper::HaveMatchingHandles(const CSeq_id_Handle& idh)
{
    return x_GetTree(idh).HaveMatch(idh);
}


void CSeq_id_Mapper::GetMatchingHandles(const CSeq_id_Handle& idh,
                                        TSeq_id_HandleSet& h_set)
{
    x_GetTree(idh).FindMatch(idh, h_set);
}


bool CSeq_id_Mapper::HaveReverseMatch(const CSeq_id_Handle& idh)
{
    return x_GetTree(idh).HaveReverseMatch(idh);
}


void CSeq_id_Mapper::GetReverseMatchingHandles(const CSeq_id_Handle& idh,
                                               TSeq_id_HandleSet& h_set)
{
    x_GetTree(idh).FindReverseMatch(idh, h_set);
}


void CSeq_id_Mapper::GetMatchingHandlesStr(string sid,
                                           TSeq_id_HandleSet& h_set)
{
    if (sid.find('|') != string::npos) {
        NCBI_THROW(CObjMgrException, eIdMapperError,
                   "Symbol \'|\' is not supported here");
    }

    ITERATE(TTrees, tree_it, m_Trees) {
        (*tree_it)->FindMatchStr(sid, h_set);
    }
}


bool CSeq_id_Mapper::x_IsBetter(const CSeq_id_Handle& h1, const CSeq_id_Handle& h2)
{
    CSeq_id_Which_Tree& tree1 = x_GetTree(h1);
    CSeq_id_Which_Tree& tree2 = x_GetTree(h2);
    if ( &tree1 != &tree2 )
        return false;

    // Compare versions if any
    return tree1.IsBetterVersion(h1, h2);
}


END_SCOPE(objects)
END_NCBI_SCOPE



/*
* ---------------------------------------------------------------------------
* $Log$
* Revision 1.46  2004/06/10 16:21:27  grichenk
* Changed CSeq_id_Mapper singleton type to pointer, GetSeq_id_Mapper
* returns CRef<> which is locked by CObjectManager.
*
* Revision 1.45  2004/06/08 19:18:40  grichenk
* Removed CSafeStaticRef from seq_id_mapper.hpp
*
* Revision 1.44  2004/05/21 21:42:13  gorelenk
* Added PCH ncbi_pch.hpp
*
* Revision 1.43  2004/02/19 17:25:35  vasilche
* Use CRef<> to safely hold pointer to CSeq_id_Info.
* CSeq_id_Info holds pointer to owner CSeq_id_Which_Tree.
* Reduce number of calls to CSeq_id_Handle.GetSeqId().
*
* Revision 1.42  2004/02/10 21:15:16  grichenk
* Added reverse ID matching.
*
* Revision 1.41  2004/01/22 20:10:41  vasilche
* 1. Splitted ID2 specs to two parts.
* ID2 now specifies only protocol.
* Specification of ID2 split data is moved to seqsplit ASN module.
* For now they are still reside in one resulting library as before - libid2.
* As the result split specific headers are now in objects/seqsplit.
* 2. Moved ID2 and ID1 specific code out of object manager.
* Protocol is processed by corresponding readers.
* ID2 split parsing is processed by ncbi_xreader library - used by all readers.
* 3. Updated OBJMGR_LIBS correspondingly.
*
* Revision 1.40  2003/10/07 13:43:23  vasilche
* Added proper handling of named Seq-annots.
* Added feature search from named Seq-annots.
* Added configurable adaptive annotation search (default: gene, cds, mrna).
* Fixed selection of blobs for loading from GenBank.
* Added debug checks to CSeq_id_Mapper for easier finding lost CSeq_id_Handles.
* Fixed leaked split chunks annotation stubs.
* Moved some classes definitions in separate *.cpp files.
*
* Revision 1.39  2003/09/30 16:22:03  vasilche
* Updated internal object manager classes to be able to load ID2 data.
* SNP blobs are loaded as ID2 split blobs - readers convert them automatically.
* Scope caches results of requests for data to data loaders.
* Optimized CSeq_id_Handle for gis.
* Optimized bioseq lookup in scope.
* Reduced object allocations in annotation iterators.
* CScope is allowed to be destroyed before other objects using this scope are
* deleted (feature iterators, bioseq handles etc).
* Optimized lookup for matching Seq-ids in CSeq_id_Mapper.
* Added 'adaptive' option to objmgr_demo application.
*
* Revision 1.38  2003/09/05 17:29:40  grichenk
* Structurized Object Manager exceptions
*
* Revision 1.37  2003/06/10 19:06:35  vasilche
* Simplified CSeq_id_Mapper and CSeq_id_Handle.
*
* Revision 1.35  2003/05/20 15:44:38  vasilche
* Fixed interaction of CDataSource and CDataLoader in multithreaded app.
* Fixed some warnings on WorkShop.
* Added workaround for memory leak on WorkShop.
*
* Revision 1.34  2003/05/06 18:51:15  grichenk
* Fixed minor bug in keys allocation
*
* Revision 1.33  2003/04/24 16:12:38  vasilche
* Object manager internal structures are splitted more straightforward.
* Removed excessive header dependencies.
*
* Revision 1.32  2003/04/18 13:45:48  grichenk
* Fixed bug in CSeq_id_Mapper::IsBetter()
*
* Revision 1.31  2003/03/11 16:15:04  kuznets
* Misprint corrected
*
* Revision 1.30  2003/03/11 15:51:06  kuznets
* iterate -> ITERATE
*
* Revision 1.29  2003/03/10 16:31:30  vasilche
* Moved implementation constant to .cpp file.
*
* Revision 1.28  2003/02/21 14:33:51  grichenk
* Display warning but don't crash on uninitialized seq-ids.
*
* Revision 1.27  2003/01/29 22:03:46  grichenk
* Use single static CSeq_id_Mapper instead of per-OM model.
*
* Revision 1.26  2003/01/22 20:11:54  vasilche
* Merged functionality of CSeqMapResolved_CI to CSeqMap_CI.
* CSeqMap_CI now supports resolution and iteration over sequence range.
* Added several caches to CScope.
* Optimized CSeqVector().
* Added serveral variants of CBioseqHandle::GetSeqVector().
* Tried to optimize annotations iterator (not much success).
* Rewritten CHandleRange and CHandleRangeMap classes to avoid sorting of list.
*
* Revision 1.25  2003/01/02 19:40:04  gouriano
* added checking CSeq_id for validity
*
* Revision 1.24  2002/12/26 20:55:18  dicuccio
* Moved seq_id_mapper.hpp, tse_info.hpp, and bioseq_info.hpp -> include/ tree
*
* Revision 1.23  2002/11/08 19:43:35  grichenk
* CConstRef<> constructor made explicit
*
* Revision 1.22  2002/11/04 21:29:12  grichenk
* Fixed usage of const CRef<> and CRef<> constructor
*
* Revision 1.21  2002/10/02 21:26:52  ivanov
* A CSeq_id_Which_Tree class declaration moved from .cpp to .hpp to make
* KCC happy
*
* Revision 1.20  2002/08/30 18:33:27  grichenk
* Fixed bug in segment releasing code
*
* Revision 1.19  2002/08/20 14:27:22  grichenk
* Fixed the problem with PDB ids
*
* Revision 1.18  2002/07/15 20:33:49  ucko
* CExceptString is now CStringException.
*
* Revision 1.17  2002/07/12 19:32:10  grichenk
* Fixed exception name
*
* Revision 1.16  2002/05/24 14:57:13  grichenk
* SerialAssign<>() -> CSerialObject::Assign()
*
* Revision 1.15  2002/05/09 14:18:55  grichenk
* Fixed "unused variable" warnings
*
* Revision 1.14  2002/05/03 13:18:44  grichenk
* OM_THROW_TRACE -> THROW1_TRACE
*
* Revision 1.13  2002/05/03 03:15:24  vakatov
* Temp. fix for the missing header <objects/objmgr1/om_defs.hpp>
*
* Revision 1.12  2002/05/02 20:42:37  grichenk
* throw -> THROW1_TRACE
*
* Revision 1.11  2002/04/22 20:03:48  grichenk
* Redesigned keys usage table to work in 64-bit mode
*
* Revision 1.10  2002/04/09 17:49:46  grichenk
* Fixed signed/unsigned warnings
*
* Revision 1.9  2002/03/22 21:51:04  grichenk
* Added indexing textseq-id without accession
*
* Revision 1.8  2002/03/15 18:10:09  grichenk
* Removed CRef<CSeq_id> from CSeq_id_Handle, added
* key to seq-id map th CSeq_id_Mapper
*
* Revision 1.7  2002/02/25 21:05:29  grichenk
* Removed seq-data references caching. Increased MT-safety. Fixed typos.
*
* Revision 1.6  2002/02/21 19:27:06  grichenk
* Rearranged includes. Added scope history. Added searching for the
* best seq-id match in data sources and scopes. Updated tests.
*
* Revision 1.5  2002/02/12 19:41:42  grichenk
* Seq-id handles lock/unlock moved to CSeq_id_Handle 'ctors.
*
* Revision 1.4  2002/02/06 21:46:11  gouriano
* *** empty log message ***
*
* Revision 1.3  2002/02/05 21:46:28  gouriano
* added FindSeqid function, minor tuneup in CSeq_id_mapper
*
* Revision 1.2  2002/02/01 21:49:51  gouriano
* minor changes to make it compilable and run on Solaris Workshop
*
* Revision 1.1  2002/01/23 21:57:49  grichenk
* Splitted id_handles.hpp
*
* ===========================================================================
*/
