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
 * File Description:
 *      SWIG %ignore's for things that are declared but not defined
 *
 */

%ignore BLAST_RPSSearchEngine;

%ignore ncbi::CAlignSelector::SetPublicOnly;

%ignore ncbi::CHTML_area::CHTML_area();

// objtools/data_loaders/cdd/cdd.hpp
%ignore ncbi::objects::CCddDataLoader::CCddDataLoader();

%ignore ncbi::objects::CFlatCodonQV::CFlatCodonQV(const string&);
%ignore ncbi::CIntervalTree::Add(const interval_type &interval,
                                 const mapped_type &value);
%ignore ncbi::CIntervalTree::Add(const interval_type &interval,
                                 const mapped_type &value, const nothrow_t &);
%ignore ncbi::CIntervalTree::Set(const interval_type&, const mapped_type&);
%ignore ncbi::CIntervalTree::Find();
%ignore ncbi::CIntervalTree::Find() const;
%ignore ncbi::CIntervalTree::Reset(const interval_type&);
%ignore ncbi::CIntervalTree::Delete(const interval_type&);
%ignore ncbi::CIntervalTree::Delete(const interval_type&, nothrow_t const&);
%ignore ncbi::CIntervalTree::Replace(const interval_type&, const mapped_type&);
%ignore ncbi::CIntervalTree::Replace(const interval_type&, const mapped_type&,
                                     nothrow_t const&);

%ignore ncbi::x_SwapPointers(void* volatile*, void*);
%ignore ncbi::CPIDGuard::Remove;

%ignore ncbi::objects::CAlnMap::
    GetNumberOfInsertedSegmentsOnLeft(TNumrow row, TNumseg seg) const;
%ignore ncbi::objects::CAlnMap::
    GetNumberOfInsertedSegmentsOnRight(TNumrow row, TNumseg seg) const;

%ignore ncbi::CHTML_table::ColumnWidth(CHTML_table *, TIndex, const string &);
%ignore ncbi::CHTMLHelper::LoadIDList(TIDList& ids,  
                                      const TCgiEntries& values,
                                      const string& hiddenPrefix,
                                      const string& checkboxPrefix);
%ignore ncbi::CHTMLHelper::StoreIDList(CHTML_form *form,
                                       const TIDList&  ids,
                                       const string&  hiddenPrefix,
                                       const string&  checkboxPrefix) ;

%ignore ncbi::COptionDescription::COptionDescription();


/*
 * ===========================================================================
 * $Log$
 * Revision 1.5  2005/07/05 14:54:06  jcherry
 * Removed commented-out %ignore's altogether
 *
 * Revision 1.4  2005/06/29 16:12:24  vasilche
 * Commented out fixed %ignore statements.
 *
 * Revision 1.3  2005/06/28 14:50:31  jcherry
 * Restore %ignore for BLAST_RPSSearchEngine
 *
 * Revision 1.2  2005/06/27 17:11:38  jcherry
 * Ignore CScope_Impl::x_GetBioseq_Lock; remove some obsolete %ignore's
 *
 * Revision 1.1  2005/05/11 22:23:14  jcherry
 * Initial version
 *
 * ===========================================================================
 */
