#ifndef OBJECTS_GBPROJ__IGBPROJECT_HPP
#define OBJECTS_GBPROJ__IGBPROJECT_HPP

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
 * Authors:  Mike DiCuccio, Vladimir Tereshkov, Liangshou Wu
 *
 * File Description:
 *
 */

#include <corelib/ncbitime.hpp>

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(objects)


class CProjectDescr;
class CProjectAnnot;
class CProjectFolder;
class CProjectItem;
class CProjectHistoryItem;
class CDate;
class CViewDescriptor;
class CLoaderDescriptor;
class CUser_object;

//class CScope;


///////////////////////////////////////////////////////////////////////////////
/// IGBProject - abstract interface for a project used by CGBProjectHandle.
class IGBProject
{
public:
    enum EProjectVersion {
        eVersion_Invalid = -1,
        eVersion_Unknown = 0,
        eVersion1 = 1,
        eVersion2 = 2
    };

    typedef CProjectDescr                       TDescr;
    typedef CProjectFolder                      TData;
    typedef list< CRef< CProjectAnnot > >       TAnnot;
    typedef list< CRef< CViewDescriptor > >     TViews;
    typedef list< CRef< CLoaderDescriptor > >   TDataLoaders;
    typedef list< CRef< CUser_object > >        TViewSettings;


    virtual ~IGBProject() {}

    /// retrieve this project's version; this is fixed per subclass
    virtual EProjectVersion GetVersion() const = 0;

    /// Add an item to the current project
    virtual void AddItem(CProjectItem& item, CProjectFolder& folder) = 0;

    /// retrieve our project's data, in the form of a project folder
    /// this may be a contrived entity, and it is up to a project to determine
    /// what parts belong where
    virtual const CProjectFolder& GetData() const = 0;
    virtual CProjectFolder&       SetData() = 0;

    /// retrieve our project's descriptor set
    virtual bool                 IsSetDescr() const = 0;
    virtual const CProjectDescr& GetDescr() const = 0;
    virtual CProjectDescr&       SetDescr() = 0;

    /// retrieve a set of annotations for this project
    virtual bool          IsSetAnnot() const = 0;
    virtual const TAnnot& GetAnnot() const = 0;
    virtual TAnnot&       SetAnnot() = 0;

    /// SetCreateDate() will add a descriptor for creation date
    virtual void SetCreateDate(const CDate& date) = 0;

    /// SetModifiedDate() will add a descriptor for the update date
    virtual void SetModifiedDate(const CDate& date) = 0;

    /// data loader info.
    virtual bool IsSetDataLoaders() const = 0;
    virtual const TDataLoaders& GetDataLoaders() const   = 0;
    virtual TDataLoaders&       SetDataLoaders()         = 0;

    /// view info
    virtual bool          IsSetViews() const = 0;
    virtual const TViews& GetViews() const   = 0;
    virtual TViews&       SetViews()         = 0;

    /// view-specific settings.
    virtual bool IsSetViewSettings() const = 0;
    virtual const TViewSettings& GetViewSettings() const   = 0;
    virtual TViewSettings&       SetViewSettings()         = 0;
};


END_SCOPE(objects)
END_NCBI_SCOPE

#endif  // OBJECTS_GBPROJ__IGBPROJECT_HPP
