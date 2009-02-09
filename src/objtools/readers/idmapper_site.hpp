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
 *
 */

#ifndef OBJTOOLS_IDMAPPER___IDMAPPER_SITE__HPP
#define OBJTOOLS_IDMAPPER___IDMAPPER_SITE__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class CIdMapperSite:
//  ============================================================================
    public CIdMapper
{
    friend class CIdMapper;
    
public:
    virtual ~CIdMapperSite() {};
    
    virtual void Setup(
        const CArgs& );

    virtual void Setup(
        const std::string& );               // filename for user mappings
                    
    virtual CSeq_id_Handle MapID(
        const std::string&,
        unsigned int& );

    virtual void Dump(
        CNcbiOstream&,
        const std::string& = "" );
                
protected:                                  
    CIdMapperSite();

    void ReadSiteMap(
        CNcbiIstream& );
        
    static bool MakeSourceId(
        const std::string&,
        const std::string&,
        std::string& );
        
    static bool MakeTargetId(
        const std::string&,
        CRef<CSeq_id>& );
            
    CIdMap m_Map;
};
           
END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_IDMAPPER___IDMAPPER_USER__HPP
