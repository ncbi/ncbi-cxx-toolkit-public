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
 * Authors:  Jim Luther, Joshua Juran, Vlad Lebedev
 *
 * File Description:
 *   Mac specifics
 *
 */

/*
 * NOTICE: Portions of this file have been taken and modified from
 * original sources provided as part of Apple's "MoreFiles" package
 * that carries the following copyright notice:
 *
 *    You may incorporate this sample code into your applications without
 *    restriction, though the sample code has been provided "AS IS" and the
 *    responsibility for its operation is 100% yours.  However, what you are
 *    not permitted to do is to redistribute the source as "DSC Sample Code"
 *    after having made changes. If you're going to re-distribute the source,
 *    we require that you make it clear in the source that the code was
 *    descended from Apple Sample Code, but that you've made changes.
 */


#include <ncbi_pch.hpp>
#include <TextUtils.h>

#include <corelib/ncbi_os_mac.hpp>


extern bool g_Mac_SpecialEnvironment = false;

BEGIN_NCBI_SCOPE


/////////////////////////////////////////////////////////////////////////////
//   COSErrException_Mac
//

static string s_OSErr_ComposeMessage(const OSErr& os_err, const string& what)
{
    string message;
    if ( !what.empty() ) {
        message = what + ": ";
    }

    message += os_err;

    return message;
}


COSErrException_Mac::COSErrException_Mac(const OSErr&  os_err,
                                         const string& what) THROWS_NONE
    : exception(), m_OSErr(os_err)
{
    return;
}


COSErrException_Mac::~COSErrException_Mac(void) THROWS_NONE
{
    return;
}


static	OSErr	GetVolumeInfoNoName(ConstStr255Param pathname,
									short vRefNum,
									HParmBlkPtr pb)
{
	Str255 tempPathname;
	OSErr error;
	
	/* Make sure pb parameter is not NULL */ 
	if ( pb != NULL )
	{
		pb->volumeParam.ioVRefNum = vRefNum;
		if ( pathname == NULL )
		{
			pb->volumeParam.ioNamePtr = NULL;
			pb->volumeParam.ioVolIndex = 0;		/* use ioVRefNum only */
		}
		else
		{
			BlockMoveData(pathname, tempPathname, pathname[0] + 1);	/* make a copy of the string and */
			pb->volumeParam.ioNamePtr = (StringPtr)tempPathname;	/* use the copy so original isn't trashed */
			pb->volumeParam.ioVolIndex = -1;	/* use ioNamePtr/ioVRefNum combination */
		}
		error = PBHGetVInfoSync(pb);
		pb->volumeParam.ioNamePtr = NULL;	/* ioNamePtr may point to local	tempPathname, so don't return it */
	}
	else
	{
		error = paramErr;
	}
	return ( error );
}

static	OSErr GetCatInfoNoName(short vRefNum,
							   long dirID,
							   ConstStr255Param name,
							   CInfoPBPtr pb)
{
	Str31 tempName;
	OSErr error;
	
	/* Protection against File Sharing problem */
	if ( (name == NULL) || (name[0] == 0) )
	{
		tempName[0] = 0;
		pb->dirInfo.ioNamePtr = tempName;
		pb->dirInfo.ioFDirIndex = -1;	/* use ioDirID */
	}
	else
	{
		pb->dirInfo.ioNamePtr = (StringPtr)name;
		pb->dirInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	}
	pb->dirInfo.ioVRefNum = vRefNum;
	pb->dirInfo.ioDrDirID = dirID;
	error = PBGetCatInfoSync(pb);
	pb->dirInfo.ioNamePtr = NULL;
	return ( error );
}

static	OSErr	DetermineVRefNum(ConstStr255Param pathname,
								 short vRefNum,
								 short *realVRefNum)
{
	HParamBlockRec pb;
	OSErr error;

	error = GetVolumeInfoNoName(pathname,vRefNum, &pb);
	if ( error == noErr )
	{
		*realVRefNum = pb.volumeParam.ioVRefNum;
	}
	return ( error );
}

static	OSErr	GetDirectoryID(short vRefNum,
							   long dirID,
							   ConstStr255Param name,
							   long *theDirID,
							   Boolean *isDirectory)
{
	CInfoPBRec pb;
	OSErr error;

	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	if ( error == noErr )
	{
		*isDirectory = (pb.hFileInfo.ioFlAttrib & kioFlAttribDirMask) != 0;
		if ( *isDirectory )
		{
			*theDirID = pb.dirInfo.ioDrDirID;
		}
		else
		{
			*theDirID = pb.hFileInfo.ioFlParID;
		}
	}
	
	return ( error );
}

extern	OSErr	FSpGetDirectoryID(const FSSpec *spec,
								  long *theDirID,
								  Boolean *isDirectory)
{
	return ( GetDirectoryID(spec->vRefNum, spec->parID, spec->name,
			 theDirID, isDirectory) );
}

static	OSErr	CheckObjectLock(short vRefNum,
								long dirID,
								ConstStr255Param name)
{
	CInfoPBRec pb;
	OSErr error;
	
	error = GetCatInfoNoName(vRefNum, dirID, name, &pb);
	if ( error == noErr )
	{
		/* check locked bit */
		if ( (pb.hFileInfo.ioFlAttrib & kioFlAttribLockedMask) != 0 )
		{
			error = fLckdErr;
		}
	}
	
	return ( error );
}

/*****************************************************************************/

extern	OSErr	FSpCheckObjectLock(const FSSpec *spec)
{
	return ( CheckObjectLock(spec->vRefNum, spec->parID, spec->name) );
}

static	OSErr	GetFileSize(short vRefNum,
							long dirID,
							ConstStr255Param fileName,
							long *dataSize,
							long *rsrcSize)
{
	HParamBlockRec pb;
	OSErr error;
	
	pb.fileParam.ioNamePtr = (StringPtr)fileName;
	pb.fileParam.ioVRefNum = vRefNum;
	pb.fileParam.ioFVersNum = 0;
	pb.fileParam.ioDirID = dirID;
	pb.fileParam.ioFDirIndex = 0;
	error = PBHGetFInfoSync(&pb);
	if ( error == noErr )
	{
		*dataSize = pb.fileParam.ioFlLgLen;
		*rsrcSize = pb.fileParam.ioFlRLgLen;
	}
	
	return ( error );
}

extern	OSErr	FSpGetFileSize(const FSSpec *spec,
							   long *dataSize,
							   long *rsrcSize)
{
	return ( GetFileSize(spec->vRefNum, spec->parID, spec->name, dataSize, rsrcSize) );
}

extern	OSErr	GetDirItems(short vRefNum,
							long dirID,
							ConstStr255Param name,
							Boolean getFiles,
							Boolean getDirectories,
							FSSpecPtr items,
							short reqItemCount,
							short *actItemCount,
							short *itemIndex) /* start with 1, then use what's returned */
{
	CInfoPBRec pb;
	OSErr error;
	long theDirID;
	Boolean isDirectory;
	FSSpec *endItemsArray;
	
	if ( *itemIndex > 0 )
	{
		/* NOTE: If I could be sure that the caller passed a real vRefNum and real directory */
		/* to this routine, I could rip out calls to DetermineVRefNum and GetDirectoryID and this */
		/* routine would be much faster because of the overhead of DetermineVRefNum and */
		/* GetDirectoryID and because GetDirectoryID blows away the directory index hint the Macintosh */
		/* file system keeps for indexed calls. I can't be sure, so for maximum throughput, */
		/* pass a big array of FSSpecs so you can get the directory's contents with few calls */
		/* to this routine. */
		
		/* get the real volume reference number */
		error = DetermineVRefNum(name, vRefNum, &pb.hFileInfo.ioVRefNum);
		if ( error == noErr )
		{
			/* and the real directory ID of this directory (and make sure it IS a directory) */
			error = GetDirectoryID(vRefNum, dirID, name, &theDirID, &isDirectory);
			if ( error == noErr )
			{
				if ( isDirectory )
				{
					*actItemCount = 0;
					endItemsArray = items + reqItemCount;
					while ( (items < endItemsArray) && (error == noErr) )
					{
						pb.hFileInfo.ioNamePtr = (StringPtr) &items->name;
						pb.hFileInfo.ioDirID = theDirID;
						pb.hFileInfo.ioFDirIndex = *itemIndex;
						error = PBGetCatInfoSync(&pb);
						if ( error == noErr )
						{
							items->parID = pb.hFileInfo.ioFlParID;	/* return item's parID */
							items->vRefNum = pb.hFileInfo.ioVRefNum;	/* return item's vRefNum */
							++*itemIndex;	/* prepare to get next item in directory */
							
							if ( (pb.hFileInfo.ioFlAttrib & kioFlAttribDirMask) != 0 )
							{
								if ( getDirectories )
								{
									++*actItemCount; /* keep this item */
									++items; /* point to next item */
								}
							}
							else
							{
								if ( getFiles )
								{
									++*actItemCount; /* keep this item */
									++items; /* point to next item */
								}
							}
						}
					}
				}
				else
				{
					/* it wasn't a directory */
					error = dirNFErr;
				}
			}
		}
	}
	else
	{
		/* bad itemIndex */
		error = paramErr;
	}
	
	return ( error );
}

static	OSErr	FSpGetFullPath(const FSSpec *spec,
							   short *fullPathLength,
							   Handle *fullPath)
{
	OSErr		result;
	OSErr		realResult;
	FSSpec		tempSpec;
	CInfoPBRec	pb;
	
	*fullPathLength = 0;
	*fullPath = NULL;
	
	
	/* Default to noErr */
	realResult = result = noErr;
	
	/* work around Nav Services "bug" (it returns invalid FSSpecs with empty names) */
	if ( spec->name[0] == 0 )
	{
		result = FSMakeFSSpec(spec->vRefNum, spec->parID, spec->name, &tempSpec);
	}
	else
	{
		/* Make a copy of the input FSSpec that can be modified */
		BlockMoveData(spec, &tempSpec, sizeof(FSSpec));
	}
	
	if ( result == noErr )
	{
		if ( tempSpec.parID == fsRtParID )
		{
			/* The object is a volume */
			
			/* Add a colon to make it a full pathname */
			++tempSpec.name[0];
			tempSpec.name[tempSpec.name[0]] = ':';
			
			/* We're done */
			result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
		}
		else
		{
			/* The object isn't a volume */
			
			/* Is the object a file or a directory? */
			pb.dirInfo.ioNamePtr = tempSpec.name;
			pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
			pb.dirInfo.ioDrDirID = tempSpec.parID;
			pb.dirInfo.ioFDirIndex = 0;
			result = PBGetCatInfoSync(&pb);
			// Allow file/directory name at end of path to not exist.
			realResult = result;
			if ( (result == noErr) || (result == fnfErr) )
			{
				/* if the object is a directory, append a colon so full pathname ends with colon */
				if ( (result == noErr) && (pb.hFileInfo.ioFlAttrib & kioFlAttribDirMask) != 0 )
				{
					++tempSpec.name[0];
					tempSpec.name[tempSpec.name[0]] = ':';
				}
				
				/* Put the object name in first */
				result = PtrToHand(&tempSpec.name[1], fullPath, tempSpec.name[0]);
				if ( result == noErr )
				{
					/* Get the ancestor directory names */
					pb.dirInfo.ioNamePtr = tempSpec.name;
					pb.dirInfo.ioVRefNum = tempSpec.vRefNum;
					pb.dirInfo.ioDrParID = tempSpec.parID;
					do	/* loop until we have an error or find the root directory */
					{
						pb.dirInfo.ioFDirIndex = -1;
						pb.dirInfo.ioDrDirID = pb.dirInfo.ioDrParID;
						result = PBGetCatInfoSync(&pb);
						if ( result == noErr )
						{
							/* Append colon to directory name */
							++tempSpec.name[0];
							tempSpec.name[tempSpec.name[0]] = ':';
							
							/* Add directory name to beginning of fullPath */
							(void) Munger(*fullPath, 0, NULL, 0, &tempSpec.name[1], tempSpec.name[0]);
							result = MemError();
						}
					} while ( (result == noErr) && (pb.dirInfo.ioDrDirID != fsRtDirID) );
				}
			}
		}
	}
	
	if ( result == noErr )
	{
		/* Return the length */
		*fullPathLength = GetHandleSize(*fullPath);
		result = realResult;	// return realResult in case it was fnfErr
	}
	else
	{
		/* Dispose of the handle and return NULL and zero length */
		if ( *fullPath != NULL )
		{
			DisposeHandle(*fullPath);
		}
		*fullPath = NULL;
		*fullPathLength = 0;
	}
	
	return ( result );
}

extern OSErr MacPathname2FSSpec(const char *inPathname, FSSpec *outFSS)
{
	OSErr err;
	size_t len;
	char *p;
	short vRefNum;
	long dirID;
	FSSpec fss;
	
	if (inPathname == NULL || outFSS == NULL) {
		return paramErr;
	}
	
	err = HGetVol(NULL, &vRefNum, &dirID);  // default volume and directory
	if (err != noErr) return err;
	
	len = strlen(inPathname);
	
	p = strchr(inPathname, ':');
	
	if (p == inPathname  &&  len == 1) {
		err = FSMakeFSSpec(vRefNum, dirID, "\p", outFSS);
		return err;
	}
	
	if (p == NULL) {
		// Partial pathname -- filename only
		Str31 filename;
		assert(len <= 31);
		Pstrcpy(filename, inPathname);
		err = FSMakeFSSpec(vRefNum, dirID, filename, outFSS);
	} else {
		Str31 name;
		int nameLen;
		if (inPathname[0] == ':') {
			// Relative pathname including directory path
			
		} else {
			// Absolute pathname
			//Str31 volName;  // We would use Str28 if it was defined -- 27, plus 1 for ':'.
			nameLen = p - inPathname;
			assert(nameLen <= 27);
			name[0] = nameLen + 1;
			memcpy(name + 1, inPathname, nameLen + 1);  // Copy the volume name and the colon.
			err = DetermineVRefNum(name, 0, &vRefNum);
			if (err != noErr) return err;
			dirID = 2;
		}
		// vRefNum and dirID now specify the directory in which we should descend
		// the path pointed to by p (pointing to the first colon).
		p++;
		while (p != NULL && *p != '\0') {
			char *q = strchr(p, ':');
			if (q != NULL) {
				Boolean isDir;
				nameLen = q - p;
				assert(nameLen <= 31);
				name[0] = nameLen;
				memcpy(name + 1, p, nameLen);
				err = FSMakeFSSpec(vRefNum, dirID, name, &fss);
				if (err != noErr) return err;
				if (q[1] == '\0') {
					p = NULL;
					*outFSS = fss;
				} else {
					err = FSpGetDirectoryID(&fss, &dirID, &isDir);
					assert(isDir == true);
					if (err != noErr) return err;
					p = q + 1;
				}
			} else {
				q = strchr(p, '\0');  // go to end of string
				nameLen = q - p;
				assert(nameLen > 0);
				assert(nameLen <= 31);
				Pstrcpy(name, p);
				p = NULL;
				err = FSMakeFSSpec(vRefNum, dirID, name, outFSS);
			}
		}
	}
	return err;
}

extern OSErr MacFSSpec2FullPathname(const FSSpec *inFSS, char **outPathname)
{
	OSErr err;
	Handle h;
	short fullPathLength;
	static char *fullPath = NULL;
	
	if (fullPath != NULL) {
		delete [] fullPath;
		fullPath = NULL;
	}
	err = FSpGetFullPath(inFSS, &fullPathLength, &h);
	if (err != noErr) return err;
	
	assert(fullPathLength >= 2);  // An absolute pathname must be at least two chars long
	fullPath = new char [fullPathLength + 1];
	if (fullPath == NULL) {
		err = memFullErr;
	} else {
		memmove(fullPath, *h, fullPathLength);
		fullPath[fullPathLength] = '\0';
	}
	
	DisposeHandle(h);
	
	*outPathname = fullPath;
	return err;
}


END_NCBI_SCOPE


/* --------------------------------------------------------------------------
 * $Log$
 * Revision 1.8  2004/05/14 13:59:26  gorelenk
 * Added include of ncbi_pch.hpp
 *
 * Revision 1.7  2003/05/23 20:34:14  lavr
 * Show permitting Apple's notice about changes in original "MoreFiles" code
 *
 * Revision 1.6  2003/05/23 20:22:59  ucko
 * Correct author list; include Jim Luther, who wrote the MoreFiles code.
 *
 * Revision 1.4  2003/02/27 22:03:59  lebedev
 * COSErrException_Mac changed from runtime_error to exception
 *
 * Revision 1.3  2001/12/18 21:40:39  juran
 * Copy in MoreFiles fucntions that we need.
 *
 * Revision 1.2  2001/12/03 22:00:55  juran
 * Add g_Mac_SpecialEnvironment global.
 *
 * Revision 1.1  2001/11/19 18:11:08  juran
 * Implements Mac OS-specific header.
 * Inital check-in.
 *
 * ==========================================================================
 */
