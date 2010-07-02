var MJH = {};

MJH.SetVersion = function (objFile, objVersion, intReadOnly) {
    var out = WScript.StdOut;
    var objText  = objFile.OpenAsTextStream(1);  //read-only for now
    var strLines = [];   //array of lines of text
    var idx      = 0;
    var major, minor, revision, build;

    var bFileVersion = false;
    var bProjectVersion = false;
    var bFileInfo = false;
    var bProductInfo = false;

    while (!objText.atEndOfStream) {
        strLine = objText.readLine();
        //out.WriteLine(strLine);

        if (strLine.match(/FILEVERSION\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$/)) {
            if (major === undefined) {
                major = RegExp.$1;
                minor = RegExp.$2;
                revision = RegExp.$3;
                build = RegExp.$4;
            } else {
                if (RegExp.$1 !== major) {
                    out.WriteLine("FILEVERSION has bad major number.");
                    return;
                }

                if (RegExp.$2 !== minor) {
                    out.WriteLine("FILEVERSION has bad minor number.");
                    return;
                }

                if (RegExp.$3 !== revision) {
                    out.WriteLine("FILEVERSION has bad revision number.");
                    return;
                }

                if (RegExp.$4 !== build) {
                    out.WriteLine("FILEVERSION has bad build number.");
                    return;
                }
            }

            bFileVersion = true;

            out.Write("old:");
            out.WriteLine(strLine);

            if (intReadOnly >= 0) {
                strLine =
                    RegExp["$`"] +
                    "FILEVERSION " +
                    objVersion.toMajor(major) +
                    "," +
                    objVersion.toMinor(minor) +
                    "," +
                    objVersion.toRevision(revision) +
                    "," +
                    objVersion.toBuild(build);

                out.Write("new:");
                out.WriteLine(strLine);
                out.WriteLine();
            }

        } else if (strLine.match(/PRODUCTVERSION\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$/)) {
            if (major === undefined) {
                major = RegExp.$1;
                minor = RegExp.$2;
                revision = RegExp.$3;
                build = RegExp.$4;
            } else {
                if (RegExp.$1 !== major) {
                    out.WriteLine("PRODUCTVERSION has bad major number.");
                    return;
                }

                if (RegExp.$2 !== minor) {
                    out.WriteLine("PRODUCTVERSION has bad minor number.");
                    return;
                }

                if (RegExp.$3 !== revision) {
                    out.WriteLine("PRODUCTVERSION has bad revision number.");
                    return;
                }

                if (RegExp.$4 !== build) {
                    out.WriteLine("PRODUCTVERSION has bad build number.");
                    return;
                }
            }

            bProductVersion = true;

            out.Write("old:");
            out.WriteLine(strLine);

            if (intReadOnly >= 0) {
                strLine =
                    RegExp["$`"] +
                    "PRODUCTVERSION " +
                    objVersion.toMajor(major) +
                    "," +
                    objVersion.toMinor(minor) +
                    "," +
                    objVersion.toRevision(revision) +
                    "," +
                    objVersion.toBuild(build);

                out.Write("new:");
                out.WriteLine(strLine);
                out.WriteLine();
            }

        } else if (strLine.match(/VALUE\s+\"FileVersion\"\s*,\s*\"\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\"\s*$/)) {
            if (major === undefined) {
                out.WriteLine("version not defined in info section");
                return;
            }

            if (RegExp.$1 !== major) {
                out.WriteLine("\"FileVersion\" has bad major number.");
                return;
            }

            if (RegExp.$2 !== minor) {
                out.WriteLine("\"FileVersion\" has bad minor number.");
                return;
            }

            if (RegExp.$3 !== revision) {
                out.WriteLine("\"FileVersion\" has bad revision number.");
                return;
            }

            if (RegExp.$4 !== build) {
                out.WriteLine("\"FileVersion\" has bad build number.");
                return;
            }

            bFileInfo = true;

            out.Write("old:");
            out.WriteLine(strLine);

            if (intReadOnly >= 0) {
                strLine =
                    RegExp["$`"] +
                    "VALUE \"FileVersion\", \"" +
                    objVersion.toMajor(major) +
                    ", " +
                    objVersion.toMinor(minor) +
                    ", " +
                    objVersion.toRevision(revision) +
                    ", " +
                    objVersion.toBuild(build) +
                    "\"";

                out.Write("new:");
                out.WriteLine(strLine);
                out.WriteLine();
            }

        } else if (strLine.match(/VALUE\s+\"ProductVersion\"\s*,\s*\"\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\"\s*$/)) {
            if (major === undefined) {
                out.WriteLine("version not defined in info section");
                return;
            }

            if (RegExp.$1 !== major) {
                out.WriteLine("\"ProductVersion\" has bad major number.");
                return;
            }

            if (RegExp.$2 !== minor) {
                out.WriteLine("\"ProductVersion\" has bad minor number.");
                return;
            }

            if (RegExp.$3 !== revision) {
                out.WriteLine("\"ProductVersion\" has bad revision number.");
                return;
            }

            if (RegExp.$4 !== build) {
                out.WriteLine("\"ProductVersion\" has bad build number.");
                return;
            }

            bProductInfo = true;

            out.Write("old:");
            out.WriteLine(strLine);

            if (intReadOnly >= 0) {
                strLine =
                    RegExp["$`"] +
                    "VALUE \"ProductVersion\", \"" +
                    objVersion.toMajor(major) +
                    ", " +
                    objVersion.toMinor(minor) +
                    ", " +
                    objVersion.toRevision(revision) +
                    ", " +
                    objVersion.toBuild(build) +
                    "\"";

                out.Write("new:");
                out.WriteLine(strLine);
                out.WriteLine();
            }

        }

        strLines[idx++] = strLine;
    }

    objText.Close();

    if (!bFileVersion) {
        out.WriteLine("FILEVERSION not found");
        return;
    }

    if (!bProductVersion) {
        out.WriteLine("PRODUCTVERSION not found");
        return;
    }

    if (!bFileInfo) {
        out.WriteLine("\"FileVersion\" info not found");
        return;
    }

    if (!bProductInfo) {
        out.WriteLine("\"ProductVersion\" info not found");
        return;
    }

    if (intReadOnly < 0) {
        return;
    }

    if (intReadOnly === 0) {
        out.WriteLine("No changes made (file was opened for reading only).");
        return;
    }

    out.WriteLine("Opening file for writing.");

    objText = objFile.OpenAsTextStream(2);  //2 = for writing

    for (idx = 0; idx < strLines.length; ++idx) {
        objText.WriteLine(strLines[idx]);
    }

    objText.Close();

    out.WriteLine("Changes were made to file.");
};


MJH.Main = function() {
    var out = WScript.StdOut;
    var objFSO = new ActiveXObject("Scripting.FileSystemObject");
    var objArgs = WScript.Arguments;
    var objRootFolder;
    var objVersion;
    var intReadWrite = -1;  //read-only by default

    if (objArgs.Length > 2) {
        out.WriteLine("Too many arguments.");
        return;
    }

    if (objArgs.Length >= 1) {
        out.Write("arg[0]: ");
        out.WriteLine(objArgs(0));

        if (!objArgs(0).match(/(\+?\d+)\.(\+?\d+)\.(\+?\d+)\.(\+?\d+)/)) {
            out.WriteLine("bad version value");
            return;
        }

        objVersion = function (major, minor, revision, build) {
            out.Write("version: major=");
            out.Write(minor);
            out.Write(" minor=");
            out.Write(minor);
            out.Write(" revision=");
            out.Write(revision);
            out.Write(" build=");
            out.Write(build);
            out.WriteLine();
            out.WriteLine();

            function transform(str, pat) {
                var strnum, patnum, result;

                if (pat.charAt(0) !== "+") {
                    return pat;
                }

                strnum = parseInt(str, 10);
                patnum = parseInt(pat.slice(1), 10);
                result = strnum + patnum;
                return result.toString();
            }

            return {
                toMajor : function (str) {
                    return transform(str, major);
                },
                toMinor : function (str) {
                    return transform(str, minor);
                },
                toRevision : function (str) {
                    return transform(str, revision);
                },
                toBuild : function (str) {
                    return transform(str, build);
                }
            };
        }(RegExp.$1, RegExp.$2, RegExp.$3, RegExp.$4);

        intReadWrite = 0;
    }

    if (objArgs.Length >= 2) {
        out.Write("arg[1]: ");
        out.WriteLine(objArgs(1));

        intReadWrite = parseInt(objArgs(1), 10);

        if (isNaN(intReadWrite)) {
            out.WriteLine("read-write flag has bad syntax");
            return;
        }

        if (intReadWrite < 0) {
            out.WriteLine("read-write flag is out-of-range (too small)");
            return;
        }

        if (intReadWrite > 1) {
            out.WriteLine("read-write flag is out-of-range (too large)");
            return;
        }
    }

    objRootFolder = objFSO.GetFolder(".");
    out.Write("solution folder: ");
    out.WriteLine(objRootFolder.Path);
    out.WriteLine();

    function process(strName) {
        var strPath = objFSO.BuildPath(objRootFolder.Path, strName);
        var objFolder, objFile;

        out.Write("project folder: ");
        out.WriteLine(strPath);

        if (!objFSO.FolderExists(strPath)) {
            out.WriteLine("project folder does not exist");
            return;
        }

        objFolder = objFSO.GetFolder(strPath);
        strPath = objFSO.BuildPath(objFolder.Path, strName + ".rc");

        if (!objFSO.FileExists(strPath)) {
            out.WriteLine("resource file not found");
            return;
        }

        objFile = objFSO.GetFile(strPath);

        out.Write("path: ");
        out.WriteLine(objFile.Path);
        out.Write("name: ");
        out.WriteLine(objFile.Name);
        out.WriteLine();

        MJH.SetVersion(objFile, objVersion, intReadWrite);
        out.WriteLine();
    }

    process("makewebm");
    process("playwebm");
    process("vp8decoder");
    process("vp8encoder");
    process("webmmux");
    process("webmsource");
    process("webmsplit");
};


MJH.Main();
