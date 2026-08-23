#include "core/GeoRasterCore.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <unordered_map>

namespace GeoRaster {
namespace {

std::string Lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return text;
}

std::uint32_t ReadBigEndian32(const std::byte* bytes)
{
    return (std::to_integer<std::uint32_t>(bytes[0]) << 24U) |
           (std::to_integer<std::uint32_t>(bytes[1]) << 16U) |
           (std::to_integer<std::uint32_t>(bytes[2]) << 8U) |
           std::to_integer<std::uint32_t>(bytes[3]);
}

std::uint16_t ReadBigEndian16(const std::byte* bytes)
{
    return static_cast<std::uint16_t>(
        (std::to_integer<std::uint16_t>(bytes[0]) << 8U) |
        std::to_integer<std::uint16_t>(bytes[1])
    );
}

bool IsStartOfFrame(std::uint8_t marker)
{
    return (marker >= 0xC0 && marker <= 0xC3) ||
           (marker >= 0xC5 && marker <= 0xC7) ||
           (marker >= 0xC9 && marker <= 0xCB) ||
           (marker >= 0xCD && marker <= 0xCF);
}

Validated<RasterInfo> ReadPngInfo(
    const std::filesystem::path& path,
    const std::vector<std::byte>& bytes
)
{
    constexpr std::array<std::uint8_t, 8> signature {137, 80, 78, 71, 13, 10, 26, 10};
    if (bytes.size() < 24) {
        return {{}, ValidationResult::Failure(ValidationCode::CorruptPng)};
    }
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (std::to_integer<std::uint8_t>(bytes[index]) != signature[index]) {
            return {{}, ValidationResult::Failure(ValidationCode::CorruptPng)};
        }
    }
    const auto* data = bytes.data();
    if (ReadBigEndian32(data + 8) != 13 || data[12] != std::byte{'I'} ||
        data[13] != std::byte{'H'} || data[14] != std::byte{'D'} || data[15] != std::byte{'R'}) {
        return {{}, ValidationResult::Failure(ValidationCode::CorruptPng)};
    }
    const std::uint32_t width = ReadBigEndian32(data + 16);
    const std::uint32_t height = ReadBigEndian32(data + 20);
    if (width == 0 || height == 0) {
        return {{}, ValidationResult::Failure(ValidationCode::CorruptPng)};
    }
    return {RasterInfo {path, RasterFormat::PNG, width, height}, ValidationResult::Success()};
}

Validated<RasterInfo> ReadJpegInfo(
    const std::filesystem::path& path,
    const std::vector<std::byte>& bytes
)
{
    if (bytes.size() < 4 || bytes[0] != std::byte{0xFF} || bytes[1] != std::byte{0xD8}) {
        return {{}, ValidationResult::Failure(ValidationCode::CorruptJpeg)};
    }

    std::size_t offset = 2;
    while (offset < bytes.size()) {
        while (offset < bytes.size() && bytes[offset] != std::byte{0xFF}) {
            ++offset;
        }
        while (offset < bytes.size() && bytes[offset] == std::byte{0xFF}) {
            ++offset;
        }
        if (offset >= bytes.size()) {
            break;
        }
        const std::uint8_t marker = std::to_integer<std::uint8_t>(bytes[offset++]);
        if (marker == 0xD9 || marker == 0xDA) {
            break;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7)) {
            continue;
        }
        if (offset + 2 > bytes.size()) {
            break;
        }
        const std::uint16_t segmentLength = ReadBigEndian16(bytes.data() + offset);
        if (segmentLength < 2 || offset + segmentLength > bytes.size()) {
            break;
        }
        if (IsStartOfFrame(marker)) {
            if (segmentLength < 7) {
                break;
            }
            const std::uint16_t height = ReadBigEndian16(bytes.data() + offset + 3);
            const std::uint16_t width = ReadBigEndian16(bytes.data() + offset + 5);
            if (width == 0 || height == 0) {
                break;
            }
            return {RasterInfo {path, RasterFormat::JPEG, width, height}, ValidationResult::Success()};
        }
        offset += segmentLength;
    }
    return {{}, ValidationResult::Failure(ValidationCode::CorruptJpeg)};
}

} // namespace

Validated<std::vector<std::byte>> ReadBinaryFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return {{}, ValidationResult::Failure(
            std::filesystem::exists(path) ? ValidationCode::FileReadFailed : ValidationCode::FileNotFound,
            {path.u8string()}
        )};
    }
    input.seekg(0, std::ios::end);
    const auto length = input.tellg();
    if (length < 0) {
        return {{}, ValidationResult::Failure(ValidationCode::FileReadFailed, {path.u8string()})};
    }
    input.seekg(0, std::ios::beg);
    std::vector<std::byte> bytes(static_cast<std::size_t>(length));
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()), length)) {
        return {{}, ValidationResult::Failure(ValidationCode::FileReadFailed, {path.u8string()})};
    }
    return {std::move(bytes), ValidationResult::Success()};
}

Validated<RasterInfo> ReadRasterInfo(const std::filesystem::path& path)
{
    const std::string extension = Lower(path.extension().u8string());
    if (extension != ".png" && extension != ".jpg" && extension != ".jpeg") {
        return {{}, ValidationResult::Failure(
            ValidationCode::UnsupportedRasterFormat, {extension}
        )};
    }
    auto binary = ReadBinaryFile(path);
    if (!binary.IsValid()) {
        return {{}, binary.validation};
    }
    return extension == ".png" ? ReadPngInfo(path, *binary.value) : ReadJpegInfo(path, *binary.value);
}

WorldFileDiscovery DiscoverWorldFiles(const std::filesystem::path& rasterPath)
{
    WorldFileDiscovery discovery;
    const std::string extension = Lower(rasterPath.extension().u8string());
    std::vector<std::string> precedence;
    if (extension == ".png") {
        precedence = {".pgw", ".pngw", ".wld"};
    } else if (extension == ".jpg") {
        precedence = {".jgw", ".jpgw", ".wld"};
    } else if (extension == ".jpeg") {
        precedence = {".jgw", ".jpegw", ".wld"};
    } else {
        return discovery;
    }

    std::unordered_map<std::string, std::filesystem::path> files;
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(rasterPath.parent_path(), error)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (Lower(entry.path().stem().u8string()) == Lower(rasterPath.stem().u8string())) {
            files.emplace(Lower(entry.path().extension().u8string()), entry.path());
        }
    }
    for (const std::string& candidateExtension : precedence) {
        const auto found = files.find(candidateExtension);
        if (found != files.end()) {
            discovery.candidates.push_back(found->second);
        }
    }
    if (!discovery.candidates.empty()) {
        discovery.preferred = discovery.candidates.front();
    }
    return discovery;
}

} // namespace GeoRaster
