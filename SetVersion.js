var MJH = {};

MJH.SetVersion = function (objFile, objVersion) {
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
            //out.WriteLine("FILEVERSION found");

            out.Write("old:");
            out.WriteLine(strLine);

            strLine =
                RegExp["$`"] +
                "FILEVERSION " +
                objVersion.major +
                "," +
                objVersion.minor +
                "," +
                objVersion.revision +
                "," +
                objVersion.build;

            out.Write("new:");
            out.WriteLine(strLine);
            out.WriteLine();

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
            //out.WriteLine("PRODUCTVERSION found");

            out.Write("old:");
            out.WriteLine(strLine);

            strLine =
                RegExp["$`"] +
                "PRODUCTVERSION " +
                objVersion.major +
                "," +
                objVersion.minor +
                "," +
                objVersion.revision +
                "," +
                objVersion.build;

            out.Write("new:");
            out.WriteLine(strLine);
            out.WriteLine();

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
            //out.WriteLine("\"FileVersion\" info found");

            out.Write("old:");
            out.WriteLine(strLine);

            strLine =
                RegExp["$`"] +
                "VALUE \"FileVersion\", \"" +
                objVersion.major +
                ", " +
                objVersion.minor +
                ", " +
                objVersion.revision +
                ", " +
                objVersion.build +
                "\"";

            out.Write("new:");
            out.WriteLine(strLine);
            out.WriteLine();

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
            //out.WriteLine("\"ProductVersion\" info found");
            out.Write("old:");
            out.WriteLine(strLine);

            strLine =
                RegExp["$`"] +
                "VALUE \"ProductVersion\", \"" +
                objVersion.major +
                ", " +
                objVersion.minor +
                ", " +
                objVersion.revision +
                ", " +
                objVersion.build +
                "\"";

            out.Write("new:");
            out.WriteLine(strLine);
            out.WriteLine();

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

//    objText = objFile.OpenAsTextStream(2);  //2 = for writing
//
//    for (idx = 0; idx < strLines.length; ++idx) {
//        objText.WriteLine(strLines[idx]);
//    }
//
//    objText.Close();
};


MJH.Main = function() {
    var out = WScript.StdOut;
    var objFSO = new ActiveXObject("Scripting.FileSystemObject");
    var objArgs = WScript.Arguments;
    var objRootFolder;
    var objVersion;

    if (objArgs.Length <= 0) {
        out.WriteLine("Too few arguments.");
        return;
    }

    if (objArgs.Length > 2) {
        out.WriteLine("Too many arguments.");
        return;
    }

    out.Write("arg[0]: ");
    out.WriteLine(objArgs(0));

    if (!objArgs(0).match(/(\d+)\.(\d+)\.(\d+)\.(\d+)/)) {
        out.WriteLine("bad version value");
        return;
    }

    objVersion = {
        major : RegExp.$1,
        minor : RegExp.$2,
        revision : RegExp.$3,
        build : RegExp.$4 };

    out.Write("version: major=");
    out.Write(objVersion.major);
    out.Write(" minor=");
    out.Write(objVersion.minor);
    out.Write(" revision=");
    out.Write(objVersion.revision);
    out.Write(" build=");
    out.Write(objVersion.build);
    out.WriteLine();
    out.WriteLine();

    if (objArgs.Length <= 1) {
        out.WriteLine("using solution folder root");
        objRootFolder = objFSO.GetFolder(".");

    } else {
        out.Write("arg[1]: ");
        out.WriteLine(objArgs(1));

        if (!objFSO.FolderExists(objArgs(1))) {
            out.WriteLine("root folder does not exist");
            return;
        }

        objRootFolder = objFSO.GetFolder(objArgs(1));
    }

    out.Write("root: ");
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

        //out.WriteLine("found resource file:");
        out.Write("path: ");
        out.WriteLine(objFile.Path);
        out.Write("name: ");
        out.WriteLine(objFile.Name);
        out.WriteLine();

        MJH.SetVersion(objFile, objVersion);
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
