
"GUID-Finder" 
An IDA Pro 5.xx plug-in to find GUID/UUIDs.
By Sirmabus  V: 1.0B

The COM side of RE'ing (at least with "dead listing") can be pretty elusive.
With this you can at least partially glean what interfaces and classes a target is 
using.

This plug-in scans the IDB for class and interfaces GUIDs and creates the matching 
structure with label.  IDA can find these on it's own, but it often misses them, so
this can fill in the gap. 
Plus this plug-in allows you to easily add custom declarations, and is handy to do
a general audit for such GUIDs.


This is based Frank Boldewin's IDA Python script that you can find here:
http://www.openrce.org/downloads/details/250/ClassAndInterfaceToNames
or off his home page:
http://www.reconstructer.org/code/ClassAndInterfaceToNames.zip

It's a great utility, I found me self using it regularly. But I wanted one that 
wasn't dependant on IDA Python, and one that might be a bit faster.
I've made some enhancements too (see below).

Some interesting reading:
http://en.wikipedia.org/wiki/Globally_Unique_Identifier
http://en.wikipedia.org/wiki/UUID


[Install]
Copy the plug-in to your IDA Pro 5.xx "plugins" directory. 
Edit your "plugins.cfg" with a hotkey to run it, etc., as you would install any other
plug-in.  See the IDA docs for more help on this.

Create a subdirectory (in your "plugins") called "GUID-Finder" and put the two text 
files: "Interfaces.txt" and "Classes.txt" in it. If you want you can put the plug-in
in there as well (just edit your "plugins.cfg" accordingly).


[How to run it]
Just invoke it using your selected IDA hot-key, or from "Edit->Plugins".
Normally you will want to keep the ""Skip code segments for speed"" check box checked, 
because it can make a big difference in the run time. With unchecked, code segments are 
also scanned.  You'll want to scan the code to if the target is a Delphi, or others where 
data tends to be code/.text segment, or if you just want to be more thorough.

It might take some time to scan everything depending on the size of the IDB your computer,
etc..

When it's done, you should see a list of interfaces and classes in the IDA log window.
If you want to go look at a particular entry to RE (to look at xrefs, etc.) just click on 
the line and IDA will jump to it.


[How it works]
1. Loads in GUID/UUID defs for the two text files "Interfaces.txt" and "Classes.txt".
   A little enhancement here over Frank's format, you can have blank lines and have
   comments prefixed with '#' (first char, whole line only. Not a very forgiving parser).
   
   In the source is "DumpLib", a utility I created to parse LIB files (like "uuid.lib")
   to gather more GUIDs. As of this build, it's a collection of Frank's original UUIDs
   plus all the ones to be found in VS2005 libraries along with DirectX 9.1,.
   
   There could be more explicitly created in header (.h/.hpp) files but have yet to make
   a utility to parse them.
   
   If you want to add custom GUID defines (from 3rd party software, etc.), just edit 
   these text files manually.
   
2. After it loads in the defs, the plug-in iterates through all segments in your currently
   open IDB. By default it will skip code/".text" segments, and import/export segments for
   speed.  Usually you find GUIDs in the ".rdata", and ".data" segments.
   
   I originally intended to sort all the GUIDs by similarity and search with partial wild 
   cards for speed.  If you take a look at the GUID defs you will see that many GUIDs share 
   common numbers that often differ only be the least significant digits ("Data4").
   At least in theory, searching for groups wild cards should make searching faster.
   Maybe next version..

   
[Known problems/issues/limitations]
1. If a given GUID 16byte def just so happens to match something that is not really a GUID, 
   the plug-in will try to convert it to one regardless (another reason not to run it 
   over code sections).  So far I have not found this to be much of issue, although it could 
   be.  Could add a confirm dialog for each to let the user decide.
   
2. Some GUID set operations will fail.  This is usually because something is bad/wrong at the
   particular address; like a partial code def, or incorrect xref.
   The plug-in will display most of these errors in the IDA log window for manual correction.
   
3. TODO: Other GUID times like "DIID", "LIBID", "CATID", usefull?

   
-Sirmabus


Terms of Use
------------
This software is provided "as is", without any guarantee made as to its
suitability or fitness for any particular use. It may contain bugs, so use
this software is at your own risk.  The author(s) no responsibly for any 
damage that may unintentionally be caused through its use.   
   
