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
 * Author: Mike DiCuccio
 *
 * File Description:
 *
 */

#ifndef OBJTOOLS_IDMAPPER___IDMAPPER__HPP
#define OBJTOOLS_IDMAPPER___IDMAPPER__HPP

BEGIN_NCBI_SCOPE
BEGIN_objects_SCOPE // namespace ncbi::objects::

//  ============================================================================
class NCBI_XOBJREAD_EXPORT IIdMapper
//  ============================================================================
{
public:
    virtual ~IIdMapper() {};
    
    virtual CSeq_id_Handle
    Map(
        const CSeq_id_Handle& ) =0;

    virtual void 
    MapObject(
        CRef< CSerialObject >& ) =0; 

};

//  ============================================================================
class NCBI_XOBJREAD_EXPORT CIdMapper: public IIdMapper
//  ============================================================================
{
public:
    //
    //  The "strContext" parameter allows for additional specialization of the
    //    mapper.
    //  The "bInvert" parameter indicates the direction of the mapping.
    //
    CIdMapper(
        const std::string& strContext = "",
        bool bInvert = false,
        IErrorContainer* pErrors = 0 )
    : 
        m_strContext(strContext), 
        m_bInvert(bInvert),
        m_pErrors( pErrors ) 
    {};
    
    virtual void
    AddMapping(
        const CSeq_id_Handle& to,
        const CSeq_id_Handle& from ) 
    {
        if (!m_bInvert) {
            m_Cache[ to ] = from;
        } else {
            m_Cache[ from ] = to;
        }
    };
        
    virtual CSeq_id_Handle
    Map(
        const CSeq_id_Handle& ); 
        
    //
    //  Map all IDs embedded in this object
    //
    virtual void 
    MapObject(
        CRef< CSerialObject >& ); 

    /* any other convenience functions with shared implementations*/ 
    
protected:
    static std::string
    MapErrorString(
        const CSeq_id_Handle& );
        
    typedef std::map< CSeq_id_Handle, CSeq_id_Handle > CACHE;
    
    //
    //  Initialize the cache of mappings. Most implementations will have their
    //  own special way of doing this.
    //
    virtual void
    InitializeCache() {};
    
    const std::string m_strContext;
    const bool m_bInvert;
    CACHE m_Cache;
    IErrorContainer* m_pErrors;   
};

            
//  ============================================================================
class NCBI_XOBJREAD_EXPORT CIdMapperBuiltin: public CIdMapper
//  
//  A mapper that will load all its mappings from a compiled in table.
//  ============================================================================
{
public:
    //
    //  The "strContext" parameter determines which of various compiled in
    //  tables will be used.
    //
    CIdMapperBuiltin(
        const std::string& strContext,
        bool bInvert = false,
        IErrorContainer* pErrors = 0 ): CIdMapper(strContext, bInvert, pErrors)
    {
        InitializeCache();
    };
    
protected:
    virtual void
    InitializeCache();
    
    void
    AddMapEntry(
        const std::string&,
        int );
};

//  ============================================================================
class NCBI_XOBJREAD_EXPORT CIdMapperConfig: public CIdMapper
//
//  A mapper that will read its mappings from an input stream (typically, a
//  config file).
//  ============================================================================
{
public:
    //
    //  The "istr" parameter provides the stream from which the mappings will be
    //  initialized.
    //
    CIdMapperConfig(
        CNcbiIstream& istr,
        const std::string& strContext = "",
        bool bInvert = false,
        IErrorContainer* pErrors = 0)
        : CIdMapper(strContext, bInvert, pErrors), m_Istr(istr)
    {
        InitializeCache();
    };
    
protected:
    virtual void
    InitializeCache();
    
    void
    AddMapEntry(
        const std::string& );

    void
    SetCurrentContext(
        const std::string&,
        std::string& );
    
    CSeq_id_Handle
    SourceHandle(
        const std::string& );
        
    CSeq_id_Handle
    TargetHandle(
        const std::string& );
            
    CNcbiIstream& m_Istr;
};

//  ============================================================================
class NCBI_XOBJREAD_EXPORT CIdMapperDatabase: public CIdMapper
//
//  A mapper that will get its mappings from a database. Any mappings will be
//  cached locally after lookup.
//  ============================================================================
{
public:
    //
    //  The "strServer" parameter gives the host to where the database is 
    //    located.
    //  The "strDatabase" parameter gives the name of the database itself.
    //
    CIdMapperDatabase(
        const std::string& strServer,
        const std::string& strDatabase,
        const std::string& strContext,
        bool bInvert = false,
        IErrorContainer* pErrors = 0)
        : CIdMapper(strContext, bInvert, pErrors), 
          m_strServer(strServer), 
          m_strDatabase(strDatabase)
    {};
        
    virtual CSeq_id_Handle
    Map(
        const CSeq_id_Handle& from );
        
protected:
    const std::string m_strServer;
    const std::string m_strDatabase;
};

           
END_objects_SCOPE
END_NCBI_SCOPE

#endif // OBJTOOLS_IDMAPPER___IDMAPPER__HPP
