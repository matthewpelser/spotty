#pragma once
#include <functional>

#include "sonos/sonos.h"

// Everything a preset can play: Sonos Favourites plus lists from signed-in
// music services (currently Spotify). All items are Sonos-playable Favourites.
//
// There can be hundreds of service items, so they are generated one at a time
// and their DIDL metadata (~700 bytes each) only when asked for.
namespace catalog {

// Visit every item. Without metadata unless withMeta is true.
void forEach(const std::function<void(const Favourite &)> &fn, bool withMeta = false);
// Look up one item by URI, with metadata.
bool find(const String &uri, Favourite &out);

}  // namespace catalog
