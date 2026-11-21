# Web UI — "Preset tiles"

Served from `src/web_ui.h` (single self-contained page, ~43 KB, no external assets).
Mockups: <https://claude.ai/artifact/EH1vgUjyb6Jrg5dZc4XV3n> — direction **B · Preset tiles** was chosen
(A Now Playing and C Inset grouped were the alternatives; the Editor / Picker / Controller screens are shared).

## Screens

| Screen | How you get there | What it does |
|---|---|---|
| Home | `/` | Large title + "Room · State" subtitle, gear → Controller. Live LCD simulation (240×280 at 1 CSS px per LCD pixel, crisp, in a bezel; 300×350 on wide screens). 2×2 preset tiles (cover, name, "rooms · volume", PLAYING/PAUSED badge on the active preset, "Can't play" for TuneIn). Controls card: vertical volume slider + Play/Pause, Next, Stop, Random. Floating mini-player (cover, title, artist · room, play/pause) |
| Edit mode | "Edit" on Home | Tiles jiggle and show a pencil; tap opens the editor. Empty tiles always open the editor |
| Edit preset (sheet) | tile in edit mode / empty tile | Name, Play (→ picker), Rooms (checkmarks; first ticked = "Leads group"), Set volume switch + slider, Shuffle switch, "Save and play now", "Clear preset". Save is disabled until a room and a playable item are chosen |
| Choose music (sheet over the editor) | "Play" row | Search, segmented filter All / Favourites / Playlists / Albums, grouped list with covers; unplayable items dimmed with the reason. Picking an item names an unnamed preset after it |
| Controller (push view) | gear | Live LCD preview (same simulation as Home), Sonos rooms/favourites/status + "Search for speakers again", Spotify row → Spotify sheet, Wi-Fi (SSID + quality) and address |
| Spotify (sheet) | Controller → Spotify | Set-up steps + client ID, two-step sign-in (open Spotify, paste the 127.0.0.1 address back), or connected summary with Refresh / Disconnect |

## Visual system

- iOS-like: system font stack (`-apple-system`… → SF on Apple devices), large titles, inset grouped lists, sheets with grabber, segmented control, switches.
- Light/dark follow `prefers-color-scheme`. Tokens on `:root`: `--bg #F2F2F7/#000`, `--card #FFF/#1C1C1E`, `--fill`, `--fill2`, `--text`, `--text2`, `--text3`, `--sep`, `--accent #0A7AFF/#0A84FF`, `--danger`, `--warn`, `--green`, `--glass`.
- Radii: tiles 20, cards 20, list groups 12, controls 16, sheets 12 (top).
- Mobile: `viewport-fit=cover` + `env(safe-area-inset-*)` paddings, Add-to-Home-Screen metas (standalone), inputs 17 px (no iOS zoom), targets ≥ 44 px, `:focus-visible` rings, `prefers-reduced-motion` respected, toast is `aria-live`.
- Cover art: `<img>` straight from the Sonos player (`http://<ip>:1400/getaa…`) or Spotify's CDN (https); falls back to a gradient tile with a note icon.

## Data flow

- Initial: `/api/rooms`, `/api/presets` (includes `art`), `/api/nowplaying`, `/api/services`.
- Live: SSE `/events` → `np` (now-playing JSON) and `frame` (screen version → fetch `/api/screen`, draw on both LCD canvases). Fallback poll every 5 s (`/api/nowplaying`, `/api/services`, `/api/state`) also catches up a missed frame.
- `/api/favourites` (~75 KB, 246 items with art) is loaded lazily the first time the editor opens.
- Volume slider sends `/api/action?type=volume&n=0..100` (debounced 120 ms); the worker coalesces queued volume actions. Incoming `np.volume` is ignored for 2.5 s after the user touches the slider.
- Play/Pause/Stop and preset taps update the UI optimistically; the next `np` event corrects it.

## Testing the page

`curl` can't run the JS, and desktop Chrome won't shrink a window below ~500 px. A small DevTools driver
(`node cdp.mjs <url> <steps.json> <outdir>`, kept in the session scratchpad; recreate from history if needed)
launches headless Chrome with `Emulation.setDeviceMetricsOverride` (390×844 @2x, mobile, touch), can switch
`prefers-color-scheme`, click selectors, evaluate JS and save screenshots.
