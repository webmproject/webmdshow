// webmtestshell_2008.cc : Defines the entry point for the console application.
//

#include <conio.h>
#include <stdio.h>
#include <tchar.h>

#include "webmtestshell.h"
#include "gtest/gtest.h"


int _tmain(int argc, _TCHAR* argv[])
{
    testing::InitGoogleTest(&argc, argv);
    RUN_ALL_TESTS();

    printf("press a key to exit...\n");

    while( !_kbhit() )
    {
        Sleep(1);
    }

    return 0;
}
