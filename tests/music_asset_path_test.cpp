#include <gtest/gtest.h>

#include "../gmp-client/gothic2a/src/audio/music_asset.h"

namespace {

TEST(MusicAssetPaths, PreservesSubdirectoriesBelowBothRoots) {
  const auto paths = gmp::audio::MusicAssetPaths("Ambient/Theme.mp3");
  ASSERT_TRUE(paths);
  EXPECT_EQ((*paths)[0], "Multiplayer/Music/Ambient/Theme.mp3");
  EXPECT_EQ((*paths)[1], "_Work/Data/Music/Ambient/Theme.mp3");
}

TEST(MusicAssetPaths, BareNamesAndLeadingDotUseTheSameTwoRoots) {
  const auto expected = gmp::audio::MusicAssetPaths("MAINMENU.MP3");
  ASSERT_TRUE(expected);
  EXPECT_EQ((*expected)[0], "Multiplayer/Music/MAINMENU.MP3");
  EXPECT_EQ((*expected)[1], "_Work/Data/Music/MAINMENU.MP3");
  for (const auto* path : {"./MAINMENU.MP3", ".\\MAINMENU.MP3"}) {
    SCOPED_TRACE(path);
    EXPECT_EQ(gmp::audio::MusicAssetPaths(path), expected);
  }
  const auto nested = gmp::audio::MusicAssetPaths("Ambient/forest.mp3");
  ASSERT_TRUE(nested);
  EXPECT_EQ((*nested)[0], "Multiplayer/Music/Ambient/forest.mp3");
  EXPECT_EQ((*nested)[1], "_Work/Data/Music/Ambient/forest.mp3");
}

TEST(MusicAssetPaths, RecognizesMixedCaseSeparatorsAndCurrentDirectorySegments) {
  const auto first = gmp::audio::MusicAssetPaths(".\\Ambient\\Theme.OGG");
  const auto second = gmp::audio::MusicAssetPaths("./Ambient//./Theme.OGG");
  ASSERT_TRUE(first);
  ASSERT_TRUE(second);
  EXPECT_EQ(*first, *second);
}

TEST(MusicAssetPaths, DoesNotStripLegacyInputPrefixes) {
  for (const std::string path : {"Multiplayer/Music/theme.mp3", "_Work/Data/Music/theme.mp3", "Music/theme.mp3"}) {
    const auto paths = gmp::audio::MusicAssetPaths(path);
    ASSERT_TRUE(paths);
    EXPECT_EQ((*paths)[0], "Multiplayer/Music/" + path);
    EXPECT_EQ((*paths)[1], "_Work/Data/Music/" + path);
  }
}

TEST(MusicAssetPaths, RejectsRootedPathsTraversalAndDirectoryOnlyNames) {
  for (const auto* path : {"../theme.mp3", "Multiplayer/Music/../Textures/theme.mp3", "_Work/Data/Music/../../theme.mp3",
                          "/Multiplayer/Music/theme.mp3", "C:/Multiplayer/Music/theme.mp3", "Ambient/", "./Music/", "", "."}) {
    SCOPED_TRACE(path);
    EXPECT_FALSE(gmp::audio::MusicAssetPaths(path));
  }
}

}  // namespace

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
