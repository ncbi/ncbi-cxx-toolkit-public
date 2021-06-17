//$Id$
//  Javascript code to print authors in a table format.
//  ToDo:  make a print_authors() function so can just list names if desired,
//         and that will print the table containing author names so it's not
//         needed in each doc page

function print(t)   { document.write(t); }
function printbr(t) { document.write(t, "<br>"); }

function print_author(username, realname, datestr, hostname)
{
    var host='ncbi.nlm.nih.gov';
	if (hostname)
	    host = hostname;
	print('<tr><td>\n<address>\n');
	print_mailto(username, realname, hostname);
	print('</address>\n</td>');
	if (datestr)
        print('<td ALIGN=RIGHT><i>' + datestr.replace(/Date:/,"Last Updated: ").replace(/\$/g," ") + '</i></td>\n');
	//if (datestr && Date.parse(document.lastModified) !=0)
	//    print('<td ALIGN=RIGHT><i>' + document.lastModified + '</i></td>\n');
	print('</tr>\n');
}

//  Note:  print_mailto has hard-coded the ncbi.nlm.nih.gov host to avoid
//         accidentally sending mail elsewhere; modify as needed.

function print_mailto(username, realname, hostname)
{
    var host='ncbi.nlm.nih.gov';

    if (hostname)
	    host = hostname;
    print('<a href="mailto:' + username + '@' + host + '">' + realname + '</a>\n');
}
