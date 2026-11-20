#include <array>
#include <string_view>

#include <gtest/gtest.h>

#include <nori/core/encoding.hpp>

namespace
{
TEST(encoding_test, path_round_trip)
{
    constexpr std::array<std::string_view, 5> paths{
        "",
        "ascii/file.txt",
        "한글/파일.nori",
        "조합/e\u0301.txt",
        "보조 평면/😀.bin"
    };

    for (const std::string_view path : paths)
    {
        EXPECT_EQ(nori::core::path_to_utf8(nori::core::utf8_to_path(path)), path);
    }
}

#if defined(_WIN32)
TEST(encoding_test, utf16_round_trip)
{
    constexpr std::array<std::string_view, 6> texts{
        "",
        "Noritor",
        "한글 창 제목",
        "e\u0301",
        "😀",
        std::string_view{"A\0B", 3}
    };

    for (const std::string_view text : texts)
    {
        const auto utf16{nori::core::utf8_to_utf16(text)};
        EXPECT_EQ(nori::core::utf16_to_utf8(utf16), text);
    }
}

TEST(encoding_test, invalid_encoding)
{
    EXPECT_TRUE(nori::core::utf8_to_utf16(std::string_view{"\xC0\xAF", 2}).empty());
    EXPECT_TRUE(nori::core::utf16_to_utf8(std::wstring_view{L"\xD800", 1}).empty());
}

#endif
} // namespace
