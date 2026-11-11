// SPDX-FileCopyrightText: 2026 Abdolmanaf Mehrabi Sisakht
// SPDX-License-Identifier: MIT
#include "Text/SystemFonts.hpp"

#include <string>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite_2.h>
#include <windows.h>
#include <wrl/client.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#include <climits>
#elif defined(GLW_HAVE_FONTCONFIG)
#include <fontconfig/fontconfig.h>
#endif

namespace glw::text::detail {

#if defined(_WIN32) || defined(__APPLE__)
namespace {

// A character in UTF-16: one unit, or a surrogate pair.
int toUtf16(char32_t c, char16_t out[2]) {
    if (c < 0x10000) {
        out[0] = static_cast<char16_t>(c);
        return 1;
    }
    c -= 0x10000;
    out[0] = static_cast<char16_t>(0xD800 + (c >> 10));
    out[1] = static_cast<char16_t>(0xDC00 + (c & 0x3FF));
    return 2;
}

} // namespace
#endif

#if defined(_WIN32)

namespace {

using Microsoft::WRL::ComPtr;

std::string toUtf8(const wchar_t* text) {
    const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 1)
        return {};
    std::string out(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
    return out;
}

// The text DirectWrite's fallback is asked about: the character alone, in a locale.
class Source final : public IDWriteTextAnalysisSource {
public:
    Source(const wchar_t* text, UINT32 length, const wchar_t* locale) : text_(text), length_(length), locale_(locale) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** out) override {
        if (id == __uuidof(IDWriteTextAnalysisSource) || id == __uuidof(IUnknown)) {
            *out = static_cast<IDWriteTextAnalysisSource*>(this);
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    // (on the stack, for the call only)
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }

    HRESULT STDMETHODCALLTYPE GetTextAtPosition(UINT32 at, const WCHAR** text, UINT32* length) override {
        *text = at < length_ ? text_ + at : nullptr;
        *length = at < length_ ? length_ - at : 0;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetTextBeforePosition(UINT32 at, const WCHAR** text, UINT32* length) override {
        *text = at > 0 && at <= length_ ? text_ : nullptr;
        *length = at > 0 && at <= length_ ? at : 0;
        return S_OK;
    }
    DWRITE_READING_DIRECTION STDMETHODCALLTYPE GetParagraphReadingDirection() override { return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT; }
    HRESULT STDMETHODCALLTYPE GetLocaleName(UINT32 at, UINT32* length, const WCHAR** locale) override {
        *length = at < length_ ? length_ - at : 0;
        *locale = locale_;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE GetNumberSubstitution(UINT32 at, UINT32* length, IDWriteNumberSubstitution** substitution) override {
        *length = at < length_ ? length_ - at : 0;
        *substitution = nullptr;
        return S_OK;
    }

private:
    const wchar_t* text_;
    UINT32 length_;
    const wchar_t* locale_;
};

} // namespace

std::optional<SystemFontFile> systemFontFor(char32_t c, std::string_view language) {
    static ComPtr<IDWriteFactory2> factory;
    static ComPtr<IDWriteFontFallback> fallback;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (SUCCEEDED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory2),
                                          reinterpret_cast<IUnknown**>(factory.GetAddressOf()))))
            factory->GetSystemFontFallback(fallback.GetAddressOf());
    }
    if (!fallback)
        return std::nullopt;
    char16_t units[2] = {};
    const int length = toUtf16(c, units);
    const wchar_t text[3] = {static_cast<wchar_t>(units[0]), length > 1 ? static_cast<wchar_t>(units[1]) : L'\0', L'\0'};
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = L"en-us";
    if (!language.empty() && language.size() < LOCALE_NAME_MAX_LENGTH) {
        const int n = MultiByteToWideChar(CP_UTF8, 0, language.data(), static_cast<int>(language.size()), locale, LOCALE_NAME_MAX_LENGTH - 1);
        locale[n > 0 ? n : 0] = L'\0';
    }
    Source source(text, static_cast<UINT32>(length), locale);
    UINT32 mapped = 0;
    ComPtr<IDWriteFont> font;
    FLOAT scale = 1;
    if (FAILED(fallback->MapCharacters(&source, 0, static_cast<UINT32>(length), nullptr, L"Segoe UI", DWRITE_FONT_WEIGHT_NORMAL,
                                       DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, &mapped, font.GetAddressOf(), &scale)) ||
        !font)
        return std::nullopt;
    // its file, on the disk
    ComPtr<IDWriteFontFace> face;
    if (FAILED(font->CreateFontFace(face.GetAddressOf())))
        return std::nullopt;
    UINT32 files = 1;
    ComPtr<IDWriteFontFile> file;
    if (FAILED(face->GetFiles(&files, file.GetAddressOf())) || !file)
        return std::nullopt;
    ComPtr<IDWriteFontFileLoader> loader;
    ComPtr<IDWriteLocalFontFileLoader> local;
    if (FAILED(file->GetLoader(loader.GetAddressOf())) || FAILED(loader.As(&local)))
        return std::nullopt;
    const void* key = nullptr;
    UINT32 keySize = 0;
    UINT32 pathLength = 0;
    if (FAILED(file->GetReferenceKey(&key, &keySize)) || FAILED(local->GetFilePathLengthFromKey(key, keySize, &pathLength)))
        return std::nullopt;
    std::wstring path(pathLength + 1, L'\0');
    if (FAILED(local->GetFilePathFromKey(key, keySize, path.data(), pathLength + 1)))
        return std::nullopt;
    return SystemFontFile{toUtf8(path.c_str()), static_cast<int>(face->GetIndex()), {}};
}

#elif defined(__APPLE__)

std::optional<SystemFontFile> systemFontFor(char32_t c, std::string_view) {
    char16_t units[2] = {};
    const int length = toUtf16(c, units);
    UniChar chars[2] = {units[0], units[1]};
    CFStringRef text = CFStringCreateWithCharacters(nullptr, chars, length);
    CTFontRef base = CTFontCreateWithName(CFSTR("Helvetica"), 12, nullptr);
    CTFontRef found = CTFontCreateForString(base, text, CFRangeMake(0, length));
    std::optional<SystemFontFile> result;
    CGGlyph glyphs[2] = {};
    // (Last Resort: macOS's font of a placeholder for every character, what it falls back to when none has it: as none)
    bool lastResort = false;
    if (found) {
        if (CFStringRef name = CTFontCopyPostScriptName(found)) {
            lastResort = CFStringCompare(name, CFSTR("LastResort"), 0) == kCFCompareEqualTo;
            CFRelease(name);
        }
    }
    if (found && !lastResort && CTFontGetGlyphsForCharacters(found, chars, glyphs, length)) {
        if (auto url = static_cast<CFURLRef>(CTFontCopyAttribute(found, kCTFontURLAttribute))) {
            UInt8 path[PATH_MAX];
            if (CFURLGetFileSystemRepresentation(url, true, path, sizeof path)) {
                SystemFontFile file{reinterpret_cast<const char*>(path), 0, {}};
                if (CFStringRef name = CTFontCopyPostScriptName(found)) {
                    char buffer[256];
                    if (CFStringGetCString(name, buffer, sizeof buffer, kCFStringEncodingUTF8))
                        file.postScriptName = buffer;
                    CFRelease(name);
                }
                result = std::move(file);
            }
            CFRelease(url);
        }
    }
    if (found)
        CFRelease(found);
    CFRelease(base);
    CFRelease(text);
    return result;
}

#elif defined(GLW_HAVE_FONTCONFIG)

std::optional<SystemFontFile> systemFontFor(char32_t c, std::string_view language) {
    static FcConfig* config = FcInitLoadConfigAndFonts();
    if (!config)
        return std::nullopt;
    FcPattern* pattern = FcPatternCreate();
    FcCharSet* wanted = FcCharSetCreate();
    FcCharSetAddChar(wanted, static_cast<FcChar32>(c));
    FcPatternAddCharSet(pattern, FC_CHARSET, wanted);
    if (!language.empty()) {
        const std::string lang(language);
        FcPatternAddString(pattern, FC_LANG, reinterpret_cast<const FcChar8*>(lang.c_str()));
    }
    FcConfigSubstitute(config, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    FcResult result = FcResultNoMatch;
    FcFontSet* fonts = FcFontSort(config, pattern, FcFalse, nullptr, &result);
    std::optional<SystemFontFile> found;
    // the best one that has it
    for (int i = 0; fonts && i < fonts->nfont && !found; ++i) {
        FcCharSet* has = nullptr;
        FcChar8* file = nullptr;
        int index = 0;
        if (FcPatternGetCharSet(fonts->fonts[i], FC_CHARSET, 0, &has) != FcResultMatch || !FcCharSetHasChar(has, static_cast<FcChar32>(c)))
            continue;
        if (FcPatternGetString(fonts->fonts[i], FC_FILE, 0, &file) != FcResultMatch)
            continue;
        FcPatternGetInteger(fonts->fonts[i], FC_INDEX, 0, &index);
        found = SystemFontFile{reinterpret_cast<const char*>(file), index, {}};
    }
    if (fonts)
        FcFontSetDestroy(fonts);
    FcCharSetDestroy(wanted);
    FcPatternDestroy(pattern);
    return found;
}

#else

std::optional<SystemFontFile> systemFontFor(char32_t, std::string_view) {
    return std::nullopt;
}

#endif

} // namespace glw::text::detail
