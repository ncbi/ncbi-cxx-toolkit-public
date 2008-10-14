static char const rcsid[] = "$Id$";

/*
* ===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's offical duties as a United States Government employee and
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
* ===========================================================================*/

/*****************************************************************************

File name: patterns.cpp

Author: Greg Boratyn

Contents: Default CDD patterns for CMultiAlignerOptions.
          Derived from PROSITE database
          Hulo et al., Nucleic Acid Res. 34:D227-D230, 2006

******************************************************************************/

/// @file options.cpp
/// Default CDD patterns for CMultiAlignerOptions

#include <ncbi_pch.hpp>
#include <corelib/ncbistl.hpp>
#include <algo/cobalt/patterns.hpp>


static const int kNumDefPatterns = 1319;

static const char* kDefPatterns[] = {
    "C-x-[DN]-x(4)-[FY]-x-C-x-C",

    "[DEQGSTALMKRH]-[LIVMFYSTAC]-[GNQ]-[LIVMFYAG]-[DNEKHS]-S-[LIVMST]-{PCFY}-[S"
    "TAGCPQLIVMF]-[LIVMATN]-[DENQGTAKRHLM]-[LIVMWSTA]-[LIVGSTACR]-{LPIY}-{VY}-["
    "LIVMFA]",

    "{DERK}(6)-[LIVMFWSTAG](2)-[LIVMFYSTAGCQ]-[AGS]-C",

    "[KRHEQSTAG]-G-[FYLIVM]-[ST]-[LT]-[LIVP]-E-[LIVMFWSTAG](14)[AG]-x(4)-G-K-[S"
    "T]",

    "[LIVM]-[VIC]-x-{H}-G-[DENQTA]-x-[GAC]-{L}-x-[LIVMFY](4)-x(2)-G",

    "[LIVMF]-G-E-x-[GAS]-[LIVM]-x(5,11)-R-[STAQ]-A-x-[LIVMA]-x-[STACV]",

    "[EQ]-{LNYH}-x-[ATV]-[FY]-{LDAM}-x-W-x-N",

    "[LIVM]-x-[SGNL]-[LIVMN]-[DAGHENRS]-[SAGPNV]-x-[DNEAG]-[LIVM]-x-[DEAG]-x(4)"
    "-[LIVM]-x-[LM]-[SAG]-[LIVM]-[LIVMT]-W-x(0,1)-[LIVM](2)",

    "[CSH]-C-x(2)-[GAP]-x(7,8)-[GASTDEQR]-C-[GASTDEQL]-x(3,9)-[GASTDEQN]-x(2)-["
    "CE]-x(6,7)-C-C",

    "C-x(3)-[LIVMFY]-x(5)-[LIVMFY]-x(3)-[DENQ]-[LIVMFY]-x(10)-C-x(3)-C-T-x(4)-C"
    "-x-[LIVMFY]-F-x-[FY]-x(13,14)-C-x-[LIVMFY]-[RK]-x-[ST]-x(14,15)-S-G-x-[ST]"
    "-[LIVMFY]-x(2)-C",

    "C-C-x(13)-C-x(2)-[GN]-x(12)-C-x-C-x(2,4)-C",

    "C-x(2)-G-x-C-C-x-[NQRS]-C-x-[FM]-x(6)-C-[RK]",

    "D-x-[DNS]-{ILVFYW}-[DENSTG]-[DNQGHRK]-{GP}-[LIVMC]-[DENQSTAGC]-x(2)-[DE]-["
    "LIVMFYW]",

    "C-x-C-x(5)-G-x(2)-CC-x-C-x(2)-[GP]-[FYW]-x(4,8)-C",

    "[DEQN]-x-[DEQN](2)-C-x(3,14)-C-x(3,7)-C-x-[DN]-x(4)-[FY]-x-C",

    "C-x(1,2)-C-x(5)-G-x(2)-C-x(2)-C-x(3,4)-[FYW]-x(3,15)-C",

    "[GASP]-W-x(7,15)-[FYW]-[LIV]-x-[LIVFA]-[GSTDEN]-x(6)-[LIVF]-x(2)-[IV]-x-[L"
    "IVT]-[QKMT]-G",

    "P-x(8,10)-[LM]-R-x-[GE]-[LIVP]-x-G-C",

    "W-[LIV]-x(3)-[KRQ]-x-[LIVM]-x(2)-[QH]-x(0,2)-[LIVMF]-x(6,8)-[LIVMF]-x(3,5)"
    "-F-[FY]-x(2)-[DENS]",

    "[HYW]-x(9)-[DENQSTV]-[SA]-x(3)-[FYC]-[LIVM]-x(2,3)-[ACVWD]-x(2)-[LM]-x(2)-"
    "[FY]-[GM]-x-[DENQSTH]-[LIVMFYS]",

    "C-x(6,8)-[LFY]-x(5)-[FYW]-x-[RK]-x(8,10)-C-x-C-x(6,9)-C",

    "C-x(2)-P-F-x-[FYWIV]-x(7)-C-x(8,10)-W-C-x(4)-[DNSR]-[FYW]-x(3,5)-[FYW]-x-["
    "FYWI]-C",

    "W-W-[LIVMFYW]-x(2)-C-x(2)-[GSA]-x(2)-N-G",

    "E-X(2)-[ERK]-E-X-C-X(6)-[EDR]-x(10,11)-[FYA]-[YW]",

    "[LIFAT]-x(3)-W-x(2,3)-[PE]-x(2)-[LIVMFY]-[DENQS]-[STA]-[AV]-[LIVMFY]",

    "[FY]-C-[RH]-[NS]-x(7,8)-[WY]-C",

    "C-[VILMA]-x(5)-C-[DNH]-x(3)-[DENQHT]-C-x(3,4)-[STADE]-[DEH]-[DE]-x(1,5)-C",

    "C-[LIVMFYATG]-x(5,12)-[WL]-x-[DNSR]-{C}-{LI}-C-x(5,6)-[FYWLIVSTA]-[LIVMSTA"
    "]-C",

    "C-x(2)-C-x(15,21)-[FYWHPCR]-H-x(2)-[CH]-x(2)-C-x(2)-C-x(3)-[LIVMF]",

    "C-x(15)-A-x(3,4)-[GK]-x(3)-C-x(2)-G-x(8,9)-P-x(7)-C",

    "C-x-[DNS]-x(2)-C-x(2)-G-[KRH]-x-C-x(6,7)-P-x-C-x-C-x(3,5)-C-P",

    "F-P-x-R-[IM]-x-D-W-L-x-[NQ]",

    "[KRH]-x(2)-C-x-[FYPSTV]-x(3,4)-[ST]-x(3)-C-x(4)-C-C-[FYWH]",

    "W-N-[STAGR]-[STDN]-[LIVM]-x(2)-[GST]-x-[GST]-x(2)-[LIVMFT]-[GA]",

    "C-G-G-x(4,7)-G-x(3)-C-x(4,5)-C-x(3,5)-[NHGS]-x-[FYWM]-x(2)-Q-C",

    "C-x(4,5)-C-C-S-x(2)-G-x-C-G-x(3,4)-[FYW]-CC-G-[KR]-C-L-x-V-x-N",

    "V-[DN]-Y-[EQD]-F-V-[DN]-C",

    "[HKEPILVY]-x(2)-R-x(3,7)-[FYW]-x(11,14)-[STAN]-G-[LMF]-x-[FYHDA]-x(4)-[DES"
    "L]-x(2,3)-C-X(2)-C-x(6)-[WA]-X(9)-H-x(4)-[PRSD]-x-C-x(2)-[LIVMA]",

    "C-{G}-{C}-[DN]-x(2)-C-x(3)-{G}-x-C-C",

    "[GD]-x(0,10)-[FYWA]-x(0,1)-G-[LIVM]-x(0,1)-[LIVMFYD]-x(0,4)-G-[KN]-[NHW]-x"
    "(0,1)-G-[STARCV]-x(0,2)-[GD]-x(0,2)-[LY]-[FC]",

    "[EQR]-C-[LIVMFYAH]-x-C-x(5,8)-C-x(3,8)-[EDNQSTV]-C-{C}-x(5)-C-x(12,24)-C",

    "[GE]-x-[LIVMFY](2)-x(3)-[STA]-x(10,11)-[LV]-x(4)-[LIVMF]-x(6,7)-C-[LIVM]-x"
    "-F-x-[LIVMFY]-x(3)-[GSC]",

    "C-x-C-x(3)-C-x(5,6)-C-C-x-[DN]-[FY]-x(3)-C",

    "[GNRVM]-x(5)-[GLKA]-x(2)-[EQ]-x(6)-[WPS]-[GLKH]-x(2)-C-x(3)-[FYW]-x(8)-[CM"
    "]-x(3)-G",

    "[FYWHPVAS]-x(3)-C-x(3,4)-[SG]-x-[FYW]-x(3)-Q-x(5,12)-[FYW]-C-[VA]-x(3,4)-["
    "SG]",

    "C-x(2,3)-C-{CG}-C-x(6,14)-C-x(3,4)-C-x(2,10)-C-x(9,16)-C-C-x(2,4)-C",

    "W-x(9,11)-[VFY]-[FYW]-x(6,7)-[GSTNE]-[GSTQCR]-[FYW]-{R}-{SA}-P",

    "[CH]-X(2,4)-C-X(7,17)-C-X(0,2)-C-X(4)-[YFT]-C-X(3)-[CH]-X(6,9)-H-X(3,4)-C",

    "C-x(1,2)-C-x(5,45)-[VMFLWIE]-x-C-x(1,4)-C-x(1,4)-[WYFVQHLT]-H-x(2)-C-x(5,4"
    "5)-[WFLYI]-x-C-x(2)-C",

    "H-x-[LIVMFYW]-x(8,11)-C-x(2)-C-x(3)-[LIVMFC]-x(5,10)-C-x(2)-C-x(4)-[HD]-x("
    "2)-C-x(5,9)-C",

    "W-x-C-x(2,4)-C-x(3)-N-x(6)-C-x(2)-CC-x-H-x-[LIVMFY]-C-x(2)-C-[LIVMYA]",

    "C-x(2)-C-x(4,8)-[RHDGSCV]-[YWFMVIL]-x-[CS]-x(2,5)-[CHEQ]-x-[DNSAGE]-[YFVLI"
    "]-x-[LIVFM]-C-x(2)-C",

    "[LIVMFYW]-x(7)-[STAPDNLR]-x(3)-[LIVMFYW]-x-[LIVMFYW]-x-[LIVMFYW]-x(2)-C-[L"
    "IVMFYW]-x-[STA]-[PSLT]-x(2,4)-[DENSG]-x-[STADNQLFM]-x(6)-[LIVM](2)-x(3,4)-"
    "C",

    "[LVFYT]-x-[DA]-x(2,5)-[DNGSATPHY]-[FYWPDA]-x(4)-[LIV]-x(2)-[GTALV]-x(4,6)-"
    "[LIVFYC]-x(2)-G-x-[PGSTA]-x(2,3)-[MFYA]-x-[PGAV]-x(3,10)-[LIVMA]-[STKR]-[R"
    "Y]-x-[EQ]-x-[STALIVM]",

    "[LIVMFYG]-[ASLVR]-x(2)-[LIVMSTACN]-x-[LIVM]-x(4)-[LIV]-[RKNQESTAIY]-[LIVFS"
    "TNKH]-W-[FYVC]-x-[NDQTAH]-x(5)-[RKNAIMW]",

    "[LIVMFE]-[FY]-P-W-M-[KRQTA]L-M-A-[EQ]-G-L-Y-NR-P-C-x(11)-C-V-S",

    "[RKQ]-R-[LIM]-x-[LF]-G-[LIVMFY]-x-Q-x-[DNQ]-V-G",

    "S-Q-[STK]-[TA]-I-[SC]-R-[FH]-[ET]-x-[LSQ]-x(0,1)-[LIR]-[ST]",

    "C-x(2,4)-C-x(3)-[LIVMFYWC]-x(8)-H-x(3,5)-H",

    "C-x(2)-C-x(7)-[CS]-x(13)-C-x(2)-C-x-R-x-W-T-x-G-G",

    "C-x(2)-C-x(9)-[LIVMQSAR]-[QH]-[STQL]-[RA]-[SACR]-x-[DE]-[DET]-[PGSEA]-x(6)"
    "-C-x(2,5)-C-x(3)-[FW]",

    "C-x(2)-C-x-[DE]-x(5)-[HN]-[FY]-x(4)-C-x(2)-C-x(2)-F-F-x-R",

    "C-x-[DN]-C-x(4,5)-[ST]-x(2)-W-[HR]-[RK]-x(3)-[GN]-x(3,4)-C-N-[AS]-C",

    "C-[KR]-x-C-x(3)-I-x-[KA]-x(3)-[RG]-x(16,18)-W-[FYH]-H-x(2)-C",

    "[GASTPV]-C-x(2)-C-[RKHSTACW]-x(2)-[RKHQ]-x(2)-C-x(5,12)-C-x(2)-C-x(6,8)-C",

    "C-[DESN]-x-[CT]-x(3)-I-x(3)-R-x(4)-P-x(4)-[CS]-x(2)-[CA]",

    "M-[LIVMF](3)-x(3)-[KN]-[MY]-A-C-x(2)-C-[IL]-[KR]-x-H-[KR]-x(3)-C-x-H-x(8)-"
    "[KR]-x-[KR]-G-R-P",

    "L-x(6)-L-x(6)-L-x(6)-L",

    "[KR]-x(1,3)-[RKSAQ]-N-{VL}-x-[SAQ](2)-{L}-[RKTAENQ]-x-R-x-[RK]",

    "W-[ST]-x-{PTLN}-E-[DE]-{GIYS}-x-[LIV]",

    "W-x(2)-[LI]-[SAG]-x(4,5)-R-{RE}-x(3)-{AG}-x(3)-[YW]-x(3)-[LIVM]",

    "M-C-N-S-S-C-[MV]-G-G-M-N-R-RC-V-S-E-x-I-S-F-[LIVM]-T-[SG]-E-A-[SC]-[DE]-[K"
    "RQ]-C",

    "Y-V-N-A-K-Q-Y-x-R-I-L-K-R-R-x-A-R-A-K-L-EC-S-R-C-C-[DE]-[KR]-K-S-C",

    "[FYKH]-G-[FL]-[IL]-x(6,7)-[DER]-[LIVM]-[FQ]-x-H-x-[STKR]-x-[LIVMFYC]",

    "R-K-R-K-Y-F-K-K-H-E-K-RC-x(2)-C-x(2)-H-x(8)-H-x(3,4)-C-x(4)-C-x-C-x(2,3)-C"
    "",

    "L-[FYW]-[QEDH]-F-[LI]-[LVQK]-{N}-[LI]-L",

    "[RKH]-x(2)-M-x-Y-[DENQ]-x-[LIVM]-[STAG]-R-[STAG]-[LI]-R-x-Y",

    "[KR]-P-[PTQ]-[FYLVQH]-S-[FY]-x(2)-[LIVM]-x(3,4)-[AC]-[LIM]",

    "W-[QKR]-[NSD]-[SA]-[LIV]-R-H",

    "L-x(3)-[FY]-K-H-x-N-x-[STAN]-S-F-[LIVM]-R-Q-L-[NH]-x-Y-x-[FYW]-[RKH]-K-[LI"
    "VM]",

    "W-x-[DNH]-x(5)-[LIVF]-x-[IV]-P-W-x-H-x(9,10)-[DE]-x(2)-[LIVF]-F-[KRQ]-x-[W"
    "R]-A",

    "F-R-Y-x-C-E-G",

    "[RG]-x-[RKS]-x(5)-[IL]-x-[DNGSK]-x(3)-[KR]-x(2)-T-[FY]-x-[RK](3)-x(2)-[LIV"
    "M]-x-[KE]-K-[AT]-x-[EQ]-[LIVM]-[STA]-x-L-x(4)-[LIVM]-x-[LIVM](3)-x(6)-[LIV"
    "MF]-x(2)-[FYLS]",

    "[LQI]-W-x(2)-[FCL]-x(3,4)-[NT]-E-[MQ]-[LIVNM]-[LIV]-[TLF]-x(2)-[GR]-[RG]-["
    "KRQM]",

    "[LIVMFYWE]-H-[PADHL]-[DENQRS]-[GSE]-x(3)-G-x(2)-[WL]-[ML]-x(3)-[IVA]-x-F",

    "G-R-N-E-L-I-x(2)-[YH]-I-x(3)-[TC]-x(3)-R-T-[RK](2)-Q-[LIVM]-S-S-H-[LIVM]-Q"
    "-V",

    "G-[KR]-x(3)-[STAGN]-x-[LIVMYA]-[GSTA](2)-[CSAV]-[LIVM]-[LIVMFY]-[LIVMA]-[G"
    "SA]-[STAC]",

    "Y-x-[PK]-x(2)-[IF]-x(2)-[LIVM](2)-x-[KRH]-x(3)-P-[RKQ]-x(3)-L-[LIVM]-F-x-["
    "STN]-G-[KR]-[LIVMA]-x(3)-G-[TAGL]-[KR]-x(7)-[AGCS]-x(7)-[LIVMF]",

    "M-D-L-V-K-x-H-L-x(2)-A-V-R-E-E-V-E",

    "[EKH]-[LHVI]-x(9,10)-[IVNLR]-x(3)-[LIV]-x(6)-G-D-x(2)-E-N-[GSA]-x-Y",

    "S-x(2)-S-[PK]-[LIVMF]-[AG]-x-[SAGNE]-[LIVM]-[LIVY]-x(4)-[DNG]-[DE]",

    "[LIVMF](2)-D-E-A-D-[RKEN]-x-[LIVMFYGSTN][GSAH]-x-[LIVMF](3)-D-E-[ALIV]-H-["
    "NECR]",

    "[GST]-[LIVMAPKR]-[IVEAT]-[FY]-[GSAC]-[IV]-E-[FYV]-[SA]-x(0,1)-[REA]-x(2)-["
    "RQSFT]-[DEK]",

    "G-[IVT]-[LVAC](2)-[IVT]-D-[DE]-[FL]-[DNST]",

    "C-x-[DE]-C-x(3)-[LIVMF]-x(1,2)-D-x(2)-L-x(3)-F-x(4)-C-x(2)-C",

    "[LIVM](2)-T-[KR]-T-E-x-K-x-[DE]-Y-[LIVMF](2)-x-D-x-[DE]",

    "[VILT]-[KREIT]-[PV]-x-[FYIL]-[VI]-[FW]-D-G-x(2)-[PILHST]-x-[LVCQMF]-K",

    "[GS]-[LIVM]-[PER]-[FYS]-[LIVM]-x-A-P-x-E-A-[DE]-[PAS]-[QS]-[CLM]",

    "[KRQ]-[LIVMA]-x(2)-[GSTALIV]-{FYWPGDN}-x(2)-[LIVMSA]-x(4,9)-[LIVMF]-x-{PLH"
    "}-[LIVMSTA]-[GSTACIL]-{GPK}-x(2)-[GANQRF]-[LIVMFY]-x(4,5)-[LFY]-x(3)-[FYIV"
    "A]-{FYWHCM}-x(3)-[GSADENQKR]-x-[NSTAPKL]-[PARL]",

    "C-x(2)-D-[LIVM]-x(6)-[ST]-x(4)-S-[HYR]-[HQ]",

    "[GSTAP]-x(2)-[DNEQA]-[LIVM]-[GSA]-x(2)-[LIVMFYT]-[GAN]-[LIVMST]-[ST]-x(6)-"
    "R-[LIVT]-x(2)-[LIVM]-x(3)-G",

    "[LIVM]-[STAG]-[RHNW]-x(2)-[LIM]-[GA]-x-[LIVMFYA]-[LIVSC]-[GA]-x-[STACN]-x("
    "2)-[MST]-x-[GSTN]-R-x-[LIVMF]-x(2)-[LIVMF]",

    "R-{G}-x(2)-[LIVM]-x(3)-[LIVM]-x(16,17)-[STA]-x(2)-T-[LIVMA]-[RH]-[KRNAQ]-D"
    "-[LIVMF]",

    "[LIVM]-x-[DE]-[LIVM]-A-x(2)-[STAGV]-x-V-[GSTP]-x(2)-[STAG]-[LIVMA]-x(2)-[L"
    "IVMFYAN]-[LIVMC]",

    "[GDC]-x(2)-[NSTAVY]-x(2)-[IV]-[GSTA]-x(2)-[LIVMFYWCT]-x-[LIVMFYWCR]-x(3)-["
    "NST]-[LIVM]-x(5)-[NRHSA]-[LIVMSTA]-x(2)-[KR]",

    "[STNAQ]-[LIAMV]-x-[RNGSYK]-x(4,5)-[LM]-[EIVLA]-x(2)-[GESD]-[LFYWHA]-[LIVC]"
    "-x(7)-[DNS]-[RKQG]-[RK]-x(6)-[TS]-x(2)-[GAS]",

    "[GSA]-x-[LIVMFA]-[ASM]-x(2)-[STACLIV]-[GSDENQR]-[LIVC]-[STANHK]-x(3)-[LIVM"
    "]-[RHF]-x-[YW]-[DEQ]-x(2,3)-[GHDNQ]-[LIVMF](2)",

    "[GS]-[LIVMFYSP]-x(2,3)-[TS]-[LIVMTA]-x(2)-[LIVM]-x(5)-[LIVQSA]-[STAGENQH]-"
    "x-[GPART]-x-[LIVMFA]-[FYSTNRH]-x-[HFYRA]-[FVW]-x-[DNSTKAG]-[KQMT]-x(2,3)-["
    "LIVM]",

    "[ST]-[DM]-H-[LIC]-x(2)-[FA]-[LIY]-[EQK]-R-x(2)-[QNKA]",

    "P-[LIVM]-x-[LIVM]-x(2)-[LIVM]-A-x(2)-[LIVMFT]-x(2)-[HS]-x-S-T-[LIVM]-S-R",

    "R-R-T-[IV]-[ATN]-K-Y-R",

    "[DE]-[LIVMF](2)-[HEQS]-x-G-x-[LIVMFA]-G-L-[LIVMFYE]-x-[GSAM]-[LIVMAP]",

    "[STN]-x(2)-[DENQ]-[LIVMT]-[GAS]-x(4)-[LIVMF]-[PSTG]-x(3)-[LIVMA]-x-[NQR]-["
    "LIVMA]-[EQH]-x(3)-[LIVMFWK]-x(2)-[LIVM]",

    "[STAIV]-[PQDEL]-[DE]-[LIV]-[LIVTA]-Q-x-[STAV]-[LIVMFYC]-[LIVMAK]-x-[GSTAIV"
    "]-[LIMFYWQ]-x(12,14)-[STAP]-[FYW]-[LIF]-x(2)-[IV]",

    "[LIVMFY](3)-x-G-[DEQ]-[STE]-G-[STAV]-G-K-x(2)-[LIVMFY]",

    "[GS]-x-[LIVMF]-x(2)-A-[DNEQASH]-[GNEK]-G-[STIM]-[LIVMFY](3)-[DE]-[EK]-[LIV"
    "M]",

    "[FYW]-P-[GS]-N-[LIVM]-R-[EQ]-L-x-[NHAT]",

    "[GSK]-F-x(2)-[LIVMF]-x(4)-[RKEQA]-x(2)-[RST]-x(1,2)-[GA]-x-[KN]-P-x-[TN]",

    "H-[FW]-x-[LIVM]-x-G-x(5)-[LV]-H-x(3)-[DE]",

    "[LIVMFY]-[DH]-x-[LIVM]-[GA]-E-R-x(3)-[LIF]-[GDN]-x(2)-[PA]H-N-H-P-[SQ]-G",

    "A-L-[KR]-[IF]-[FY]-[STA]-[STAD]-[LIVMQ]-R",

    "[LIFV]-x(6)-[LIF]-[LIVF]-x-[GSDE]-[GSTADNPE]-[PASG]-x(2)-R-R-x-[FYW]-[LIVM"
    "F]-[DN]",

    "[LIV]-[LIFYM]-x-[LIVM]-D-[DEA]-[LIVF]-x(2)-[EHCG]-L-D-x(2)-[KRH]-x(3)-[LIV"
    "F]",

    "C-x(2)-C-x(3,5)-[STACD]-x(4)-C-x-[LIVFQ]-C-x(4)-[RD]-[NQDS]",

    "[STAC]-G-[LIVM]-x-L-x-G-x-E-[LIVM]-[KQ]-[SA]-[LIVMA][AC]-G-L-x-F-P-V",

    "[KR]-E-[LIVM]-[EQ]-T-x(2)-[KR]-x-[LIVM](2)-x-[PAG]-[DE]-L-x-[KR]-H-A-[LIVM"
    "]-[STA]-E-G",

    "K-A-P-R-K-[QH]-[LI]P-F-x-[RA]-L-[VA]-[KRQ]-[DEG]-[IV]G-A-K-R-H",

    "[FI]-S-[KR]-K-C-x-[EK]-R-W-K-T-[MV][AT]-x(1,2)-[RK](2)-[GP]-R-G-R-P-[RK]-x"
    "",

    "[RQ]-R-S-A-[RS]-L-S-A-[RKM]-[PL]",

    "[STANVFHG]-x(2)-[FAS]-x(4)-[DNSPAKT]-x(0,7)-[DENQTFG]-Y-[HFYLRKT]-x(2)-[LI"
    "VMFY]-x(3)-[LIVM]-x(4)-[LIVM]-x(6,10)-Y-x(12,13)-[LIVM]-x(2)-N-[SACF]-x(2)"
    "-[FY]",

    "[FYL]-x-[LIVMC]-[KR]-W-x-[GDNR]-[FYWLME]-x(5,6)-[ST]-W-[ESV]-[PSTDEN]-x(2,"
    "3)-[LIVMC]",

    "G-x-N-D-x(2)-[AV]-L-G-R-x-T",

    "[LIVMFA]-[STAGC](2)-G-x-{TAV}-H-[STAGLI]-[LIVMFA]-{KI}-[LIVM]",

    "[AV]-R-[NFY]-R-x(2,3)-[ST]-{S}-S-{NS}-SS-K-R-K-Y-R-KH-x(3)-H-S-[NS]-S-x-P-"
    "Q-[SG]",

    "K-x-R-K-x(2)-E-G-K-x(2)-K-[KR]-K",

    "[IMGV]-x(2)-[LIVA]-x(2,3)-[LIVMY]-[GAS]-x(2)-[LMSF]-[GSNH]-[PTKR]-[KRAVG]-"
    "[GN]-x-[LIMF]-P-[DENSTKQPRAGVI]",

    "P-x(2)-R-G-[STAIV](2)-x-N-[APK]-x-[DE]",

    "[FL]-x(6)-[DN]-x(2)-[AGS]-x-[ST]-x-G-[KRH]-G-x(2)-G-x(3)-R",

    "[LIVM]-x(2)-[LIVM]-[STAVC]-[GE]-[QV]-x(2)-[LIVMA]-x-[STC]-x-[STAG]-[KRH]-x"
    "-[STA]",

    "[PS]-[DENS]-x-Y-K-[GA]-K-G-[LIVM]",

    "[QLS]-x(3)-[LIVM]-x(2)-[KRWYF]-x(2)-R-x-F-x-D-G-[LIVM]-[YF]-[LIVM]-x(2)-[K"
    "R]",

    "G-x(2)-[GNF]-x(4)-[VAI]-x(2)-G-[FY]-x(2)-[NH]-[FYWL]-L-x(5)-[GA]-x(3)-[STN"
    "G]",

    "[KNQ]-x(2)-{K}-x(3)-{A}-x(10)-[LIVMFY]-x(2)-[DENHR]-x(2)-[GS]-[LIVMF]-[STD"
    "NQC]-[VTA]-x-[DENQKHPSA]-[LIVMSAD]-x(2)-[LIMF]-[KR]",

    "[RKN]-x-[LIVM]-x-G-[ST]-x(2)-[SNQ]-[LIVM]-G-x(2)-[LIVM]-x(0,1)-[DENG]",

    "[LIVM]-[KRVLYFS]-[GKR]-M-[LIV]-[PST]-x(4,5)-[GSKR]-[NQEKRAH]-x(5)-[LIVM]-x"
    "-[AIVL]-[LFYV]-x-[GDNS]",

    "[GA]-[LIV](3)-x(9,10)-[DNS]-G-x(4)-[FY]-x(2)-[NT]-x(2)-V-[LIV]",

    "K-[LIVM](2)-[GASL]-x-[GT]-x-[LIVMA]-x(2,5)-[LIVM]-x-[LIVMF]-x(3,4)-[LIVMFC"
    "A]-[ST]-x(2)-A-x(3)-[LIVM]-x(3)-G",

    "[KRG]-[KR]-x-[GSAC]-[KRQVA]-[LIVMK]-[WY]-[LIVM]-[KRN]-[LIVM]-[LFY]-[APK]",

    "R-M-G-x-[GR]-K-G-x(4)-[FWKR]",

    "I-x-[ST]-[GT]-x(2)-[KR]-x-[KR]-x(6)-[DE]-x-[LIMV]-[LIVMT]-T-x-[STAG]-[KR]",

    "[LIVMF]-x-[KRGTIEQS]-x-[GSAIYN]-[KRQDAVLSI]-[VGAIT]-[RSNAK]-x(0,1)-[KRAQ]-"
    "[SAKG]-[KYR]-[KLI]-[LYSFT]-[YF]-[LIM]-[RK]",

    "K-x(3)-[KRCV]-x-[LIVM]-W-[IV]-[STNALVQCMI]-[RH]-[LIVM]-[NS]-x(3)-[RKHSG]",

    "[IVTL]-x(3)-[KR]-x(3)-[KRQ]-[KT]-x(6)-G-[HFY]-[RK]-[RQT]-x(2)-[STL]",

    "[RKQN]-x(4)-[RH]-[GAS]-x-G-[KRQS]-x(8)-{L}-[HDN]-[LIVM]-{A}-[LIVMS]-x-[LIV"
    "M]",

    "[RK](2)-[AM]-[IVFYT]-[IV]-[RKT]-L-[STANEQK]-x(7)-[LIVMFT]",

    "[GDEN]-D-x-[IV]-x-[IV]-[LIVMA]-x-G-x(2)-[KRA]-[GNQK]-x(2,3)-[GA]-x-[IV]",

    "G-x-[LIVM](2)-x-R-Q-R-G-x(5)-G",

    "[KNQS]-[PSTLNH]-{D}-x-[LIVMFA]-[KRGSADN]-x-[LIVYSTA]-[KR]-[KRHQS]-[DESTANQ"
    "RL]-[LIV]-A-[KRCQVT]-[LIVMA]",

    "[IVTAS]-[LIVM]-x(2)-[LF]-x-[LI]-x-[KRHQEG]-x(2)-[STNQH]-x-[IVTR]-x(10)-[LM"
    "SN]-[LIV]-x(2)-[LIVA]-x(2)-[LMFY]-[IVT]",

    "[DES]-[IVT]-x(4)-H-[PT]-[FAVY]-[FYW]-[TISN]-x(9,13)-[GN]-[KRHNQ]",

    "[YW]-x-[STKV]-x-[KR]-[NSKQ]-x(3,4)-[PATQS]-x(1,2)-[LIVMF]-[EAQVSIT]-x(2)-K"
    "-[FYH]-[CSD]",

    "[KF]-[RGM]-T-[FYWL]-[EQS]-x(5)-[KRHS]-x(4,5)-G-F-x(2)-R",

    "K-[STNV]-{F}-x-[GSAM]-[SAILV]-x-[KRA]-R-[IVFY]-x(14,16)-[GSANQKR]-H",

    "[CHDS]-x(2)-C-x(2)-[LIVM]-x-R-x(3)-[LIVMNR]-x-[LIVM]-x-[CN]-x(3,4)-[KRS]-["
    "HLF]-x-[QCAV]-x-Q",

    "[NP]-x(3)-[KRM]-x(2)-A-[LIVTK]-x-[SA]-[AC]-[LIV]-x-[ALCM]-[STL]-[SGAKTI]-x"
    "(7)-[RK]-[GS]-H",

    "N-x(2)-P-[LI]-R-R-x(4)-[FY]-V-I-A-T-S-x(1,2)-K",

    "[CAS]-x(4)-[IVA]-P-[FY]-x(2)-[LIVM]-x-[GSQNK]-[KRQ]-x(2)-L-G",

    "A-D-R-x(3)-[GR]-[MH]-R-x-[SAP]-[FYW]-G-[KRVTS]-[PAN]-x-[GS]-x(2)-A-[KRLV]-"
    "[LIV]",

    "[KR]-x-G-[KR]-G-F-[ST]-[LVF]-x-E-[LVI]-x(3)-G",

    "[DENT]-[KR]-[AL]-R-x-[LI]-G-[FY]-x-[SAP]-x(2)-G-[LIVMFY]-[LIVMFYKS]-[LIVMF"
    "Y]-[LIVMFYA]-R-x-[RNAS]-[IVL]-x-[KRC]-G",

    "[KREWD]-x-L-x(2)-[PS]-[KRS]-x(2)-[RH]-[PSA]-x-[LIVM]-[NSA]-[LIVM]-x-[RK]-["
    "LIVM]",

    "Q-[KR]-R-[LIVM]-x-[SA]-x(4)-[CV]-G-x(3)-[IV]-[WK]-[LIVF]-[DNSV]-[PE]",

    "G-[DEQ]-x-V-x(10,11)-[GVT]-x(2)-[FYHDN]-x(2)-[FY]-x-G-x-[TV]-G",

    "[FY]-x-[GSHE]-x(2)-[IVLF]-x-P-[GA]-x-G-x(2)-[FYV]-x-[KRHE]-x-D",

    "G-K-[NS]-x-W-F-F-x(2)-L-[RH]-F",

    "[STA]-x(5)-G-x-[QKR]-x(2)-[LIVM]-[KRQT]-x(2)-[KR]-x-[GS]-x(2)-K-x-[LIVM](3"
    ")",

    "[DE]-[LM]-G-[STALPD]-x(2)-[GK]-[KR]-x(6)-[LIVM]-x-[LIVM]-x-[DEN]-x-[GI]",

    "[VI]-[KRWVI]-[LIV]-[DSAG]-x(2)-[LIV]-[NS]-x-[AKQEHFYLCT]-x-W-x-[KRQE]-[GS]"
    "",

    "F-x-R-x(4)-[KRL]-x(2)-[KRT]-[LIVMFT]-x(3,5)-W-R-[KR]-x(2)-G",

    "[ST]-x-S-x-[KRQ]-x(4)-[KR]-[TL]-P-[GS]-[GN]",

    "G-[KT]-[LIVM]-x-[RD]-x-H-G-x(2)-G-x-V-x-[AVS]-x-F-x(3)-[LI]-P",

    "P-Y-E-[KR]-R-x-[LIVMT]-[DE]-[LIVM](2)-[KR]",

    "G-T-x-[SAVT]-x-G-x-[KRHIM]-x(4,5)-H-x(2)-C-x-R-C-G",

    "[KRM]-[PTKS]-x(3)-[LIVMFG]-x(2)-[NHS]-x(3)-R-[DNHY]-W-R-[RS]",

    "K-x-[TV]-K-K-x(2)-L-[KR]-x(2)-C",

    "[LIVMFA]-x-{GPRV}-[LIVMFYC](2)-{LPC}-[STAC]-[GSTANQEKR]-[STALV]-[HY]-[LIVM"
    "F]-G",

    "P-x(2)-[LIVMF](2)-[LIVMS]-x-[GDN]-x(3)-[DENL]-x(3)-[LIVM]-x-E-x(4)-[GNQKRH"
    "]-[LIVM]-[AP]",

    "[GSTA]-[KR]-x(6)-G-x-[LIVMT]-x(2)-[NQSCH]-x(1,3)-[LIVFCA]-x(3)-[LIV]-[DENQ"
    "]-x(7)-[LMT]-x(2)-G-x(2)-[GS]",

    "[LIVM]-[DER]-x-R-[LI]-x(3)-[LIVMC]-[VMFYHQ]-[KRT]-x(3)-[STAGCVF]-x-[ST]-x("
    "3)-[SAI]-[KR]-x-[LIVMF](2)",

    "G-[KRQE]-x(3)-[FYVI]-x-[ACVTI]-x(2)-[LIVMA]-[LIVM]-[AG]-[DN]-x(2,3)-G-x-[L"
    "IVMA]-[GS]-x-[SAG]-x(5,6)-[DEQG]-[LIVMARF]-x(2)-A-[LIVMFR]",

    "G-x-[KRC]-[DENQRH]-L-[SA]-Y-x-I-[KRNSA]",

    "[DENSK]-x-[LIVMDET]-x(3)-[LIVMFTA](2)-x(6)-G-K-[KR]-x(5)-[LIVMF]-[LIVMFC]-"
    "x(2)-[STAC]",

    "[GE]-x(2)-[LIV](2)-[STY]-[ST]-x(2)-G-[LIVM](2)-x(4)-[AG]-[KRHAYIL]",

    "[GS]-G-G-x(2)-[GSA]-Q-x(2)-[SA]-x(3)-[GSA]-x-[GSTAV]-[KR]-[GSALVD]-[LIFV]",

    "[AV]-x(3)-[GDNSR]-[LIVMSTAG]-x(3)-G-P-[LIVM]-x-[LIVM]-P-T",

    "[LIVMFR]-x-[GSTACQI]-[LIVMF]-x(1,2)-[GSTALVM]-x(0,1)-[GSN]-[LIVMFY]-x-[LIV"
    "M]-x(4)-[DEN]-x-[TS]-[PS]-x-[PA]-[STCHF]-[DN]",

    "[RK]-x-P-N-S-[AR]-x-R",

    "[KRQSEAT]-[GS]-x-R-H-x(2)-[GSNHKLCD]-x(2)-[LIVMCT]-[RNH]-G-Q",

    "[RP]-x(0,1)-C-x(11,12)-[LIVMF]-x-[LIVMF]-[SC]-[RG]-x-{D}-{PK}-[RN]",

    "[LIVM]-x(2)-H-[LIVMFY]-x(3)-{S}-x-D-x(2)-[STAGN]-x(3)-[LF]-x(2)-{A}-x(6)-["
    "LIVM]-x(2)-[FY]",

    "[LIVMT]-x-[LIVM]-[KR]-L-[STAK]-R-{E}-G-[AKR]",

    "G-D-x-[LIV]-x-[LIVA]-x-[QEK]-x-[RK]-P-[LIV]-S",

    "[IVR]-[DY]-[YLF]-x(2,3)-[LIVMTP]-x(2)-[LIVM]-x(2)-[FYT]-[LIVM]-[ST]-[DERP]"
    "-x(1,2)-[GYAH]-[KCR]-[LIVM]-x(3)-[RHG]-[LIVMASR]",

    "[STDNQ]-G-[KRNQMHSI]-x(6)-[LIVM]-x(4)-[LIVMC]-[GSD]-x(2)-[LFI]-[GAS]-[DE]-"
    "[FY]-x(2)-[ST]",

    "[DE]-x-A-[LIY]-[KR]-[RA]-[FL]-K-[KR]-x(3)-[KR]",

    "[LIV]-x-[GHN]-R-[IVNT]-x-E-x-[SCT]-[LV]-x-[DE]-[LVI]",

    "H-x-K-R-[LIVMF]-[SANK]-x-P-x(2)-[WY]-x-[LIVM]-x-[KRP]",

    "[LIVM]-[STAMR]-G-G-x-[DG]-x(2)-G-x-[PV]-M",

    "[KRAI]-L-x-R-E-L-E-K-K-F-[SAPQG]-x-[KRN]-[HED]",

    "[KR]-x(2)-[ST]-G-[GAR]-x(5,6)-[KRHSA]-x-[KRT]-x-[KR]-x-[EA]-[LIMPA]-G",

    "[SA]-[LI]-[KREQP]-x(2)-[LIVM]-x(2)-[SA]-x(3)-[DNG]-G-[LIV]-x(2)-G-[LIV]",

    "[IVDYP]-x-[ST]-K-x-[LIVMT]-[RI]-N-x-[IV]-[SA]-G-[FY]-x-T-[HKR]",

    "P-x(6)-[SANG]-x(2)-[LIVMAC]-x-R-x-[ALIV]-[LV]-[QH]-x-L-[EQ]",

    "L-Y-[IV]-P-R-K-C-S-[SA]",

    "[FYAT]-G-x(2)-[KREIL]-[STAI]-x-[GCAV]-[FYKRE]-[GTALV]-x-[LIVYA]-Y-[DENQAKY"
    "]-[STDN]-x(7)-E",

    "[YH]-C-[VI]-[SA]-C-A-I-H",

    "[QKRTE]-C-x(2)-C-x(6)-F-[GSDA]-x-[PSA]-x(5)-C-x(2)-C-[GSAQ]-x(2)-[LIV]-x(2"
    ")-[PS]-x-G",

    "E-[ST]-[EA]-R-E-A-[RK]-x-[LI]G-F-R-G-E-[AG]-L",

    "[STA]-[LIVMF]-x-[LIVM]-x-D-E-[LIVMFY]-[GCA]-[RKHAS]-[GS]-[GST]-x(4)-G",

    "G-x(5)-E-x(4)-[TAGCV]-[LIVMACF]-x-R-[EL]-[LIVMFGSTA]-x-[EA]-E-x-[GNDTHR]",

    "[IL]-[GA]-x(2)-[LIVMF]-[SGADENK]-x(0,1)-[KR]-x-H-[STPA]-[STAV]-[LIVM]-x(2)"
    "-[SGAMN]-x(3)-[LIVM]",

    "K-x-E-[LIV]-A-x-[DE]-[LIVMF]-G-[LIVMF]",

    "[KRC]-[SAQ]-x-G-x-[VF]-G-[GA]-x-[LIVM]-x-[KR]-[KRC]-[LIVM](2)",

    "G-H-E-x(2)-G-x(5)-[GA]-x(2)-[IVSAC]",

    "[GSD]-[DEQHKM]-x(2)-L-x(3)-[SAG](2)-G-G-x-G-x(4)-Q-x(2)-[KRS]",

    "[STALIV]-[LIVF]-x-[DE]-x(6,7)-P-x(4)-[ALIV]-x-[GST]-x(2)-D-[TAIVM]-[LIVMF]"
    "-x(4)-E",

    "[GSW]-x-[LIVTSACD]-[GH]-x(2)-[GSAE]-[GSHYQ]-x-[LIVTP]-[GAST]-[GAS]-x(3)-[L"
    "IVMT]-x-[HNS]-[GA]-x-[GTAC]",

    "[LIVSPADNK]-x(12)-Y-[PSTAGNCV]-[STAGNQCIVM]-[STAGC]-K-{PC}-[SAGFYR]-[LIVMS"
    "TAGD]-x-{K}-[LIVMFYW]-x(2)-{YR}-[LIVMFYWGAPTHQ]-[GSACQRHM]",

    "G-[FY]-R-[HSAL]-[LIVMF]-D-[STAGCL]-[AS]-x(5)-[EQ]-x(2)-[LIVMCA]-G",

    "[LIVMFY]-x(8)-{L}-[KREQ]-{K}-[LIVM]-G-[LIVM]-[SC]-N-[FY]",

    "[LIVM]-[PAIV]-[KR]-[ST]-{EPQG}-x(3)-R-{SVAF}-x-[GSTAEQK]-[NSL]-x(2)-[LIVMF"
    "A]",

    "A-x(3)-G-[LIVMFY]-[STAG]-x(2,3)-[DNS]-P-x(2)-D-[LIVM]-x-G-x-D-x(3)-K",

    "[GSA]-[ATIVS]-[LIVMYCAFST]-K-[DN]-[LIVM]-[LIVMFYT]-[GA]-x-[GACKMSI]-x-G-[A"
    "LIVMF]-x(2)-[SGA]-[LIVMYERAKQFS]-x(0,1)-[TLIVMFYWAQ]-[ETGAS]-x(0,1)-[NDVS]"
    "",

    "[IV]-G-G-G-x(2)-G-[STACV]-G-x-[AT]-x-[DQ]-x(3)-[RAS]-G",

    "G-G-K-x(2)-[GSTE]-Y-R-x(2)-A[LIVMY]-x-[FS]-x(2)-[STAGCV]-x-V-D-R-[IV]-x-[P"
    "S]",

    "[IVT]-[DE]-x(2)-[AYEP]-G-[PT]-[ST]-E-[LIVST]-[LIVMAECGF]-[LIVMA]-[LIVMAYF]"
    "-[ACNDSTI]-x(3)-[ACNGVST]-x(4)-[LIVMA]-[AVLKI]-[SACLYWNRMT]-[DE]-[LIVMFC]-"
    "[LIVMKF]-[SAG]-x(2)-E-H",

    "[LIVMA]-G-[EQ]-H-G-[DN]-[ST]",

    "[LIVMA]-[AG]-[IVT]-[LIVMFY]-[AG]-x-G-[NHKRQGSAC]-[LIV]-G-x(13,14)-[LIVMFT]"
    "-{A}-x-[FYWCTH]-[DNSTK]",

    "[LIVMFYWA]-[LIVFYWC]-x(2)-[SAC]-[DNQHR]-[IVFA]-[LIVF]-x-[LIVF]-[HNI]-x-P-x"
    "(4)-[STN]-x(2)-[LIVMF]-x-[GSDN]",

    "[LMFATCYV]-[KPQNHAR]-x-[GSTDNK]-x-[LIVMFYWR]-[LIVMFYW](2)-N-x-[STAGC]-R-[G"
    "P]-x-[LIVH]-[LIVMCT]-[DNVE]",

    "[LIVMFY](2)-G-L-G-x-[MQ]-G-x(2)-[MA]-[SAV]-x-[SNHR]",

    "[RKH]-x(2)-{I}-x-{I}-x-D-x-M-G-x-N-x-[LIVMA][LIVM]-G-x-[LIVM]-G-G-[AG]-T",

    "A-[LIVM]-x-[STAN]-x(2)-[LI]-x-[KRNQ]-[GSA]-H-[LM]-x-[FYLH]",

    "[DNES]-x(2)-[GA]-F-[LIVMFYA]-x-[NT]-R-x(3)-[PA]-[LIVMFY]-[LIVMFYST]-x(5,6)"
    "-[LIVMFYCT]-[LIVMFYEAH]-x(2)-[GVE]",

    "[LIVM]-T-[TRKMN]-L-D-x(2)-R-[STA]-x(3)-[LIVMFY]",

    "[FM]-x-[DV]-D-x(2)-[GS]-T-[GSA]-x-[IV]-x-[LIVMAT]-[GAST](2)-[LIVMFA]-[LIVM"
    "FY]",

    "[NSK]-[LIMYTV]-[FYDNH]-[GEA]-[DNGSTY]-[IMVYL]-x-[STGDN]-[DN]-x(1,2)-[SGAP]"
    "-x(3,4)-[GE]-[STG]-[LIVMPA]-[GA]-[LIVMF]",

    "[LIVM]-x-[DG]-x(2)-[GAEHS]-[NQSD]-[KS]-G-[TE]-G-x-WD-H-Y-L-G-K-[EQK]",

    "[LIVMT]-[RK]-[LIVM]-G-[LIVM]-G-x-G-[SRK]-[LIVMAT]-C-x-T",

    "[DEN]-[WV]-x(3)-G-[RKNM]-x(6)-[FYW]-[SV]-x(4)-[LIVM]-N-x(2)-N-V-x(2)-L-[RK"
    "T]",

    "W-x(4)-[YF]-D-x(3)-[DN]-[LIVMFYT]-[LIVMFY](3)-x(2)-G-x(2)-[STAG]-[PVT]",

    "S-N-H-G-[AG]-R-Q",

    "[GA]-[RKNC]-x-[LIVW]-G(2)-[GST](2)-x-[LIVM]-N-x(3)-[FYWA]-x(2)-[PAG]-x(5)-"
    "[DNESHQ]",

    "[GS]-[PSTA]-x(2)-[ST]-[PS]-x-[LIVM](2)-x(2)-S-G-[LIVM]-G",

    "[GA]-{A}-x(2)-[KRNQHT]-x(11,14)-[LIVMFYWS]-x(4)-{GK}-x(3)-[LIVMF]-x-C-x(2)"
    "-[DEN]-R-x(2)-[DE]",

    "[STAN]-x-[CH]-x(2,3)-C-[STAG]-[GSTVMF]-x-C-x-[LIVMFYW]-x-[LIVMA]-x(3,4)-[D"
    "ENQKHT]",

    "[STA]-x-[STAC](2)-x(2)-[STA]-D-[LIVMY](2)-L-P-x-[STAC](2)-x(2)-E",

    "A-x(3)-[GDT]-I-x-[DNQTK]-x-[DEA]-x-[LIVM]-x-[LIVMC]-x-[NS]-x(2)-[GS]-x(5)-"
    "A-x-[LIVM]-[ST]",

    "[LIVMFGA]-E-[LIMSTAC]-[GS]-G-[KNLM]-[SADN]-[TAPFV]",

    "[FYLVA]-x(2)-{DILV}-G-[QE]-{LPYG}-C-[LIVMGSTANC]-[AGCN]-x-[GSTADNEKR]",

    "[LIVM]-[SADN]-x(2)-C-x-R-[LIVM]-x(4)-[GSC]-H-[STA][ASV]-S-C-[NT]-T-{S}-x-["
    "LIM]",

    "[LIVMA]-[GSA]-x-[PA]-G-C-[FYN]-[AVP]-T-[GSAC]-x(3)-[GTAC]-[LIVMCA]-x-P",

    "V-x(5)-A-[LIVAMT]-x-[HWF]-[IM]-x(2)-[HYWNRFT]-[GSNT]-[STAG]-x(0,1)-H-[ST]-"
    "[DE]-x(1,2)-I",

    "[ERND]-[IVL]-x-[ED]-x-H-x(3)-K-x-[DE]-x(2)-S-[GA]-[TAS]-[ALCGS]",

    "G-x(2)-[LIVMF]-[IFYH]-[DN]-x-[FYWM]-x-G-x(2)-[LF]-[NY]-P-[RQ]",

    "[LIVM](2)-[LIVMFT]-[HWD]-R-x(2)-R-D-x(3)-C-x(2)-K-Y-[GK]-x(2)-[FW]-x(2)-Y",

    "[GSA]-x(4)-[GK]-[GSTA]-[LIVFSTA]-[GST]-x(3)-[NQRK]-x-G-[NHY]-x(2)-P-[RTV]",

    "[LIVM](2)-[GSA]-x-G-G-[IV]-x-[STGDN]-x(3)-[ACV]-x(2)-{A}-x(2)-{L}-G-A",

    "K-x-[WQA]-[CA]-x(2)-[FYH](2)-x-[LIVM]-x-H-R-x-E-x-R-G-[LIVMT]-G-G-[LIVM]-F"
    "-[FY]-D",

    "R-[ST]-H-[ST]-x(2)-A-x-G-G[GAC]-[LIVM]-[ST]-E-x(2)-[GSAN]-G-[ST]-D-x(2)-[G"
    "SA]",

    "[QDE]-x-{P}-G-[GS]-x-G-[LIVMFY]-x(2)-[DEN]-x(4)-[KR]-x(3)-[DEN]",

    "[GA]-[LIVM]-P-x-E-x(3)-[NG]-E-x(1,3)-R-V-A-x-[ST]-P-x-[GSTV]-V-x(2)-[LI]-x"
    "-[KRHNGS]-x-G",

    "[LIVM]-[LIVMF]-G-[GAV]-G-x-[AV]-G-x(2)-[SA]-x(3)-[GA]-x-[SGR]-[LIVM]-G-A-x"
    "-V-x(3)-D",

    "[LIV]-x(2)-G-G-[SAG]-K-x-[GV]-x(3)-[DNST]-[PL]",

    "[LIVM](2)-H-[NHA]-Y-G-x-[GSA](2)-x-G-x(5)-G-x-A",

    "[LIVFM]-E-F-W-[QHG]-x(4)-R-[LIVM]-H-[DNE]-R",

    "[LIVM]-[LIVMA]-[LIVMF]-x(4)-[ST]-x(2)-N-Y-[DE]-[YN]",

    "T-x-[GS]-x(2)-H-[LIVMF]-x(3)-E-[DE]-x-PW-x-W-H-x-C-H-x-H-[YN]-H-S-[MI]-[DE"
    "]",

    "[PALF]-x(2,3)-[LIV]-x(3)-[LIVM]-[STAC]-[STV]-x-[GANK]-G-x-T-x(2)-[AG]-[LIV"
    "]-x(2)-[LMF]-[DENQK]",

    "[LVAGC]-[LIF]-G-x(4)-[LIVMF]-P-W-x(4,5)-[DE]-x(3)-[FYIV]-x(3)-[STIQ]",

    "[EQLT]-x-[EQKDS]-[LIVM](2)-x(2)-[LIVM]-x(2)-[LIVMY]-N-x-[DNS]-x(5)-[LIVMF]"
    "(3)-Q-[LM]-P-[LVI]",

    "P-[GK]-G-[VI]-G-P-[MFI]-T-[IVA]",

    "P-x(10)-[DE]-[LIVM]-x(3)-[LIVM]-x(9,12)-[LIVM]-x(3)-[GSA]-[GSTCHRQ]-G-H",

    "G-G-x-C-[LIVA]-x(2)-G-C-[LIVM]-P",

    "C-x(2)-C-D-[GAS]-x(2,4)-[FYA]-x(4)-[LIVMAT]-x(0,1)-[LIVM](2)-[GI]-[GDS]-[G"
    "RD]-[DN]",

    "G-[LIVMFYKRSAQT]-[LIVMAGPF]-[QAM]-x-[LIVMFYC]-x-D-[AGIM]-[LIVMFTA]-[KS]-[L"
    "VMYSTI]-[LIVMFYGA]-x-[KRE]-[EQG]",

    "P-F-D-[LIVMFYQN]-[STAGPVMI]-E-[GACS]-E-x(0,1)-[EQLN]-[LIVMS]-x(1,2)-G",

    "[GN]-x-[DE]-[KRHST]-[LIVMFA]-[LIVMF]-P-[IV]-D-[LIVMFYWA]-[LIVMFYWK]-x-P-x-"
    "C-P-[PT]",

    "[DKG]-x(2)-[FLV]-[STKD]-x(5)-C-[LMNQ]-[GA]-x-C-x(2)-[GA]-P",

    "E-R-E-x(2)-[DE]-[LIVMFY](2)-x(6)-[HK]-x(3)-[KRP]-x-[LIVM]-[LIVMYS]",

    "[LIVMH]-H-[RT]-[GA]-x-E-K-[LIVMTN]-x-E-x-[KRQ]",

    "G-[AM]-G-[AR]-Y-[LIVM]-C-G-[DE](2)-[STA](2)-[LIM](2)-[END]-S",

    "E-[ST]-C-G-x-C-x-P-C-R-x-GP-x(2)-C-[YWSD]-x(7)-[GA]-x-C-R-x-C",

    "C-P-x-C-[DE]-x-[GS](2)-x-C-x-L-QR-C-[LIVM]-x-C-x-R-C-[LIVMT]-x-[LMFY]",

    "[STV]-G-C-x(3)-C-x(6)-[DE]-[LIVMF]-[GAT]-[LIVMF]",

    "[LV]-x-[LV]-[LIV]-K-[STV]-[ST]-x-[SN]-x-F-x(2)-[FY]-x(4)-[FY]-x(2)-L-x(5)-"
    "R",

    "[YWG]-[LIVFYWTA](2)-[VGS]-H-[LNP]-x-V-x(44,47)-H-H",

    "V-x-H-x(33,40)-C-x(3)-C-x(3)-H-x(2)-M",

    "[LIVM](2)-[FYW]-x(10)-C-x(2)-C-G-x(2)-[FY]-K-L",

    "[LIV]-R-x-K-x-[FYW]-x-W-[GS]-D-G-x-[KH]-[ST]-x-F-x-N",

    "G-x-[FYW]-x-[LIVMFYW]-x-[CST]-x-{PR}-x(5)-{LFH}-G-[LM]-x(3)-[LIVMFYW]",

    "H-C-H-x(3)-H-x(3)-[AG]-[LM]",

    "[DET]-[LIVMTA]-x(2)-[LIVM]-[LIVMSTAG]-[SAG]-[LIVMSTAG]-H-[STA]-[LIVMFY]",

    "[SGATV]-{D}-x(2)-[LIVMA]-R-[LIVMA]-x-[FW]-H-{V}-[SAC]",

    "R-[LIVMFSTAN]-F-[GASTNP]-Y-x-D-[AST]-[QEH]",

    "[IF]-x-[RH]-x(4)-[EQ]-R-x(2)-H-x(2)-[GAS]-[GASTFY]-[GAST]",

    "[GNDR]-[RKHNQFYCS]-x-[LIVMFCS]-[LIVMF](2)-x-N-[VT]-x-[STCA]-x-C-[GA]-x-[TA"
    "]",

    "[LIV]-[AGD]-F-P-[CS]-[NG]-Q-F",

    "[HQ]-[EQ]-x(3)-H-x-[LMA]-[NEQHRCS]-[GSTA]-H-[LIVMSTAC](2)-x-E",

    "[LIVMACST]-H-P-[LIVM]-x-[KRQV]-[LIVMF](2)-x-[AP]-H",

    "[GNTIV]-x-H-x(5,7)-[LIVMF]-Y-x(2)-[DENTA]-P-x-[GP]-x(2,3)-E",

    "[LIVMF]-x-G-x-[LIVM]-x(4)-[GS]-x(2)-[LIVMA]-x(4)-[LIVM]-[DE]-[LIVMFYC]-x(6"
    ")-G-x-[FY]",

    "G-G-S-[AN]-[GA]-Q-S-S-x(2)-Q[FY]-L-[DQ]-[DE]-[LIVM]-x(2)-Y-M-x(3)-H-[KR]",

    "P-H-H-D-[SA]-S-T-FC-x-H-R-[GAR]-x(7,8)-[GEKVI]-[NERAQ]-x(4,5)-C-x-[FY]-H",

    "[GR]-C-[IV]-G-R-[ILS]-x-W",

    "[GA]-[LIVM]-P-[LIVM]-x-[LIVMFY]-x-W-x(6)-[RK]-x(6)-Y-x(3)-[AR]",

    "H-P-[LIV]-[AG]-G-Q-G-x-N-x-G-x(2)-D",

    "P-D-x(2)-H-[DE]-[LIVF]-[LIVMFY]-G-H-[LIVMC]-[PA]H-H-M-x(2)-F-x-C",

    "H-x-F-x(4)-H-T-H-x(2)-GH-x(4,5)-F-[LIVMFTP]-x-[FW]-H-R-x(2)-[LVM]-x(3)-E",

    "D-P-x-F-[LIVMFYW]-x(2)-H-x(3)-DG-E-x-[FYN]-H-N-[FY]-H-H-x-F-P-x-D-Y",

    "[ST]-[SA]-x(3)-[QR]-[LI]-x(5,6)-D-Y-x(2)-[LIVMFYW]-[LIVM]-[DE]",

    "[FW]-[SGNH]-x-[GD]-{F}-[RKHPT]-{P}-C-[LIVMFAP]-[GAD]",

    "L-[IV]-A-H-[STACH]-Y-[STV]-[RT]-Y-[LIVM]-G",

    "[GA]-[IMFAT]-H-[LIVF]-H-x(2)-[GP]-[SDG]-x-[STAGDE]",

    "G-[GN]-[SGA]-G-x-R-x-[SGA]-C-x(2)-[IV]D-x-[WF]-E-H-[STA]-[FY](2)",

    "W-x(2)-[LIVF]-x(6,7)-G-[LIVM]-[FYRA]-[NH]-x(3)-[STAQLIVM]-[ASC]-x(2)-[PA]",

    "[IVMSEQ]-E-x(1,2)-[LIVTA]-[HY]-[GSA]-x-[STAVM]-Y-x(2)-[LIVMQ]-x(3)-[LIVFY]"
    "-[IVFYCSA]",

    "[LIVMFYH]-[LIVMFST]-H-[AG]-[AGSP]-[LIVMNQA]-[AG]-C",

    "[STANQ]-[ET]-C-x(5)-G-D-[DN]-[LIVMT]-x-[STAGR]-[LIVMFYST]",

    "E-x-G-G-P-x(2)-[GA]-x-G-C-[AG]-GD-x-L-G-D-V-V-C-G-G-F-[AGSP]-x-P",

    "[RK]-x-[STA]-x(2)-S-x-C-Y-[SL][LIVM](2)-x-C-G-[STA]-x(2)-[STAG]-x(2)-T-x-["
    "DNG]",

    "R-G-[LIVMF]-E-x(15)-[QESMP]-[RK]-x-C-[GR]-[LIVM]-C",

    "[FY]-D-[PI]-C-[LIMAV]-[ASG]-C-x(2,3)-H",

    "[HY]-[LIVMA]-x(2)-[LIVM]-[GSTACIV]-[GSTAC]-[GSA]-[LIVMFA]-[DEQHY]-S-x-[LIV"
    "MAS]-[LIVMFKS]-[GF]-[DE]-x-[EQRD]-[IV]-[LIVTQA]-[TAGRKS]-Q-[LIVMF]-[KRE]",

    "[NG]-x-[FYWV]-[LIVMF]-x-G-[AGC]-[GS]-[TA]-[HQT]-P-G-[STAV]-G-[LIVM]-x(5)-["
    "GS]",

    "[STIV]-x-R-[IVT]-[CSA]-G-Y-x-[GACV]L-I-D-I-G-S-G-P-T-[IV]-Y-Q-[LV]-L-[SA]-"
    "A-C",

    "[DN]-P-[PA]-R-x-G-x(14,16)-[LIVM](2)-Y-x-S-C-N-x(2)-T",

    "[LIVMF]-D-x-F-P-[QHY]-[ST]-x-H-[LIVMFY]-E",

    "R-x(2)-[LIVMT]-x(2,3)-[FWY]-[QNYDI]-x(8,13)-[LVESI]-x-P-C-[HAVMLC]-x(3)-[Q"
    "MTLHD]-[FYWL]-x(0,1)-[LV]",

    "[LIVMA]-[LIVMFYW]-[DE]-x-G-[STAPVLCG]-G-x-[GA]-x-[LIVMF]-[ST]-x(2,3)-[LIVM"
    "A]-x(5,7)-[LIVMYF]-x-[STAGVLC]-[LIVMFYHCS]-E-x-D",

    "[LIVMF]-P-C-H-R-[LIVMF](2)[LIVMAC]-[LIVFYWA]-{DYP}-[DN]-P-P-[FYW]",

    "[LIVMF]-T-S-P-P-[FY][DENKS]-x-[FLIV]-x(2)-[GSTC]-x-P-C-x-{V}-[FYWLIM]-S",

    "[RKQGTF]-x(2)-G-N-[SA]-[LIVF]-x-[VIP]-x-[LVMT]-x(3)-[LIVM]-x(3)-[LIVM]",

    "[GSAED]-[DN]-G-x(2)-G-[FYWLV]-x(3)-[GSA]-[PTL]-[FY]-[DNSHE]-x-I",

    "[LIVM]-[GS]-[STAL]-G-P-G-x(3)-[LIVMFY]-[LIVM]-T-[LIVM]-[KRHQG]-[AG]",

    "[VW]-x(2)-[LI]-x(2)-[GA]-[DT]-x(3)-[FYW]-[GS]-x(8)-[LIVFA]-x(5,6)-[LIVMFYW"
    "PAC]-x-[LIVMY]-x-[PN]-G",

    "Y-[DN]-x-[MLAFTI]-N-x(2)-[LIVMN]-S-x(3)-[HQD]-x(2)-W",

    "R-V-[LIVMCT]-[KRQ]-[PVRK]-[GMKD]-[GAS]-x-[LIVMFAT]-x(2)-[LIVMCA]-[ED]-x-[S"
    "GT]",

    "[DEQH]-[LIVMFYA]-x-[GSTMVA]-[GSTAV]-[ST]-[STV]-[HQ]-K-[STG]-[LFMI]-x-[GAS]"
    "-[PGAC]-[RQ]-[GSARH]-[GA]",

    "G-x-[STM]-[IVT]-x-[FYWVQELK]-[VMAT]-x-[DEVMKA]-x-[LIVMY]-D-x-G-x(2)-[LIVT]"
    "-x(6)-[LIVM]",

    "F-x-[EK]-x-S-[GT]-R-T",

    "R-x(3)-[LIVMTA]-[DENQSTHKF]-x(5,6)-[GSN]-G-H-[PLIVMF]-[GSTA]-x(2)-[LIMC]-["
    "GS]",

    "[GP]-[DEQGSANPHVT]-[DN]-G-[PAEQ]-[ST]-[HQ]-x-[PAGM]-[LIVMYACNQS]-[DEFYWLA]"
    "-x(2)-[STAPG]-x(2)-[RGANQS]",

    "[DGH]-[IVSAC]-T-[ST]-N-P-[STA]-[LIVMF](2)",

    "[LIVMA]-x-[LIVM]-K-[LIVM]-[PAS]-x-[STC]-x-[DENQPAS]-[GC]-[LIVM]-x-[AGV]-x("
    "0,1)-[QEKRSTH]-x-[LIVMF]",

    "[LI]-[PK]-x-[LVPQ]-P-[IVTAL]-P-x-[LIVMA]-x-[DENQAS]-[ST]-[LIVMA]-x(2)-[LY]"
    "",

    "R-[FYW]-x-[DA]-[KA]-x(0,1)-[LIVMFY]-x-[LIVMFY](2)-x(3)-[DNS]-[GSA]-x(6)-[D"
    "E]-[HS]-x(3)-[DE]-[GAC]",

    "[LIVM]-[NST]-x(2)-C-[SAGLI]-[ST]-[SAG]-[LIVMFYNS]-x-[STAG]-[LIVM]-x(6)-[LI"
    "VM]",

    "N-x(2)-G-G-x-[LIVM]-[SA]-x-G-H-P-x-[GA]-x-[ST]-G",

    "[AG]-[LIVMA]-[STAGCLIVM]-[STAG]-[LIVMA]-C-x-[AG]-x-[AG]-x-[AG]-x-[SAG]",

    "Q-[LIV]-H-H-[SA]-x(2)-D-G-[FY]-H",

    "[LIV]-[GAED]-x(2)-[STAV]-x-[LIV]-x(3)-[LIVAC]-x-[LIV]-[GAED]-x(2)-[STAVR]-"
    "x-[LIV]-[GAED]-x(2)-[STAV]-x-[LIV]-x(3)-[LIV]",

    "G-x-{KGR}-x(2)-[LIVMFTAP]-x(2)-[AGC]-C-[STA](2)-[STAG]-x(2)-{LI}-[LIVMF]",

    "R-[LIVMFYS]-x-[LIVM]-x-[QHG]-x-G-C-[FYNA]-[GAPV]-G-[GAC]-[STAVK]-x-[LIVMF]"
    "-[RAL]",

    "[DE]-[IV]-N-F-L-C-x-H-KK-F-G-x-G-D-G[RHQ]-[ST]-W-[GSA]-G-A-R-P-E",

    "T-[STA]-H-x-[ST]-[LIVMA]-x(4)-G-[SN]-x-V-[STA]-x-T-x-T-[LIVM]-[NE]-x(1,2)-"
    "[FY]-G",

    "[GT]-Q-[CA]-W-V-x-[SA]-[GAS]-[IVT]-x(2)-T-x-[LMSC]-R-[CSAG]-[LV]-G",

    "E-A-[SC]-G-x-[GS]-x-M-K-x(2)-[LM]-N",

    "[FW]-x(2)-[QL]-x(2)-[LIVMYA]-[LIMV]-x(4,6)-[LVGAC]-[LVFYAHM]-[LIVMF]-[STAG"
    "CM]-[HNQ]-[STAGC]-G-x(2)-[STAG]-x(3)-[STAGL]-[LIVMFA]-x(4,5)-[PQR]-[LIVMTA"
    "]-x(3)-[PA]-x(2,3)-[DES]-[QEHNR]",

    "[LIVMFYWCTA]-[LIVM]-[LIVMA]-[LIVMFC]-[DE]-D-[LIVMS]-[LIVM]-[STAVD]-[STAR]-"
    "[GAC]-x-[STAR]",

    "[PAS]-[LIVMFYT]-[LIVMFY]-G-[LIVMFY]-C-[LIVMFYN]-G-x-[QEH]-x-[LIVMFA]",

    "x(0,11)-C-[GS]-[IV]-[LIVMFYW]-[AG]",

    "[GST]-x-G-[LIVM]-G-x-[PA]-S-x-[GSTAL]-[IL]-x(3)-E-L",

    "[LIVF]-x(3)-[GS]-x(2)-H-x-[LIVMFY]-x(4)-[LIVMF]-x(3)-[ATV]-x(1,2)-[LIVM]-x"
    "-[ATV]-x(4)-[GN]-x(3,4)-[LIVMF](2)-x(2)-[STN]-[SAGT]-x-G-[GS]-[LIVM]",

    "[SA]-[GS]-R-[GA]-[LIV]-x(2)-[TAP]-[GAS]-G-T-x-D-x-[LIVM]-E",

    "E-x(5)-G-x-[SAG]-x(2)-[IV]-x-D-[LIV]-x(2)-[ST]-G-x-T-[LM]",

    "[FY]-x-[FY]-K-x(2)-H-[FY]-x-L-[STI]-x-AG-R-x-[GA]-N-F-[LIVMF]-N-x-E-x(2)-G"
    "",

    "[GN]-[AS]-G-D-Q-G-x(3)-G-[FYHG]G-[GA]-G-[ASC]-F-S-x-K-[DE]",

    "[LIVM](2)-x-D-D-x(2,4)-D-x(4)-R-R-[GH]",

    "[LIVMFY]-G-x(2)-[FYL]-Q-[LIVM]-x-D-D-[LIVMFY]-x-[DNG]",

    "[DEH]-[LIVMF]-[LIVMFC]-[LIVMF]-R-[STPV]-[SGAC]-[GEN]-x(1,2)-R-x-S-x-[FY]-["
    "LMFV]-[LIPMVT]-[YWL]",

    "N-x(3)-[DEH]-x(2)-[LIMF]-D-x(2)-[VM]-x-R-[ST]-x(2)-R-x(4)-[GYNK]",

    "V-[LA]-[LIV](2)-G-G-G-x-G-x(2)-[LIV]-x-E",

    "Y-[CSAM]-x(2)-[VSG]-A-[GSA]-[LIVAT]-[IV]-G-x(2)-[LMSC]-x(2)-[LIV]",

    "[LIVM]-G-x(3)-Q-x(2,3)-[ND]-[IFL]-x-[RE]-D-[LIVMFY]-x(2)-[DE]-x(4,7)-R-x-["
    "FY]-x-P",

    "[PSIAV]-{IVM}-[NDFV]-[NEQIY]-{PRNK}-[LIVMAGP]-W-[NQSTHF]-[FYHQ]-[LIVMR]",

    "[LIVMF]-x(5)-G-[STADNQ]-[KREQIYW]-V-N-[LIVM]-E",

    "[LIVM]-x-[AG]-[LIVMF](2)-N-x-T-x-[DN]-S-[FLMI]-x-D-x-[SG]",

    "[GE]-[SAV]-x-[LIVM](2)-D-[LIVMF]-G-[GPA]-x(2)-[STA]-x-P",

    "[LIVF]-{LV}-x-[GANQK]-[NLG]-[SA]-[GA]-[TAI]-[STAGV]-x-R-x-[LIVMFYAT]-x-[GS"
    "TAP]",

    "[KR]-x-[KH]-E-[CSTVI]-[DNE]-R-[LIVMY]-x-[GSTAVLD]-[LIVMCTF]-x(3)-[LIVMFA]-"
    "x(2)-[LIVMFCGANY]-G",

    "G-x(3)-F-E-R-V-[FY]-x-A-[NQ]-x-N-C",

    "[GS]-[LIVMFYTAC]-[GSTA]-K-x(2)-[GSALVN]-[LIVMFA]-x-[GNAR]-x-R-[LIVMA]-[GA]"
    "",

    "T-[LIVMFYW]-[STAG]-K-[SAG]-[LIVMFYWR]-[SAG]-{ENKR}-{TNDR}-[SAG]",

    "[LIVMFYWCS]-[LIVMFYWCAH]-x-D-[ED]-[IVA]-x(2,3)-[GAT]-[LIVMFAGCYN]-x(0,1)-["
    "RSACLIH]-x-[GSADEHRM]-x(10,16)-[DH]-[LIVMFCAG]-[LIVMFYSTAR]-x(2)-[GSA]-K-x"
    "(2,3)-[GSTADNV]-[GSAC]",

    "E-x-[STAGCI]-x(2)-N-[LIVMFAC]-[FY]-x(6,12)-[LIVMFA]-x-T-x(6,8)-[LIVM]-x-[G"
    "S]-[LIVM]-x-[KR]",

    "[LIVFYCHT]-[DGH]-[LIVMFYAC]-[LIVMFYA]-x(2)-[GSTAC]-[GSTA]-[HQR]-K-x(4,6)-G"
    "-x-[GSAT]-x-[LIVMFYSAC]",

    "[LIVM]-G-F-[TN]-F-S-[FY]-P-x(5)-[LIVM]-[DNST]-x(3)-[LIVM]-x(2)-W-T-K-x-[LF"
    "]",

    "G-R-x-N-[LIV]-I-G-[DE]-H-x-D-Y",

    "[LIVM]-[PK]-x-[GSTA]-x(0,1)-G-[LM]-[GS]-S-S-[GSA]-[GSTAC]",

    "[RK]-x(4)-G-H-x-Q-[QR]-G-G-x(5)-D-R",

    "[AG]-G-x(0,1)-[GAP]-x-N-x-[STA]-x(2)-{A}-x-{G}-x-[GS]-x(9)-G",

    "[DNSK]-[PSTV]-x-[SAG](2)-[GD]-D-x(3)-[SAGV]-[AG]-[LIVMFYA]-[LIVMSTAP]",

    "[LIVM]-x(2)-G-[LIVMFCT]-G-x-[GA]-[LIVMFA]-x(3)-{V}-x(4)-G-x(3,5)-[GATP]-{G"
    "}-x-G-[RKH]",

    "K-[LIVM]-x-R-D-x(3)-R-G-x-[ST]-x-E",

    "[GA]-x(1,2)-[DE]-x-Y-x-[STAPV]-x-C-[NKR]-x-[CH]-[LIVMFYWH]",

    "[MFYGS]-x-[PST]-x(2)-K-[LIVMFYW]-{G}-W-[LIVMF]-{E}-[DENQTKR]-[ENQH]",

    "[GSA]-x-[LIVMFYW]-x-G-[LIVM]-x(7,8)-[HDENQ]-[LIVMF]-{PEQ}-{DTAI}-[AS]-[STA"
    "LIVM]-[LIVMFY]-[DEQ]",

    "[GN]-x-[LIVFM]-x-R-x(2)-[LIM]-[GSAR]-x(3)-[FY]-x(8)-[LV]-x(5)-P-x-[LIVM]",

    "[LIV]-G-{P}-G-{P}-[FYWMGSTNH]-[SGA]-{PW}-[LIVCAT]-{PD}-x-[GSTACLIVMFY]-x(5"
    ",18)-[LIVMFYWCSTAR]-[AIVP]-[LIVMFAGCKR]-K",

    "[LIVMFYC]-x-[HY]-x-D-[LIVMFY]-K-x(2)-N-[LIVMFYCT](3)",

    "[LIVMFYC]-{A}-[HY]-x-D-[LIVMFY]-[RSTAC]-{D}-x-N-[LIVMFYC](3)",

    "F-x(10)-R-E-x(72,86)-R-D-x-K-x(9)-[CS]",

    "C-P-x-[LIVMYAT]-x-C-x(5)-[LI]-P-[LIVMCA]-G-x(9)-V-[KRM]-x(2)-C-[PA]-x-C",

    "[LIVAC]-x-[LIVM](2)-[SAPCV]-K-[LIV]-E-[NKRST]-x-[DEQHS]-[GSTA]-[LIVM]",

    "[KR]-x(2)-E-x(3)-[LIVMF]-x(8,12)-[LIVMF](2)-[SA]-x-G(3)-x-[LIVMFG]",

    "E-x-[LIVM]-N-[ST]-[SA]-[LIV]-E-x(2)-V-D",

    "[LIVMFAC]-K-x(1,3)-[DEA]-[DE]-[LIVMCP]-R-Q-[DE]-x(4)-Q",

    "[GSET]-x-[AVE]-x(3)-[LIVM]-x(2)-[FYHW]-[LIVM](2)-x-[LIVMFN]-x-D-R-[HNG]-x("
    "2)-N",

    "[LIVMFANT]-[LIVM]-x-[LIVMA]-N-x-G-S-[ST](2)-x-[KE]",

    "[LIVMFATQ]-[LIVMA]-x(2)-H-x-G-x-[GT]-x-[ST]-[LIVMA]-x-[TAVC]-x(3)-G",

    "[KRHGTCVN]-[VT]-[LIVMF]-[LIVMC]-R-x-D-x-N-[SACV]-P",

    "[LIVM]-x-K-[FY]-G-G-[ST]-[SC]-[LIVM]",

    "[GSTNAD]-x(2)-[GAS]-x-G-[GC]-[IM]-x-[STAG]-K-[LIVMCT]-x-[SAI]-[TCAGFS]-x(2"
    ")-[GALVCMI]",

    "C-P-x(0,1)-[ST]-N-[ILV]-G-TG-[LIVM]-H-[STAV]-R-[PAS]-[GSTA]-[STAMVN]",

    "[GSTADE]-[KREQSTIV]-x-{EPRK}-x(2)-[KRDN]-S-[LIVMF](2)-x-[LIVM]-{EATN}-x-[L"
    "IVM]-[GADE]",

    "G-x(2)-[LIVMFA]-[LIVMF](2)-H-[LIVMF]-G-[LIVMF]-x-T-[LIVA]",

    "[DENQ]-x(6)-[LIVMF]-[GA]-x(2)-[LIVM]-A-[LIVM]-P-H-[GAC]",

    "N-[LIVMFY]-x(5)-C-x-T-R-[LIVMF]-x-[LIVMF]-x-[LIVM]-x-[DQEN]",

    "[LIVMFYWCA]-[LIVMFYW](2)-D-G-[FYI]-P-R-x(3)-[NQ]",

    "N-x(2)-H-[GA]-S-D-[GSA]-[LIVMPKNE]",

    "[TS]-[ST]-R-x(2)-[KR]-x(2)-[DE]-x(2)-[GA]-x(2)-Y-x-[FY]-[LIVMKHRT]",

    "[LIV]-[LIVMGSTC]-[DET]-[RH]-[FYHCS]-x(2)-S-[GSTNP]-x-[AVC]-[FY]-[STANQ]",

    "D-[LI]-H-[SA]-x-Q-[IMST]-[QM]-G-[FY]-F-x(2)-P-[LIVMFC]-D",

    "[KRHD]-x-[GA]-[PSAE]-R-x(2)-D-[LIV]-D-[LIVM](2)",

    "P-[LIVM]-x(2)-D-[GA]-[ST]-[AC]-[SN]-[GA]-[LIVMFY]-Q",

    "[LIVMF]-x-R-x(3)-K-x(2)-[LIVMF]-M-[PT]-x(2)-YY-[ST]-P-[ST]-S-P-[STANK]",

    "G-x-[KN]-[LIVMFA]-[STAC]-[GSTNR]-x-[HSTA]-[GSAI]-[QNH]-K-[GL]-[IVTEC]",

    "[FY]-C-x-[DEKSTG]-C-[GNK]-[DNSA]-[LIVMHG]-[LIVM]-x(8,14)-C-x(1,2)-C",

    "N-[SGAT]-[LIVMF]-R-R-x(9)-[SAR]-x(3)-[VA]-x(4)-N-x-[STA]-x(3)-[DN]-E-x-[LI"
    "V]-[GSA]-x-R-[LI]-[GAS]-[LIVM](2)-[PV]",

    "H-[NQEIVMYD]-[LIVMYAS]-V-P-[EKT]-H-x(2)-[LIVM]-x(2)-[DESAG]-[ET]",

    "[ST]-x-[FY]-E-x-[AT]-R-x-[LIVM]-[GSA]-x-R-[SA]-x-Q",

    "[LIVMFA]-x(2)-[EYD]-x-[HYN]-[ST]-[LIVMFY]-x-[NSTR]-x-[LIV]-x(3)-[LIVA]-x(4"
    ",5)-[VPG]-x(4)-[FY]-x(3)-[HPY]-[PEV]",

    "[LIVMFD]-[LIVMFP]-P-[LIVM]-x-C-[FL]-[ST]-C-G",

    "R-x(2)-[GSAV]-K-x(3)-[LIVMFY]-[AGQ]-x(2)-Y-x(2)-[GS]-x(3)-[LIVMA]",

    "[YA]-[GLIVMSTAC]-D-T-D-[SG]-[LIVMFTC]-{LA}-[LIVMSTAC]",

    "G-[SG]-[LFY]-x-R-[GE]-x(3)-[SGCL]-x-D-[LIVM]-D-[LIVMFY](3)-x(2)-[SAP]",

    "F-E-N-[RK]-G-x(3)-G-x(4)-H-P-H-x-Q",

    "D-L-P-I-[VS]-G-G-[ST]-[LIVM](2)-[STAV]-H-[DEN]-H-[FY]-Q-[GAT]-G",

    "[AG]-G-G-x-G-[STKA]-x-L-x(2)-L-[TA]-x(3)-[AST]-x-P-[AS]-[LV]",

    "W-[FY]-x-G-[ST]-[AS]-[DNSH]-[AS]-[LIVMFYW]",

    "[APV]-[GS]-M-G-[LIVMN]-Y-[IVC]-[LIVMFY]-x(2)-[DENPHKRQS]",

    "S-x-[LIVMF]-K-R-x(4)-K-D-x-[GSA]-x(2)-[LIF]-[PGS]-x-H-G-G-[LIVMF]-x-D-R-[L"
    "IVMFT]-D",

    "[CA]-[DE]-[LIVM](2)-[NQV]-[GTA]-D-[GA]-[SG]-x(2)-[TAV]-[AT]",

    "G-S-x-[AG]-[KRN]-x-T-x-L-[KRN]-x(3)-[DE]-x-[DET]-[LM]-[VI]-x-F",

    "R-P-[VI]-I-L-D-P-x-[DE]-P-TD-G-x(2)-A-R-x(7,8)-G-x(3)-D-x(3)-D",

    "[KRA]-x(2)-[TIVK]-P-[ST]-[MGA]-[GA]-G-[LIVSA]-x-[LIVMF](2)",

    "N-x(2)-[NK]-x-[TI]-[DN]-G-[ILVM]-D-G-[LM]",

    "G-[GA]-x-[STN]-x-H-[STA]-[STAV]-[LIVM](2)-[STAV]-[RG]",

    "[DEQSKN]-x-[LIVMF]-[SA]-[LIVMF]-G-[ST]-N-D-[LIVM]-x-Q-[LIVMFYGT]-[STALIV]-"
    "[LIVMFY]-[GAS]-x(2)-R",

    "[FY]-x(3)-H-[LIV]-P-G-A-x(2)-[LIVF][AV]-x(2)-[FY]-[DEAP]-G-[GSA]-[WF]-x-E-"
    "[FYW]",

    "[DN]-[GN]-x(2)-[LIVMFA](3)-G-G-F-x(3)-G-x-P[LF]-[HQ]-S-E-N-G-[LIVF](2)-[GA"
    "]",

    "[IVT]-[LIVMC]-[IVT]-[HS]-D-[SGAV]-[AV]-RC-C-{P}-x-H-{LGY}-x-C",

    "[LIVMA]-C-{LIVMFYWPCST}-C-D-{GS}-x(3)-{QS}-C",

    "[LIV]-{KG}-[LIVFY]-[LIVMST]-G-[HYWV]-S-{YAG}-G-[GSTAC]Y-x(2)-Y-Y-x-C-x-C",

    "[LIVMFYAG](4)-G-D-S-[LIVM]-x(1,2)-[TAG]-G",

    "[LIVMF](2)-x-[LIVMF]-H-G-G-[SAG]-[FY]-x(3)-[STDN]-x(2)-[ST]-H",

    "[LIVM]-x-[LIVMF]-[SA]-G-D-S-[CA]-G-[GA]-x-L-[CA]",

    "F-[GR]-G-x(4)-[LIVM]-x-[LIV]-x-G-x-S-[STAG]-G",

    "[ED]-D-C-L-[YT]-[LIV]-[DNS]-[LIV]-[LIVFYW]-x-[PQR]",

    "[GSTNP]-x(6)-[FYVHR]-[IVN]-[KEP]-x-G-[STIVKRQ]-Y-[DNQKRMV]-[EP]-x(3)-[LIMV"
    "A]",

    "[IV]-x-G-[STAD]-[LIVT]-D-[FYI]-[IV]-[FSN]-G",

    "[FYH]-x(2)-[TN]-[RK]-H-N-x-G-x(2)-[LIVMFAYCT]-[LIVMFA]-[DEN]",

    "[GS]-x(3)-H-N-G-[LIVM]-[KR]-[DNS]-[LIVMTC]",

    "[QR]-[IV]-x(4)-[TC]-D-x(2)-G-[IV]-V-x-[HF]-x(2)-[FY]",

    "[IV]-x-D-S-[GAS]-[GASC]-[GAST]-[GA]-T",

    "[LIVM]-x(2)-[LIVMA]-x(2)-[LIVM]-x-R-H-[GN]-x-R-x-[PAS]",

    "[LIVMF]-x-[LIVMFAG]-x(2)-[STAGI]-H-D-[STANQ]-x-[LIVM]-x(2)-[LIVMFY]-x(2)-["
    "STA]",

    "G-S-Y-P-S-G-H-T[LIVM]-x-[LIVM](2)-[HEA]-[TI]-x-D-x-H-[GSA]-x-[LIVMF]",

    "[FYPH]-x(4)-[LIVM]-G-N-H-E-F-[DN]",

    "[AG]-[RK]-[LI]-x(1,2)-[LIV]-[FY]-E-x(2)-P-[LIVM]-[GSA][LIVMN]-[KR]-G-N-H-E"
    "",

    "E-F-D-Y-L-K-S-L-E-I-E-E-K-I-NN-[AG]-H-[TA]-Y-H-I-N-S-I-S-[LIVM]-N-S-D",

    "[LIVMFY]-[LIVMFYA]-[GSAC]-[LIVM]-[FYC]-D-G-H-[GAV]",

    "[LIVMF]-H-C-x(2)-G-x(3)-[STC]-[STAGP]-x-[LIVMFY]",

    "[FWV]-x(0,1)-[LIVM]-D-P-[LIVM]-D-[SG]-[ST]-x(2)-[FYA]-x-[HKRNSTY]",

    "[WYV]-D-x-[AC]-[GSA]-[GSAPV]-x-[LIVFACP]-[LIVM]-[LIVAC]-x(3)-[GH]-[GA]",

    "H-Y-x-[GT]-D-[LIVMAF]-[DNSH]-x-P-x-H-[PA]-x-N",

    "H-D-[LIVMFY]-x-H-x-[AG]-x(2)-[NQ]-x-[LIVMFY]",

    "H-x-H-L-D-H-[LIVM]-x-[GS]-[LIVMA]-[LIVM](2)-x-S-[AP]",

    "[SAPG]-[LIVMST]-[CS]-[STACG]-P-[STA]-R-x(2)-[LIVMFW](2)-[TAR]-G",

    "G-[YV]-x-[ST]-x(2)-[IVAS]-G-K-x(0,1)-[FYWMK]-[HL]G-x-T-L-x-H-E-H-[LIV]",

    "A-x-A-x-A-x(4)-G-x-P-[LIVM]-x(2)-H[APF]-D-[LIVMF](2)-{T}-[LIVM]-Q-E-{G}-K",

    "D-[ST]-[FY]-[RP]-[KHQ]-x(7,8)-[FYWD]-[ST]-[FYW](2)",

    "N-x-G-x-R-[LIVM]-D-[LIVMFYH]-x-[LV]-x-S",

    "H-[GSAD]-x-Y-[LIF]-[LIMN]-N-[LIVMFCAP]-[AGC][GSAR]-[LIVMF]-C-[LIVMFY]-D-T-"
    "C-H",

    "[LIVMFW]-H-x-N-[DEG]-[SA]-x(4)-[GNAQ]-x(3)-D-x-H",

    "[LIVM](2)-[AP]-[LQ]-H-[STA]-[STAE]-P-x(5)-E-[LIVM]-[DN]-x-L-x-[DE]-V",

    "G-D-F-N-A-x-C-[SA]",

    "G-x-[GA]-x-A-x-K-x-[EQ]-[IV]-x(16,19)-D-x-[SAV]-D-A-x-A-[LIVM]-A",

    "C-x(3)-[KRSN]-P-[KRAGL]-C-x(2)-C-x(5)-C",

    "[GSTENA]-x-[LIVMF]-P-x(5)-[LIVMW]-x(2,3)-[LI]-[PAS]-G-[IV]-[GA]-x(3)-[GAC]"
    "-x(2,3)-[LIVMA]-x(1,2)-[GSALVI]-[LIVMFYW]-[GANKD]",

    "[HI]-[FYE]-[GSTAM]-[LIVM]-x(4,5)-Y-[STALV]-x-[FWVAC]-[TV]-[SA]-P-[LIVMA]-["
    "RQ]-[KR]-[FY]-x-D-x(3)-[HQ]",

    "[DEQ]-[KRQT]-[LMF]-E-[FYW]-[LV]-G-D-[SARHG]",

    "[LIVMFYSNAD]-x(2)-A-x(2)-R-[NH]-[KRQLYAT]-[LIVMFSA]-[KRA]-R-x-[LIVMTA]-[KR"
    "]",

    "[FYWL]-x-[LIVM]-H-G-L-[WY]-P[LIVMF]-x(2)-[HDGTY]-[EQ]-[FYW]-x-[KR]-H-G-x-C"
    "",

    "C-K-x(2)-N-T-FD-R-G-H-[QLIM]-x(3)-[AG]",

    "D-G-D-T-[LIVM]-x-[LIVMC]-x(9,10)-R-[LIVM]-x(2)-[LIVM]-D-x-P-E",

    "D-[KR]-Y-[GQ]-R-x-[LV]-[GA]-x-[IV]-[FYW][LIVMFY](2)-D-[STA]-H-x-H-[LIVMFP]"
    "-[DN]",

    "P-[LIVM]-x-[LIVM]-H-x-R-x-[TA]-x-[DE]",

    "[LVSAT]-[LIVA]-x(2)-[LIVMT]-[PSD]-x(3)-[LI]-[LIVMT]-[LIVMST]-E-T-D-x-P",

    "H-x-C-G-G-N-V-G-DG-x-[SA]-G-E-[LIVM]-R-Y-P-S-Y",

    "[STN]-[GP]-x(1,2)-[DE]-x-W-E-E-x(2)-[GS]",

    "[GSDENKRH]-x(2)-[VMFC]-x(2)-[GS]-H-G-[LIVMAG]-x(1,2)-[LIVM]-G-S",

    "D-[LIVMFY]-[DNV]-x-[DNS]-x(2)-[LIVM]-[DN]-[SALM]-x-D-x(3)-[LIVMF]-x-[RKS]-"
    "x-[LIVMF]",

    "[LIVMFY]-[DN]-G-[LIVMF]-[DN]-[LIVMF]-[DN]-x-E",

    "C-x(4,5)-F-Y-[ST]-x(3)-[FY]-[LIVMF]-x-A-x(3)-[YF]-x(2)-F-[GSA]",

    "[LIVM]-[GSA]-F-x-[STAG](2)-[LIVMFY]-W-[FY]-W-[LIVM]",

    "C-x(3)-C-x(2)-[LMF]-x(3)-[DEN]-[LI]-x(5)-C",

    "G-[LIVMFY]-x(2)-[LIVMFY]-x-[LIVM]-D-[DF]-x(1,2)-W-x(3,7)-[RV]-[DNSF]",

    "P-G-G-R-F-x-E-x-Y-x-W-D-x-YQ-W-D-x-P-x-[GAV]-W-[PAS]-PP-x(2)-L-x(3)-K-W-E-"
    "x-C",

    "[LIVMFSTC]-[LIVFYS]-[LIV]-[LIVMST]-E-N-G-[LIVMFAR]-[CSAGN]",

    "F-x-[FYWM]-[GSTA]-x-[GSTA]-x-[GSTA](2)-[FYNH]-[NQ]-x-E-x-[GSTA]",

    "N-x-[LIVMFYWD]-R-[STACN](2)-H-Y-P-x(4)-[LIVMFYWS](2)-x(3)-[DN]-x(2)-G-[LIV"
    "MFYW](4)",

    "[DENQLF]-[KRVW]-N-[HRY]-[STAPV]-[SAC]-[LIVMFS](3)-W-[GS]-x(2,3)-N-E",

    "[LIVM](2)-[KR]-x-[EQK]-x(4)-G-[LIVMFT]-[LIVT]-[LIVMF]-[ST]-D-x(2)-[SGADNI]"
    "",

    "P-x-[SA]-x-[LIVMFY](2)-[QN]-x(2)-N-P-x(4)-[TA]-x(9,10)-[KRD]-x-[LIV]-[GN]-"
    "x-C",

    "[LIV]-[LIVMFYWGA](2)-[DNEQG]-[LIVMGST]-{SENR}-N-E-[PV]-[RHDNSTLIVFY]",

    "V-x-Y-x(2)-P-x-R-D-C-[GSAF]-x(2)-[GSA](2)-x-G",

    "[LIVMYA]-[LIVA]-[LIVT]-[LIV]-E-P-D-[SAL]-[LI]-[PSAG]",

    "A-[ST]-D-[AG]-D-x(2)-[IM]-A-x-[SA]-[LIVM]-[LIVMG]-x-A-x(3)-[FW]",

    "[STV]-x-[LIVMFY]-[STV]-x(2)-G-x-[NKR]-x(4)-[PLIVM]-H-x-R",

    "[FYW]-x-D-x(4)-[FYW]-x(3)-E-x-[STA]-x(3)-N-[STA]",

    "[GTA]-x(2)-[LIVN]-x-[IVMF]-[ST]-E-[LIY]-[DN]-[LIVMF]",

    "[PSA]-[LQ]-x-E-Y-Y-[LIVM](2)-[DE]-x-[FYWHN]",

    "[LIVMF]-x(2)-E-[AG]-[YWG]-[QRFGS]-[SG]-[STAN]-G-x-[SAF]",

    "E-[LIV]-D-[LIVF]-x(0,1)-E-x(2)-[GQ]-[KRNF]-x-[PSTA]",

    "[LIVMKS]-x-[LIVMFYWA](3)-[STAG]-E-[STACVI]-G-[WY]-P-[STN]-x-[SAGQ]",

    "D-[LIVM]-x(3)-[NQ]-[PGE]-x(9,15)-[GR]-x(4)-[LIVMFY](2)-K-x-[ST]-E-[GS]-x(2"
    ")-[FYL]-x-[DN]",

    "[GFY]-[LIVMF]-W-x-D-M-[NSA]-E",

    "G-[AVP]-[DT]-[LIVMTAS]-[CG]-G-[FY]-x(3)-[ST]-x(3)-L-[CL]-x-R-W-x(2)-[LVMI]"
    "-[GSA]-[SA]-F-x-P-[FY]-x-R-[DN]",

    "H-x(2)-P-x(4)-[LIVM]-N-D-P-N-GG-G-P-[LIVM](2)-x(2)-Q-x-E-N-E-[FY]",

    "W-x-F-E-x-W-N-E-P-[DN][STA]-T-R-Y-[FYW]-D-x(5)-[CA]",

    "E-[DNQ]-x(8,17)-Y-x(7)-D-x-[RD]-[GP]-x-[TS]-x(3)-[AIVFLY]-G-x(5,11)-D",

    "[LIVM]-x(3)-E-S-x(3)-[AP]-x(3)-S-x(5)-G-[LIVM]-[LIVMFYW]-x-[LIVMFYW]-x(4)-"
    "[SAG]",

    "D-x-D-[PT]-[GA]-x-D-D-[TAV]-[VI]-A",

    "G-[IV]-[GK]-x-W-[ST]-[AVI]-x-[LIVMFY](2)-x-[LIVM]-x(8)-[MF]-x(2)-[ED]-D",

    "C-x(1,4)-C-[GSANH]-x(2)-[IVM]-x(7,11)-R-[GSANPV]-x(2)-[FYWIL]-C-x(2)-C-Q",

    "[KR]-[LIVA]-[LIVC]-[LIVM]-x-G-[QI]-D-P-Y",

    "[GSAP]-[CS]-N-x-[FYLM]-S-[ST]-[QALKHD]-[DENG]-x-[AV]-[AVTS]-[ADERQ]-[ACS]-"
    "[LIVMCG]",

    "[GA]-[KSR]-x(3)-[LIV]-x-G-[FY]-G-x-[VC]-G-[KRL]-[GA]-x(1,2)-[ASC]",

    "[NS]-T-D-A-E-G-R-[LV]",

    "[HA]-[GSYR]-[LIVMT]-[SG]-H-x-[LIV]-G-[LIVMNKS]-x-[IVEL]-[HNC]-[DEV]",

    "[MFY]-x-G-H-G-[LIVMC]-[GSH]-x(3)-H-x(4)-[LIVM]-x-[HN]-[YWVH]",

    "[DA]-[LIVMY]-x-K-[LIVM]-D-x-G-x-[HQ]-[LIVM]-[DNS]-G-x(3)-[DN]",

    "[LIVM]-E-G-[GA]-x(2)-[LIVMF]-x(6)-L-x(3)-Y-x(2)-G-[LIVM]-R",

    "[LIVM]-x-[GSTA]-E-S-Y-[AG]-[GS]",

    "[LIVF]-x(2)-[LIVSTA]-x-[IVPST]-x-[GSDNQL]-[SAGV]-[SG]-H-x-[IVAQ]-P-x(3)-[P"
    "SA]",

    "[PK]-x-[LIVMFY]-x-[LIVMFY]-x(2)-{E}-x-H-[STAG]-x-E-x-[LIVM]-[STAG]-{L}-x(5"
    ")-[LIVMFYTA]",

    "H-[STAG]-{ADNV}-x(2)-[LIVME]-{SDEP}-x-[LIVMFYW]-P-[FYW]",

    "G-x(2)-[GAP]-x(4)-[LIV]-[ST]-x-E-[KR]-[LIVC]-[AG]-x-[NG]",

    "[LIVF]-x-[GSAVC]-x-[LIVM]-S-x-[STAD]-A-G-x-[FY]-[LIVN]-C-[DNS]",

    "[LIVM]-[ST]-A-[STAG]-H-C",

    "[DNSTAGC]-[GSTAPIMVQH]-x(2)-G-[DE]-S-G-[GS]-[SAPHV]-[LIVMFYWH]-[LIVMFYSTAN"
    "QH]",

    "[STAIV]-{ERDL}-[LIVMF]-[LIVM]-D-[DSTA]-G-[LIVMFC]-x(2,3)-[DNH]",

    "H-G-[STM]-x-[VIC]-[STAGC]-[GS]-x-[LIVMA]-[STAGCLV]-[SAGM]",

    "G-T-S-x-[SA]-x-P-x(2)-[STAVC]-[AG]",

    "[ST]-G-[LIVMFYW](3)-[GN]-x(2)-T-[LIVM]-x-T-x(2)-H",

    "T-x(2)-[GC]-[NQ]-S-G-S-x-[LIVM]-[FY]",

    "D-x(3)-A-x(3)-[LIVMFYW]-x(14)-G-x-S-x-G-G-[LIVMFYW](2)",

    "T-x(2)-[LIVMF]-G-x-A-[SAC]-S-[MSA]-[PAG]-[STA]",

    "R-x(3)-[EAP]-x(3)-[LIVMFYT]-[LM]-[LIVM]-H-Q-P",

    "D-G-[PD]-S-A-[GS]-[LIVMCA]-[TA]-[LIVM]",

    "Q-x(2)-{DE}-[GE]-x-C-[YW]-{DN}-x-[STAGC]-[STAGCV]",

    "[LIVMGSTAN]-{IEVK}-H-[GSACE]-[LIVM]-x-[LIVMAT](2)-G-{SLAG}-[GSADNH]",

    "[FYCH]-[WI]-[LIVT]-x-[KRQAG]-N-[ST]-W-x(3)-[FYW]-G-x(2)-G-[LFYW]-[LIVMFYG]"
    "-x-[LIVMF]",

    "Q-x(3)-N-[SA]-C-G-x(3)-[LIVM](2)-H-[SA]-[LIVM]-[SA]",

    "G-[LIVMFY]-x(1,3)-[AGCY]-[NASMQG]-x-C-[FYWC]-[LIVMFCA]-[NSTAD]-[SACV]-x-[L"
    "IVMSF]-[QF]",

    "Y-x-L-x-[SAG]-[LIVMFT]-x(2)-H-x-G-x(4,5)-G-H-Y",

    "H-x(2,4)-[SC]-x(2)-{A}-x-[LIVMF](2)-[ST]-H-G",

    "K-P-K-[LIVMF]-[LIVMFY]-[LIVMF](2)-[QP]-[AF]-C-[RQG]-[GE]",

    "[LIVMFGAC]-[LIVMTADN]-[LIVFSA]-D-[ST]-G-[STAV]-[STAPDENQ]-{GQ}-[LIVMFSTNC]"
    "-{EGK}-[LIVMFGTA]",

    "W-T-D-x-S-x-H-P-x-TA-G-Y-Q-E-[ST]-R-[FYW]-S-[FYW]-[TN]-A-x-G-G-[ST]-Y",

    "[GSTALIVN]-{PCHR}-{KND}-H-E-[LIVMFYW]-{DEHRKP}-H-x-[LIVMFYWGSPQ]",

    "P-R-C-[GN]-x-P-[DR]-[LIVSAPKQ]",

    "G-x(8,9)-G-x-[STA]-H-[LIVMFY]-[LIVMC]-[DERN]-[HRKL]-[LMFAT]-x-[LFSTH]-x-[G"
    "STAN]-[GST]",

    "[KR]-[GSAT]-x(4)-[FYWLMH]-[DQNGKRH]-x-P-x-[LIVMFY]-x(3)-H-x(2)-[GSA]-H-[LI"
    "VMFA]",

    "[FYNAGS]-x(4)-[STNLV]-x-[FYW]-S-[PDS]-x(0,1)-G-[RKHDS]-x(2)-Q-[LIVA]-[DENR"
    "]-[YH]-[GSAD]-x(2)-[GSAG]",

    "[LIVMACFT]-[GSA]-[LIVMF]-x-[FYLVGAC]-x(2)-[GSACFYI]-[LIVMSTACF]-[LIVMSTAC]"
    "-[LIVMFSTAC]-[GACI]-[GSTACV]-[DES]-x(15,16)-[RK]-x(12,13)-G-x(2)-[GSTA]-D",

    "[GS]-{PR}-S-M-{RS}-[PS]-[AT]-[LF]",

    "K-R-[LIVMSTA](2)-[GA]-x-[PG]-G-[DEQ]-x-[LIVM]-x-[LIVMFY]",

    "[LIVMFYW](2)-x-{T}-G-D-[NH]-x(3)-[SND]-x(2)-[SG]",

    "[LIVM]-x-[GASF]-[GA]-[GAST]-[LIVMTF]-[GAS]-N-[LVMFGI]-[LIVFYGT]-D-[RI]-[LI"
    "VMFAC]",

    "E-x-F-x(2)-G-[SA]-[LIVM]-C-x(4)-G-x-C-x-[LIVM]-S",

    "G-[GAV]-S-[GS](2)-G-x-[GSA]-[GSAVYCT]-x-[LIVMT]-[GSA]-x(6)-[GSAT]-x-[GA]-x"
    "-[DE]-x-[GA]-x-S-[LIVM]-R-x-P-[GSACT]",

    "[LIVM]-x(2)-T-G-G-T-[IV]-[AGS][GA]-x-[LIVM]-x(2)-H-G-T-D-T-[LIVM]",

    "T-[AY]-[GA]-[GATR]-[LIVM]-D-x-H-[LIVM]-H-x(3)-[PA]",

    "[LIVM](2)-[CT]-H-[HNG]-L-x(3)-[LIVM]-x(2)-D-[LIVM]-x-F-[AS]",

    "[LIV]-[GALMY]-[LIVMF]-{Q}-[GSA]-H-x-D-[TV]-[STAV]",

    "[GSTAI]-[SANQCVIT]-D-x-K-[GSACN]-x(1,2)-[LIVMA]-x(2)-[LIVMFY]-x(12,17)-[LI"
    "VM]-x-[LIVMF]-[LIVMSTAGC]-[LIVMFA]-x(2)-[DNGM]-E-E-x(0,1)-[GSTNE]",

    "D-[LIVMFYWSAP]-H-[LIVA]-H-[LIVF]-[RN]-x-[PGANF][GAVS]-[ST]-D-x-A-P-H-x(4)-"
    "K",

    "[FY]-x-[LIVMFY]-x-S-[TV]-x-K-x(3)-{T}-[AGLM]-{D}-x-[LC]",

    "[FY]-E-[LIVM]-G-S-[LIVMG]-[SA]-K[PA]-x-S-[ST]-F-K-[LIV]-[PALV]-x-[STA]-[LI"
    "]",

    "[LI]-x-[STN]-[HN]-x-H-[GSTAD]-D-x(2)-G-[GP]-x(7,8)-[GS]",

    "P-x(3)-[LIVM](2)-x-G-x-C-[LIVMF](2)-K",

    "[LIVMF]-G-G-x-H-x-[LIVMT]-[STAV]-x-[PAG]-x-{A}-x-[GSTA]",

    "[LIVM](2)-{A}-[LIVMFY]-D-[AS]-H-x-D",

    "[ST]-[LIVMFY]-D-[LIVM]-D-x(3)-[PAQ]-x(3)-P-[GSA]-x(7)-G",

    "[SA]-[LIVM]-[NGS]-[STA]-D-D-P",

    "[CH]-[AGV]-E-x(2)-[LIVMFGAT]-[LIVM]-x(17,33)-P-C-x(2,8)-C-x(3)-[LIVM]",

    "[DENGQS]-[LIVMPF]-[LIVM]-x(1,2)-[KRNQELD]-[DENKGS]-[LIVM]-x(3)-[STG]-x-C-["
    "EP]-H-H",

    "[SA]-x-[RK]-x-Q-[LIVMT]-Q-E-[RNAK]-[LIM]-[TSNV]",

    "G-x(2)-[LIVMFY](2)-x-[IF]-x-E-x(2)-[LIVM]-x-G-Y-P",

    "G-[GAQ]-x(2)-C-[WA]-E-[NH]-x(2)-[PST]-[LIVMFYS]-x-[KR]",

    "D-[SGDN]-D-[PE]-[LIVMF]-D-[LIVMGAC][LIV]-x-G-x-V-Q-G-V-x-[FM]-R",

    "G-[FYW]-[AVC]-[KRQAM]-N-x(3)-G-x-V-x(5)-GP-[SAP]-[LIV]-[DNH]-x-{F}-{S}-S-x"
    "-S",

    "[IV]-T-x-E-x(2)-[DE]-x(3)-G-A-x-[SAKR]",

    "[LIVM]-x-[LIVMFYT]-x(3)-[LIVMT]-[DENQK]-x-{G}-[LIVM]-x-[GSA]-G-[LIVMFYGA]-"
    "{S}-[LIVM]-[KRHENQ]-x-[GSEN]",

    "[STAGN]-{E}-[STAG]-[LIVMF]-R-L-{LP}-[SAGV]-N-[LIVMT]",

    "[GSTA]-R-[NQ]-P-x(5)-{A}-x(4)-[LIVMFYW](2)-x(3)-[LIVMFYW]-x-[DE]",

    "D-K-T-G-T-[LIVM]-[TI]",

    "[FYWMV]-x(2)-[FYWM]-x-[FYW]-[DN]-x(6)-[LIVMF]-[GA]-R-T-x(3)-[WRL]",

    "[RK]-x(2)-C-[RKQWI]-x(5)-L-x(2)-C-[SA]-G",

    "[LIVM]-x-G-x(2)-E-G-x-[FYLS]-x-[FW]-[LIVA]-[TAG]-x-N-[HYF]",

    "R-P-L-[IV]-x-[NS]-F-G-S-[CA]-[TS]-C-P-x-F",

    "P-x-[STA]-x-[LIV]-[IVT]-x-[GS]-G-Y-S-[QL]-G",

    "C-x(3)-D-x-[IV]-C-x-G-[GST]-x(2)-[LIVM]-x(2,3)-H",

    "S-[LIVMFYW]-x(5)-K-[LIVMFYWGH]-[LIVMFYWG]-x(3)-[LIVMFYW]-{V}-[CA]-x(2)-[LI"
    "VMFYWQ]-{K}-x-[RK]",

    "[STAV]-x-S-x-H-K-x(2)-[GSTAN](2)-x-[STA]-Q-[STA](2)",

    "[FY]-[PA]-x-K-[SACV]-[NHCLFW]-x(4)-[LIVMF]-[LIVMTA]-x(2)-[LIVMA]-x(3)-[GTE"
    "]",

    "[GSA]-x(2,6)-[LIVMSCP]-x(2)-[LIVMF]-[DNS]-[LIVMCA]-G-G-G-[LIVMFY]-[GSTPCEQ"
    "]",

    "[LIVMFTAR]-[LIVMF]-x-D-x-K-x(2)-D-[IV]-[ADGP]-x-T-[CLIVMNTA]",

    "[VTI]-x-T-A-H-P-T-[EQ]-x(2)-R-[KRHAQ][IVLC]-M-[LIVM]-G-Y-S-D-S-x-K-[DF]-[S"
    "TAG]-G",

    "F-P-S-A-C-G-K-T-NL-I-G-D-D-E-H-x-W-x-[DEPKV]-x-[GV]-[IV]-x-N",

    "[SP]-[IVCLAM]-W-[LIVMFYC]-[LM]-R-[QR]-[AVS]-G-R",

    "[LIMF]-[GAVS]-F-[STAGCV]-[STAGC]-x-[PA]-[FWYV]-T-[LIVM]-x(2)-Y-x(2,3)-[ADE"
    "]-[GK]",

    "[LIVMFY]-[LIVMC]-x-E-[LIVMFYC]-K-[KRSPQV]-[STAHKRYC]-S-P-[STRK]-x(3,4)-[LI"
    "VMFYST]",

    "G-x-[DN]-F-x-K-x-D-E[SA]-[FY]-[LIV]-L-[STN]-E-S-S-[LIVMF]-F-[LIV]E-G-G-E-L"
    "-G-Y",

    "G-x(3)-D-x-P-x(2)-[LIVF]-x(3)-[LIVM]-x-G-D-G-E",

    "[LIVM]-x-[LIVMFYW]-E-G-x-[LS]-L-K-P-[SN]",

    "[FYVMT]-x(1,3)-[LIVMH]-[APNT]-[LIVM]-x(1,2)-[LIVM]-H-x-D-H-[GACH]",

    "[LIVM]-E-x-E-[LIVM]-G-x(2)-[GM]-[GSTA]-x-E",

    "[KR]-[DENQ]-[HN]-x(2)-G-L-N-x-G-x-W-D-Y-[LIVM]-FS-V-A-G-L-G-G-C-P-Y",

    "N-x-[DN]-[IV]-E-G-[IV]-D-x(2)-N-A-C-[FY]-x-G",

    "G-[FYA]-[GA]-H-x-[IV]-x(1,2)-[RKT]-x(2)-D-[PS]-RL-R-[DE]-G-x-Q-x(10)-K",

    "[LIVMFW]-x(2)-H-x-H-[DN]-D-x-G-x-[GAS]-x-[GASLI]G-[LIVM]-x(3)-E-[LIV]-T-[L"
    "F]-R",

    "G-x(3)-[LIVMF]-K-[LF]-F-P-[SA]-x(3)-GK-[KR]-C-G-H-[LMQR]",

    "[YV]-x-D-x(3)-M-S-[GA]-K-K-D-x-[LIVMF]-[LIVMAG]-x-[LIVM]-G-G",

    "T-G-x-P-[LIVM](2)-D-A-x-M-[RA]-x-[LIVM]",

    "[DN]-R-x-R-[LIVM]-[LIVMN]-x-[STA]-[STAQ]-F-[LIVMFA]-x-K-x-L-x(2,3)-W-[KRQ]"
    "",

    "F-x-E-E-x-[LIVM](2)-R-R-E-L-x(2)-N-F",

    "G-x-H-D-x(2)-W-x-E-R-x-[LIVM]-F-G-K-[LIVM]-R-[FY]-M-N",

    "S-E-[HN]-x-[LIVM]-x(4)-[FYH]-x(2)-E-[LIVMGA]-H-[LIVMFA](2)",

    "C-[SA]-D-S-R-[LIVM]-x-[AP]",

    "[EQ]-[YF]-A-[LIVM]-x(2)-[LIVM]-x(4)-[LIVMF](3)-x-G-H-x(2)-C-G",

    "G-S-x(2)-M-x-{RS}-K-x-N",

    "[LIVM]-x(2)-[GSACIVM]-x-[LIV]-[GTIV]-[STP]-C-x(0,1)-T-N-[GSTANI]-x(4)-[LIV"
    "MA]",

    "G-x(2)-[LIVWPQT]-x(3)-[GACST]-C-[GSTAM]-[LIMPTA]-C-[LIMV]-[GA]",

    "C-D-K-x(2)-P-[GA]-x(3)-[GA]",

    "[SGALC]-[LIMF]-[LIVMF]-T-D-[GA]-R-[LIVMFY]-S-[GA]-[GAV]-[ST]",

    "D-[LIVM]-[DE]-[LIVMN]-x(18,20)-[LIVM](2)-x-[SC]-[NHY]-H-[DN]",

    "[LIVM]-[NQHS]-G-P-N-[LVI]-x(2)-[LT]-G-x-R-[QED]-x(3)-[FY]-G",

    "[LIVT]-[LIVP]-[LIV]-[KQ]-x-[ND]-Q-[INV]-G-[ST]-[LIV]-[STL]-[DERKAQG]-[STA]"
    "",

    "[DESH]-x(4,5)-[STVG]-{EVKD}-[AS]-[FYI]-K-[DLIFSA]-[RLVMF]-[GA]-[LIVMGA]",

    "[LIVM]-[STAG]-x-[LIVM]-[DENQRHSTA]-G-x(3)-[AG](3)-x(4)-[LIVMST]-x-[CSTA]-["
    "DQHP]-[LIVMFYA]",

    "[LIVMY]-[DE]-x-H-H-x(2)-E-x(2)-[GCA]-[LIVM]-[STAVCL]-[LIVMF]",

    "[GW]-x-[DNIE]-x-H-H-x(2)-E-[STAGC]-x-[VMFYH]-K",

    "[LIVM]-E-[LIVM]-G-x(2)-[FYCHT]-[STP]-[DEK]-[PA]-[LIVMYG]-[SGALIMY]-[DE]-[G"
    "N]",

    "[LIVMYAHQ]-x-[HPYNVF]-x-G-[STA]-H-K-x-N-x(2)-[LIVM]-x-[QEH]",

    "G-x-D-x-[LIVM](2)-[IV]-K-P-[GSA]-x(2)-YR-D-H-x-D-x-[GS]-[GS]-x(2)-S-P-x-R-"
    "E-T",

    "[FY]-x-[LIVM]-x(2)-[LIVM]-x(5)-[DN]-x(5)-T-R-F-[LIVMW]-x-[LIVM]",

    "[LIVM]-[ST]-[KR]-[LIVMF]-E-[ST]-R-P",

    "[GSA]-[LIVM]-[LIVMFY]-x(2)-G-[ST]-[TG]-G-E-[GASNF]-x(6)-[EQ]",

    "Y-[DNSAH]-[LIVMFAN]-P-x(2)-[STAV]-x(2,3)-[LIVMFT]-x(13,14)-[LIVMCF]-x-[SGA"
    "]-[LIVMFNS]-K-[DEQAFYH]-[STACI]",

    "[LIVCA]-[NHYTQ]-R-[LI]-[DG]-x(2)-T-[STAC]-G-[LIVAGC]-[LIVMF](2)-[LIVMFGCA]"
    "-[SGTACV]",

    "G-R-L-D-x(2)-[STA]-x-G-[LIVFA]-[LIVMF](3)-[ST]-[DNST]",

    "K-x-E-x(3)-[PA]-[STAGC]-x-S-[IVAPM]-K-x-R-x-[STAG]-x(2)-[LIVM]",

    "[SP]-G-P-[LIVMW]-G-G-D-x(0,1)-Q",

    "[GS]-[STG]-[LIVM]-[STG]-[SAC]-S-G-[DH]-L-x-P-L-[SA]-x(2,3)-[SAGVT]",

    "E-[KR]-x-[LIVMFAT]-x(3)-[LIVMFAC]-x-[GSALV]-[GSANHD]-C-x-[IVTACS]-[PLA]-[L"
    "IVMF]-[GSA]",

    "[DQ]-[LIVMFY]-x(3)-[STAGCN]-[STAGCIL]-T-K-[FYWQI]-[LIVMF]-x-G-[HQD]-[SGNH]"
    "",

    "[HQ]-[IVT]-x-[LIVFY]-x-[IV]-x(4)-{E}-[STA]-x(2)-F-[YM]-x(2,3)-[LMF]-G-[LMF"
    "]",

    "G-[NTKQ]-x(0,5)-[GA]-[LVFY]-[GH]-H-[IVF]-[CGA]-x-[STAGLE]-x(2)-[DNC]",

    "H-N-x(2)-N-E-x(2)-W-[NQKRS]-x(4)-W-EP-F-D-R-H-D-WE-Y-F-G-[SA](2)-L-W-x-L-Y"
    "-K",

    "Y-R-N-x-W-[NS]-E-[LIVM]-R-T-L-H-F-x-G",

    "[GD]-[VI]-[LIVM]-x(0,1)-[GS]-x(5)-[FY]-x-[LIVM]-[FYWL]-[GS]-[DNTHKWE]-[DNT"
    "AS]-[IV]-[DNTAY]-x(5)-[DEC]",

    "G-[DES]-S-H-[GC]-x(2)-[LIVM]-[GTIV]-x-[LIVT]-[LIV]-[DEST]-[GH]-x-[PV]",

    "[GE]-x(2)-S-[AG]-R-x-[ST]-x(3)-[VT]-x(2)-[GA]-[STAVY]-[LIVMF]",

    "R-[SHF]-D-[PSV]-[CSAVT]-x(4)-[SGAIVM]-x-[IVGSTAPM]-[LIVM]-x-E-[STAHNCG]-[L"
    "IVMA]",

    "C-N-N-x(2)-G-H-G-H-N-YD-H-K-N-L-D-x-D",

    "[LIVMF]-[LIVMFC]-x-[ST]-x-H-[GS]-[LIVM]-P-x(4,5)-[DENQKRLHAFSTI]-x-[GN]-[D"
    "PC]-x(1,4)-[YA]",

    "[SACVLG]-[AIPTV]-x(0,1)-K-[ADGS]-[DEN]-[GA]-Y-G-[HACILN]-[GD]",

    "N-x-D-G-S-x(4)-C-G-N-[GA]-x-R",

    "[IVA]-[LIVM]-x-C-x(0,1)-N-[ST]-[MSA]-[STH]-[LIVFYSTANK]",

    "[LIVM](2)-x-[AG]-C-T-[DEH]-[LIVMFY]-[PNGRS]-x-[LIVM]",

    "[AT]-x-[SAGCN]-[SAGC]-[LIVM]-[DEQ]-x-A-[LA]-x-[DE]-[LIA]-x-[GA]-[KRQ]-x(4)"
    "-[PSA]-[LIV]-x(2)-L-[LIVMF]-G",

    "[LIVF]-x(2)-D-x-[NH]-x(7)-[ACL]-x(6)-[LIVMF]-x(7)-[LIVM]-E-[DENQ]-P",

    "[LIVMF]-H-[LIVMFY]-D-[LIVM]-x-D-x(1,2)-[FY]-[LIVM]-x-N-x-[STAV]",

    "[LIVMA]-x-[LIVM]-M-[ST]-[VS]-x-P-x(3)-[GN]-Q-x(0,1)-[FMK]-x(6)-[NKR]-[LIVM"
    "C]",

    "[NS]-x-T-N-H-x-Y-[FW]-N-[LI]",

    "[FY]-x(2)-[STCNLVA]-x-[FV]-H-[RH]-[LIVMNS]-[LIVM]-x(2)-F-[LIVM]-x-Q-[AGFT]"
    "-G",

    "F-[GSADEI]-x-[LVAQ]-A-x(3)-[ST]-x(3,4)-[STQ]-x(3,5)-[GER]-G-x-[LIVM]-[GS]",

    "[AVG]-[YLV]-E-P-[LIVMEPKST]-[WYEAS]-[SAL]-[IV]-[GN]-[TEKDVS]-[GKNAD]",

    "[LI]-E-P-K-P-x(2)-P[FL]-H-D-{K}-D-[LIV]-x-[PD]-x-[GDE]Y-x-D-x-N-H-K-P-E",

    "H-A-Y-[LIVM]-x-G-x(2)-[LIVM]-E-x-M-A-x-S-D-N-x-[LIVM]-R-A-G-x-T-P-K",

    "[DENSA]-x-[LIVM]-[GP]-G-R-[FY]-[ST]-[LIVMFSTAP]-x-[GSTA]-[PSTACM]-[LIVMSA]"
    "-[GSAN]",

    "[GSA]-x-[LIVMCAYQS]-[LIVMFYWN]-x(4)-[FY]-[DNTH]-Q-x-[GA]-[IV]-[EQST]-x(2)-"
    "K",

    "[LIVM]-x(3)-[GNH]-x(0,1)-[LITCRV]-x-[LIVWF]-x-[LIVMF]-x-[GS]-[LIVM]-G-x-[D"
    "ENV]-G-[HN]",

    "[LIVM]-x-R-H-G-[EQ]-x-{Y}-x-N[GSA]-[LIVMF]-x-[LIVM]-[ST]-[PGA]-S-H-[NIC]-P"
    "",

    "R-I-A-R-N-[TQ]-x(2)-[LIVMFY](2)-x-[EQH]-E-x(4)-[KRN]-x(2)-D-P-x-[GSA]-G-S",

    "[DE]-G-S-W-x-[GE]-x-W-[GA]-[LIVM]-x-[FY]-x-Y-[GA]",

    "[DEN]-x(6)-[GS]-[IT]-S-K-x(2)-Y-[LIVM]-x(3)-[LIVM]",

    "[EQ]-x-L-Y-[DEQSTLM]-x(3,12)-[LIVST]-[ST]-Y-x-R-[ST]-[DEQSN]",

    "[LIVMA]-{R}-E-G-[DN]-S-A-{F}-[STAG]",

    "P-x(0,2)-[GSTAN]-[DENQGAPK]-x-[LIVMFP]-[HT]-[LIVMYAC]-G-[HNTG]-[LIVMFYSTAG"
    "PC]",

    "[QYR]-[GH]-[DNEAR]-x-[LIV]-[KR]-x(2)-K-x(2)-[KRNG]-[AS]-x(4)-[LIV]-[DENK]-"
    "x(2)-[IV]-x(2)-L-x(3)-K",

    "S-[KR]-S-G-[GT]-[LIVM]-[GST]-x-[EQ]-x(8,10)-G-x(4)-[LIVM]-[GA]-[LIVM]-G-G-"
    "D",

    "G-x(2)-A-x(4,7)-[RQT]-[LIVMF]-G-H-[AS]-[GH]",

    "G-x-[IVT]-x(2)-[LIVMF]-x-[NA]-G-[GA]-G-[LMAI]-[STAV]-x(4)-D-x-[LIVM]-x(3,4"
    ")-G-[GREAK]",

    "[FYWL]-D-G-S-S-x(6,8)-[DENQSTAK]-[SA]-[DE]-x(2)-[LIVMFY]",

    "K-P-[LIVMFYA]-x(3,5)-[NPAT]-[GA]-[GSTAN]-[GA]-x-H-x(3)-S",

    "K-[LIVM]-x(5)-[LIVMA]-D-[RK]-[DN]-[LI]-Y",

    "H-[GN]-x(2)-[GC]-E-[DNT]-G-x-[LIVMAFT]-[QSAPH]-[GSA]",

    "[LIVAMSFT]-x(3)-[GAHDVSI]-x-[GSAIVCT]-R-[LIVMCAFST]-[DE]-[LIVMFAYGT]-[LIVM"
    "FAR]-x(7,12)-[LIVWCAF]-x-[EK]-[LIVAPMT]-N-[STPA]-x-P-[GA]",

    "[LIVMRPA]-[LIVFY]-[PLNRKG]-[LIVMF]-E-x-[IV]-[LVCATI]-R-x(3)-[TAEYSI]-G-[ST"
    "]",

    "[LI]-[IVCAP]-D-x-K-[LIFY]-E-[FI]-G",

    "[LIVMFY]-x-[LIVM]-[STAG]-G-T-[NK]-G-K-x-[STG]-x(4)-{A}-x-{EAD}-[LIVM](2)-x"
    "(3,4)-[GSKQT]",

    "[LIVMFY](2)-[EK]-x-G-[LIVM]-[GA]-G-x(2)-D-x-[GST]-x-[LIVM](2)",

    "K-[AI]-[CL]-S-G-K-[FI]-x-[PQ]P-[LIVMG]-C-T-[LIVM]-[KRHA]-x-[FTNM]-P",

    "[FYWLSP]-H-[PC]-[NH]-[LIV]-x(3,4)-G-x-[LIV]-C-[LIV]-x-[LIV]",

    "G-[LIVM]-K-G-G-A-A-G-G-G-YV-[AS]-T-[IV]-R-A-[LI]-K-x-[HN]-G-G",

    "[QGF]-[WLCF]-G-D-E-[GA]-K-[GA]G-I-[GR]-P-x-Y-x(2)-K-x(2)-R",

    "[ASL]-[FY]-S-G-G-[LV]-D-T-[ST]G-x-T-x-[KRM]-G-N-D-x(2)-R-F",

    "R-[LF]-G-D-P-E-x-[EQIM]",

    "[FYV]-[PS]-[LIVMC]-[LIVMA]-[LIVM]-[KR]-[PSA]-[STA]-x(3)-[SG]-G-x-[AG]",

    "[LIVMF]-[LIMN]-E-[LIVMCA]-N-[PATLIVM]-[KR]-[LIVMSTAC]",

    "[EDQH]-x-K-{VEDI}-[DN]-G-{GLYN}-R-[GACIVM]",

    "E-G-[LIVMA]-[LIVM](2)-[KR]-x(5,8)-[YW]-[QNEK]-x(2,6)-[KRH]-x(3,5)-K-[LIVMF"
    "Y]-K",

    "K-[LIVMF]-D-G-[LIVMAS]-[SAG]-x(4)-Y-x(2)-[GRD]-x-[LF]-x(4)-[ST]-R-G-[DN]-G"
    "-x(2)-G-[DE]-[DENL]",

    "[IV]-G-[KR]-[ST]-G-x-[LIVM]-[STNK]-x-[VTLYF]-x(2)-[LVMF]-x-[PS]-[IV]",

    "[RH]-G-x(2)-P-x-G(3)-x-[LIV]R-G-G-x(2)-T-[FYWCAH]-H-x(2)-[GH]-Q-x-[LIVMT]-"
    "x-Y",

    "[LMFYCVI]-[DN]-R-x(3)-[PGA]-L-[LIVMCA]-E-[LIVMT]-x-[STL]-x-[PA]",

    "Y-[LIVAC]-R-[VA]-S-[ST]-x(2)-Q",

    "G-[DE]-x(2)-[LIVM]-{E}-x-{V}-[LIVM]-[DT]-R-[LIVM]-[GSA]",

    "D-x(3)-G-[LIVMF]-x(6)-[STAV]-[LIVMFYW]-[PT]-x-[STAV]-x(2)-[QR]-x-C-x(2)-H",

    "R-G-x(2)-E-N-x-N-G-[LIVM](2)-R-[QE]-[LIVMFY](2)-P-K",

    "[LMFYA]-R-x(3)-F-x(2)-[KRQ]-x(2)-W-x-[LIVM]-x(6,9)-E-x-D-x-[FY]-D",

    "[LIVMF]-[GSA]-x(5)-P-x(4)-[LIVMFYW]-x-[LIVMF]-x-G-D-[GSA]-[GSAC]",

    "[GDN]-[DEQTR]-x-[LIVMFY]-x(2)-[LIVM]-x-[AIV]-M-K-[LVMAT]-x(3)-[LIVM]-x-[SA"
    "V]",

    "[GDN]-x(2)-[LIVF]-x(3)-{VH}-x-[LIVMFCA]-x(2)-[LIVMFA]-{LDFY}-x(2)-K-[GSTAI"
    "VW]-[STAIVQDN]-x(2)-[LIVMFS]-x(5)-[GCN]-x-[LIVMFY]",

    "[LIVMFY]-{E}-{VES}-[STG]-[STAG]-G-[ST]-[STEI]-[SG]-x-[PASLIVM]-[KR]",

    "[LIVMCA]-[LIVM](2)-[LITF]-[LITN]-G-G-T-G-x(4)-D",

    "S-x-[GS]-x(2)-D-x(5)-[LIVW]-x(10,12)-[LIV]-x(2)-[KR]-P-G-[KRL]-P-x(2)-[LIV"
    "MF]-[GA]",

    "[LIV]-x(3)-C-[NDP]-[LIVMF]-[DNQRS]-C-x-[FYM]-C",

    "[GVPS]-x-[GKS]-x-[KRS]-x(3)-[FL]-x(2)-G-x(0,1)-C-x(3)-C-x(2)-C-x-[NLF]",

    "S-x-D-L-P-F-[AS]-x(2)-[KRQ]-[FWI]-C",

    "S-[DN]-[GA]-D-[LIVA]-[LIVAG]-x-H-[STAC]-x(2)-[DNT]-[SAG]-x(2)-[SGA]",

    "[LIV]-x-F-[LIV]-H-[VYWT]-[FY]-H-H",

    "[LV]-P-[VI]-[VTPI]-[NQLHT]-[FL]-[ATVS]-[AS]-G-G-[LIV]-[AT]-T-P-[AQS]-D-[AG"
    "VS]-[AS]-[LM]",

    "[GA]-L-I-[LIV]-P-G-G-E-S-T-[STA][FY]-[LIVMK]-{I}-{Q}-H-P-[GA]-G",

    "[LIV]-x-[ST]-[LIVF]-R-[FYW]-x(2)-[IVL]-H-[STGAV]-[LIV]-[STGA]-[IV]-P",

    "R-[LIVMFYW]-x-H-W-[LIVM]-x(2)-[LIVMF]-[STAC]-[LIVM]-x(2)-L-x-[LIVM]-T-G",

    "[RH]-[STA]-[LIVMFYW]-H-[RH]-[LIVM]-x(2)-W-x-[LIVMF]-x(2)-F-x(3)-H",

    "R-P-[LIVMT]-x(3)-[LIVM]-x(6)-[LIVMWPK]-x(4)-S-x(2)-H-R-x-[ST]",

    "H-x(3)-[GA]-[LIVMT]-R-[HF]-[LIVMF]-x-[FYWM]-D-x-[GVA]",

    "[LIVMF]-[LIVMSTA]-x-[LIVMFYC]-[FYWSTHE]-x(2)-[FYWGTN]-C-[GATPLVE]-[PHYWSTA"
    "]-C-{I}-x-{A}-x(3)-[LIVMFYWT]",

    "[LIVMD]-[FYSA]-x(4)-C-[PV]-[FYWH]-C-x(2)-[TAV]-x(2,3)-[LIV]",

    "[GA]-x(0,2)-[YSA]-x(0,1)-[VFY]-{SEDT}-C-x(1,2)-[PG]-x(0,1)-H-x(2,4)-[MQ]",

    "C-{C}-{C}-[GA]-{C}-C-[GAST]-{CPDEKRHFYW}-C",

    "C-x(2)-[STAQ]-x-[STAMV]-C-[STA]-T-C-[HR]C-x-{P}-C-x(2)-C-{CP}-x(2)-C-[PEG]"
    "",

    "C-x(6,9)-[LIVM]-x(3)-G-[YW]-C-x(2)-[FYW]C-[TK]-H-[LV]-G-C-[LIVSTP]",

    "C-P-C-H-{H}-[GSA][LIV]-[LIVFY]-[FY]-x-[ST]-{V}-x-[AGC]-x-T-{P}-x(2)-A-x(2)"
    "-[LIV]",

    "[LIVM]-x-{G}-{R}-W-x-C-P-x-C-[AGD]",

    "[LI]-Y-[LIVM]-[AT]-x-[GA]-[IV]-[SD]-G-x-[IV]-Q-[HP]-x(2)-G-x(6)-[IV]-x-A-["
    "IV]-N",

    "[IVAG]-x-[KR]-x(2)-[DE]-[GDE](2)-x(1,2)-[EQHF]-x-[LIV]-x(4)-P-x-[LIVM](2)-"
    "[TACS]",

    "C-x-C-[GSTAP]-x(2)-C-x-C-x(2)-C-x-C-x(2)-C-x-K",

    "E-x-[KR]-E-x(2)-E-[KR]-[LF]-[LIVMA]-x(2)-Q-N-x-R-x-G-R",

    "D-x(2)-[LIVMF]-[STAC]-[DH]-F-[LI]-[EN]-x(2)-[FY]-L-x(6)-[LIVM]-[KN]",

    "M-x-G-x(3)-[IV]-[LIV]-x(2)-[LM]-x(3)-L-x(3)-L",

    "[IV]-[LIVF]-[NS]-[KRTAVS]-[QH]-x-[PAVS]-x(2)-[EQ]-[LIVM]-W-x-[STA]-[STAD]",

    "Y-x(0,1)-[VAS]-V-[IVAC]-[IVA]-[IVA]-[RKH]-[RKS]-[GDENSA]",

    "[YI]-x-G-A-[FLI]-[KRHNQS]-C-L-x(3,4)-G-[DENQ]-V-[GAT]-[FYW]",

    "[DENQ]-[YF]-x-[LY]-L-C-x-[DN]-x(5,8)-[LIV]-x(4,5)-C-x(2)-A-x(4)-[HQR]-x-[L"
    "IVMFYW]-[LIVM]",

    "F-[LF]-x(4)-[GE]-G-[PAT]-x(2)-[YW]-x-[GSE]-[KRQA]-x(1,5)-[LIVM]-x(3)-H",

    "[SN]-P-x-[LV]-x(2)-H-A-x(3)-F",

    "H-F-x(2)-[EQ]-[ENQ]-x(2)-[LMF]-x(4,7)-[FY]-x(5,6)-H-x(3)-[HR]",

    "Y-[FYW]-x-E-D-[LIVM]-x(2)-N-x(6)-H-x(3)-PT-x(2)-R-D-P-x-[FY]-[FYW]",

    "[LIVNS]-x(2)-[LIVMFA]-x-C-x-[STAGCDNH]-C-x(3)-[LIVFG]-x(3)-[LIV]-x(9,11)-["
    "IVA]-x-[LVFYS]",

    "[LIVMFYC]-[SA]-[SAPGLVFYKQH]-G-[DENQMW]-[KRQASPCLIMFW]-[KRNQSTAVM]-[KRACLV"
    "M]-[LIVMFYPAN]-{PHY}-[LIVMFW]-[SAGCLIVP]-{FYWHP}-{KRHP}-[LIVMFYWSTA]",

    "[GAP]-[LIVMFA]-[STAVDN]-x-{H}-x(2)-[GSAV]-[LIVMFY](2)-Y-[ND]-x(3)-[LIVMF]-"
    "x-[KNDE]",

    "G-[FYIL]-[DE]-[LIVMT]-[DE]-[LIVMF]-x(3)-[LIVMA]-[VAGC]-{TPRG}-{GL}-[LIVMAG"
    "N]",

    "[AG]-x(6,7)-[DNEG]-x(2)-[STAIVE]-[LIVMFYWA]-x-[LIVMFY]-x-[LIVM]-[KR]-[KRHD"
    "E]-[GDN]-[LIVMA]-[KNGSP]-[FW]",

    "[FY]-x(6)-C-C-x(2)-{C}-x(4)-C-[LFY]-x(6)-[LIVMFYW]",

    "[KH]-[IV]-L-[DN]-x(3)-G-x-P-[AG]-x(2)-[LIVM]-x-[IV]",

    "[YWF]-[TH]-[IVT]-[AP]-x(2)-[LIVM]-[STA]-[PQ]-[FYWG]-[GS]-[FY]-[QST]",

    "[DENY]-x(2)-[KRI]-[STA]-x(2)-V-G-x-[DN]-x-[FW]-T-[KR]",

    "[SN]-V-D-T-[GAME]-A-[LIVM]-A-x-[LM]-A-[LIVMF]-[ST]-C",

    "[DENG]-{A}-[DENQGSTARK]-x(0,2)-[DENQARK]-[LIVFY]-{CP}-G-{C}-W-[FYWLRH]-{D}"
    "-[LIVMTA]",

    "[GSAIVK]-{FE}-[FYW]-x-[LIVMF]-x(4)-[NHG]-[FY]-[DE]-x-[LIVMFY]-[LIVM]-x-{G}"
    "-[LIVMAKR]",

    "[PTLV]-[GSTA]-x-[DENQA]-x-[LMFK]-x(2)-[LIVMFY]-[YV]-[GSA]-x-[FYH]-K-Q-[GSA"
    "V]-[STL]-x-G",

    "[PA]-[GA]-[LIVMC]-x(2)-R-[IV]-[ST]-x(3)-L-x(5)-[EQAV]-x(4)-[LIVM]-[EQK]-x("
    "8)-P",

    "[FYL]-x-[LVM]-[LIVF]-x-[TIVM]-[DC]-P-D-x-P-[SNG]-x(10)-H",

    "[LIVM]-[PA]-x(2)-C-x(1,2)-[LIVM]-x(1,2)-[LIVMST]-x-[LIVMFY]-x(1,2)-[LIVMF]"
    "-[STRD]-x(3)-[DN]-C-x(2)-[LIVM]",

    "[GA]-x(3)-I-C-P-x-[LIVMF]-x(3)-[LIVM]-[DE]-x-[LIVMF](2)",

    "[DEQ]-x(4)-[SN]-x(5)-[DEQ]-x-I-x(2)-S-[PSE]-[LS]-C",

    "[LIVMSTAG]-[LIVMFSAG]-{SH}-{RDE}-[LIVMSA]-[DE]-x-[LIVMFYWA]-G-R-[RK]-x(4,6"
    ")-[GSTA]",

    "[LIVMF]-x-G-[LIVMFA]-x(2)-G-x(8)-[LIFY]-x(2)-[EQ]-x(6)-[RK]",

    "G-[LIVM](2)-x-D-[RK]-L-G-L-[RK](2)-x-[LIVM](2)-W",

    "P-x-[LIVMF](2)-N-R-[LIVM]-G-x-K-N-[STA]-[LIVM](3)",

    "[GA]-[GAS]-[LIVMFYWA]-[LIVM]-[GAS]-D-x-[LIVMFYWT]-[LIVMFYW]-G-x(3)-[TAV]-["
    "IV]-x(3)-[GSTAV]-x-[LIVMF]-x(3)-[GA]",

    "[FYT]-x(2)-[LMFY]-[FYV]-[LIVMFYWA]-x-[IVG]-N-[LIVMAG]-G-[GSA]-[LIMF]",

    "Y-x(2)-[EQTFP]-x-C-x(2)-[GSTDNL]-C-x-[QTAV]-x(2)-[LIVMT]-[LIVMS]-x(2)-C-x-"
    "C",

    "G-G-x-[GA](2)-[LIVM]-F-W-M-W-[LIVM]-x-[STAV]-[LIVMFA](2)-G",

    "P-x(0,1)-[GP]-[DES]-x-[LIVMF](2)-x-[LIVMA]-[LIVM]-[KREQS]-[LIVMG]-[LIVM]-["
    "LIVMF]-x-[PS]",

    "P-x-G-x-[STA]-x-[NT]-[LIVMCP]-D-[GA]-[STANQF]-x-[LIVM]-[FY]-x(2)-[LIVM]-x("
    "2)-[LIVM]-[FY]-[LIV]-[SA]-[QH]",

    "[DG]-x(3)-G-x(3)-[DN]-x(6,8)-[GA]-[KRHQ]-[FSAR]-[KRL]-[PT]-[FYW]-[LIVMWQ]-"
    "[LIV]-x(0,1)-[GAFV]-[GSTA]",

    "W-[RK]-F-[GPA]-[YF]-x(4)-[NYHS]-G-G-[GCA]-x-[FY]",

    "[YF]-[LIVMFY]-x(2)-[SC]-[LIVMFY]-[STQV]-x(2)-[LVI]-P-W-x(2)-C-x(3,4)-[NWDS"
    "]-[GSTERHAK]",

    "[GS]-x(2)-[LIY]-x(3)-[LIVMFYWSTAG](7)-x(3)-[LIY]-[STAV]-x(2)-G-G-[LMF]-x-["
    "SAP]",

    "[GAST]-[LIVM]-x(3)-[KR]-x(4)-G-A-x(2)-[GAS]-[LIVMGS]-[LIVMW]-[LIVMGAT]-G-x"
    "-[LIVMGA]",

    "[STACPI]-S-x(2)-[FY]-x(2)-P-[LIVM]-[GSA]-x(3)-N-x-[LIVM]-V",

    "[QEK]-[RF]-G-x(3)-[GSA]-[LIVF]-[WL]-[NS]-x-[SA]-[HM]-N-[LIV]-[GA]-G",

    "D-[FYWS]-[AS]-G-[GSC]-x(2)-[IV]-x(3)-[SAG](2)-x(2)-[SAG]-[LIVMF]-x(3)-[LIV"
    "MFYWA](2)-x-[GK]-x-R",

    "[GSDNA]-W-T-[LIVM]-x-[FY]-W-x-W-WA-[LMF]-x-[GAT]-T-[LIVMF]-x-G-x-[LIVMF]-x"
    "(7)-P",

    "[KR]-G-N-[LIV](2)-D-[LIVM]-A-[LIVM]-[GA]-[LIVM](3)-G",

    "[LIVMA]-[LIVMY]-x-G-[GSTA]-[DES]-L-[FI]-[TN]-[GS]",

    "[GA]-x(2)-[CA]-N-[LIVMFYW](2)-V-C-[LV]-AK-x-[NQEK]-[GT]-G-[DQ]-x-[LIVM]-x("
    "3)-Q-S",

    "N-P-K-[ST]-S-G-x-A-R",

    "[PAV]-x-[FYLCVI]-[GS]-L-Y-[STAG]-[STAGL]-x(4)-[LIVFYA]-[LIVMST]-[YI]-x(3)-"
    "[GA]-[GST]-[SRV]-[KRNP]",

    "[STAGC]-G-[PAG]-x(2,3)-[LIVMFYWA](2)-x-[LIVMFYW]-x-[LIVMFWSTAGC](2)-[STAGC"
    "]-x(3)-[LIVMFYWT]-x-[LIVMST]-x(3)-[LIVMCTA]-[GA]-E-x(5)-[PSAL]",

    "I-G-[GA]-G-M-[LF]-[SA]-x-P-x(3)-[SA]-G-x(2)-F",

    "[LIVM]-P-x-[PASIF]-V-[LIVMG]-[GF]-[GA]-x(4)-[LIVM]-[FY]-[GSA]-x-[LIVM]-x(3"
    ")-[GA]",

    "F-G-G-[LIVM](2)-[KR]-D-[LIVM]-[RK]-R-R-Y[FI]-L-I-S-L-I-F-I-Y-E-T-F-x-K-L",

    "[HNQA]-{D}-N-P-[STA]-[LIVMF]-[ST]-[LIVMF]-[GSTAFY]",

    "[LIVMFY]-x(2)-G-x(2)-Y-x-F-x-K-x(2)-[SN]-[STAV]-[LIVMFYW]-V",

    "[LIVMA]-x-[GT]-x-[TA]-[DAN]-x(2,3)-[DG]-[GSTPNKQ]-x(2)-[LFYDEPAVI]-[NQS]-x"
    "(2)-[LI]-[SG]-[QEA]-[KRQENAD]-R-A-x(2)-[LVAIT]-x(3)-[LIVMF]-x(4,5)-[LIVMF]"
    "-x(4)-[LIVM]-x(3)-[SGW]-x-G",

    "[YH]-x(2)-D-[SPCAD]-x-[STA]-x(3)-[TAG]-[KR]-[LIVMF]-[DNSTA]-[DNS]-x(4)-[GS"
    "TAN]-[LIVMA]-x-[LIVMY]",

    "[GP]-C-[GSET]-[CE]-[CA]-x(2)-C-[ALP]-x(6)-CN-P-[AV]-P-[LF]-G-L-x-[GSA]-F",

    "G-Q-D-Q-T-K-Q-Q-I[FY]-[LIV]-[GV]-[DE]-E-[ARV]-[QLAH]-x(1,2)-[RKQ](2)-[GD]",

    "W-[IVC]-[STAK]-[RK]-x-[DE]-Y-[DNE]-[DE]",

    "[LM]-[LIVMA]-T-E-[GAPQ]-x-[LIVMFYWHQPK]-[NS]-[PSTAQ]-x(2)-N-[KR]",

    "[TG]-[STV]-x(8)-[LIVMF]-x(2)-R-x(3)-[DEQNH]-x(2)-{S}-x(4)-[IFY]-x(7)-[LIVM"
    "F]-x(3)-[LIVMF]-x(5)-{I}-x(5)-[LIVMFA]-x(2)-[LIVMF]",

    "F-E-D-[LV]-I-A-[DE]-[PA]F-L-A-[QH]-[QE]-E-S",

    "[KR]-[DS]-x-[SE]-[KR]-[LIVMF]-[KR]-x-[LIVM]-[LIVMY]-[LIVM]-x-L-[KA]",

    "C-K-P-C-L-K-x-T-CC-L-[RK]-M-[RK]-x-[EQ]-C-[ED]-K-C",

    "C-[DNH]-[TL]-x-Q-P-G-C-x(2)-[VAI]-C-[FY]-D",

    "C-x(3,4)-P-C-x(3)-[LIVMT]-[DENT]-C-[FYN]-[LIVMQ]-[SA]-[KRH]-P",

    "L-P-[RK]-[GD]-[STNK]-[GN]-[LIVM]-[VI]-T-R",

    "H-x-[IV]-x-G-[KR]-x-F-[GA]-S-x-V-[ST]-[HY]-E",

    "N-[ST]-D-x-[QS]-x-L-x(16,18)-G-x-G-[ATVS]-G-[GSAN]-x-P-x(2)-G",

    "[DNHKR]-[LIVMF]-x-[LIVMF](2)-[VSTAC]-[STAC]-G-x-G-[GKN]-G-T-G-[ST]-G-[GSAR"
    "C]-[STA]-P-[LIVMFT]-[LIVMF]-[SGAV]",

    "[GN]-[DNQPSA]-x-C-[GSTANK]-[GSTADNQ]-[STNQI]-[PTIV]-x-C-C-[DENQKPST]",

    "[IV]-{K}-[TACI]-Y-[RKH]-{E}-[LM]-L-[DE]M-S-[QH]-Q-x-T-[LV]-P-V-T-[LV]",

    "[GSAT]-[KRHPSTQVME]-[LIVMFY]-x-[LIVMF]-[IVC]-[DN]-[LS]-[AH]-G-[SAN]-E",

    "[DEQR]-A-L-x(3)-[GEQ]-x(3)-G-x-[DNS]-x-P-x-V-A-x(3)-N-x-L-[AS]-x(5)-[QR]-x"
    "-[KR]-[FY]-x(2)-[AV]-x(4)-[HKNQ]",

    "V-V-H-F-F-K-NS-[KR]-S-x-K-[AG]-x-[SA]-E-K-K-[STA]-KG-[MV]-A-L-F-C-G-C-G-H",

    "C-x-[ST]-x-[DE]-x(3)-[ST]-[FY]-x-L-[FY]-I-x(4)-G-AM-L-C-C-[LIVM]-R-R",

    "S-F-R-G-H-I-x-R-K-K-[LIVM][KQ]-x-[TA]-x(2)-[GA]-S-S-E-E-K",

    "D-[GS]-V-P-F-[ST]-C-C-N-P-x-S-P-R-P-C",

    "x(0,1)-[STA]-x(0,1)-W-[DENQH]-x-[YI]-x-[DEQ]I-P-C-C-P-VL-R-R-R-L-S-D-S",

    "G-H-A-H-[SA]-G-M-G-K-[IV]-K",

    "N-[LIVMF]-[DENST]-[KLN]-[VAI]-x-[DEQ]-R-x(2)-[KRN]-[LIVM]-[STDEA]-x-[LIVM]"
    "-x-[DEQG]-[KR]-[TAS]-[DEA]",

    "L-S-V-[DE]-C-x-N-K-TL-K-[EAD]-A-E-x-R-A-[ET][SAG]-G-G-T-G-[SA]-GM-R-[DE]-["
    "IL]",

    "G-S-x(2)-N-x(2)-H-x-[PA]-[AG]-G(2)[STAGDN]-Y-x-Y-E-{AV}-{L}-[DE]-[KR]-[STA"
    "GCI]",

    "[VA]-H-[FY](2)-[ER]-[DEC]-[GV]-N-[VL]K-x-[LM]-R-R-x-L-P-[IV]-[NT]-R",

    "C-[DE]-[YF]-N-R-D[KR]-x-[LIVMF]-x(3)-[LIVMA]-x(2)-[LIVM]-x(6)-R-Q-Q-E-L",

    "[LIVM]-x-[QA]-A-x(2)-W-[IL]-x-[DN]-PG-[VT]-[EK]-[FY]-V-C-C-PG-Y-E-N-P-T-Y-"
    "[KR]",

    "[LIV]-x-[LIV]-x-D-x-N-D-[NH]-x-P",

    "G-x(7)-[DEN]-G-x(6)-[FY]-x-A-[DNG]-x(2,3)-G-[FY]-x-[AP]",

    "[LIVM]-x-[DE]-[LIVMFYT]-[LIVM]-[DE]-x-[LIVM](2)-[DKR](2)-G-x-[LIVMA]-[LIVM"
    "]",

    "R-[LIVA](3)-A-[GS]-[LIVMFY]-x-[TK]-x(3)-[YFI]-[AG]",

    "F-L-x(2)-T-x(3)-R-x(3)-A-x(2)-Q-x(3)-L-x(2)-F",

    "D-x(0,1)-M-x-K-[SAG](2)-x-[IV]-x-[LIVM]-[LIVMA]-[GCSY]-x(4)-[GDE]-[SGPDR]-"
    "[GA]",

    "[GTARYQ]-x-{R}-x(7)-[LIVMYSTA](2)-[GSTA]-[STADEN]-N-[LIVM]-[SAN]-N-x-[SADE"
    "NFR]-[STV]",

    "[PA]-[AS]-[FY]-x-[LIVT]-[STH]-[EQ]-[LI]-x(2)-[GA]-F-[KREQ]-[IM]-[GV]-[LIF]"
    "",

    "P-[LIVMF]-K-[LIVMF](5)-x-[LIVMA]-[DNGS]-G-W",

    "[FYW]-x-[PSTA]-x(7)-G-x-[LIVM]-x-[LIVM]-x-[FYWI]-x(2)-D-x(5)-P",

    "[RK]-[FYW]-A-[GAP]-F-D-x-F-x(2)-[LV]-x(3)-[GASTY]-[GASTV]",

    "C-x-[LIVMFQ]-x-[LIVMF]-x(2)-[FY]-P-x-D-x(3)-C",

    "G-G-x-[LIVM]-G-[LIVM]-x-[IV]-x-W-x-C-[DN]-L-D-x(5)-C-x-P-x-Y-x-F",

    "[GSTALIVMFYWC]-[GSTANCPDE]-{EDPKRH}-x-{PQ}-[LIVMNQGA]-x(2)-[LIVMFT]-[GSTAN"
    "C]-[LIVMFYWSTAC]-[DENH]-R-[FYWCSH]-{PE}-x-[LIVM]",

    "C-x(3)-[FYWLIV]-D-x(3,4)-C-[FW]-x(2)-[STAGV]-x(8,9)-C-[PF]",

    "[QL]-G-[LMFCAV]-[LIVMFTA]-[LIV]-x-[LIVFSTM]-[LIFHV]-[VFYHLG]-C-[LFYAVI]-x-"
    "[NKRQDS]-x(2)-[VAI]",

    "[LV]-x-N-[LIVM](2)-x-L-F-x-I-[PA]-Q-[LIVM]-[STA]-x-[STA](3)-[STAN]",

    "C-C-[FYW]-x-C-x(2)-C-x(4)-[FYW]-x(2,4)-[DN]-x(2)-[STAH]-C-x(2)-C",

    "[FL]-N-[ED]-[STA]-K-x-I-[STAG]-[FM]-[ST]-[MV]",

    "[LIVMFWAC]-[PSGAC]-x-{G}-x-[SAC]-K-[STALIMR]-[GSACPNV]-[STACP]-x(2)-[DENF]"
    "-[AP]-x(2)-[IY]",

    "R-Y-x-[DT]-W-x-[LIVMF]-[ST]-[TV]-P-[LIVM]-[LIVMNQ]-[LIVM]",

    "[FYIV]-{ND}-[FYVG]-[LIVM]-D-[LIVMF]-x-[STA]-K-x-{K}-[FY][DN]-[LIV]-Y-x(3)-"
    "Y-Y-R",

    "G-x-H-x-N-[LIVM]-V-N-L-L-G-A-C-T",

    "F-x-[DN]-x-[GAW]-[GA]-C-[LIVM]-[SA]-[LIVM](2)-[SA]-[LV]-[KRHQ]-[LIVA]-x(3)"
    "-[KR]-C-[PSAW]",

    "C-x(2)-[DE]-G-[DEQKRG]-W-x(2,3)-[PAQ]-[LIVMT]-[GT]-x-C-x-C-x(2)-G-[HFY]-[E"
    "Q]",

    "[LIV]-x-[LIV]-x-W-x(12,16)-Y-[EQ]-[LIV]-x(25,27)-L-x(10)-[RKH]-x-[RKH]-x(5"
    ",9)-[FYW]-x(2)-[FYW]-x(5)-[LIV]",

    "N-x(4)-S-x(28,35)-[LVIM]-x-W-x(0,3)-P-x(5,9)-[YF]-x(1,2)-[VILM]-x-W",

    "[LIV]-x-P-D-P-P-x(2)-[LIV]-x(8,11)-[LVA]-x(3)-W-x(2)-P-x-[ST]-W-x(4,6)-[FY"
    "]-x-L-x-[FY]-x-[LVI]",

    "[LIVF]-x(9)-[LIV]-[RK]-x(9,20)-W-S-x-W-S-x(4)-[FYW]",

    "[LIVM]-x-C-x-W-x(2)-G-x(5)-D-x(2)-Y-x-[LIVM]-x(10,14)-C",

    "C-x(4,6)-[FYH]-x(5,10)-C-x(0,2)-C-x(2,3)-C-x(7,11)-C-x(4,6)-[DNEQSKP]-{PD}"
    "-{CP}-C",

    "[FYWS]-[RK]-x-G-F-F-x-RC-x-[GNQ]-x(1,3)-G-x-C-x-C-x(2)-C-x-C",

    "G-P-x-C-x-Y-x-A-A-x-V-x-R-x(3)-H-W",

    "[NQH]-x(4)-P-x-H-x(2)-[SAG]-x(11)-[SAGC]-x-H-[SAG](2)",

    "[LIVFAG]-x-[GASV]-[LIVFA]-x-[IV]-H-x(3)-[LIVM]-[GSTAE]-[STANH]-x(1,3)-[STN"
    "]-W-[LIVMFYW]",

    "[EQ]-x(4)-[HGQ]-x(5)-[GSTA]-x(3)-[FYV]-x(3)-[AG]-x(2)-[AV]-H-x(7)-P",

    "C-D-G-P-G-R-G-G-T-C",

    "[GTND]-[FPMI]-x-[LIVMH]-x-[DEAT]-x(2)-[GA]-x-[GTAM]-[STA]-x-G-H-x-[LIVM]-["
    "GAS]",

    "[RGS]-[GSA]-[PV]-H-x-C-H-x(2)-Y",

    "x(10,115)-[DENF]-[ST]-[LIVMF]-[LIVSTEQ]-V-{AGPN}-[AGP]-[STANEQPK]",

    "[LYGSTANEQ]-x(3)-[GSTAENQ]-x-[PGE]-R-x-[LIVFYWA]-x-[LIVMFTA]-[STAGNQ]-[LIV"
    "MFYGTA]-x-[LIVMFYWGTADQ]-x-F",

    "[GC]-x(3)-[LIVMFS]-x(2)-[GSA]-[LIVMFTC]-[LIVMFA]-G-[CLYI]-x-[GA]-[STAPL]-x"
    "(2)-[EGAR]-x(2)-[CWNLF]-[LIVMGA]-[LIVM]",

    "R-T-E-[EQ]-Q-x(2)-[SA]-[LIVM]-x-[EQ]-T-A-A-S-M-E-Q-L-T-A-T-V",

    "G-[LIV]-S-x-[KR]-x-[QH]-x-L-[FY]-x-[LIV](2)-[FYW]-x(2)-R-Y",

    "L-E-[SA]-V-A-I-[LM]-P-Q-[LI]",

    "[KRQ]-[LF]-[CST]-x-K-[IF]-Q-x-[FY]-[ST]-[PA]-x(3)-G-x-E-F-x(5)-[FY](2)-x(2"
    ")-[SA]",

    "C-x-D-x(2)-H-C-C-P-x(4)-C",

    "G-x-[LIM]-x-[STAGP]-x(6,7)-[DENA]-C-x-[FLM]-x-[EQ]-x(6)-Y",

    "S-[DE]-C-x-[DE]-W-x-W-x(2)-C-x-P-x-[SN]-x-D-C-G-[LIVMA]-G-x-R-E-G",

    "C-[KR]-[LIVM]-P-C-N-W-K-K-x-F-G-A-[DE]-C-K-Y-x-F-[EQ]-x-W-G-x-C",

    "[GSR]-C-[KRL]-G-[LIV]-[DE]-x(3)-[YW]-x-S-x-CP-[PSR]-C-V-x(3)-R-C-[GSTA]-G-"
    "C-C",

    "C-x-C-[LIVM]-x(5,6)-[LIVMFY]-x(2)-[RKSEQ]-x-[LIVM]-x(2)-[LIVM]-x(5)-[STAG]"
    "-x(2)-C-x(3)-[EQ]-[LIVM](2)-x(9,10)-C-L-[DN]",

    "C-C-[LIFYT]-x(5,6)-[LI]-x(4)-[LIVMF]-x(2)-[FYW]-x(6,8)-C-x(3,4)-[SAG]-[LIV"
    "M](2)-[FL]-x(8)-C-[STA]",

    "[LIVM]-x(2)-P-x(2)-[FY]-x(4)-C-x-G-x-C",

    "[LV]-x-[LIVM]-{V}-x-{L}-G-[LIVMF]-Y-[LIVMFY](2)-x(2)-[QEKHL]-[LIVMGT]-x-[L"
    "IVMFY]",

    "C-[KR]-C-H-G-[LIVMT]-S-G-x-C",

    "[FYH]-[FY]-x-[GNRCDS]-[LIVM]-x(2)-[FYL]-L-x(7)-[CY]-[AT]-W",

    "C-P-[LP]-T-x-E-[ST]-x-C",

    "[FC]-x-S-[ASLV]-x(2)-P-x(2)-[FYLIV]-[LI]-[SCA]-T-x(7)-[LIVM]",

    "T-E-[LF]-x(2)-L-x-C-L-x(2)-E-L",

    "[LI]-x-E-[LIVM](2)-x(4,5)-[LIVM]-[TL]-x(5,7)-C-x(4)-[IVA]-x-[DNS]-[LIVMA]",

    "C-x(9)-C-x(6)-G-L-x(2)-[FY]-x(3)-LN-x-[LAP]-[SCT]-F-L-K-x-L-L",

    "[KQS]-x(4)-C-[QYC]-x(4)-[LIVM](2)-x-[FL]-[FYT]-[LMV]-x-[DERT]-[IV]-[LMF]",

    "[PST]-x(4)-F-[NQ]-x-K-x(3)-C-x-[LF]-L-x(2)-Y-[HK]",

    "[DE]-P-[CLV]-[APT]-x(3)-[LIVM]-x-S-[IS]-[GT]-x-[LIVM]-[GST]",

    "Q-[LV]-[NT]-[FY]-[ST]-x(2)-WW-A-x-G-[SH]-[LF]-M",

    "C-[SAGDN]-[STN]-x(0,1)-[SA]-T-C-[VMA]-x(3)-[LYF]-x(3)-[LYF]",

    "[PQA]-x-[LIVM]-S-[LIVM]-x(2)-[PST]-[LIVMF]-x-[LIVM]-L-R-x(2)-[LIVMW]",

    "[LIVM]-x(3)-C-[KR]-x-[DENGRH]-C-[FY]-x-[STN]-x(2)-F-x(2)-C",

    "P-x(4)-C-D-x-R-[LIVM](2)-x-[KR]-x(14)-C[DE]-[SN]-L-[SAN]-{EGI}-{ELT}-[DE]-"
    "x-E-L",

    "C-[LIVM](2)-E-[LIVM](2)-S-[DN]-[STA]-L-x-K-x-[SN]-x(3)-[LIVM]-[STA]-x-E-C",

    "G-W-T-L-N-S-A-G-Y-L-L-G-PY-x(0,1)-[GD]-[WH]-M-[DR]-F",

    "[YH]-[STAIVGD]-[DEQ]-[AGF]-[LIVMSTE]-[FYLR]-x-[DENSTAK]-[DENSTA]-[LIVMFYG]"
    "-x(8)-{K}-[KREQL]-[KRDENQL]-[LVFYWG]-[LIVQ]",

    "C-x-G-C-C-[FY]-S-[RQS]-A-[FY]-P-T-PN-H-T-x-C-x-C-x-T-C-x(2)-H-K",

    "C-[STAGM]-G-[HFYL]-C-x-[ST]",

    "[PA]-V-A-x(2)-C-x-C-x(2)-C-x(4)-[STDA]-[DEY]-C-x(6,8)-[PGSTAVMI]-x(2)-C",

    "Q-[HY]-[FYW]-S-x(4)-P-GC-C-{P}-{P}-x-C-[STDNEKPI]-x(3)-[LIVMFS]-x(3)-C",

    "C-F-G-x(3)-[DEA]-[RH]-I-x(3)-S-x(2)-G-CC-[LIFY]-[LIFYV]-x-N-[CS]-P-x-G",

    "F-[LIVMF]-F-R-P-R-N",

    "C-x(3)-C-x(2)-C-x(2)-[KRH]-x(6,7)-[LIF]-[DNS]-x(3)-C-x-[LIVM]-[EQ]-C-[EQ]-"
    "x(8)-W-x(2)-C",

    "[FY]-x(3)-[LIVM]-x(2)-Y-x(3)-[LIVMFY]-x-R-x-R-[YF]V-S-E-x-Q-x(2)-H-x(2)-G",

    "F-[GSTV]-P-R-L-[G]",

    "C-x-[STN]-x(2)-[LIVMFYS]-x-[LIVMSTA]-P-x(5)-[TALIV]-x(7)-[LIVMFY]-x(6)-[LI"
    "VMFY]-x(2)-[STACV]-W",

    "C-[LIVMFY]-{PT}-x-D-[LIVMFYSTA]-x(2)-{RK}-x(2)-[LIVMFY]-x(2)-[LIVMFYT]-x(2"
    ")-C",

    "F-[IVFY]-G-[LM]-M-[G]L-[KR]-K-T-[DENT]-T-x(2)-K-N-[PT]-LC-F-W-K-Y-C",

    "W-x(0,2)-[KDN]-x-{L}-K-[KRE]-[LI]-E-[RKN]C-x-C-x(3,5)-C-x(7)-G-x-C-x(9)-C-"
    "C",

    "C-x(2,3)-[HNS]-C-x(3,4)-[GR]-{A}-x-[GRQ]-[GA]-x-C-x(4,7)-C-x-C",

    "Y-x-[ED]-x-V-x-[RQ]-A-[LIVMA]-[DQG]-x-[LIVMFY]-N-[EQ]",

    "F-x-[LIVM]-K-E-T-x-C-x(10)-C-x-F-[KR]-[KE]C-x-C-x(4)-D-x(2)-C-x(2)-[FY]-C",

    "C-[IV]-[KGR]-X(1,3)-[RG]-C-[NRP]-[AES]-X(0,2)-G-X(1,3)-C-C-S-X(2,4)-C-X(6,"
    "10)-C",

    "C-C-x(5)-R-x(2)-[FY]-x(2)-C",

    "[KRGAQS]-x-C-x(3)-[SVA]-x(2)-[FYWH]-x(1,2)-[GF]-x-C-x(5)-C-x(3)-C",

    "G-C-x(1,3)-C-P-x(8,10)-C-C-x(2)-[PDEN]",

    "K-x-C-H-x-K-x(2)-H-C-x(2)-K-x(3)-C-x(8)-K-x(2)-C-x(2)-[RK]-x-K-C-C-K-K",

    "C-x(3)-C-x(6,9)-[GAS]-K-C-[IMQT]-x(3)-C-x-C",

    "C-[LAV]-X-[DEK]-X(3)-C-X(6,7)-C-C-X(4)-C-X-C-X(5)-C-X-C",

    "C-Q-C-C-x(2)-N-[GA]-[FY]-C-S",

    "C-X(5)-W-C-X(4)-D-C-C-C-X(3)-C-X(2)-A-W-Y-X(5)-C-X(10,11)-CC-[IT]-P-S-G-Q-"
    "P-C",

    "C-C-[GE]-[ML]-T-P-X-CC-{C}(6)-C-X(2)-C-C-X-C-C-X(4)-C-X(9,10)-C",

    "C-[KALRVG]-X-{M}-X(1,3)-C-X(4,6)-C-C-X(4,6)-C-X(4)-[ERK]-W-C",

    "C-X(1,4)-[FLIV]-[SEP]-C-[DE]-[EIQ]-X(4,7)-C-X(0,7)-C-[KST]-X(4,18)-C-[YK]-"
    "X(1,3)-C",

    "C-[LI]-X(2)-G-[SA]-X-C-X-G-X(2)-K-X-C-C-X(4,5)-C-X(2)-Y-A-N-[RK]-C",

    "C-C-[SHYN]-X(0,1)-[PRG]-[RPATV]-C-[ARMFNHG]-X(0,4)-[QWHDGENFYVP]-[RIVYLGSD"
    "W]-C",

    "C-C-[TGN]-[PFG]-[PRG]-X(0,2)-C-[KRS]-[DS]-[RK]-[RQW]-C-[KR]-[PD]-[MLQH]-X("
    "0,1)-[KR]-C-C",

    "C-[SREYKLIMQVN]-x(2)-[DGWET]-x-[FYSPKV]-C-[GNDSRHTP]-x(1,5)-[NPGSMTAHF]-[G"
    "WPNIYRSKLQ]-x-C-C-[STRHGD]-x(0,2)-[NFLWSRYIT]-C-x(0,3)-[VFGAITSNRKL]-[FLIK"
    "RNGH]-[VWIARKF]-C",

    "C-x(2)-[EPSA]-x(3)-C-[GSND]-x(0,3)-[PILV]-x-[FPNDS]-[GQ]-x-C-C-x(3,4)-C-[F"
    "LVIA]-x(1,2)-[FVI]-C",

    "C-{C}(6)-C-{C}(5)-C-C-{C}(1,3)-C-C-{C}(2,4)-C-{C}(3,10)-C",

    "C-C-x(2)-C-C-x-P-A-C-x-G-C[KT]-x(2)-N-W-x(2)-T-[DN]-T",

    "[LIVMA]-x-[LIVMSTA](2)-x-E-[SAGV]-[STAL]-R-[FY]-[RKNQST]-x-[LIVM]-[EQS]-x("
    "2)-[LIVMF]",

    "T-x(2)-W-x-P-[LIVMFY](3)-x(2)-E",

    "[LIVMA](4)-C-[LIVMFA]-T-[LIVMA](2)-x(4)-[LIVM]-x-[RG]-x(2)-L-[CY]",

    "Y-G-G-[LIV]-T-x(4)-NK-x(2)-[LIVF]-x(4)-[LIVF]-D-x(3)-R-x(2)-L-x(5)-[LIV]-Y"
    "",

    "[RK]-E-C-T-G-L-x-W-E-W-W-[RK]Y-x(6)-[FY]-G-T-H-[FY]F-x(3)-G-C-x(6)-[FY]-x("
    "5)-C",

    "C-x(5,6)-[DENQKRHSTA]-C-[PASTDH]-[PASTDK]-[ASTDV]-C-[NDEKS]-[DEKRHSTA]-C",

    "C-x(4)-{C}-x(2)-C-x-{A}-x(4)-Y-x(3)-C-x(2,3)-C",

    "[LIVM]-x-D-{EK}-[EDNTY]-[DG]-[RKHDENQ]-x-[LIVM]-x-{E}-x(3)-Y-x-[LIVM]",

    "[LIVMFY]-{G}-[LIVMFYAC]-[DNQ]-[RKHQS]-[PST]-F-[LIVMFY]-[LIVMFYC]-x-[LIVMFA"
    "H]",

    "[FYW]-P-[EQH]-[LIV](2)-G-x(2)-[STAGV]-x(2)-A",

    "C-P-x(5)-C-x(2)-[DN]-x-D-C-x(3)-C-x-CC-x-P-x(2,3)-G-x-H-P-x(4)-A-C-[ATD]-x"
    "-L",

    "[GSTEQKRV]-Q-[LIVT]-[VAF]-[SAGQ]-G-{DG}-[LIVMNK]-{TK}-x-[LIVMFY]-x-[LIVMFY"
    "A]-[DENQKRHSIV]",

    "C-x-C-x-P-x-H-P-Q-x(2)-[FIV]-C",

    "C-x(4)-[SAGDV]-x(4)-[SPAL]-[LF]-x(2)-C-[RH]-x-[LIVMFYA](2)-x(3,4)-C",

    "[PG]-x-[GS]-C-[GA]-E-[EQ]-x-[LIVM]",

    "E-S-x-L-x-R-x(2)-[KR]-x-L-x(4)-[KR](2)-x(2)-[DE]-x-L",

    "A-[AS]-{L}-[DEQ]-E-x(2)-{R}-x-G-G-[GA]",

    "[LIVMFY]-x-P-[ILT]-x-[DEN]-[KR]-[LIVMFA](3)-[KREQS]-x(8,9)-[SG]-x-[LIVMFY]"
    "(3)",

    "[RKEL]-[ST]-x-[LMFY]-G-P-x-[GSA]-x-x-K-[LIVMF](2)",

    "[LIVM]-[TS]-[NK]-[DN]-[GA]-[AVNHK]-[TAVC]-[LIVM](2)-x(2)-[LIVMA]-x-[LIVM]-"
    "x-[SNH]-[PQHA]",

    "Q-[DEK]-x-x-[LIVMGTA]-[GA]-D-G-T[IV]-D-L-G-T-[ST]-x-[SC]",

    "[LIVMF]-[LIVMFY]-[DN]-[LIVMFS]-G-[GSH]-[GS]-[AST]-x(3)-[ST]-[LIVM]-[LIVMFC"
    "]",

    "[LIVMY]-x-[LIVMF]-x-G-G-x-[ST]-{LS}-[LIVM]-P-x-[LIVM]-x-[DEQKRSTA]",

    "Y-x-[NQH]-K-[DE]-[IVA]-F-[LM]-R-[ED]D-[AI]-[SGA]-N-[LIVMF](2)-K-[PT]-x-L-x"
    "(2)-G",

    "[RGT]-[LIVMFY]-[DN]-x-[ST]-E-[LIVMFY]-x-[ED]-[KRQEAS]-x-[STA]-x-[STAD]-[KR"
    "S]-[LIVM]-x-G-[STAP]",

    "[FY]-{GL}-x-[LIVMA]-{IP}-x(2)-[FYWHNT]-[DENQSA]-x-L-x-[DN]-x(3)-[KR]-x(2)-"
    "[FYI]",

    "C-[DEGSTHKR]-x-C-x-G-x-[GK]-[AGSDM]-x(2)-[GSNKR]-x(4,6)-C-x(2,3)-C-x-G-x-G"
    "",

    "[FYLV]-[DNST]-[PHEAYVS]-x(2)-[HMACNQ]-x-[ALV]-[LIVMTNSF]-x(16,21)-[GYP]-[F"
    "Y]-x(3,4)-[DENGKS]-x(2,3)-[LIV]-[KRIV]-x-[STAG]-x-V-x(0,1)-[IV]",

    "P-x(6)-F-x(4)-[LF]-x(3)-D-[LIVM]-A-[LIVM]-x-[LIVM]-N-x-[LIVMQ]-x-[LF]",

    "[GRH]-[DEQKG]-[STVM]-[LIVMA](3)-[GA]-G-[LIVMFY]-x(11)-[LIVM]-P-[LIVMFYWGS]"
    "-[LIVMF]-[GSAE]-x-[LIVMS]-P-[LIVMFYW]-[LIVMFYWS]-x(2,3)-[LV]-[FK]",

    "[LIVM]-R-x(2)-P-D-x-[LIVM](3)-G-E-[LIVM]-R-D",

    "[KRQ]-[LIVMAW]-x(2)-[SAIV]-[LIVM]-x-[TY]-P-x(2)-[LIVM]-x(3)-[STAGV]-x(6)-["
    "LMY]-x(3)-[LIVMF](2)-P",

    "G-T-L-W-x-G-x(11)-L-x(4)-W",

    "R-[LIVM]-[GSAT]-E-V-[GSAR]-A-R-F-[STAIV]-L-D-[GSA]-[LM]-P-G-K-Q-M-[GSA]-I-"
    "D-[GSA]-[DAE]",

    "[IV]-x-[IV]-[SA]-T-[NQ]-M-A-G-R-G-x-D-I-x-L",

    "[GSTL]-[LIVMFK]-[LIVMFCA]-x-[LIVMF]-[GSAN]-[LIVM]-x-P-[LIVMFYN]-[LIVMFY]-x"
    "-[AS]-[GSTQD]-[LIVMFAT](3)-[EQ]-[LIVMFA](2)",

    "[LIVMFYW](2)-x-[DE]-x-[LIVM]-[STDNQ]-x(2,3)-[GK]-[LIVMF]-[GST]-[NST]-G-x-["
    "GST]-[LIV]-[LIVFP]",

    "[LIVMFY]-x(2)-[DENQGA]-x-{E}-x(2)-[LIVMFTA]-x-[KRV]-x(2)-[KW]-P-x(3)-[SEQ]"
    "-x(5)-{D}-x-[LIVT]-[LIVGA]-[LIVFGAST]",

    "[LIVMFY]-[APN]-x-[DNS]-[KREQ]-E-[STR]-[LIVMAR]-x-[FYWTE]-x-[NCS]-[LIVM]-x("
    "2)-[LIVM]-P-[PAS]",

    "[VL]-[PASQ]-[PAS]-G-[PAD]-[FY]-x-[LI]-[DNQSTAP]-[DNH]-[LIVMFY]",

    "P-[LIVM]-x-[FYL]-[LIVMAT]-[GS]-{Q}-[GS]-[EQ]-x-{K}-x(2)-[LIVMF]",

    "Y-S-x-[KR]-Y-x-[DE](2)-x-[FY]-E-Y-R-H-V-x-[LV]-[PT]-[KRP]",

    "H-x-P-E-x-H-[IV]-L-L-F-[KR]H-x-C-x-[ST]-W-x-[ST][FY]-{L}-C-x-[VA]-{LC}-H",

    "A-G-A-A-A-A-G-A-V-V-G-G-L-G-G-Y",

    "E-x-[ED]-x-K-[LIVM](2)-x-[KR]-[LIVM](2)-x-[QE]-M-C-x(2)-Q-Y",

    "R-x(2)-[LIVMSA]-x(2)-[FYWS]-[LIVM]-x(8)-[LIVMFC]-x(4)-[LIVMFYA]-x(2)-[STAG"
    "C]-[LIVMFYQ]-x-[LIVMFYC]-[LIVMFY]-D-[RKH]-[LIVMFYW]",

    "[GSTA]-[LIVMF]-x-[LIVMAS]-x-[GSAVI]-[LIVM]-[DS]-x-[NSAED]-[HKRNS]-[VIT]-x-"
    "[LMYF]-[VIGAL]-x-[LIVMF]-x-[LIVM]-x(4)-F",

    "[RKA]-C-[DE]-[RH]-x(3)-[LIVMF]-x(3)-[LIVM]-x-[SGAN]-[LIVMF]-x-K-[LIVMF](2)"
    "",

    "P-[DES]-x-[SA]-x-[LIVMT]-[KR]-x-[KR]-[LIVM](2)-[YA]-[STA](3)-x(3)-[LIVMF]-"
    "[KRS]",

    "[LVMENQ]-[FTLS]-x-[GSDECQ]-[GLPCKH]-x(1,2)-[NST]-[YW]-G-[RK]-[LIV]-[LIVC]-"
    "[GAT]-[LIVMF](2)-x-F-[GSAEC]-[GSARY]",

    "W-[LIM]-x(3)-[GR]-G-[WQ]-[DENSAV]-x-[FLGA]-[LIVFTC]",

    "[LIVAT]-x(3)-L-[KARQ]-x-[IVAL]-G-D-[DESG]-[LIMFV]-[DENSHQ]-[LVSHRQ]-[NSR]",

    "[DS]-[NT]-R-[AE]-[LI]-V-x-[KD]-[FY]-[LIV]-[GHS]-Y-K-L-[SR]-Q-[RK]-G-[HY]-x"
    "-[CW]",

    "G-x(2)-[LIVM]-[GC]-P-x-[LI]-x(4)-[SAGD]-x(4,6)-[LIVM](2)-x(2)-A-x(2)-[MG]-"
    "T-x-[LIVM]-x-F",

    "[FY]-R-Y-G-x-[DE](2)-x-[DE]-[LIVM](2)-G-[LIVM]-x-F-x-[RK]-[DEQ]-[LIVM]",

    "[LIVMTR]-x-[LIVMT]-[LIVMF]-x-[GATMC]-[ST]-[NS]-x(4)-[LIVM]-D-x-[AS]-[LIFAV"
    "]-x(1,2)-R",

    "K-x(2)-[LIVM]-x-[DESAK]-x(3)-[LIVM]-[PAQ]-x(3)-Q-x-[LIVM]-[LIVMC]-[LIVMFY]"
    "-x-G-x(4)-[DE]",

    "[HRQT]-x-[FYWI]-x-[LIVM]-x(4)-A-x(2)-G-x(2)-[LIVM]-x(2)-[GSA]-[LIVMF]-x-[W"
    "K]-[LIVM]",

    "D-T-A-G-Q-E-[KR]-[LFY]-G-G-L-R-[DE]-G-Y-[YF]",

    "R-x-[LIVM]-E-[LV]-F-[MPT]-C-S-[LIVM]-[LIVMY]-x-[KRQ]-x-G-Y-x-[DE]-[AG]-[FI"
    "]-x-W-[LIVM]-x-[NQK]-Y",

    "R-x(2)-[LIV]-[SAN]-x(6)-[LIV]-D-x(2)-T-x(2)-W-G-[LIVT]-[KRH]-[LIV]-x-[KRA]"
    "-[LIV]-E-[LIV]-[KRQ]",

    "[LIVMSTAC]-[LIVMFYWSTAGC]-[LIMSTAG]-[LIVMSTAGC]-x(2)-[DN]-x-{P}-[LIVMWSTAC"
    "]-{DP}-[LIVMFSTAG]-W-[DEN]-[LIVMFSTAGCN]",

    "[GSNA]-x-[LIVMF]-[FYCI]-[LIVMFY]-R-[LIVMFY](2)-[GACNS]-[PAV]-[AV]-[LIV]-[L"
    "IVM]-[SGANT]-P",

    "[LM]-x(2)-[LIVMFYW]-L-x(2)-P-[LIVM]-x(2)-[LIVM]-x-[KRS]-x(2)-L-x-[LIVM]-x-"
    "[DEQ]-[LIVM]-x(3)-[ST]",

    "[VI]-P-[FYWVI]-x-[GPSV]-x(2)-[LIVMFYK]-x-[DNE]-[LIVM]-x(13,35)-[IVL]-N-[FY"
    "ME]-x-K",

    "G-Q-E-N-G-H-V-[KR]",

    "E-T-P-K(5)-x(0,1)-F-S-F-K-K-x-F-K-L-S-G-x-S-F-K-[KR]-[NS]-[KR]-K-E",

    "P-[KRQ]-[KR](2)-[DE]-x-S-L-[EG]-EA-E-[KR]-R-E-H-E-[KR]-E-V",

    "D-[KRSTGANQFYW]-x(3)-E-[KRAQ]-x-[RKQD]-[GC]-[IVMK]-[ST]-[IV]-x(2)-[GSTACKR"
    "NQ]",

    "[DE]-[DEG]-[DEK]-[DE]-[LIVMF]-D-L-F-G",

    "[IV]-Q-S-x-D-[LIVM]-x-A-[FWM]-[DNQS]-K-[LIVM]",

    "L-R-x(2)-[TS]-[GSDNQ]-x-[GSA]-[LIVMF]-x(0,1)-[DENKAC]-x-K-[KRNEQS]-[AV]-L",

    "[ELAS]-[LIVMF]-[NVCKGST]-[SCVA]-[QE]-T-D-[FS]-[VLA]-[SAT]-[KRNLAQS]",

    "K-x-[AV]-x(4)-G-x(2)-[LIVT]-x-V-P-x(2)-[LIVC]-x(2)-[GD]",

    "[IMVALH]-x-G-x-[GSDENK]-[KRHFW]-x(4)-[CL]-x-D-G-x(2)-[RY]-x(2)-[RH]-[IL]-x"
    "-G",

    "[DE]-[IFYL]-x(2)-F-[KRL]-x(2)-[LIVM]-x-P-x-W-E-[DVA]-x(5)-G-G-[KR]-W",

    "[PT]-G-K-H-G-x-A-K",

    "G-x-[LIVM]-x(2)-L-[KR]-[KRHNS]-x-K-x(5)-[LIVM]-x(2)-G-x-[DEN]-C-G",

    "[KR]-[LIVM](2)-[DN]-[FY]-[GSTN]-[KR]-[LIVMFYS]-x-[FY]-[DEQTAHI]-x(2)-[KRQ]"
    "",

    "[ARH]-[STA]-x-G-x-G-G-Q-[HNGCSY]-[VI]-N-x(3)-[ST]-[AKG]-[IV]",

    "[LIVMF]-[QKRHSA]-{E}-x-[LIVMAC]-x(5,6)-[LIVMW]-[RKAYF]-x-[STACIVMF]-[PV]-{"
    "LG}-[LIVMF]-x-[FYI]-x(2)-D",

    "[LIVM]-F-G-[KRW]-x-T-P-[IV]-x-[LIVM]",

    "[LIVM]-x-[LS]-Q-[MASY]-G-[STY]-[NT]-[KRQ]-x(2)-[STN]-Q-x-G-x(3,4)-G",

    "[LIVM](2)-x-R-L-[DE]-x(4)-R-L-ED-[LIVMFY]-x-E-x-[PA]-x-P-E-Q-[LIVMFY]-K",

    "[KRHN]-x-[DEQN]-[DEQNK]-x(3)-C-G-G-[AG]-[FY]-[LIVM]-[KN]-[LIVMFY](2)",

    "[LIVM](2)-F-G-P-D-x-C-[AG][IVM]-x-[DV]-x-[DENST]-x(2)-K-P-[DEH]-D-W-[DEN]",

    "[EQ]-[DE]-G-L-[DN]-F-P-x-Y-D-G-x-D-R-V",

    "[DE]-L-E-D-W-[LIVM]-E-D-V-L-x-G-x-[LIVM]-N-T-E-D-D-D",

    "[LIVMFYW](2)-x(2)-[LK]-D-x(3)-[DN]-x(3)-[DNSG]-[FY]-x-[ES]-[FYVC]-x(2)-[LI"
    "VMFS]-[LIVMF]",

    "D-x-[LI]-x(4)-G-x-D-x-[LI]-x-G-G-x(3)-D",

    "[LIVM]-x(2)-G-[LM]-x(3)-[STGAV]-x-[LIVMT]-x-[LIVMTK]-[GE]-x-[KR]-x-[LIVMFY"
    "W](2)-x-[LIVMFYW](2)-[LIVMFYWK]",

    "Y-[KR]-G-[AS]-[AE]-Y[ST]-x(3)-G-[DY]-G-[KR]-[IV]-[FW]-[LIVM]-x(2)-[LIVM]",

    "[RA]-N-L-[LIV]-S-[VG]-[GA]-Y-[KN]-N-[IVA]",

    "Y-K-[DE]-[SG]-T-L-I-[IML]-Q-L-[LF]-[RHC]-D-N-[LF]-T-[LS]-W-[TANS]-[SAD]",

    "Y-x(2)-[HP]-W-[FYH]-[APS]-[DE]-x-P-x-K-G-x-[GA]-[FY]-R-C-[IV]-[RH]-[IV]",

    "[LV]-P-x-[DE]-[LM]-[ST]-[LIVM]-W-[IV]-D-P-x-E-V-[SC]-x-[RQ]-x-G-E",

    "[LIV]-K-x(2)-[LIV]-x(2)-L-[IL]-[DEQ]-[KRHNQ]-x-Y-[LIVM]-x-R-x(5,7)-[FY]-x-"
    "Y-x-[SAT]",

    "Y-D-I-[SA]-x-L-[FY]-x-F-[IV]-D-x(3)-D-[LIV]-S",

    "[DNSE]-x-F-x-Y-[DN]-x(2)-[STNR]-[LIVM]-[RQ]-x(2)-G",

    "L-C-C-x-[KR]-C-x(4)-[DE]-x-N-x(4)-C-x-C-R-V-PC-x-H-C-G-C-[KRH]-G-C-[SA]",

    "G-[PA]-E-x-[LIV]-[STAM]-G-S-[ST]-R-[LIVM]-K-[STGA](3)-x(2)-K",

    "D-[LIVMA]-P-G-[LIVM](2)-[DEYPKQV]-[GN]-A-x(2)-G-x-G",

    "[NQAR]-x(4)-[GSAV]-x-[QFLPA]-x-[LIVM]-x-[HWYRQ]-[LIVMFYST]-H-[LIVMFT]-H-[L"
    "IVMF]-[LIVMFP]-[PSGAW]",

    "C-L-[LV]-A-x-A-[LVF]-A",

    "[IVT]-[GSP]-W-R-x(2,3)-[GAD]-x(2)-[HY]-x(2)-N-x-[LIVMAFY](3)-D-[LIVM]-[LIV"
    "MT]-E",

    "[LIV]-x-[FL]-[IQ]-P-P-x-G-x-[LIVMFY]-x-[LV]-x(2)-Y",

    "[LIVMC]-[LIVM]-Y-[KR]-x(4)-L-Y-F[VI]-x(3)-[DE]-L-x(2)-D-[FY]-x-[NS]-[PS]-I"
    "-[DE]",

    "[DE]-[SQLM]-[KAH]-[NT]-[QEK]-[SQ]-C-[SRKH]-x-[EQKM]-[STM]-L",

    "F-x-[PL]-P-[STA]-[FYT]-C-[DEQ]-[GAMI]-[LVMA]-x-[TLF]-[DE]-[DEK]",

    "[LT]-L-E-[FY]-[AVC]-[DE]-[DE]-[KNQHT]-[LMT]",

    "[RQLKA]-x(3)-[LIVMAW]-x(2)-[LIVM]-[ESHLKAGQV]-x(2)-[LIVMT]-x-[DEVMNRAST]-["
    "LIVMT]-x(2)-[LIVMQ]-[FSAQGVM]-x(2)-[LIVMF]-x(3)-[LIVT]-x(2)-Q-[GADEQVST]-x"
    "(2)-[LIVMA]-[DNQTEI]-x-[LIVMF]-[DESVHAG]-x(2,3)-[LIVM]",

    "[GDER]-H-[FYWH]-[TVS]-[QA]-[LIVM]-[LIVMA]-W-x(2)-[STN]",

    "[LIVMFYH]-[LIVMFY]-x-C-[NQRHS]-Y-x-[PARH]-x-[GL]-N-[LIVMFYWDN]",

    "C-[DN]-[DE]-x(54)-C-H-x(9)-C-x(12,14)-C-x(17,19)-C-x(13)-C-x(2)-C",

    "[ND]-x-L-E-T-x-C-H-x-L[LIV]-[STAG]-V-[DEQV]-[FLI]-D-[ST]",

    "[LIV]-{LA}-[EDQ]-[FYWKR]-V-{VF}-[LIVF]-G-[LF]-[ST]",

    "W-[GEK]-x-[EQ]-x-[KRE]-x(3,6)-[PCTF]-[LIVMF]-[NQEGSKV]-x-[GH]-x(3)-[DENKHS"
    "]-[LIVMFC]",

    "[STA]-C-[LIVM]-[LIVMFYW]-A-x-[LIVMFYW]-x(3)-[LIVMFYW]-x(3)-Y",

    "C-x(2)-D-x(3,4)-[LIVM](2)-P-[LIVM]-x-[LIVM]-G-x(2)-[LIVM]-x-G-[LIVM](2)-x-"
    "[LIVM](4)-A-[FY]-x-[LIVM]-x(2)-[KR]-[RH]-x(1,2)-[STAG](2)-Y-[EQ]",

    "I-I-x-[GAC]-V-M-A-G-[LIVM](2)",

    "[LIVMF]-[LIVMFC]-[LIVMF](2)-[SA]-[TL]-x(2)-[DNKS]-x-W-x(9,13)-[LIV]-W-x(2)"
    "-[CG]",

    "[RQ]-[AVS]-x-[MC]-[IV]-L-[SA]-x-[LI]-x(4)-[GSA]-[LIVMF]-[LIVMFS]-[LIVMF]",

    "[GN]-L-W-x(2)-C-x(7,9)-[STDENQH]-C",

    "E-[KQ]-x-[SC]-H-[HR]-[PG]-[PL]-x(1,2)-[STACFI]-[ACG]",

    "S-Q-[IV]-[STGNH]-D-G-Q-[LIV]-Q-[AIV]-[STA]",

    "[IVM]-x-G-Q-D-x-V-K-x(5)-[KN]-G-x(3)-[STLV][GSA]-Q-x-K-S-[FY]-x-Q-x-K-[SA]"
    "",

    "A-R-G-N-Y-[ED]-A-x-[QKR]-R-G-x-G-G-x-W-AC-G-x(2)-[LIY]-x(4)-G-x-I-x(9)-C-x"
    "-W-T",

    "C-x-K-E-x-[LIVM]-E-[LIVM]-x-[DE]-x(3)-[GSE]-x(5)-K-x-CP-W-Y-[ST](2)-R-L",

    "[KR]-Y-[DE]-F-[FY]-Q-x(2)-S-x-[LIVM]-G-G-L-L",

    "C-x(2)-C-x-G-[LIVM]-x(4)-P-C-x(2)-[FY]-C-x(2)-[LIVM]-x(2)-G-C",

    "[FY]-R-[IM]-[KR]-K(2)-D-E-G-S-YW-K-x-K-C-x(2)-T-x-[DEN]-T-E-C-D-[LIVM]-T-D"
    "-E",

    "[IFAE]-[GA]-[GAS]-N-[PAK]-S-[GTA]-E-[GDEV]-[PAGEQV]-[DEQGAV]",

    "[FLIV]-x(4)-[FLVH]-[FY]-[MIVCT]-G-E-x(4,7)-[DENP]-[GAST]-x-[LIVM]-[GAVI]-x"
    "(3)-[FYWQ]",

    "F-[KRHQ]-G-R-V-[ST]-x-A-S-V-K-N-F-Q",

    "A-F-[AG]-I-[GSAC]-[LIVM]-[ST]-S-F-x-[GST]-K-x-A-C-EH-R-H-R-G-H-x(2)-[DE](7"
    ")",

    "A-G-Y-G-S-T-x-T",

    "[NV]-x(5)-[GTR]-[LIVMA]-x-P-[PTLIVME]-x-G-[LIVM]-x(3)-[LIVMFW](2)-S-[YSAQ]"
    "-G-G-[STN]-[SA]",

    "G-[LIVMFY]-N-[LIVM]-K-Y-R-Y-E[FYW]-x(2)-G-x-G-Y-[KR]-F",

    "[GS]-x(4)-[LIVM]-x(4)-[LIVMF]-x(2)-[CSAM]-[LMFY]-x(6)-[STC]-x(4,5)-[PAC]-x"
    "-[LIVMF]-x-[LIVMF]-x(8)-C-x(1,2)-[CH]",

    "M-C-[LIV]-[GA]-[LIV]-P-x-[QKR]-[LIV]",

    "A-R-P-x(3)-K-x-S-x-T-N-A-Y-N-V-T-T-x(2)-[DN]-G-x(3)-Y-G",

    "N-G-x-[DE](2)-x-[LIVMF]-C-[ST]-x(11,12)-[PAG]-DC-X(0,1)-[ES]-S-C-V-[FYW]-I"
    "-P-C",

    "C-G-E-[ST]-C-[FTV]-[GLT]-G-[TSK]-C",

    "S(5)-[DE]-x-[DE]-[GV]-x(1,4)-[GE]-x(0,1)-[KR](4)[KR]-[LIM]-K-[DE]-K-[LIM]-"
    "P-G",

    "G-x(4)-H-x-H-P-x-[AGS]-x-E-[LIVM]",

    "[AG]-[ST]-x(2)-[AG]-x(2)-[LIVM]-[SAD]-[TI]-P-[LIVMF](4)-F-S-P-[LIVM](3)-P-"
    "A",

    "G-[EQ]-T-V-V-P-G-G-T",

    "G-x(2)-[LIVMF]-x(4)-E-x(2,3)-[CSTAENV]-x(8,9)-[GNDS]-[GS](2)-[CS]-x(2)-[KT"
    "]-x(4)-[FY]",

    "[EQT]-G-x-V-Y-C-D-[TNP]-C-RG-x-[GF]-x-C-x-T-[GA]-D-C-x(1,2)-[GQ]-x(2,3)-C",

    "W-x(2)-[LIVM]-D-[VFY]-[LIVM](3)-D-x-P-P-G-T-[GS]-D",

    "[GS]-[PTAV]-x-[YH]-C-P-S-[LIVM]-[ED]-x-K-[LIVMF]-x-[KRN]-[FY]",

    "[AC]-G-[QNV]-x-[NTAI]-G-x(2)-G-Y-x-[EA]-[SAG](2)-[SAGC]-[QSAGMVIT]-G-[LIVM"
    "WF]-[LIVMAF]-[AST]-[GA]-[LIVMTAR]-[NYAMF]-[ALVST]",

    "[FV]-[DQ]-[KRA]-[LIVMA]-L-x-D-[AV]-P-C-[ST]-[GA]",

    "[LIVFYAN]-[LIVMFA]-x(2)-D-[LIVMF]-[ND]-G-T-[LIV]-[LVY]-[STANLM]",

    "[LIVMFC]-G-D-[GSANQ]-x-N-D-x(3)-[LIMFY]-x(2)-[AV]-x(2)-[GSCP]-x(2)-[LMP]-x"
    "(2)-[GAS]",

    "[LIVMY]-[VI]-H-[GA]-D-[LF]-[SN]-E-[FY]-N-x-[LIVM]",

    "[LIVMTA](3)-[LIVMFYC]-[PG]-T-[DE]-[STA]-x-[FYL]-[GA]-[LIVM]-[GS]",

    "[FW]-H-[FM]-[IV]-G-x-[LIV]-Q-x-[NKRQ]-[KN]-x(3)-[LIV]",

    "G-[STIF]-V-x(2)-[LIVM]-x(6)-[LIVMF]-x(3)-[DQT]-x(3)-[LIVH]-x-[LIV]-P-[NW]-"
    "x(2)-[LIVMF]-[LIVFSTA]-x(5)-[NV]",

    "[LIVM]-x-[LIVMT]-x(2)-G-C-x(3)-C-[STAN]-[FY]-C-x-[LIVMT]-x(4)-G",

    "S-[DN]-[AS]-G-x-P-x-[LIV]-[SNC]-D-P-G",

    "[GTAL]-x(2)-[IVT]-C-Y-D-[LIVM]-x-F-P-x(9)-[GD]E-[LIVM]-G-D-K-T-F-[LIVMF](2"
    ")-A",

    "D-x(8)-[GN]-[LFY]-x(4)-[DET]-[LY]-Y-x(3)-[ST]-x(7)-[IV]-x(2)-[PS]-x-[LIVM]"
    "-x-[LIVM]-x(3)-[DN]-D",

    "D-P-[LIVMF]-C-G-[ST]-G-x(3)-[LI]-EC-K-x(2)-F-x(4)-E-x(22,23)-S-G-G-K-D",

    "[DEN]-x(2)-[LIVF]-[DE](2)-[LIV]-L-x(4)-[IV]-[FY]-x(4)-K-G",

    "G-x-K-D-[KRAT]-x-[AG]-[LVRSIT]-[TKS]-x-Q-x-[LIVF]-[SGCYAT]",

    "D-V-[LIV]-x(2)-G-H-[ST]-H-x(12)-[LIVMF]-N-P-G",

    "Q-[LIVM]-x-N-x-A-x-[LIVM]-P-x-I-x(6)-[LIVM]-P-D-x-H-x-G-x-G-x(2)-[IV]-G",

    "[GA]-[GS]-G-[GA]-A-R-G-x-[SA]-H-x-G-x(9)-[IVY]-x-[IV]-[DV]-x(2)-[GA]-G-x-S"
    "-x-G",

    "G-x(2)-[LIVM](2)-x(2)-[LIVM]-x(4)-[LIVM]-x(5)-[LIVM](2)-x-R-[FYW](2)-G-G-x"
    "(2)-[LIVM]-G",

    "[SAV]-[IVW]-[LVA]-[LIV]-G-[PNS]-G-L-[GP]-x-[DENQT]",

    "[GA]-G-x-G-D-[TV]-[LT]-[STA]-G-x-[LIVM]",

    "Y-x(2)-[FL]-[LIVMAF]-[LIVMAFT]-x-[LVSI]-x(4)-[GA]-x(2)-F-[EQ]-[LIVMF]-P-[L"
    "IVM]",

    "[LIV]-[DEN]-x(2)-[TAG]-x(2)-C-P-x-[PT]-x-[LIVMF]-x(11)-[GN]",

    "[LIVM]-[DNG]-[LIVMF]-N-x-G-C-P-x(3)-[LIVMASQ]-x(5)-G-[SAC]",

    "L-L-T-x-R-[SA]-x(3)-R-x(3)-G-x(3)-F-P-G-G",

    "H-x-S-G-H-[GA]-x(3)-[DE]-x(3)-[LM]-x(5)-P-x(3)-[LIVM]-P-x-H-G-[DE]",

    "L-[ST]-x(3)-K-x(3)-[KR]-[SGA]-x-[GA]-H-x-L-x-P-[LIV]-x(2)-[LIV]-[GA]-x(2)-"
    "G",

    "S-x(2)-[LIV]-x-[LIV]-x(2)-G-x(4)-G-T-W-Q-x-[LIV]",

    "H-[GSA]-x-[LVCYT]-H-[LAI]-[LIMSANQVF]-G-[FYWMH]-x-[HD]",

    "[LIVF]-x-[STAC]-[LIVF](3)-P-[PF]-[LIVA]-[GAV]-[IV]-x(4)-[GKN]",

    "G-[AV]-F-[STA]-x-R-[SA]-x(2)-R-P-ND-V-x(5)-H-I-[SA]-C-D-x(4)-S-E",

    "L-x(3)-[GRS]-[LIVY]-x(2)-[STA]-x(2)-G-x(2)-G-G-[FYIV]-x-[LIF]",

    "[PA]-[ASTPV]-R-[SACVF]-x-[LIVMFY]-x(2)-[GSAKR]-x-[LMVA]-x(5,8)-[LIVM]-E-[M"
    "I]",

    "F-x-[LIVMFY]-x-[NH]-[PGT]-[NSKQR]-x(4)-C-x-C-[GS]-x-S-F",

    0 
};

BEGIN_NCBI_SCOPE
BEGIN_SCOPE(cobalt)

void AssignDefaultPatterns(vector<char*>& patterns)
{
    patterns.clear();
    patterns.resize(kNumDefPatterns);
    for (int i=0;kDefPatterns[i] && i < kNumDefPatterns;i++) {
	patterns[i] = (char*)kDefPatterns[i];
    }
}


END_SCOPE(cobalt)
END_NCBI_SCOPE
