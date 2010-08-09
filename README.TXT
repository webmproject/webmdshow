WebM Project DirectShow Filters
v0.9.10.0 - 2010/08/09
http://www.webmproject.org/
mailto:matthewjheaney@google.com


Summary
   The WebmM Project DirectShow Filters package ("webmdshow") allows you
   to encode and decode VP8 video, and write and read WebM files, all using
   Microsoft DirectShow.


Preliminaries
   A website has been set up to host artifacts associated with the WebM
   project:

      http://www.webmproject.org/

   The actual download page is here:

      http://code.google.com/p/webm/downloads/list

   Look for the "webmdshow-..." link to find the latest version of the
   filters.

   Some mailing lists have been set up to discuss WebM-related issues.
   The main page is here:

      http://www.webmproject.org/about/discuss/

   That page has instructions for subscribing to the lists.  The actual
   lists are hosted here:

      http://lists.webmproject.org/
      http://groups.google.com/a/webmproject.org

   If you find a bug, or there's a feature you want, then please file a
   report using the WebM issue tracking system:

      http://www.webmproject.org/code/bug-reporting/

   You can browse existing issues here:

      http://code.google.com/p/webm/issues/list

   You can create a new issue here:

      http://code.google.com/p/webm/issues/entry

   If you are a developer and would like to build from sources, then
   you can use Git to clone the origin, like this:

     git clone git://review.webmproject.org/webmdshow.git


Installation
   A DirectShow filter is a "COM InProc Server DLL", and so it needs to be
   registered with the system.  The delivery artifacts should include a file
   named "install_webmdshow.exe". Run that installer (either from a command
   prompt, or from a Windows Explorer shell) and it will automatically
   register the filters on your system. The DLLs are placed in the following
   directory:

     C:\Program Files\Common Files\WebM Project\webmdshow

   To uninstall, you can choose the uninstall option from the Add or Remove
   Programs control panel, or run the uninstall_webmdshow.exe in the install
   directory listed above.

   The webmdshow package also includes a couple of simple command-line
   utilities for reading and writing WebM files, that are implemented using the
   Webm DirectShow filters.  The utility PLAYWEBM (playwebm.exe) is for basic
   testing and playback of WebM files, and the utility MAKEWEBM (makewebm.exe)
   is for simple creation of WebM files.  They are not automatically installed
   by the installer, so if you'd like to use them, just extract them from the
   archive and place them in your path.


Description of Microsoft DirectShow and the WebM DirectShow Filters
   Microsoft DirectShow is a framework for manipulation of media streams on
   Windows.  Streams are constructed from "filters" that are connected via
   "pins".  You mix and match different kinds of filters in order to construct
   a stream having the characteristics you desire.

   A WebM file is a container for a video stream encoded as VP8, and an audio
   stream encoded as Vorbis.  The file itself uses the ".webm" file extension.
   The file format is a subset of the Matroska container format, with "webm"
   as the DocType.

   In order to play back WebM files in a DirectShow-based media player (such
   as Windows Media Player), you need a "source" filter that understands the
   format of the file, and "decoder" filters for each stream.

   When a media player attempts to render a file using DirectShow, it looks
   at the file extension to determine whether a source filter has been
   specifically associated with that extension.  When the WEBMSOURCE filter
   (webmsource.dll) is installed, it registers itself as the source filter
   for ".webm" files, so if things are working normally, an instance of that
   source filter will be created automatically by the DirectShow graph builder,
   and the filter will then load the file.

   (Note that this association of a file extension with a DirectShow source
   filter is distinct from the association of a file extension with a
   rendering application. The former is controlled by DirectShow, and the
   latter by the Windows shell.)

   The source filter exposes as many output pins as there are streams in the
   WebM file.  This will typically be one VP8 video stream and one Vorbis
   audio stream.  The WebM VP8DECODER filter (vp8decoder.dll) decodes the
   VP8 video stream, which should be all you need for video rendering.

   To render the audio stream, you'll need a filter that can decode Vorbis
   audio.  I usually just use the FFDShow filter, which is included in the
   Windows Essentials Codec Pack:

      http://www.mediacodec.org/

   The WEBMMUX filter (webmmux.dll) creates ("multiplexes") WebM files.  It
   accepts a VP8-encoded video input stream and a Vorbis-encoded audio input
   stream.  The VP8ENCODER filter (vp8encoder.dll) is used to encode the video,
   and is connected directly to the WEBMUX filter.

   You'll need a filter to compress the audio stream to Vorbis.  You can
   try the filters available from Xiph.org:

      http://xiph.org/dshow/

   It might be useful to have all the filters (both video and audio) necessary
   for Webm muxing and demuxing together in one downloadable package. If you
   think that this would be of interest to the WebM Project community, please
   file a bug report at the link listed above.


Playing WebM Files on Windows using DirectShow
   Once you have installed the WEBMSOURCE filter, VP8DECODER filter, and
   a Vorbis decoder filter, then you should be able to play WebM files in
   any DirectShow-based media player, such as Windows Media Player.

   If you're familiar with DirectShow, you can also build filter graphs
   using GraphEdit or GraphStudio.  (GraphEdit is part of the Windows SDK.)
   The friendly-name of each filter begins with "WebM", so they're listed
   all together in the filter list dialog.

   The webmdshow package also includes a command-line application called
   PLAYWEBM (playwebm.exe) for simple playback of WebM files. In particular
   it is useful for determining whether the necessary DirectShow filters
   for rendering WebM files have been installed on your system.  If you
   can successfully play back the WebM file using PLAYWEBM, then that
   usually indicates that the file can be played back by any other
   DirectShow-based media player.

   The PLAYWEBM command-line swiches can be specified using either short-form
   (single hyphen) or long-form (double hyphen) syntax.  Long forms are
   case-insensitive and may be abbreviated.  Long forms can also be specified
   using Windows-style (forward slash) syntax.

   If you need help, use the --help (or -h) switch, or simply run the
   app without specifying any arguments.  For more detailed information,
   use the --help switch in combination with the --verbose (or -v) switch.

   The WebM input file can be specified as the (only) command-line argument,
   or as the value of the --input (or -i) switch.  If you do not specify
   any other switches, PLAYWEBM uses IGraphBuilder::RenderFile to build the
   graph automatically.  This most closely mimics what other, more general
   DirectShow-based players do to render an arbitrary file (so it's a good
   test of whether your system has been configured correctly).

   If you use the --source (or -s) switch, PLAYWEBM will explicitly create
   an instance of the WEBMSOURCE filter, load the file using that filter,
   and then render each output pin.

   With the --splitter (or -S) switch, PLAYWEBM will load the file using
   the AsyncReader filter, explicitly create an instance of the WEBMSPLIT
   filter, connect the two filters and then render each output pin.

   If you specify the --list (or -l) switch, the app will print the values
   of the command-line arguments, and then immediately terminate.  With the
   --verbose (or -v) switch also specified, the app expands the input file
   name to its full path value.

   Whenever you're in doubt, always try the --list and --verbose options
   together, to verify that PLAYWEM has interpreted the command-line
   argument values as you have intended.

   The --version (or -V) switch is used to print the value of the
   app's version.  If you have a problem with PLAYWEBM, please include
   the app's version value in your bug report.


Authoring WebM Files On Windows Using DirectShow
   The webmdshow package includes a VP8 encoder filter and a WebM muxer
   filter, which enable you to create WebM files by transcoding files
   that are formatted or encoded differently. (A Vorbis encoder filter
   is also available from Xiph.org, as explained above.)

   The package also includes basic utility, MAKEWEBM (makewebm.exe),
   that uses those filters to create WebM files.  If you have all the
   requisite filters for parsing input files and decoding streams,
   then the application should just work.  Specify the name of the
   input file on the command-line and MAKEWEBM will make a WebM file
   with the same basename but with extension ".webm", like this:

      $ makewebm xyz.avi

   This creates file xyz.webm.

   The MAKEWEBM app has a few command-line switches to modify its
   behavior.  The switches are described here, but you can always
   get the list by running MAKEWEBM without any arguments, or by
   specifying the --help (or -h) switch.

   Switches can be specified using short-form syntax (preceded by
   a single hypen), long-form syntax (preceded by a pair of hyphens),
   or using Windows-style syntax (preceded by a forward slash).
   Switches and arguments and can be specified in any order.  Switches
   can be abbrievated, and long-form options are case-insensitive.

   The value for long-form switches can be specified as part of the
   switch itself by separating the switch from the value using an
   equal sign.  If no equal sign is present, then the value stands
   by itself as the next command-line argument.  For example:

      makewebm --input=XYZ.AVI ...
      makewebm --input XYZ.AVI ...

   The value for Windows-style switches can be specified as part of
   the switch itself by separating the switch from the value using
   a colon.  If no colon is present, then the value stands by itself
   as the next command-line argument.  For example:

      makewebm /input:XYZ.AVI ...
      makewebm /input XYZ.AVI ...

   Short-form switches that accept a value require that the value
   appear as the next command-line argument.  The reason is that you're
   allowed to use a long-form spelling for some switches, for example:

      makewebm -i XYZ.AVI  ...
      makewebm -input XYZ.AVI  ...

   Where there is ambiguity among switches (e.g. "/ver"), the precedence
   of switches is determined by their order in the help (usage) display.

   MAKEWEBM requires that the input filename be specified. The input file
   can be specified using the --input (or -i) switch, or as the (first)
   argument.

   The output file can specified using the --output (or -o) switch,
   or as the (second) argument.  This names the resulting WebM file.
   If you don't specify the output filename value explicitly, its value
   is synthesized from the input filename value.  So the following commands
   are all equivalent:

      makewebm -i xyz.avi -o xyz.webm
      makewebm -i xyz.avi xyz.webm
      makewebm xyz.avi -o xyz.webm
      makewebm xyz.avi xyz.webm
      makewebm -i xyz.avi
      makewebm xyz.avi

   MAKEWEBM only transcodes input streams if they aren't encoded as
   VP8 (for video) or Vorbis (for audio).  If a stream already has
   that encoding, then it is connected directly to the WEBMMUX filter
   as is, without being re-encoded.

   If MAKEWEBM needs to encode to Vorbis audio, and it cannot find a
   filter to perform the encoding, then by default it will make a
   WebM file without audio.  If you want the app to terminate if it
   cannot also encode the audio stream, then specify the
   --require-audio switch.

   The --list (or -l) switch will print the values of the command-line
   arguments and switches, without building the actual graph.  If you
   also specify the --verbose (or -v) switch, the input and output
   filenames are expanded to their full paths.

   If you specify the --verbose (or -v) switch (without also specifying
   the --list switch), then as the graph is being built the app prints
   the media subtype and format of the pins being connected.  This is
   helpful for determining exactly how the input streams are encoded.

   The --version (or -V) switch prints the value of the app's version
   resource. If you submit a bug report, please include the version
   value of your copy of the MAKEWEBM app in your report.

   By default, while MAKWEBM is running, it prints the progress of the
   mux by over-writing the same line on standard output.  (The value
   it writes is the current position in the output file, expressed as
   time, in units of seconds.)  This behavior can be changed by
   specifying the --script-mode switch, which causes progress to be
   written to standard output on a new line each time, and in a format
   that is easily parseable.  This switch is intended to be used when
   MAKEWEBM app is being called from within a script that is capturing
   text written to standard output by the app.  See the comments at
   the top of the script VBSMAKEWEBM.VBS that accompanies the delivery
   artifacts for an example of how to use this option.

   The VP8ENCODER filter (vp8encoder.dll) has a custom IDL-based interface
   that enables you to configure the encoding.  MAKEWEBM includes several
   command-line switches that allow you to manipulate the VP8ENCODER
   filter.  Run the MAKEWEBM app with the --help switch to get a complete
   list of switches.  The switches for VP8 encoding match the method names
   in the IDL file for the VP8 encoder.  See the file "vp8encoder.idl"
   among the delivery artifacts (it's in the IDL folder of the source
   tree) for a full description of each configuration parameter.

VP8 Encoder Filter Notes

   The VP8 encoder filter (as with the VP8 encoder library) only accepts
   YUV formats as input.  If you have a file or capture device with
   RGB as output, you'll need a separate filter to convert from RGB to YUV.
   The ffdshow filter will do such a conversion, but it might be disabled
   by default.  To enable it, open the Properties page for the ffdshow
   Video Decoder filter, and under the Codecs menu, find the Raw video item.
   Change the Decoder option so that it's not "disabled" (the values
   "all RGB" or "all supported" both seem to work), and then an instance
   of that filter will be created automatically when RGB-to-YUV conversion
   is required.

   The VP8 encoder also has a property page, so if you're using GraphEdit
   or similar you can modify encoder settings that way.  It works as follows.
   The filter has a settings cache.  When you make a change to a settings
   value (via the IVP8Encoder interface), this actually changes the value in
   the cache. If the graph is stopped, then the settings are applied only
   later (and automatically), during the transition from stopped to paused or
   running. If the graph is not stopped (and you're using the IDL interface
   directly), then you must apply the settings by calling
   IVP8Encoder::ApplySettings. The only settings that are actually applied
   are those settings that have a value that is not "out-of-band", which
   typically means equal or greater than 0.  For settings that haven't been
   set (the cached setting still has its default out-of-band value, -1), then
   the encoder uses the default supplied by the vpx encoder library itself.
   
   When the property page is first loaded, it queries the cache and displays
   any non-default settings.  Edit controls and combo boxes for settings
   that have a default value are left blank.  When you click on the OK or
   Apply buttons, the property page modifies the cache settings that
   correspond to controls with non-empty values.

   The Clear button on the property page dialog window simply clears
   all of the edit controls.  If you were to Apply the values on the
   page in this state it would have no effect, because there are no controls
   with non-empty values.  This is useful if you want to selectively change
   a value without disturbing values that already exist in the cache.

   The Reload button re-initializes the controls on the dialog window, the
   same as if the property page had been dismissed via the Cancel button
   and then immediately re-created.  This is useful if you've made changes
   to the dialog but decide to throw the values away and start over.

   The Reset button calls IVP8Encoder::ResetSettings to re-initialize
   all of the settings in the cache to their default (out-of-band) values,
   and sets the controls to empty values.  This has the effect of throwing
   away settings in the cache and starting over.
