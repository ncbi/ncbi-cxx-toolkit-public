+============================ === ==== ---- -- -
| FM SiteSearch Light Ver 1.0
+=========== === ==== ---- -- -

|This text file is best viewed in notepad
+=========== === ==== ---- -- -

COPYRIGHT NOTICE: Copyright 2002 FocalMedia.Net All Rights Reserved.

This program is free to use for commercial and non commercial use.
The scripts may be used and modified by anyone, so long as this copyright 
notices remain intact.

Selling the code for this program without prior written consent 
is expressly forbidden. You may redistribute this program as long
as this copyright notice stay's intact and the code in it's original
format. Redistribution should only take place with all the files in
it's original .zip format where it was obtained. In all cases 
copyright, header and original zip file must be original.

This program is distributed "as is" and without warranty of any
kind, either express or implied. In no event shall the liability 
of FocalMedia.Net for any damages, losses and/or causes of action 
exceed the total amount paid by the user for this software.


=================== === ==== ---- -- -
| WHaT THe SCRiPT eXaCTLy DoeS
============ === ==== ---- -- -

'FM Site Search Light' is a web site search script. Use it to add your 
own web site search engine to your web site. Users enter keywords
and 'FM Site Search' searches through your HTML files to find pages
matching the keywords. 'FM Site Search' will also give relevance to
search terms so that you will get the most relevant matched pages
first.

Please note that 'FM Site Search' has been created with the small
to medium sized web site in mind. If your web site has more than
500 pages to be searched at a time, it is not recommended to use
FM Site Search. Use FM SiteSearch Pro, located at:
http://www.focalmedia.net/fmsitesearchpro.html

=================== === ==== ---- -- -
| Is THiS SCRiPT FRee?
============ === ==== ---- -- -

Yes, it's 100% FREE. You can do with it what you like, except make
money off it. If you use it, please place our button anywhere on your 
site linking to FocalMedia.Net - Drop us a note at support@focalmedia.net
and we will do the same in return for you.

Placing the button on your site is not required but you will aid us in 
spreading the news about our scripts and it will contribute to the
internet community in general, because most of our scripts are free, 
except the high powered ones.

The following HTML code is code for a 33x88pixels button that you can 
place anywhere on your web site. The button has been designed to look
attractive.

<a href="http://www.focalmedia.net">
<img src="http://www.focalmedia.net/fmbutton.gif" width="88" height="33" border="0"></a>

If you find the script to be usefull we would like to hear about it. :)


============ === ==== ---- -- -
| ReQuiReMeNTS
======= === == --- -- - 

You will need a unix or windows platform and CGI Priviledges on your webserver


============ === ==== ---- -- -
| iNSTaLLaTioN
============ === ==== ---- -- -

1. Unzip the zip file in which the files came. Open fmsearch.cgi with your 
favourite text editor. Make sure that the first line in the script points 
to your Perl intepreter on your server or hosting account.

Example: #!/usr/bin/perl


2. Change the variables in fmsearch.cgi to point to the correct locations of 
your server. (You'll find these where the words 'Configuration - Edit below' 
is written in the script.)

[Here are the variables that should be changed:]
	
$fmsearch -> This is the URL location of fmsearch.cgi on your server.

@searchcrit -> This is the directories that contains HTML files that you would like to 
be searched. Seperate directories via comma and the " character.

@weburls -> This is URL paths to the server directories you specified in @searchcrit.
Enter the URLS in the same order as in the @searchcrit variable. The first url 
path has to match the first server path that you specified in @searchcrit. The 
second url path has to match the second server path that you specified in 
@searchcrit and so forth.

@searchext -> This is the extensions of files to be searched.

$listperpage -> This is the number of results you would like to list per page.

After you completed this operation, remember to save fmsearch.cgi as a unix file if 
you're planning on using it on a unix machine. If you're planning to use it on a 
windows machine, remember to save it as a windows file.

fmsearch.cgi should now be configured!

4. Upload fmsearch.cgi, listings.html and main_layout.html to your cgi-bin
directory and chmod 755 all the files.

5. Ok, by now the scripts should be ready for action, but you will have to add the 
search box HTML code to your web site so that users can search your site. Open
search.html for an example of the search box that you will need to place somewhere
in your web page(s). Put the search box somwhere in your web pages or web site and 
replace the URL path with the correct URL path to fmsearch.cgi

