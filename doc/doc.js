function print(t) { document.write(t); }
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
	    print('<td ALIGN=RIGHT><i>' + datestr.replace(/\$Date$/," ") + '</i></td>\n');
	//if (datestr && Date.parse(document.lastModified) !=0)
	//    print('<td ALIGN=RIGHT><i>' + document.lastModified + '</i></td>\n');
	print('</tr>\n');
}
function print_mailto(username, realname, hostname)
{
    var host='ncbi.nlm.nih.gov';

    username='cpp-core';
    if (hostname)
	    host = hostname;
    print('<a href="mailto:' + username + '@' + host + '">' + realname + '</a>\n');
}
