<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01//EN" "http://www.w3.org/TR/html4/strict.dtd">
<html>
<head>
<title>studio Nightly Builds</title>
 <STYLE TYPE="text/css">
 	TD.red { background-color: #f98ce1 }
 	TD.yellow { background-color: yellow }
 	TD.orange { background-color: #ffa500 }
 	TD.blue { background-color: #add8e6 }
 	TD.green { background-color: #90ee90 }
 	TD.gray { background-color: gray }
 </STYLE>
<meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
</head>

<body>

<TABLE BORDER="1" CELLSPACING="0">
 <caption><em>Nightly build and test results for CVS trunk</em></caption>

<!-- PHP - Read in the table rows from the nightly build script generated file -->
<?readfile("nightly_trunk_rows")?>
<!-- end PHP -->
</TABLE>

<p>
To download a CVS snapshot you must use cvs.  The required setup for using Ingex's cvs is described <a href="http://sourceforge.net/cvs/?group_id=157898">here</a>.  studio is a subdirectory of the cvs trunk.<br>
E.g. to checkout a working copy corresponding to the 2007/05/15 17:47:21 snapshot use:
<pre>        TZ=UTC cvs checkout -P -D '2007-05-15 17:47:21' ingex</pre>
<p>
All snapshot times are UTC so set your TZ environment variable as shown in the example above to get the expected result.
<hr>
<p>
<TABLE>
 <caption><strong>Table Legend</strong></caption>
 	<TR>	<TD class="green">&nbsp;&nabla;<TD>Download snapshot tarball
 	<TR>	<TD class="green">MT<TD>Make and tests succeeded
 	<TR>	<TD class="blue">MT<TD>Make succeeded, tests succeeded<br>with some tests not executed
 	<TR>	<TD class="yellow">MT<TD>Make succeeded but tests failed
 	<TR>	<TD class="orange">MT<TD>Make and tests succeeded but<br>package/rpm build and install failed
 	<TR>	<TD class="red">M<TD>Make failed
	<TR>	<TD class="gray">&nbsp;&nbsp;<TD>Failed to start build (could be network error<br>or compile farm node offline)
</TABLE>

<p>
 <a href="http://validator.w3.org/check?uri=referer"><img
	style="border:0;width:88px;height:31px"
      src="http://www.w3.org/Icons/valid-html401"
      alt="Valid HTML 4.01!" height="31" width="88"></a>
 <a href="http://jigsaw.w3.org/css-validator/check/referer"><img
	style="border:0;width:88px;height:31px"
       src="http://jigsaw.w3.org/css-validator/images/vcss" 
       alt="Valid CSS!"></a>
</p>
</body>
</html>
