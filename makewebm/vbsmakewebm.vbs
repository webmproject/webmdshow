REM VBSMAKEWEBM.VBS
REM Author: Matthew Heaney
REM mailto:matthewjheaney@google.com
REM
REM This script demonstrates how to call the MAKEWEBM.EXE app from
REM a Visual Basic script.
REM
REM This particular script synthesizes the name of the output file
REM from the name of the input file (although the app can do that
REM itself already), and puts it in the user's My Videos directory.
REM 
REM The script captures the text sent to stdout by the app, parses
REM it using a regex, and then writes the value of the time to 
REM stdout from the script itself.  This is exactly what the app does.
REM
REM To run this script, you can specify the script filename as an argument
REM to the cscript interpreter:
REM   cscript //nologo vbsmakewebm.vbs <path to input file>
REM
REM Alternatively, you can invoke the cscript interpreter implicitly.
REM First configure your environment to make cscript the default:
REM  cscript //H:cscript
REM This configuration only needs to be done once.  Now you can
REM run the script directly:
REM   vbsmakewebm <path to input file>


Option Explicit

Dim i, str  'scratch
Dim objArgs
Dim objFSO, objSrc, objTgt
Dim objShell, objExec, objStdOut
Dim strCmd, strLine, strTime, strDuration
Dim strInput, strOutput
Dim reTime, reDuration, mm

'We accept a single command-line argument, which is the path to
'the input file.

Set objArgs = Wscript.Arguments

If objArgs.Count <= 0 Then
   Wscript.Echo "too few arguments"
   Wscript.Quit
End If

If objArgs.Count > 1 Then
   Wscript.Echo "too many arguments"
   Wscript.Quit
End If

Set objFSO = CreateObject("Scripting.FileSystemObject")
Set objShell = CreateObject("Wscript.Shell")

'Find the path to the makewebm executable.
'First try the folder that contains the script.

str = objFSO.GetParentFolderName(Wscript.ScriptFullName)
str = objFSO.BuildPath(str, "makewebm.exe")

If Not objFSO.FileExists(str) Then
   'Try C:\bin
   str = objFSO.BuildPath("C:\bin", "makewebm.exe")
End If

If Not objFSO.FileExists(str) Then
   Wscript.Echo "Unable to find path to MAKEWEBM.EXE app"
   Wscript.Quit
End If

'We have found the path to the exe, so we use that to 
'form the command string, which we assemble incrementally.

strCmd = """" & str & """ --script-mode"  'exe

strInput = objArgs(0)  'the path to the input file

If Not objFSO.FileExists(strInput) Then
   Wscript.Echo "Input file not found"
   Wscript.Quit
End If   

'We have confirmed the existence of the input file,
'so we append it to the command string.

strCmd = strCmd & " --input=""" & strInput & """"  'exe + input

'We next find the output file path.  We first locate the folder
'named "My Videos" (creating it if necessary) under "My Documents".

str = objShell.SpecialFolders("MyDocuments")

If Not objFSO.FolderExists(str) Then
   Wscript.Echo "Documents folder not found"
   Wscript.Quit
End If

Set objTgt = objFSO.GetFolder(str)

str = objFSO.BuildPath(objTgt.Path, "My Videos")

If objFSO.FolderExists(str) Then
   Set objTgt = objFSO.GetFolder(str)
Else
   Set objTgt = objFSO.CreateFolder(str)
End If

'We have the folder for the output file.  Next we name
'the output file based on the basename of the input file.

strOutput = objFSO.GetBaseName(strInput) & ".webm"    'just name
strOutput = objFSO.BuildPath(objTgt.Path, strOutput)  'full path

'Finally we append the output file to the command string.

strCmd = strCmd & " --output=""" & strOutput & """"

'For Debugging:
'strCmd = strCmd & " --list --verbose"
'Wscript.Echo strCmd
'Wscript.Quit

'adjust deadline value as you see fit
strCmd = strCmd & " --deadline=realtime"

'We create a regex that matches the output of the app
'when it is run in script mode.

Set reTime = New RegExp
reTime.Pattern = "TIME=(\S+)"

Set reDuration = New RegExp
reDuration.Pattern = "DURATION=(\S+)"

'Finally we run the app, using the command line we assembled incrementally.

Set objExec = objShell.Exec(strCmd)
Set objStdOut = objExec.StdOut

'We use AtEndOfStream to terminate the loop.  The end of stream is
'reached when the app terminates, which is exactly what we want.

Do Until objStdOut.AtEndOfStream
   'Consume a line of output from the app:
   strLine = objStdOut.ReadLine

   'Parse the line to find the time
   Set mm = reTime.Execute(strLine)

   If mm.Count <> 1 Then 'regex error
      Wscript.StdOut.WriteLine

      'TODO: find a better way to handle this:
      Wscript.StdOut.WriteLine "TEXT: " & strLine

   Else
      'Write the time value to stdout.  This duplicates the behavior
      'of what the app would do, if it were run directly from the shell.
      Wscript.StdOut.Write vbCR
      Wscript.StdOut.Write mm(0).Submatches(0)

      'Parse the line to find the duration
      Set mm = reDuration.Execute(strLine)

      If mm.Count = 1 Then
         Wscript.StdOut.Write "/"
         Wscript.StdOut.Write mm(0).Submatches(0)
      End If
   End If
Loop

Wscript.StdOut.WriteLine
