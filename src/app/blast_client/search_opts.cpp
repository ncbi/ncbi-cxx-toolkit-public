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
 * Author:  Kevin Bealer
 *
 * File Description:
 *   Search-related options for blast_client.
 *
 */


#include <ncbi_pch.hpp>
#include "search_opts.hpp"

USING_NCBI_SCOPE;
USING_SCOPE(objects);


/////////////////////////////////////////////////////////////////////////////
//
//  Structures and Prototypes
//

// Functor for option reading

class COptionReader : public COptionWalker
{
public:
    COptionReader(const CArgs & args)
        : m_args(args)
    {}
    
    template <class T>
    void Same(T        & valobj,
              CUserOpt   user_opt,
              CNetName,
              CArgKey,
              COptDesc,
              EListPick)
    {
        ReadOpt(m_args, valobj, user_opt);
    }
    
    template <class T>
    void Local(T          & valobj,
               CUserOpt   user_opt,
               CArgKey,
               COptDesc)
    {
        ReadOpt(m_args, valobj, user_opt);
    }
    
    template <class T> void Remote(T &, CNetName, EListPick)
    {
    }
    
    bool NeedRemote(void) { return false; }
    
private:
    const CArgs & m_args;
};


CNetblastSearchOpts::CNetblastSearchOpts(const ncbi::CArgs & a)
{
    COptionReader optrd(a);
    Apply(optrd);
}


class CInterfaceBuilder : public COptionWalker
{
public:
    CInterfaceBuilder(CArgDescriptions & ui)
        : m_ui(ui)
    {}
    
    template <class T>
    void Same(T        & valobj,
              CUserOpt   user_opt,
              CNetName,
              CArgKey    arg_key,
              COptDesc   descr,
              EListPick)
    {
        AddOpt(m_ui, valobj, user_opt, arg_key, descr);
    }
    
    template <class T> void
    Local(T        & valobj,
          CUserOpt   user_opt,
          CArgKey    arg_key,
          COptDesc   descr)
    {
        AddOpt(m_ui, valobj, user_opt, arg_key, descr);
    }
    
    template <class T>
    void Remote(T &, CNetName, EListPick)
    {
    }
    
    bool NeedRemote(void) { return false; }
    
private:
    CArgDescriptions & m_ui;
};


void CNetblastSearchOpts::x_CreateInterface2(CArgDescriptions & ui)
{
    CInterfaceBuilder make_ui(ui);
    Apply(make_ui);
}


/////////////////////////////////////////////////////////////////////////////
//
// Helper functions
//

void CNetblastSearchOpts::CreateInterface(CArgDescriptions & ui)
{
    CNetblastSearchOpts dummy;
    dummy.x_CreateInterface2(ui);
}

// AddOpt

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptBool         &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eBoolean);
}

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptDouble       &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eDouble);
}

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptInteger      &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eInteger);
}

void COptionWalker::AddOpt(CArgDescriptions & ui,
                           TOptString       &,
                           const char       * name,
                           const char       * synop,
                           const char       * comment)
{
    ui.AddOptionalKey(name, synop, comment, CArgDescriptions::eString);
}

// ReadOpt

void COptionWalker::ReadOpt(const ncbi::CArgs & args,
                            TOptDouble        & field,
                            const char        * key)
{
    field = CheckArgsDouble(args[key]);
}

void COptionWalker::ReadOpt(const ncbi::CArgs & args,
                            TOptBool          & field,
                            const char        * key)
{
    field = CheckArgsBool(args[key]);
}

void COptionWalker::ReadOpt(const ncbi::CArgs & args,
                            TOptInteger       & field,
                            const char        * key)
{
    field = CheckArgsInteger(args[key]);
}

void COptionWalker::ReadOpt(const ncbi::CArgs & args,
                            TOptString        & field,
                            const char        * key)
{
    field = CheckArgsString(args[key]);
}

/*
 * ===========================================================================
 *
 * $Log$
 * Revision 1.4  2004/05/21 21:41:38  gorelenk
 * Added PCH ncbi_pch.hpp
 *
 * Revision 1.3  2003/12/29 19:48:30  bealer
 * - Change code to accomodate first half of new ASN changes.
 *
 * Revision 1.2  2003/09/26 20:00:48  bealer
 * - Fix compile warning.
 *
 * Revision 1.1  2003/09/26 16:53:49  bealer
 * - Add blast_client project for netblast protocol, initial code commit.
 *
 * ===========================================================================
 */

