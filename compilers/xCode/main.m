/*  $Id: main.m,v 1.1 2004/06/23 17:09:52 lebedev Exp $
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
 * Author:  Vlad Lebedev
 *
 * File Description:
 * Main startup file for AppleScript studio (xCode generated - do not modify)
 *
 */

#import <mach-o/dyld.h>

extern int NSApplicationMain(int argc, const char *argv[]);

int main(int argc, const char *argv[])
{
	if (NSIsSymbolNameDefined("_ASKInitialize"))
	{
		NSSymbol *symbol = NSLookupAndBindSymbol("_ASKInitialize");
		if (symbol)
		{
			void (*initializeASKFunc)(void) = NSAddressOfSymbol(symbol);
			if (initializeASKFunc)
			{
				initializeASKFunc();
			}
		}
	}
	
    return NSApplicationMain(argc, argv);
}


/*
 * ===========================================================================
 * $Log: main.m,v $
 * Revision 1.1  2004/06/23 17:09:52  lebedev
 * Initial revision
 *
 * ===========================================================================
 */