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
 * Author:  Justin Foley
 *
 * File Description:
 *   Feature trimming utility
 *
 * ===========================================================================
 */

#include <ncbi_pch.hpp>

#include <corelib/ncbiapp.hpp>
#include <corelib/ncbistd.hpp>
#include <corelib/ncbifile.hpp>
#include <objects/seqfeat/Seq_feat.hpp>
#include <objmgr/object_manager.hpp>
#include <objmgr/scope.hpp>
#include <serial/objostr.hpp>
#include <serial/objistr.hpp>
#include <serial/objectinfo.hpp>
#include <objtools/edit/feature_edit.hpp>
#include <objects/seq/Seq_annot.hpp>
#include <objmgr/mapped_feat.hpp>
#include <objmgr/seq_feat_handle.hpp>

BEGIN_NCBI_SCOPE
USING_SCOPE(objects);


class CFeatTrimApp : public CNcbiApplication
{
public:
    void Init(void);
    int Run(void);

private:
    CObjectIStream* x_InitObjectIStream(const CArgs& args);
    CNcbiOstream& x_InitOStream(const CArgs& args);

    TSeqPos xGetFrom(const CArgs& args) const;
    TSeqPos xGetTo(const CArgs& args) const;

    CRef<CScope> m_pScope;
};


void CFeatTrimApp::Init(void)
{

    unique_ptr<CArgDescriptions> arg_desc(new CArgDescriptions());

    arg_desc->AddKey("i", 
            "InputFile",
            "Feature input file",
            CArgDescriptions::eInputFile);

    arg_desc->AddOptionalKey("o",
            "OutputFile",
            "Output stub",
            CArgDescriptions::eOutputFile);

    arg_desc->AddOptionalKey("from", "From",
            "Beginning of range", CArgDescriptions::eInteger);

    arg_desc->AddOptionalKey("to", "To",
            "End of range", CArgDescriptions::eInteger);

    SetupArgDescriptions(arg_desc.release());
}


int CFeatTrimApp::Run(void)
{
    const CArgs& args = GetArgs();

    m_pScope = Ref(new CScope(*CObjectManager::GetInstance()));
    CRef<CSeq_annot> temp_annot = Ref(new CSeq_annot());


    const TSeqPos from = xGetFrom(args);
    const TSeqPos to = xGetTo(args);
    CRange<TSeqPos> range(from, to);

    unique_ptr<CObjectIStream> istr(x_InitObjectIStream(args));

    CNcbiOstream& ostr = x_InitOStream(args);
    CRef<CSeq_feat> infeat = Ref(new CSeq_feat());

    istr->Read(ObjectInfo(*infeat));

    temp_annot->SetData().SetFtable().push_back(infeat);
    m_pScope->AddSeq_annot(*temp_annot);

    CSeq_feat_Handle sfh = m_pScope->GetSeq_featHandle(*infeat);

    CMappedFeat mapped_feat(sfh);
    mapped_feat = edit::CFeatTrim::Apply(mapped_feat, range);

    const CSeq_feat& trimmed_feat = mapped_feat.GetMappedFeature();

    ostr << MSerial_AsnText  << trimmed_feat;
    ostr.flush();

    return 0;
}


CObjectIStream* CFeatTrimApp::x_InitObjectIStream(
        const CArgs& args)
{
    ESerialDataFormat serial = eSerial_AsnText;

    const char* infile = args["i"].AsString().c_str();
    CNcbiIstream* pInStream = new CNcbiIfstream(infile, ios::binary);

    CObjectIStream* pObjInStream = CObjectIStream::Open(
            serial, *pInStream, eTakeOwnership);

    return pObjInStream;
}


CNcbiOstream& CFeatTrimApp::x_InitOStream(
        const CArgs& args)
{
    if (args["o"]) {
        return args["o"].AsOutputFile();
    } 
    return cout;
}


TSeqPos CFeatTrimApp::xGetFrom(
        const CArgs& args) const
{
    if (args["from"]) {
        return static_cast<TSeqPos>(args["from"].AsInteger()-1);
    }

    return CRange<TSeqPos>::GetWholeFrom();
}


TSeqPos CFeatTrimApp::xGetTo(
        const CArgs& args) const
{
    if (args["to"]) {
        return static_cast<TSeqPos>(args["to"].AsInteger()-1);
    }

    return CRange<TSeqPos>::GetWholeTo();
}


END_NCBI_SCOPE
USING_NCBI_SCOPE;

int main(int argc, const char* argv[])
{
    return CFeatTrimApp().AppMain(argc, argv, 0, eDS_ToStderr, 0);
}



