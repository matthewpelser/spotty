#include "catalog.h"

#include "services/spotify.h"
#include "sonos/soap.h"

namespace catalog {

namespace {

// Spotify item -> the URI + DIDL metadata Sonos uses for its own Spotify
// favourites (same format, so playback goes through the queue path).
Favourite fromSpotify(const spotify::Item &it, bool linked, const String &sn, const String &desc,
                      bool withMeta) {
  Favourite f;
  f.title = it.title;
  f.group = it.group;
  if (!linked) {  // can't play without Sonos's Spotify account details
    f.uri = "spotify:" + it.kind + ":" + it.id;
    return f;
  }
  bool album = it.kind == "album";
  String itemId = String(album ? "1004206c" : "1006206c") + "spotify%3A" + it.kind + "%3A" + it.id;
  f.uri = "x-rincon-cpcontainer:" + itemId + "?sid=9&flags=8300&sn=" + sn;
  if (!withMeta) return f;
  f.meta =
      "<DIDL-Lite xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
      "xmlns:upnp=\"urn:schemas-upnp-org:metadata-1-0/upnp/\" "
      "xmlns:r=\"urn:schemas-rinconnetworks-com:metadata-1-0/\" "
      "xmlns=\"urn:schemas-upnp-org:metadata-1-0/DIDL-Lite/\">"
      "<item id=\"" + itemId + "\" parentID=\"" + itemId + "\" restricted=\"true\">"
      "<dc:title>" + soap::escape(it.title) + "</dc:title>"
      "<upnp:class>" + (album ? "object.container.album.musicAlbum" : "object.container.playlistContainer") +
      "</upnp:class><desc id=\"cdudn\" nameSpace=\"urn:schemas-rinconnetworks-com:metadata-1-0/\">" +
      desc + "</desc></item></DIDL-Lite>";
  return f;
}

}  // namespace

void forEach(const std::function<void(const Favourite &)> &fn, bool withMeta) {
  for (auto &f : sonos::favourites()) fn(f);
  auto items = spotify::items();
  if (items.empty()) return;
  String sn, desc;
  bool linked = sonos::spotifyAccount(sn, desc);
  for (auto &it : items) fn(fromSpotify(it, linked, sn, desc, withMeta));
}

bool find(const String &uri, Favourite &out) {
  if (sonos::findFavourite(uri, out)) return true;
  String sn, desc;
  bool linked = sonos::spotifyAccount(sn, desc);
  for (auto &it : spotify::items()) {
    if (uri.indexOf(it.id) < 0) continue;  // cheap pre-filter
    Favourite f = fromSpotify(it, linked, sn, desc, false);
    if (f.uri == uri) {
      out = fromSpotify(it, linked, sn, desc, true);
      return true;
    }
  }
  return false;
}

}  // namespace catalog
