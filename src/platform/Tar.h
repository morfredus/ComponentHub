// TAR archive (ustar): regular files and folders, relative names with "/" (never a leading "/").
// Used for the full backups of the workshop (database, configuration, attachments).
#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace chdesktop::tar {

// Safety caps: an oversized data folder or a pathological tree must produce
// a clear message, not an archive of tens of GiB or an endless walk.
inline constexpr std::uint64_t kDefaultMaxArchiveBytes = 16ull * 1024 * 1024 * 1024;  // 16 Gio
inline constexpr std::uint64_t kMaxFileBytes = 077777777777ull;  // limite ustar (11 chiffres octaux)
inline constexpr int kMaxDepth = 8;                               // components of a path

// Extra folder to archive under the name `archiveName` (e.g. {"attachments", "<dir>/attachments"}).
struct TreeSource {
    std::string archiveName;
    std::string dir;
};

// Recursively packs `dir` (and each `extraTrees`) into the archive `tarPath`. Paths are
// relative, separated by "/"; folders (even empty ones) are archived; symbolic links are
// ignored. Files whose name ends with an entry of `excludeSuffixes` (e.g. ".tmp") or
// contains ".tmp." (atomic write in progress) are ignored. Returns false on failure; `error`
// then receives a code "tar.xxx|detail" (name_too_long, too_deep, invalid_name, too_large,
// read_failed, write_failed) and the partial archive is deleted.
bool pack(const std::string& tarPath, const std::string& dir,
          const std::vector<std::string>& excludeSuffixes = {".tmp"}, std::string* error = nullptr,
          const std::vector<TreeSource>& extraTrees = {},
          std::uint64_t maxTotalBytes = kDefaultMaxArchiveBytes);

// Quickly checks the format before overwriting anything.
bool looksLikeTar(const std::string& tarPath);

// Extracts the archive `tarPath` into the folder `dir` (overwrites existing files) and returns the
// number of files restored. Any path that would leave `dir` is refused: "..", absolute path,
// backslash, ":", Windows reserved names (CON, NUL, COM1...), links, devices, excessive
// depth. `errors` receives "tar.unsafe_entry|name" (entry ignored), "tar.write_failed|name"
// (write failure or truncated data) or "tar.corrupt" (invalid header, archive without end marker).
int extract(const std::string& tarPath, const std::string& dir, std::vector<std::string>& errors);

} // namespace chdesktop::tar
