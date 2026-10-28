/*
 * PS Vita: Wesnoth's images and sound effects, packed into one file (vita-images.pak, with its
 * index vita-images.idx; see vita/make-pack.py). Opening a file on the memory card costs ~20 ms,
 * which made the first sight of each unit and terrain, and the first play of each sound, stall; the
 * pack is opened once and each file is a single read from it.
 */
#pragma once

#include <string>
#include <vector>

namespace vita_pack
{
/**
 * Reads the packed copy of the game data file at @a location (a full path, "<data dir>/data/...").
 * False when it is not in the pack, or there is no pack: then it is read from its own file.
 */
bool read(const std::string& location, std::vector<char>& bytes);
}
