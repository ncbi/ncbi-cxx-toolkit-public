#ifndef OBJECTS_ALNMGR___ALNSEGMENTS__HPP
#define OBJECTS_ALNMGR___ALNSEGMENTS__HPP

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
* Author:  Kamen Todorov, NCBI
*
* File Description:
*   Alignment sequences
*
*/


//#include <corelib/ncbimisc.hpp>
#include <corelib/ncbiobj.hpp>

BEGIN_NCBI_SCOPE

#ifndef BEGIN_objects_SCOPE
#  define BEGIN_objects_SCOPE BEGIN_SCOPE(objects)
#  define END_objects_SCOPE END_SCOPE(objects)
#endif
BEGIN_objects_SCOPE // namespace ncbi::objects::


class CAlnMixSeq;
class CAlnMixSegment;



class CAlnMixSegments : public CObject
{
public:
    
    typedef list<CAlnMixSegment*>       TSegments;

private:
    TSegments                   m_Segments;
};



class CAlnMixSegment : public CObject
{
public:
    // TStarts really belongs in CAlnMixSeq, but had to move here as
    // part of a workaround for Compaq's compiler's bogus behavior
    typedef map<TSeqPos, CRef<CAlnMixSegment> > TStarts;
    typedef map<CAlnMixSeq*, TStarts::iterator> TStartIterators;
        
    TSeqPos         m_Len;
    TStartIterators m_StartIts;
    int             m_DsIdx; // used by the truncate algorithm
};



END_objects_SCOPE // namespace ncbi::objects::

END_NCBI_SCOPE


/*
* ===========================================================================
*
* $Log$
* Revision 1.1  2005/03/01 17:28:49  todorov
* Rearranged CAlnMix classes
*
* ===========================================================================
*/

#endif // OBJECTS_ALNMGR___ALNSEGMENTS__HPP
