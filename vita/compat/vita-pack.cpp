#include "vita-pack.hpp"

#include "filesystem.hpp" // game_config::path

#include <psp2/io/fcntl.h>

#include <fstream>
#include <mutex>
#include <unordered_map>

namespace vita_pack
{
bool read(const std::string& location, std::vector<char>& bytes)
{
	struct packed_file
	{
		SceOff offset;
		SceSize size;
	};
	static std::once_flag opened;
	static SceUID pack = -1;
	static std::unordered_map<std::string, packed_file> files;
	std::call_once(opened, [] {
		std::ifstream index(game_config::path + "/vita-images.idx");
		if(!index) {
			return;
		}
		files.reserve(18000);
		for(std::string line; std::getline(index, line);) {
			const std::size_t tab1 = line.find('\t');
			const std::size_t tab2 = line.find('\t', tab1 + 1);
			if(tab1 == std::string::npos || tab2 == std::string::npos) {
				continue;
			}
			files.emplace(line.substr(0, tab1),
				packed_file{std::stoll(line.substr(tab1 + 1, tab2 - tab1 - 1)), static_cast<SceSize>(std::stoul(line.substr(tab2 + 1)))});
		}
		// The kernel call wants a device: "/data/wesnoth" is how the C library writes ux0:/data/wesnoth.
		pack = sceIoOpen(("ux0:" + game_config::path + "/vita-images.pak").c_str(), SCE_O_RDONLY, 0);
	});
	if(pack < 0) {
		return false;
	}

	// The location is the full path the file was found at: "<data dir>/data/core/images/...".
	const std::string& root = game_config::path;
	std::size_t at = location.find(root + "/");
	if(at == std::string::npos) {
		return false;
	}
	std::string rel = location.substr(at + root.size() + 1);
	for(std::size_t i; (i = rel.find("//")) != std::string::npos;) {
		rel.erase(i, 1);
	}
	const auto it = files.find(rel);
	if(it == files.end()) {
		return false;
	}
	bytes.resize(it->second.size);
	// pread: no shared file position, so the loading screen's thread can read alongside the main one.
	if(sceIoPread(pack, bytes.data(), it->second.size, it->second.offset) != static_cast<int>(it->second.size)) {
		bytes.clear();
		return false;
	}
	return true;
}
}
