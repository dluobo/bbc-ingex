The Ingex player plugin plays local MXF files and multicast addresses.
Scriptable support is not working yet e.g. for live logging web applications.

The HTML code is very simple.  For multicast:
  <embed type="application/x-ingex-player" mrl="udp://239.255.1.1:2000"
        width="945" height="576"/>
for a local MXF file:
  <embed type="application/x-ingex-player" mrl="/video/Day1/clip_v1.mxf"
        width="945" height="576"/>
Adding additional mrl= parameters will add more sources to the quad-split.
"mrl" stands for Media Resource Locator and is used by media players to
specify sources which aren't URLs (e.g. udp:// rtp:// dvd://).

The plugin is in player/IngexPlayer/mozilla-plugin/
Example HTML is in player/IngexPlayer/mozilla-plugin/test_*.html
Once built, install the resulting ingex_plugin.so into ~/.mozilla/plugins/.

Restart the browser and type about:plugins into firefox's address bar to
check it's installed.

Load a sample page (e.g. modify test_mxf.html to point to an actual file)
and you should see the Ingex player embedded inside the web page.

I've only tested this plugin with firefox but it uses the NPAPI so other
browsers which use NPAPI might work (not IE though!).
