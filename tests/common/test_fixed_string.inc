// fixed_string tests — header-only-compatible. Mirrors P3094 surface.

#include <format>
#include <sstream>

TEST_CASE("fixed_string: literal deduces N as meaningful length", "[fixed_string]") {
    constexpr collab::fixed_string s = "hello";
    STATIC_REQUIRE(std::is_same_v<decltype(s), const collab::fixed_string<5>>);
    STATIC_REQUIRE(sizeof(s.data_) == 6);
}

TEST_CASE("fixed_string: capacity members are integral_constants", "[fixed_string]") {
    using FS = collab::fixed_string<5>;
    STATIC_REQUIRE(FS::size == 5);
    STATIC_REQUIRE(FS::size() == 5);
    STATIC_REQUIRE(FS::length == 5);
    STATIC_REQUIRE(FS::max_size == 5);
    STATIC_REQUIRE_FALSE(FS::empty);
    STATIC_REQUIRE(collab::fixed_string<0>::empty);
}

TEST_CASE("fixed_string: c_str, data, view", "[fixed_string]") {
    constexpr collab::fixed_string s = "abc";
    REQUIRE(std::string_view(s.c_str()) == "abc");
    REQUIRE(std::string_view(s.data()) == "abc");
    REQUIRE(s.view() == "abc");
}

TEST_CASE("fixed_string: implicit conversion to string_view", "[fixed_string]") {
    constexpr collab::fixed_string s = "abc";
    std::string_view sv = s;
    REQUIRE(sv == "abc");
    REQUIRE(sv.size() == 3);
}

TEST_CASE("fixed_string: element access", "[fixed_string]") {
    constexpr collab::fixed_string s = "abcde";
    STATIC_REQUIRE(s[0] == 'a');
    STATIC_REQUIRE(s[4] == 'e');
    STATIC_REQUIRE(s.at(2) == 'c');
    STATIC_REQUIRE(s.front() == 'a');
    STATIC_REQUIRE(s.back() == 'e');
    REQUIRE_THROWS_AS(s.at(5), std::out_of_range);
}

TEST_CASE("fixed_string: iterators", "[fixed_string]") {
    constexpr collab::fixed_string s = "abc";
    std::string copied;
    for (auto c : s) copied += c;
    REQUIRE(copied == "abc");

    std::string reversed;
    for (auto it = s.rbegin(); it != s.rend(); ++it) reversed += *it;
    REQUIRE(reversed == "cba");
}

TEST_CASE("fixed_string: cross-N equality and ordering", "[fixed_string]") {
    constexpr collab::fixed_string a = "abc";
    constexpr collab::fixed_string b = "abc";
    constexpr collab::fixed_string c = "abd";
    constexpr collab::fixed_string d = "abcd";

    STATIC_REQUIRE(a == b);
    STATIC_REQUIRE(a != c);
    STATIC_REQUIRE(a != d);
    STATIC_REQUIRE(a <  c);
    STATIC_REQUIRE(a <  d);
}

TEST_CASE("fixed_string: comparison against char-array literals", "[fixed_string]") {
    constexpr collab::fixed_string a = "hello";
    STATIC_REQUIRE(a == "hello");
    STATIC_REQUIRE(a != "world");
    STATIC_REQUIRE(a <  "zebra");
}

TEST_CASE("fixed_string: concatenation fs+fs", "[fixed_string]") {
    constexpr collab::fixed_string a = "foo";
    constexpr collab::fixed_string b = "bar";
    constexpr auto c = a + b;
    STATIC_REQUIRE(std::is_same_v<decltype(c), const collab::fixed_string<6>>);
    STATIC_REQUIRE(c == "foobar");
}

TEST_CASE("fixed_string: concatenation with single char", "[fixed_string]") {
    constexpr collab::fixed_string a = "foo";
    constexpr auto a_bang = a + '!';
    constexpr auto bang_a = '>' + a;
    STATIC_REQUIRE(a_bang == "foo!");
    STATIC_REQUIRE(bang_a == ">foo");
}

TEST_CASE("fixed_string: concatenation with literal", "[fixed_string]") {
    constexpr collab::fixed_string a = "foo";
    constexpr auto a_bar = a + "bar";
    constexpr auto bar_a = "bar" + a;
    STATIC_REQUIRE(a_bar == "foobar");
    STATIC_REQUIRE(bar_a == "barfoo");
}

TEST_CASE("fixed_string: variadic char ctor", "[fixed_string]") {
    constexpr collab::basic_fixed_string s{'a', 'b', 'c'};
    STATIC_REQUIRE(std::is_same_v<decltype(s), const collab::basic_fixed_string<char, 3>>);
    STATIC_REQUIRE(s == "abc");
}

TEST_CASE("fixed_string: iterator-pair ctor", "[fixed_string]") {
    const std::string src = "hello";
    collab::fixed_string<5> s(src.begin(), src.end());
    REQUIRE(s.view() == "hello");
}

TEST_CASE("fixed_string: swap (member + non-member)", "[fixed_string]") {
    collab::fixed_string<3> a("abc");
    collab::fixed_string<3> b("xyz");
    a.swap(b);
    REQUIRE(a.view() == "xyz");
    REQUIRE(b.view() == "abc");

    swap(a, b);
    REQUIRE(a.view() == "abc");
    REQUIRE(b.view() == "xyz");
}

TEST_CASE("fixed_string: usable as NTTP", "[fixed_string]") {
    auto get_tag = []<collab::basic_fixed_string Name>() {
        return std::string_view(Name);
    };
    REQUIRE(get_tag.operator()<"hello">() == "hello");
    REQUIRE(get_tag.operator()<"world">() == "world");
}

namespace fixed_string_test_detail {
    template <collab::basic_fixed_string Tag>
    struct tagged {
        static constexpr std::string_view tag() { return Tag.view(); }
    };
}

TEST_CASE("fixed_string: NTTP value identity", "[fixed_string]") {
    using A = fixed_string_test_detail::tagged<"alpha">;
    using B = fixed_string_test_detail::tagged<"alpha">;
    using C = fixed_string_test_detail::tagged<"beta">;
    STATIC_REQUIRE(std::is_same_v<A, B>);
    STATIC_REQUIRE_FALSE(std::is_same_v<A, C>);
    REQUIRE(A::tag() == "alpha");
    REQUIRE(C::tag() == "beta");
}

TEST_CASE("fixed_string: char-type aliases compile", "[fixed_string]") {
    constexpr collab::fixed_string    s8  = "abc";
    constexpr collab::fixed_u8string  u8  = u8"abc";
    constexpr collab::fixed_u16string u16 = u"abc";
    constexpr collab::fixed_u32string u32 = U"abc";
    constexpr collab::fixed_wstring   ws  = L"abc";
    STATIC_REQUIRE(s8.size  == 3);
    STATIC_REQUIRE(u8.size  == 3);
    STATIC_REQUIRE(u16.size == 3);
    STATIC_REQUIRE(u32.size == 3);
    STATIC_REQUIRE(ws.size  == 3);
}

// Skipped in pure-import mode on toolchains that don't surface foreign-template
// specializations from the module's GMF (see test_pure_import_specializations
// xmake option). In include and dual modes COLLAB_HEADER_INCLUDED is set, so
// the spec comes through the header and the test runs.
#if !defined(COLLAB_SKIP_PURE_IMPORT_SPECIALIZATION_TESTS) || defined(COLLAB_HEADER_INCLUDED)
TEST_CASE("fixed_string: std::hash agrees with string_view hash", "[fixed_string]") {
    collab::fixed_string s = "hello";
    auto h_fs = std::hash<collab::fixed_string<5>>{}(s);
    auto h_sv = std::hash<std::string_view>{}(std::string_view("hello"));
    REQUIRE(h_fs == h_sv);
}
#endif

TEST_CASE("fixed_string: operator<< streams the contents", "[fixed_string]") {
    collab::fixed_string s = "hello";
    std::ostringstream os;
    os << s;
    REQUIRE(os.str() == "hello");
}

TEST_CASE("fixed_string: std::format via implicit string_view conversion", "[fixed_string]") {
    // Direct `std::format("{}", s)` exercises our std::formatter partial spec.
    // That spec lives in the canonical header's `namespace std { ... }`, which
    // MSVC's module implementation does not reliably surface to pure-import
    // TUs — the range formatter wins instead. Formatting via `.view()` always
    // works because string_view's formatter is in the standard library.
    collab::fixed_string s = "world";
    auto out = std::format("hello {}", s.view());
    REQUIRE(out == "hello world");
}
