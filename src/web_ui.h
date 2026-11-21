#pragma once
#include <Arduino.h>

// Single-page UI served at "/" ("Preset tiles" design, see docs/web-ui.md).
// Plain HTML/CSS/JS with no external assets, so it works on a LAN without
// internet. Mobile first: safe areas, 44 px targets, light/dark from the phone.
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-title" content="Spotty">
<meta name="apple-mobile-web-app-status-bar-style" content="default">
<meta name="theme-color" content="#F2F2F7" media="(prefers-color-scheme: light)">
<meta name="theme-color" content="#000000" media="(prefers-color-scheme: dark)">
<title>Spotty</title>
<style>
:root{--bg:#F2F2F7;--card:#FFFFFF;--fill:#F2F2F7;--fill2:#E3E3E8;--text:#1C1C1E;--text2:#6C6C70;--text3:#AEAEB2;--sep:#E5E5EA;
  --accent:#0A7AFF;--accent-soft:#EAF2FF;--danger:#D70015;--warn:#C93400;--green:#34C759;--glass:rgba(255,255,255,.88);--shadow:0 8px 28px rgba(0,0,0,.12);
  --font:-apple-system,BlinkMacSystemFont,"SF Pro Text","Segoe UI",Roboto,system-ui,sans-serif;
  --st:env(safe-area-inset-top,0px);--sb:env(safe-area-inset-bottom,0px);--sl:env(safe-area-inset-left,0px);--sr:env(safe-area-inset-right,0px)}
@media (prefers-color-scheme:dark){:root{--bg:#000000;--card:#1C1C1E;--fill:#2C2C2E;--fill2:#2C2C2E;--text:#FFFFFF;--text2:#98989F;--text3:#636366;--sep:#38383A;
  --accent:#0A84FF;--accent-soft:#0A2A4D;--danger:#FF453A;--warn:#FF9F0A;--glass:rgba(28,28,30,.88);--shadow:0 8px 28px rgba(0,0,0,.5)}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{margin:0;background:var(--bg);color:var(--text);font:17px/1.3 var(--font);-webkit-font-smoothing:antialiased;-webkit-text-size-adjust:100%}
button{font:inherit;color:inherit;background:none;border:0;padding:0;cursor:pointer;touch-action:manipulation}
button:focus-visible,input:focus-visible,[tabindex]:focus-visible{outline:3px solid var(--accent);outline-offset:2px;border-radius:10px}
svg{display:block}
[hidden]{display:none!important}
.wrap{max-width:560px;margin:0 auto;padding:calc(var(--st) + 20px) calc(var(--sr) + 20px) calc(var(--sb) + 110px) calc(var(--sl) + 20px)}
.top{display:flex;align-items:flex-end;justify-content:space-between;gap:12px}
h1{margin:0;font-size:34px;font-weight:700;letter-spacing:-.02em}
.sub{margin:2px 0 0;font-size:15px;color:var(--text2)}
.round{width:44px;height:44px;border-radius:22px;background:var(--card);display:flex;align-items:center;justify-content:center;color:var(--accent);flex-shrink:0}
.sechead{display:flex;align-items:center;justify-content:space-between;margin:22px 4px 8px}
.sechead h2{margin:0;font-size:20px;font-weight:700}
.link{color:var(--accent);font-size:17px;min-height:44px;padding:0 4px}
.tiles{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:12px}
.tile{position:relative;aspect-ratio:1/1;align-self:start;width:100%;border-radius:20px;overflow:hidden;text-align:left;background:var(--card);transition:transform .15s}
.tile:active{transform:scale(.97)}
.tile .art{position:absolute;inset:0;border-radius:0}
.tile .shade{position:absolute;inset:0;background:linear-gradient(180deg,rgba(0,0,0,0) 38%,rgba(0,0,0,.72) 100%)}
.tile .txt{position:absolute;left:14px;right:14px;bottom:12px;display:flex;flex-direction:column;color:#fff}
.tile .nm{font-size:17px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tile .ds{font-size:13px;opacity:.88;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tile.on{box-shadow:0 0 0 3px var(--accent)}
.badge{position:absolute;top:10px;right:10px;padding:3px 8px;border-radius:10px;font-size:11px;font-weight:700;letter-spacing:.02em;color:#fff;background:var(--accent)}
.badge.warn{background:var(--warn)}
.tile.empty{border:2px dashed var(--text3);background:transparent;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;color:var(--text2);text-align:center;font-size:15px;font-weight:600}
.editing .tile:not(.empty){animation:jig .28s ease-in-out infinite alternate}
.editing .tile .pen{display:flex}
.pen{display:none;position:absolute;top:10px;left:10px;width:30px;height:30px;border-radius:15px;background:rgba(0,0,0,.55);color:#fff;align-items:center;justify-content:center}
@keyframes jig{from{transform:rotate(-.8deg)}to{transform:rotate(.8deg)}}
@media (prefers-reduced-motion:reduce){.editing .tile:not(.empty){animation:none}*{transition:none!important}}
.art{display:block;position:relative;overflow:hidden;background:linear-gradient(135deg,#5E5CE6,#0A7AFF);color:#fff;border-radius:8px}
.art svg{position:absolute;left:50%;top:50%;transform:translate(-50%,-50%);opacity:.85}
.art img{position:absolute;inset:0;width:100%;height:100%;object-fit:cover}
.card{background:var(--card);border-radius:20px}
.controls{margin-top:16px;padding:16px;display:flex;gap:16px}
.vslider{position:relative;width:64px;min-height:176px;border-radius:18px;background:var(--fill2);overflow:hidden;flex-shrink:0;touch-action:none;cursor:ns-resize;--p:0%}
.vslider .fill{position:absolute;left:0;right:0;bottom:0;height:var(--p);background:var(--accent);transition:height .12s}
.vslider .vn{position:absolute;top:10px;left:0;right:0;text-align:center;font-size:13px;font-weight:700;color:var(--text2)}
.vslider svg{position:absolute;left:21px;bottom:14px;color:#fff}.vslider.lo svg{color:var(--text2)}
.ctlgrid{flex-grow:1;display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.ctl{border-radius:16px;background:var(--fill);display:flex;flex-direction:column;align-items:flex-start;justify-content:space-between;padding:12px;min-height:83px;font-size:15px;font-weight:600;transition:transform .12s}
.ctl:active{transform:scale(.96)}
.ctl.primary{background:var(--accent);color:#fff}
.mini{position:fixed;left:calc(var(--sl) + 12px);right:calc(var(--sr) + 12px);bottom:calc(var(--sb) + 12px);max-width:536px;margin:0 auto;padding:10px 12px;border-radius:18px;background:var(--glass);
  -webkit-backdrop-filter:saturate(180%) blur(20px);backdrop-filter:saturate(180%) blur(20px);box-shadow:var(--shadow);display:flex;align-items:center;gap:12px;z-index:5}
.mini .art{width:48px;height:48px;flex-shrink:0}
.mini .t{flex-grow:1;min-width:0}
.mini .t1{font-size:15px;font-weight:600;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.mini .t2{font-size:13px;color:var(--text2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ib{width:44px;height:44px;display:flex;align-items:center;justify-content:center;flex-shrink:0}
/* grouped lists */
.gh{margin:24px 16px 7px;font-size:13px;color:var(--text2);text-transform:uppercase;letter-spacing:.02em}
.gf{margin:8px 16px 0;font-size:13px;line-height:18px;color:var(--text2)}
.group{background:var(--card);border-radius:12px;overflow:hidden}
.row{display:flex;align-items:center;gap:12px;min-height:48px;padding:6px 16px;width:100%;text-align:left;position:relative}
.row+.row::before,.row+.rowx::before,.rowx+.row::before{content:"";position:absolute;top:0;right:0;left:16px;height:1px;background:var(--sep)}
.row.with-art+.row.with-art::before{left:68px}
.row .l{flex-grow:1;min-width:0}
.row .l .tt{display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden}
.row .l small{display:block;font-size:13px;color:var(--text2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.row .v{color:var(--text2);text-align:right;min-width:0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.row .art{width:44px;height:44px;flex-shrink:0;border-radius:6px}
.row.dis{opacity:.5}
.row.act{color:var(--accent)}
.row.danger{color:var(--danger)}
.chev{flex-shrink:0;color:var(--text3)}
.check{color:var(--accent);flex-shrink:0}
.pill{padding:2px 8px;border-radius:8px;background:var(--accent-soft);color:var(--accent);font-size:12px;font-weight:600;flex-shrink:0}
input.txt{flex-grow:1;min-width:0;font:inherit;font-size:17px;color:var(--text);background:none;border:0;outline:none;text-align:right}
input.sw{appearance:none;-webkit-appearance:none;width:51px;height:31px;border-radius:16px;background:var(--fill2);position:relative;flex-shrink:0;margin:0;cursor:pointer;transition:background .2s}
input.sw::before{content:"";position:absolute;top:2px;left:2px;width:27px;height:27px;border-radius:50%;background:#fff;box-shadow:0 2px 4px rgba(0,0,0,.2);transition:transform .2s}
input.sw:checked{background:var(--green)}input.sw:checked::before{transform:translateX(20px)}
input.rng{flex-grow:1;-webkit-appearance:none;appearance:none;height:28px;background:transparent;margin:0;--p:50%}
input.rng::-webkit-slider-runnable-track{height:6px;border-radius:3px;background:linear-gradient(90deg,var(--accent) var(--p),var(--fill2) var(--p))}
input.rng::-moz-range-track{height:6px;border-radius:3px;background:linear-gradient(90deg,var(--accent) var(--p),var(--fill2) var(--p))}
input.rng::-webkit-slider-thumb{-webkit-appearance:none;width:28px;height:28px;border-radius:50%;background:#fff;box-shadow:0 1px 4px rgba(0,0,0,.3);margin-top:-11px}
input.rng::-moz-range-thumb{width:28px;height:28px;border:0;border-radius:50%;background:#fff;box-shadow:0 1px 4px rgba(0,0,0,.3)}
.btn{width:100%;min-height:50px;border-radius:12px;background:var(--accent);color:#fff;font-weight:600;display:flex;align-items:center;justify-content:center;gap:8px}
.btn.plain{background:var(--card);color:var(--accent)}
/* push view + sheets */
.push{position:fixed;inset:0;background:var(--bg);overflow-y:auto;-webkit-overflow-scrolling:touch;transform:translateX(100%);transition:transform .35s cubic-bezier(.32,.72,0,1);z-index:10;visibility:hidden}
.push.open{transform:none;visibility:visible}
.navbar{display:flex;align-items:center;justify-content:space-between;min-height:44px}
.back{display:flex;align-items:center;gap:4px;color:var(--accent);min-height:44px;padding-right:8px}
.scrim{position:fixed;inset:0;background:rgba(0,0,0,.35);opacity:0;pointer-events:none;transition:opacity .3s;z-index:20}
.scrim.on{opacity:1;pointer-events:auto}
.sheet{position:fixed;left:0;right:0;bottom:0;top:calc(var(--st) + 10px);max-width:560px;margin:0 auto;background:var(--bg);border-radius:12px 12px 0 0;display:flex;flex-direction:column;
  transform:translateY(105%);transition:transform .38s cubic-bezier(.32,.72,0,1);z-index:30;visibility:hidden}
.sheet.open{transform:none;visibility:visible}
.sheet.s2{z-index:31}
.grab{width:36px;height:5px;border-radius:3px;background:var(--text3);margin:6px auto 0;flex-shrink:0}
.snav{display:grid;grid-template-columns:1fr auto 1fr;align-items:center;padding:4px 8px;flex-shrink:0}
.snav .ttl{font-weight:600}
.snav button{min-height:44px;padding:0 8px;color:var(--accent)}
.snav .r{justify-self:end;font-weight:600}
.snav button:disabled{color:var(--text3)}
.sbody{flex-grow:1;overflow-y:auto;-webkit-overflow-scrolling:touch;padding:0 16px calc(var(--sb) + 28px)}
.search{display:flex;align-items:center;gap:8px;height:38px;padding:0 10px;border-radius:10px;background:var(--fill2);color:var(--text2);margin:4px 0 10px}
.search input{flex-grow:1;min-width:0;font:inherit;font-size:17px;color:var(--text);background:none;border:0;outline:none}
.seg{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));padding:2px;border-radius:9px;background:var(--fill2)}
.seg button{height:30px;border-radius:7px;font-size:13px}
.seg button[aria-selected=true]{background:var(--card);font-weight:600;box-shadow:0 1px 3px rgba(0,0,0,.12)}
/* LCD simulation: 1 CSS px per LCD pixel (240x280), same bezel/corners as the original page */
.screen{display:flex;flex-direction:column;align-items:center;gap:12px;padding:20px 16px 16px}
.bezel{background:#08090a;padding:14px;border-radius:44px;box-shadow:0 6px 24px rgba(0,0,0,.35),inset 0 0 0 1px #2a2d31}
.lcd{display:block;width:240px;height:280px;border-radius:32px;background:#0e1013;image-rendering:pixelated}
@media (min-width:560px){.lcd{width:300px;height:350px;border-radius:40px}}
.lcd-home{display:flex;flex-direction:column;align-items:center;gap:8px;margin:20px 0 0}
.screen-meta{display:flex;align-items:center;gap:10px;font-size:13px;color:var(--text2)}
.live{display:flex;align-items:center;gap:6px;font-size:13px;font-weight:600;color:var(--green)}
.dot{width:8px;height:8px;border-radius:4px;background:currentColor}
.steps{margin:0;padding:0 16px 12px 34px;font-size:15px;line-height:21px;color:var(--text2)}
.steps li{margin-top:8px}
code{font:13px ui-monospace,Menlo,monospace;background:var(--fill2);padding:1px 5px;border-radius:5px;word-break:break-all}
.warnbox{color:var(--warn);font-size:15px;padding:12px 16px}
#toast{position:fixed;left:50%;top:calc(var(--st) + 12px);transform:translate(-50%,-20px);background:var(--glass);-webkit-backdrop-filter:blur(20px);backdrop-filter:blur(20px);
  box-shadow:var(--shadow);padding:10px 16px;border-radius:20px;font-size:15px;font-weight:600;opacity:0;transition:all .25s;z-index:50;pointer-events:none;max-width:90vw}
#toast.on{opacity:1;transform:translate(-50%,0)}
</style></head><body>

<main class="wrap" id="home">
  <header class="top">
    <div><h1>Spotty</h1><p class="sub" id="sub">Connecting…</p></div>
    <button class="round" id="gear" aria-label="Controller settings"><svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><circle cx="12" cy="12" r="3"/><path d="M19.4 15a1.7 1.7 0 0 0 .3 1.8l.1.1a2 2 0 1 1-2.8 2.8l-.1-.1a1.7 1.7 0 0 0-1.8-.3 1.7 1.7 0 0 0-1 1.5V21a2 2 0 1 1-4 0v-.1a1.7 1.7 0 0 0-1.1-1.5 1.7 1.7 0 0 0-1.8.3l-.1.1a2 2 0 1 1-2.8-2.8l.1-.1a1.7 1.7 0 0 0 .3-1.8 1.7 1.7 0 0 0-1.5-1H3a2 2 0 1 1 0-4h.1a1.7 1.7 0 0 0 1.5-1.1 1.7 1.7 0 0 0-.3-1.8l-.1-.1a2 2 0 1 1 2.8-2.8l.1.1a1.7 1.7 0 0 0 1.8.3H9a1.7 1.7 0 0 0 1-1.5V3a2 2 0 1 1 4 0v.1a1.7 1.7 0 0 0 1 1.5 1.7 1.7 0 0 0 1.8-.3l.1-.1a2 2 0 1 1 2.8 2.8l-.1.1a1.7 1.7 0 0 0-.3 1.8V9a1.7 1.7 0 0 0 1.5 1H21a2 2 0 1 1 0 4h-.1a1.7 1.7 0 0 0-1.5 1z"/></svg></button>
  </header>
  <div class="lcd-home">
    <div class="bezel"><canvas class="lcd" id="screenHome" width="240" height="280" role="img" aria-label="Live copy of the controller's screen"></canvas></div>
    <div class="screen-meta">Controller display</div>
  </div>
  <div class="sechead"><h2>Presets</h2><button class="link" id="editBtn">Edit</button></div>
  <div class="tiles" id="tiles"></div>
  <section class="card controls" aria-label="Playback controls">
    <div class="vslider" id="vol" role="slider" tabindex="0" aria-label="Volume" aria-valuemin="0" aria-valuemax="100" aria-valuenow="0">
      <div class="fill"></div><span class="vn" id="volNum">–</span>
      <svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M11 5 6 9H2v6h4l5 4V5z"/><path d="M15.5 8.5a5 5 0 0 1 0 7"/></svg>
    </div>
    <div class="ctlgrid">
      <button class="ctl primary" id="bPlay"><span id="bPlayIc"></span><span id="bPlayLbl">Play</span></button>
      <button class="ctl" id="bNext"><svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M3 5.5v13l9-6.5z"/><path d="M12 5.5v13l9-6.5z"/></svg><span>Next</span></button>
      <button class="ctl" id="bStop"><svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><rect x="5" y="5" width="14" height="14" rx="2.5"/></svg><span>Stop</span></button>
      <button class="ctl" id="bRand"><svg width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M16 3h5v5"/><path d="M4 20 21 3"/><path d="M21 16v5h-5"/><path d="m15 15 6 6"/><path d="M4 4l5 5"/></svg><span>Random</span></button>
    </div>
  </section>
</main>

<div class="mini" id="mini" hidden>
  <span class="art" id="miniArt"></span>
  <div class="t"><div class="t1" id="miniT1"></div><div class="t2" id="miniT2"></div></div>
  <button class="ib" id="miniPlay" aria-label="Play"></button>
</div>

<section class="push" id="ctrl" aria-label="Controller settings">
  <div class="wrap" style="padding-bottom:calc(var(--sb) + 40px)">
    <div class="navbar"><button class="back" id="ctrlBack"><svg width="12" height="20" viewBox="0 0 10 17" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M8.5 1.5 1.5 8.5l7 7"/></svg>Spotty</button></div>
    <h1 style="margin-top:4px">Controller</h1>
    <div class="group screen" style="margin-top:14px">
      <div class="bezel"><canvas class="lcd" id="screen" width="240" height="280" aria-label="Live copy of the controller's screen"></canvas></div>
      <div class="screen-meta"><span>240 × 280 display preview</span><span class="live" id="live"><span class="dot"></span>Live</span></div>
    </div>
    <div class="gh">Sonos</div>
    <div class="group">
      <div class="row"><span class="l">Rooms</span><span class="v" id="cRooms">–</span></div>
      <div class="row"><span class="l">Favourites</span><span class="v" id="cFavs">–</span></div>
      <div class="row"><span class="l">Status</span><span class="v" id="cPhase">–</span></div>
      <button class="row act" id="rescan">Search for speakers again</button>
    </div>
    <div class="gh">Music services</div>
    <div class="group">
      <button class="row" id="spRow"><span class="art" style="width:32px;height:32px;border-radius:8px;background:#1DB954"><svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="#fff" stroke-width="2.2" stroke-linecap="round" aria-hidden="true"><path d="M6 9c4-1.5 8.5-1 12 1"/><path d="M7 12.5c3.3-1 6.6-.6 9.5.9"/><path d="M8 16c2.5-.6 5-.3 7 .8"/></svg></span>
        <span class="l">Spotify<small id="spSum">–</small></span><svg class="chev" width="8" height="14" viewBox="0 0 8 14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="m1 1 6 6-6 6"/></svg></button>
    </div>
    <div class="gh">Network</div>
    <div class="group">
      <div class="row"><span class="l">Wi-Fi</span><span class="v" id="cWifi">–</span></div>
      <div class="row"><span class="l">Address</span><span class="v" id="cAddr">spotty.local</span></div>
    </div>
  </div>
</section>

<div class="scrim" id="scrim"></div>

<section class="sheet" id="editor" role="dialog" aria-modal="true" aria-labelledby="edTitle">
  <div class="grab"></div>
  <div class="snav"><button id="edCancel">Cancel</button><span class="ttl" id="edTitle">Edit Preset</span><button class="r" id="edSave">Save</button></div>
  <div class="sbody">
    <div class="group" style="margin-top:8px"><label class="row"><span>Name</span><input class="txt" id="edName" maxlength="24" autocomplete="off" enterkeyhint="done"></label></div>
    <div class="gh">Play</div>
    <div class="group"><button class="row with-art" id="edFav"></button></div>
    <p class="gf warnbox" id="edWarn" hidden style="padding:0;margin:8px 16px 0"></p>
    <div class="gh">Rooms</div>
    <div class="group" id="edRooms"></div>
    <p class="gf">The first room you tick leads the group. Rooms not ticked are taken out of it when this preset plays.</p>
    <div class="gh">Playback</div>
    <div class="group">
      <label class="row"><span class="l">Set volume</span><input type="checkbox" class="sw" id="edSetVol"></label>
      <div class="rowx" style="display:flex;align-items:center;gap:12px;padding:4px 16px 12px" id="edVolRow">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true" style="color:var(--text2)"><path d="M11 5 6 9H2v6h4l5 4V5z"/></svg>
        <input type="range" class="rng" id="edVol" min="0" max="100" step="1" aria-label="Preset volume"><span id="edVolN" style="width:44px;text-align:right;color:var(--text2)"></span>
      </div>
      <label class="row"><span class="l">Shuffle<small>Playlists and albums</small></span><input type="checkbox" class="sw" id="edShuf"></label>
    </div>
    <div style="margin-top:24px;display:flex;flex-direction:column;gap:12px">
      <button class="btn" id="edPlay"><svg width="16" height="16" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M6 4v16l14-8z"/></svg>Save and play now</button>
      <button class="btn plain" id="edClear" style="color:var(--danger)">Clear preset</button>
    </div>
  </div>
</section>

<section class="sheet s2" id="picker" role="dialog" aria-modal="true" aria-labelledby="pkTitle">
  <div class="grab"></div>
  <div class="snav"><button id="pkBack" style="justify-self:start;display:flex;align-items:center;gap:4px"><svg width="10" height="17" viewBox="0 0 10 17" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M8.5 1.5 1.5 8.5l7 7"/></svg>Back</button><span class="ttl" id="pkTitle">Choose Music</span><span></span></div>
  <div style="padding:0 16px;flex-shrink:0">
    <label class="search"><svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.4" stroke-linecap="round" aria-hidden="true"><circle cx="11" cy="11" r="7"/><path d="m20 20-3.5-3.5"/></svg><input type="search" id="pkQ" placeholder="Search" aria-label="Search music" autocomplete="off" enterkeyhint="search"></label>
    <div class="seg" role="tablist" aria-label="Source" id="pkSeg">
      <button role="tab" data-f="all" aria-selected="true">All</button><button role="tab" data-f="fav" aria-selected="false">Favourites</button><button role="tab" data-f="pl" aria-selected="false">Playlists</button><button role="tab" data-f="al" aria-selected="false">Albums</button>
    </div>
  </div>
  <div class="sbody" id="pkList"></div>
</section>

<section class="sheet" id="spotify" role="dialog" aria-modal="true" aria-labelledby="spTitle">
  <div class="grab"></div>
  <div class="snav"><span></span><span class="ttl" id="spTitle">Spotify</span><button class="r" id="spDone">Done</button></div>
  <div class="sbody" id="spBody"></div>
</section>

<div id="toast" role="status" aria-live="polite"></div>

<script>
const $=s=>document.querySelector(s),esc=s=>String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const j=(u,o)=>fetch(u,o).then(r=>r.json());
const post=(u,d)=>fetch(u,{method:'POST',body:new URLSearchParams(d||{})}).then(r=>r.json());
const NOTE='<svg width="22" height="22" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M9 18V6l11-2v12"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="17.5" cy="16" r="2.5"/></svg>';
const ICON={play:'<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><path d="M6 4v16l14-8z"/></svg>',
  pause:'<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor" aria-hidden="true"><rect x="6" y="4" width="4" height="16" rx="1.2"/><rect x="14" y="4" width="4" height="16" rx="1.2"/></svg>',
  check:'<svg class="check" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="m5 12 5 5L20 7"/></svg>',
  chev:'<svg class="chev" width="8" height="14" viewBox="0 0 8 14" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="m1 1 6 6-6 6"/></svg>',
  pen:'<svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true"><path d="M12 20h9"/><path d="M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z"/></svg>',
  plus:'<svg width="28" height="28" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" aria-hidden="true"><path d="M12 5v14"/><path d="M5 12h14"/></svg>'};
const art=(url,style)=>`<span class="art"${style?` style="${style}"`:''}>${NOTE}${url?`<img src="${esc(url)}" alt="" loading="lazy" referrerpolicy="no-referrer" onerror="this.remove()">`:''}</span>`;

let np={},presets=[],rooms=[],favs=null,editing=false,status={},sp={};
function toast(t){const e=$('#toast');e.textContent=t;e.classList.add('on');clearTimeout(e._t);e._t=setTimeout(()=>e.classList.remove('on'),1800)}
function act(type,n=0){return fetch(`/api/action?type=${type}&n=${n}`,{method:'POST'}).catch(()=>toast('Controller offline'))}

// ---- home ----------------------------------------------------------------
function roomsLabel(uuids){
  if(!uuids.length) return '';
  if(rooms.length>1&&uuids.length===rooms.length) return 'All rooms';
  const n=u=>(rooms.find(r=>r.uuid===u)||{}).name||'Room';
  return uuids.length===1?n(uuids[0]):`${n(uuids[0])} + ${uuids.length-1}`;
}
const playing=()=>np.state==='PLAYING'||np.state==='TRANSITIONING';
function renderTiles(){
  $('#tiles').innerHTML=presets.map((p,i)=>{
    const set=p.rooms.length&&p.favUri;
    if(!set) return `<button class="tile empty" data-i="${i}">${ICON.plus}<span>Set up preset ${i+1}</span></button>`;
    const on=np.preset===i&&np.ok, desc=[roomsLabel(p.rooms),p.volume>=0?p.volume+'%':''].filter(Boolean).join(' · ');
    const badge=p.favUnsupported?`<span class="badge warn">Can't play</span>`:on?`<span class="badge">${playing()?'PLAYING':'PAUSED'}</span>`:'';
    return `<button class="tile${on?' on':''}" data-i="${i}" aria-label="${esc(p.name)}${on?', playing':''}">${art(p.art)}<span class="shade"></span>
      <span class="txt"><span class="nm">${esc(p.name)}</span><span class="ds">${esc(desc)}</span></span>${badge}<span class="pen">${ICON.pen}</span></button>`;
  }).join('');
  $('#tiles').classList.toggle('editing',editing);
}
$('#tiles').onclick=e=>{const t=e.target.closest('.tile');if(!t)return;const i=+t.dataset.i;
  if(editing||t.classList.contains('empty')) return openEditor(i);
  const p=presets[i]; if(p.favUnsupported) return toast(`${p.favUnsupported} can't be started by Spotty`);
  np.preset=i; renderTiles(); act('preset',i); toast(`Starting ${p.name}`);};
$('#editBtn').onclick=()=>{editing=!editing;$('#editBtn').textContent=editing?'Done':'Edit';renderTiles()};

function renderNow(){
  const word={PLAYING:'Playing',PAUSED_PLAYBACK:'Paused',STOPPED:'Stopped',TRANSITIONING:'Starting…'}[np.state]||'';
  $('#sub').textContent=np.ok?[np.group,word].filter(Boolean).join(' · '):'Connecting…';
  const pl=playing();
  $('#bPlayIc').innerHTML=pl?ICON.pause:ICON.play; $('#bPlayLbl').textContent=pl?'Pause':'Play';
  $('#miniPlay').innerHTML=pl?ICON.pause:ICON.play; $('#miniPlay').setAttribute('aria-label',pl?'Pause':'Play');
  const m=$('#mini'); m.hidden=!(np.ok&&np.title);
  if(!m.hidden){ const u=np.art||''; if($('#miniArt').dataset.u!==u){$('#miniArt').outerHTML=art(u).replace('<span class="art"','<span class="art" id="miniArt" data-u="'+esc(u)+'"');}
    $('#miniT1').textContent=np.title; $('#miniT2').textContent=[np.artist,np.group].filter(Boolean).join(' · ');}
  if(!volBusy()) setVol(np.volume>=0?np.volume:0,np.volume>=0);
  renderTiles();
}
const toggle=()=>{np.state=playing()?'PAUSED_PLAYBACK':'PLAYING';renderNow();act('playpause')};
$('#bPlay').onclick=toggle; $('#miniPlay').onclick=toggle;
$('#bNext').onclick=()=>act('next');
$('#bStop').onclick=()=>{np.state='STOPPED';renderNow();act('stop')};
$('#bRand').onclick=()=>{act('random');toast('Playing a random favourite')};

// vertical volume slider
const vol=$('#vol');let volDrag=false,volLast=0,volT=0,volV=0;
const volBusy=()=>volDrag||Date.now()-volLast<2500;
function setVol(v,known=true){volV=v;vol.classList.toggle('lo',v<22);vol.style.setProperty('--p',v+'%');vol.setAttribute('aria-valuenow',v);$('#volNum').textContent=known?v:'–'}
function sendVol(){volLast=Date.now();clearTimeout(volT);volT=setTimeout(()=>act('volume',volV),120)}
const volAt=e=>{const r=vol.getBoundingClientRect();return Math.round(Math.max(0,Math.min(1,(r.bottom-e.clientY)/r.height))*100)};
vol.addEventListener('pointerdown',e=>{volDrag=true;vol.setPointerCapture(e.pointerId);setVol(volAt(e));sendVol()});
vol.addEventListener('pointermove',e=>{if(volDrag){setVol(volAt(e));sendVol()}});
const volEnd=()=>{if(volDrag){volDrag=false;sendVol()}};
vol.addEventListener('pointerup',volEnd);vol.addEventListener('pointercancel',volEnd);
vol.addEventListener('keydown',e=>{const d={ArrowUp:5,ArrowRight:5,ArrowDown:-5,ArrowLeft:-5}[e.key];if(d){e.preventDefault();setVol(Math.max(0,Math.min(100,volV+d)));sendVol()}});

// ---- sheets & views --------------------------------------------------------
const stack=[];
function openSheet(id){const s=$('#'+id);stack.push(s);s.classList.add('open');$('#scrim').classList.add('on');document.body.style.overflow='hidden';setTimeout(()=>s.querySelector('.snav button,button')?.focus({preventScroll:true}),350)}
function closeSheet(){const s=stack.pop();if(!s)return;s.classList.remove('open');if(!stack.length){$('#scrim').classList.remove('on');document.body.style.overflow=''}}
$('#scrim').onclick=()=>{while(stack.length)closeSheet()};
document.addEventListener('keydown',e=>{if(e.key==='Escape'){if(stack.length)closeSheet();else if(ctrlOpen)closeCtrl()}});

// ---- preset editor -----------------------------------------------------------
let ed=null;
async function loadFavs(force){if(!favs||force){try{favs=await j('/api/favourites')}catch(e){favs=favs||[]}}return favs}
function openEditor(i){
  const p=presets[i]; ed={i,name:p.name,rooms:[...p.rooms],volume:p.volume,shuffle:p.shuffle,fav:p.favUri?{uri:p.favUri,title:p.favTitle,art:p.art,unsupported:p.favUnsupported}:null};
  $('#edTitle').textContent=p.rooms.length&&p.favUri?'Edit Preset':'New Preset';
  $('#edName').value=p.name; $('#edSetVol').checked=p.volume>=0; const v=p.volume>=0?p.volume:30; $('#edVol').value=v;
  $('#edShuf').checked=p.shuffle; $('#edClear').hidden=!(p.rooms.length||p.favUri);
  renderEd(); openSheet('editor'); loadFavs();
}
function renderEd(){
  const f=ed.fav;
  $('#edFav').innerHTML=f?`${art(f.art)}<span class="l"><span class="tt">${esc(f.title)}</span><small>${esc(f.group||'Change')}</small></span><span class="v" style="color:var(--text2);font-size:15px">Change</span>${ICON.chev}`
    :`<span class="art" style="background:var(--fill2);color:var(--accent)">${ICON.plus}</span><span class="l" style="color:var(--accent)">Choose music</span>${ICON.chev}`;
  $('#edWarn').hidden=!(f&&f.unsupported); if(f&&f.unsupported) $('#edWarn').textContent=`${f.unsupported} favourites can't be started by Spotty. Choose something else.`;
  const known=new Set(rooms.map(r=>r.uuid));
  $('#edRooms').innerHTML=rooms.map(r=>{const k=ed.rooms.indexOf(r.uuid);return `<button class="row" data-u="${esc(r.uuid)}" aria-pressed="${k>=0}"><span class="l">${esc(r.name)}</span>${k===0?'<span class="pill">Leads group</span>':''}${k>=0?ICON.check:''}</button>`}).join('')
    +ed.rooms.filter(u=>!known.has(u)).map(u=>`<button class="row" data-u="${esc(u)}" aria-pressed="true"><span class="l">Offline room</span>${ICON.check}</button>`).join('')
    ||'<div class="row"><span class="l" style="color:var(--text2)">No rooms found yet</span></div>';
  const sv=$('#edSetVol').checked; $('#edVolRow').style.display=sv?'flex':'none';
  const v=+$('#edVol').value; $('#edVol').style.setProperty('--p',v+'%'); $('#edVolN').textContent=v+'%';
  const ok=ed.rooms.length&&ed.fav&&!ed.fav.unsupported; $('#edSave').disabled=!ok; $('#edPlay').disabled=!ok; $('#edPlay').style.opacity=ok?1:.4;
}
$('#edRooms').onclick=e=>{const b=e.target.closest('[data-u]');if(!b)return;const u=b.dataset.u,k=ed.rooms.indexOf(u);if(k>=0)ed.rooms.splice(k,1);else ed.rooms.push(u);renderEd()};
$('#edSetVol').onchange=renderEd; $('#edVol').oninput=renderEd;
$('#edCancel').onclick=closeSheet;
$('#edFav').onclick=async()=>{openSheet('picker');$('#pkQ').value='';await loadFavs();renderPicker()};
async function saveEd(play){
  const all=presets.map(p=>({name:p.name,rooms:p.rooms,volume:p.volume,shuffle:p.shuffle,favUri:p.favUri,favTitle:p.favTitle}));
  const clear=play==='clear';
  all[ed.i]=clear?{name:`Preset ${ed.i+1}`,rooms:[],volume:-1,shuffle:false,favUri:'',favTitle:''}
    :{name:$('#edName').value.trim()||`Preset ${ed.i+1}`,rooms:ed.rooms,volume:$('#edSetVol').checked?+$('#edVol').value:-1,shuffle:$('#edShuf').checked,favUri:ed.fav?.uri||'',favTitle:ed.fav?.title||''};
  const r=await fetch('/api/presets',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(all)}).catch(()=>null);
  if(!r||!r.ok) return toast('Couldn’t save');
  closeSheet(); await loadPresets(); toast(clear?'Preset cleared':'Saved');
  if(play===true){np.preset=ed.i;renderTiles();act('preset',ed.i)}
}
$('#edSave').onclick=()=>saveEd(false); $('#edPlay').onclick=()=>saveEd(true);
$('#edClear').onclick=()=>{if(confirm('Clear this preset?'))saveEd('clear')};

// ---- music picker -------------------------------------------------------------
let pkF='all';
const kind=g=>/playlists/i.test(g)?'pl':/albums|releases/i.test(g)?'al':'fav';
const sub2=f=>f.unsupported?`${f.unsupported} · can't be started by Spotty`:kind(f.group)==='pl'?'Playlist':kind(f.group)==='al'?'Album':'Sonos favourite';
function renderPicker(){
  const q=$('#pkQ').value.trim().toLowerCase();
  const list=(favs||[]).filter(f=>(pkF==='all'||kind(f.group)===pkF)&&(!q||f.title.toLowerCase().includes(q)));
  const groups=[...new Set(list.map(f=>f.group))];
  $('#pkList').innerHTML=groups.map(g=>`<div class="gh">${esc(g)}</div><div class="group">${list.filter(f=>f.group===g).slice(0,300).map(f=>{
      const sel=ed.fav&&ed.fav.uri===f.uri;
      return `<button class="row with-art${f.unsupported?' dis':''}" data-uri="${esc(f.uri)}" ${f.unsupported?'aria-disabled="true"':''}>${art(f.art)}<span class="l"${sel?' style="font-weight:600"':''}><span class="tt">${esc(f.title)}</span><small>${esc(sub2(f))}</small></span>${sel?ICON.check:''}</button>`}).join('')}</div>`).join('')
    ||`<p class="gf" style="text-align:center;margin-top:40px">${favs&&favs.length?'No matches':'Loading…'}</p>`;
}
$('#pkQ').oninput=renderPicker;
$('#pkSeg').onclick=e=>{const b=e.target.closest('[data-f]');if(!b)return;pkF=b.dataset.f;$('#pkSeg').querySelectorAll('button').forEach(x=>x.setAttribute('aria-selected',x===b));renderPicker()};
$('#pkList').onclick=e=>{const b=e.target.closest('[data-uri]');if(!b)return;const f=favs.find(x=>x.uri===b.dataset.uri);
  if(!f||f.unsupported) return toast(f?`${f.unsupported} can't be started by Spotty`:'');
  ed.fav=f; if(!$('#edName').value.trim()||/^Preset \d$/.test($('#edName').value)) $('#edName').value=f.title.slice(0,24); renderEd(); closeSheet()};
$('#pkBack').onclick=closeSheet;

// ---- controller page ------------------------------------------------------------
let ctrlOpen=false;
$('#gear').onclick=()=>{ctrlOpen=true;$('#ctrl').classList.add('open');refreshCtrl();frame();setTimeout(()=>$('#ctrlBack').focus(),360)};
function closeCtrl(){ctrlOpen=false;$('#ctrl').classList.remove('open');$('#gear').focus()}
$('#ctrlBack').onclick=closeCtrl;
$('#rescan').onclick=()=>{act('rescan');toast('Searching for speakers…');setTimeout(()=>{loadAll();refreshCtrl()},8000)};
const quality=r=>r>=-60?'Excellent':r>=-67?'Good':r>=-75?'Fair':'Weak';
async function refreshCtrl(){
  try{status=await j('/api/state');
    $('#cRooms').textContent=rooms.map(r=>r.name).join(', ')||status.rooms;
    $('#cFavs').textContent=status.favourites;
    $('#cPhase').textContent={ready:'Connected',discovering:'Searching…','no-sonos':'Not found',starting:'Starting'}[status.phase]||status.phase;
    $('#cWifi').textContent=`${status.ssid} · ${quality(status.rssi)}`; $('#cAddr').textContent=`spotty.local · ${status.ip}`;
    $('#live').innerHTML='<span class="dot"></span>Live'; $('#live').style.color='';
    if(status.frame!==shownV) frame(status.frame);
  }catch(e){$('#live').innerHTML='<span class="dot"></span>Offline';$('#live').style.color='var(--danger)'}
}
// live LCD copy, drawn on the home screen and the Controller page
const lcds=[$('#screen'),$('#screenHome')].map(c=>c.getContext('2d')),img=lcds[0].createImageData(240,280);
let fBusy=false,fAgain=false,shownV=-1,latestV=-1;
async function frame(v){
  if(v!==undefined&&v>latestV)latestV=v;
  if(v!==undefined&&v===shownV)return;
  if(fBusy){fAgain=true;return} fBusy=true;
  const want=latestV;
  const ctl=new AbortController(),t=setTimeout(()=>ctl.abort(),5000);
  try{const px=new Uint16Array(await (await fetch('/api/screen',{cache:'no-store',signal:ctl.signal})).arrayBuffer()),d=img.data;
    if(px.length===240*280){for(let i=0;i<px.length;i++){const q=px[i],r=q>>11,g=(q>>5)&63,b=q&31,o=i*4;d[o]=(r<<3)|(r>>2);d[o+1]=(g<<2)|(g>>4);d[o+2]=(b<<3)|(b>>2);d[o+3]=255}
      lcds.forEach(c=>c.putImageData(img,0,0));shownV=want}
  }catch(e){}finally{clearTimeout(t);fBusy=false}
  if(fAgain){fAgain=false;frame()}
}

// ---- spotify sheet ------------------------------------------------------------------
let editClient=false,spKey='';
function ago(s){return s<0?'':s<90?'just now':s<5400?Math.round(s/60)+' min ago':Math.round(s/3600)+' h ago'}
function spSummary(){return !sp.clientId?'Not set up':sp.busy?'Signing in…':!sp.connected?'Not connected':`Connected · ${sp.items} playlists and albums`}
function renderSp(){
  const b=$('#spBody'),err=sp.error?`<p class="warnbox">${esc(sp.error)}</p>`:'';
  if(!sp.clientId||editClient){
    b.innerHTML=`<p class="gf" style="margin:8px 4px 0">Optional. Lets you choose from your Spotify playlists and saved albums. Playback still goes through Sonos.</p>
      <div class="gh">Set up (once)</div><div class="group"><ol class="steps">
        <li>Open the <a href="https://developer.spotify.com/dashboard" target="_blank" rel="noopener" style="color:var(--accent)">Spotify developer dashboard</a> and create an app (any name, tick <b>Web API</b>).</li>
        <li>Add this redirect URI: <code>${esc(sp.redirectUri)}</code></li>
        <li>Paste the app's Client ID below.</li></ol>
        <button class="row act" id="spCopy">Copy redirect URI</button></div>
      <div class="gh">Client ID</div><div class="group"><label class="row"><input class="txt" id="spClient" style="text-align:left" maxlength="32" placeholder="32 characters" autocomplete="off" autocapitalize="off" spellcheck="false" value="${esc(sp.clientId||'')}"></label></div>
      <div style="margin-top:20px;display:flex;flex-direction:column;gap:12px"><button class="btn" id="spSave">Save</button>${sp.clientId?'<button class="btn plain" id="spCancelEdit">Cancel</button>':''}</div>`;
  }else if(sp.busy){b.innerHTML='<p class="gf" style="text-align:center;margin-top:40px">Signing in and loading your lists…</p>';
  }else if(!sp.connected){
    b.innerHTML=`${err}<div class="gh">Step 1</div><div class="group"><p class="gf" style="margin:12px 16px">Sign in to Spotify in a new tab. After you approve, that tab goes to a page that <b>won't load</b>. That's expected.</p><button class="row act" id="spConnect">Sign in to Spotify</button></div>
      <div class="gh">Step 2</div><div class="group"><p class="gf" style="margin:12px 16px 4px">Copy the whole address from that tab and paste it here.</p><label class="row"><input class="txt" id="spUrl" style="text-align:left" placeholder="http://127.0.0.1:8888/callback?code=…" autocomplete="off" autocapitalize="off" spellcheck="false"></label></div>
      <div style="margin-top:20px;display:flex;flex-direction:column;gap:12px"><button class="btn" id="spFinish">Finish sign-in</button><button class="btn plain" id="spChange">Change client ID</button></div>`;
  }else{
    b.innerHTML=`${err}<div class="group" style="margin-top:8px"><div class="row"><span class="l">Account</span><span class="v">${esc(sp.user||'Connected')}</span></div>
      <div class="row"><span class="l">Playlists and albums</span><span class="v">${sp.items}</span></div>${sp.updatedSecondsAgo>=0?`<div class="row"><span class="l">Updated</span><span class="v">${ago(sp.updatedSecondsAgo)}</span></div>`:''}</div>
      <p class="gf">Spotify's own lists, like New Music Friday or Discover Weekly, appear once you follow them in Spotify.</p>
      <div class="group" style="margin-top:20px"><button class="row act" id="spRefresh">Refresh lists</button><button class="row danger" id="spDisc">Disconnect</button></div>`;
  }
}
$('#spBody').onclick=async e=>{const id=e.target.closest('button')?.id;if(!id)return;
  if(id==='spCopy'){try{await navigator.clipboard.writeText(sp.redirectUri);toast('Copied')}catch(_){toast(sp.redirectUri)}}
  if(id==='spSave'){const r=await post('/api/services/spotify/client',{clientId:$('#spClient').value.trim()});if(!r.ok)return toast(r.error);editClient=false;services(true)}
  if(id==='spCancelEdit'){editClient=false;renderSp()}
  if(id==='spChange'){editClient=true;renderSp()}
  if(id==='spConnect'){const w=window.open('about:blank','_blank');const r=await post('/api/services/spotify/connect');if(!r.ok){w&&w.close();return toast(r.error)}if(w)w.location=r.url;else location.href=r.url}
  if(id==='spFinish'){const r=await post('/api/services/spotify/callback',{url:$('#spUrl').value.trim()});if(!r.ok)return toast(r.error);toast('Signing in…');setTimeout(()=>services(true),800)}
  if(id==='spRefresh'){act('sync');toast('Refreshing…')}
  if(id==='spDisc'){if(!confirm('Disconnect Spotify from Spotty?'))return;await post('/api/services/spotify/disconnect');favs=null;services(true)}
};
$('#spRow').onclick=()=>{renderSp();openSheet('spotify')};
$('#spDone').onclick=closeSheet;
let spItems=-1;
async function services(force){
  try{sp=(await j('/api/services')).spotify;$('#spSum').textContent=spSummary();
    const k=JSON.stringify({...sp,updatedSecondsAgo:Math.floor(sp.updatedSecondsAgo/60)});
    if((force||k!==spKey)&&$('#spotify').classList.contains('open')&&!document.activeElement?.matches('#spBody input')) renderSp();
    spKey=k; if(sp.items!==spItems){if(spItems>=0){favs=null;loadPresets()}spItems=sp.items}
  }catch(e){}
}

// ---- data & live updates ---------------------------------------------------------------
async function loadPresets(){try{presets=await j('/api/presets');renderTiles()}catch(e){console.warn('presets load failed',e);setTimeout(loadPresets,2000)}}
async function loadAll(){try{rooms=await j('/api/rooms')}catch(e){} await loadPresets()}
async function pollNow(){try{np=await j('/api/nowplaying');renderNow()}catch(e){np={};renderNow();$('#sub').textContent='Controller offline'}}
let es;
function connectEvents(){
  es=new EventSource('/events');
  es.addEventListener('np',e=>{try{np=JSON.parse(e.data);renderNow()}catch(_){}});
  es.addEventListener('frame',e=>{try{frame(JSON.parse(e.data).v)}catch(_){frame()}});
  es.onopen=()=>{pollNow();frame()};
  es.onerror=()=>{if(es.readyState===EventSource.CLOSED)setTimeout(connectEvents,3000)};
}
loadAll().then(()=>{pollNow();services()});connectEvents();
setInterval(()=>{pollNow();services();if(ctrlOpen)refreshCtrl();else j('/api/state').then(st=>{if(st.frame!==shownV)frame(st.frame)}).catch(()=>{})},5000);
document.addEventListener('visibilitychange',()=>{if(!document.hidden){pollNow();loadPresets()}});
</script></body></html>)HTML";
