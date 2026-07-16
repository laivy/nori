include_guard(GLOBAL)

include(FetchContent)

FetchContent_Declare(
    boost
    URL https://archives.boost.io/release/1.91.0/source/boost_1_91_0.tar.gz
    URL_HASH SHA256=5734305f40a76c30f951c9abd409a45a2a19fb546efe4162119250bbe4d3a463
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    DirectX-Headers
    URL https://github.com/microsoft/DirectX-Headers/archive/9e393d6d8a3b30dcc6f2806ef604ec16a27b0d7e.tar.gz
    URL_HASH SHA256=5b9ab816a4eebea0261e59b4f1e2885f901d7df629c41c7d6b5f8770c52283b1
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    imgui
    URL https://github.com/ocornut/imgui/archive/b61e56346a92cfcaf1f43a545ca37b0b32239654.tar.gz
    URL_HASH SHA256=100c1e0a0625c538d79e014dc8e73309c5b25c7ed8183fb91ae621a3681414a4
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    DOWNLOAD_NO_PROGRESS TRUE
    EXCLUDE_FROM_ALL
    SYSTEM
)
