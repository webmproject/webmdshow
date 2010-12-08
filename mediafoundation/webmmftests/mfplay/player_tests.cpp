#include "gtest/gtest.h"
#include "player.hpp"

extern CPlayer* g_pPlayer;

TEST(WebmMfPlayTest, PlayFile)
{
    // TODO(tomfinegan): this test won't work-- it has to wait for the player
    //                   to finish setup before calling Play.
    HRESULT hr_open = g_pPlayer->OpenURL(L"D:\\src\\media\\webm\\fg.webm");
    EXPECT_TRUE(SUCCEEDED(hr_open));
    HRESULT hr_play = g_pPlayer->Play();
    EXPECT_TRUE(SUCCEEDED(hr_play));
    HRESULT hr_stop = g_pPlayer->Stop();
    EXPECT_TRUE(SUCCEEDED(hr_stop));
}