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
 * Author: Frank Ludwig
 *
 * File Description:
 *   WIGGLE transient data structures
 *
 */

#ifndef OBJTOOLS_IDMAPPER___IDMAP__HPP
#define OBJTOOLS_IDMAPPER___IDMAP__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

class CIdMapValue
{
public:
    CIdMapValue(
        CRef<CSeq_id> id,
        unsigned int uLength = 0): m_Id( id ), m_uLength( uLength ) {};

    CIdMapValue()
    { m_Id.Reset(); m_uLength = 0; };
            
public:
    CRef<CSeq_id> m_Id;
    unsigned int m_uLength;    
};

//  ============================================================================
class CIdMap
//  ============================================================================
{
//    typedef std::map< std::string, CRef<CSeq_id> > IdMap;
//    typedef std::map< std::string, CRef<CSeq_id> >::iterator IdIter;
//    typedef std::map< std::string, CRef<CSeq_id> >::const_iterator IdCiter;
    typedef std::map< std::string, CIdMapValue > IdMap;
    typedef std::map< std::string, CIdMapValue >::iterator IdIter;
    typedef std::map< std::string, CIdMapValue >::const_iterator IdCiter;
    
public:
    CIdMap();
    ~CIdMap();
    
    bool AddMapping(
        const std::string&,
        CRef<CSeq_id>,
        unsigned int = 0 );
        
    bool GetMapping(
        const std::string&,
        CRef<CSeq_id>&,
        unsigned int& );

    void ClearAll();
    
    void Dump( 
        CNcbiOstream&,
        const std::string& = "" );
        
protected:
    static CSeq_id_Handle Handle(
        const CSeq_id& );
                
protected:
    IdMap m_IdMap;
};

END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_IDMAPPER___UCSCID__HPP
