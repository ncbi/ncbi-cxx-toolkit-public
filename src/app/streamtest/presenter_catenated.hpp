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
* Author:
*
* File Description:
*
* ===========================================================================
*/

#ifndef __presenter_catenated_hpp__
#define __presenter_catenated_hpp__

//  ============================================================================
class CCatenatedPresenter
//  ============================================================================
    : public CSeqEntryPresenter
{
public:
    //  ------------------------------------------------------------------------
    CCatenatedPresenter()
    //  ------------------------------------------------------------------------
        : CSeqEntryPresenter()
    {
    };

    //  ------------------------------------------------------------------------
    virtual void Initialize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryPresenter::Initialize( args );  
        m_is.reset( 
            CObjectIStream::Open(
                (eSerial_AsnText), 
                args["i"].AsInputFile() ) );
    };

    //  ------------------------------------------------------------------------
    virtual void Run(
        CSeqEntryProcess* process )
    //  ------------------------------------------------------------------------
    {   
        CSeqEntryPresenter::Run( process );

        try {
            while (true) {
                // Get seq-entry to validate
                CRef<CSeq_entry> se(new CSeq_entry);

                try {
                    m_is->Read(ObjectInfo(*se));
                }
                catch (const CSerialException& e) {
                    if (e.GetErrCode() == CSerialException::eEOF) {
                        break;
                    } else {
                        throw;
                    }
                }
                catch (const CException& e) {
                    break;
                }
                try {
                    Process(se);
                }
                catch (const CObjMgrException& om_ex) {
                    if (om_ex.GetErrCode() == CObjMgrException::eAddDataError)
                      se->ReassignConflictingIds();
                    Process(se);
                }
            }
        }
        catch (const CException& e) {
        }
    };

    //  ------------------------------------------------------------------------
    virtual void Finalize(
        const CArgs& args )
    //  ------------------------------------------------------------------------
    {
        CSeqEntryPresenter::Finalize( args );  
    };

protected:       
    auto_ptr<CObjectIStream> m_is;
};

#endif
