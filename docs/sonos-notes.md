# Sonos notes (verified on the real system)

System: S2, firmware **27.3** (software 98.0-82170). Speakers include a Sonos Five (Kitchen, `RINCON_347E5CE4D1F001400`).
Rooms: Bedroom, Kitchen, Living Room. Everything below was tested against these players.

## Endpoints (port 1400)

| Service | Path | URN |
|---|---|---|
| AVTransport | `/MediaRenderer/AVTransport/Control` | `urn:schemas-upnp-org:service:AVTransport:1` |
| RenderingControl | `/MediaRenderer/RenderingControl/Control` | `…:RenderingControl:1` |
| GroupRenderingControl | `/MediaRenderer/GroupRenderingControl/Control` | `…:GroupRenderingControl:1` |
| ContentDirectory | `/MediaServer/ContentDirectory/Control` | `…:ContentDirectory:1` |
| ZoneGroupTopology | `/ZoneGroupTopology/Control` | `…:ZoneGroupTopology:1` |

Discovery: SSDP `M-SEARCH` for `urn:schemas-upnp-org:device:ZonePlayer:1`, then `GetZoneGroupState`
(escaped XML; skip `Invisible="1"` and `IsZoneBridge="1"` members). `/status/accounts` returns an empty `<ZPSupportInfo>`.

## Recipes

- **Group:** member → `SetAVTransportURI x-rincon:<coordUUID>`; leave → `BecomeCoordinatorOfStandaloneGroup`.
- **Queue playback:** `RemoveAllTracksFromQueue` → `AddURIToQueue(uri, meta)` → `SetAVTransportURI x-rincon-queue:<coord>#0` → (`SetPlayMode`) → `Play`.
- **Favourites:** `Browse FV:2` → DIDL items; `res` = URI, `r:resMD` = metadata (escaped). Send both back unchanged.
  Each item also has `upnp:albumArtURI` (a `/getaa?…` path on the player, or an https/cloudfront URL) — used for the web UI covers.
- **Absolute group volume:** `GroupRenderingControl#SnapshotGroupVolume` then `SetGroupVolume <DesiredVolume>` (keeps the rooms' balance).
  Relative: `SetRelativeGroupVolume <Adjustment>` → returns `NewVolume`.
- **Next track's art:** `GetPositionInfo` `<Track>` (1-based) → `Browse Q:0` with `StartingIndex = Track`, count 1, filter `upnp:albumArtURI`.
  The queue's art URL is byte-identical to the one `GetPositionInfo` reports when that track plays (so it works as a cache key).

## Spotify via Sonos (no Sonos favourite needed)

Household values taken from an existing Spotify favourite: `sid=9`, `sn=13`, desc `SA_RINCON2311_X_#Svc2311-0-Token`.

```
playlist item id: 1006206cspotify%3Aplaylist%3A<id>   class object.container.playlistContainer
album    item id: 1004206cspotify%3Aalbum%3A<id>      class object.container.album.musicAlbum
URI: x-rincon-cpcontainer:<item id>?sid=9&flags=8300&sn=<sn>
DIDL: <item id="<item id>" parentID="<item id>" restricted="true"><dc:title>…</dc:title>
      <upnp:class>…</upnp:class><desc id="cdudn" nameSpace="urn:schemas-rinconnetworks-com:metadata-1-0/">SA_RINCON2311_X_#Svc2311-0-Token</desc></item>
```

Verified: "New Music Friday" UK (`37i9dQZF1DX4W3aJJYCDfV`) queued 99 tracks.

## Quirks

- **TuneIn (`x-sonosapi-stream:…?sid=333`) → UPnP 402** on `SetAVTransportURI`, with or without metadata, any variant. Other sources work. See decision D9.
- **Sonos Radio** favourites have an empty `res` → not playable locally (filtered out).
- **Album art** (`/getaa?s=1&u=…`) is a 640×640 baseline JPEG (36–125 KB). The player sends it **chunked even to HTTP/1.0 requests** → `art.cpp` de-chunks.
  Art URLs may be relative (prefix `http://<ip>:1400`); https art URLs are ignored.
- Radio "now playing" text is in `r:streamContent`; station name in `GetMediaInfo` → `CurrentURIMetaData`.
- A speaker in a Spotify Connect session reports `x-sonos-vli:…` as its source; `SetAVTransportURI` still takes over.
- Some streams can't Pause → fall back to Stop. `Next` fails on streams → "Can't skip".
- Right after Play/Pause/Stop the player may briefly report the old/transitional state → re-poll after 0.7 s.

## Network

Google mesh, `192.168.86.0/24`, router `192.168.86.1` (answers TCP :80). ESP32 on 2.4 GHz channel 1.
The mesh has twice left the ESP32 "connected" but unreachable → netwatch (D15).
