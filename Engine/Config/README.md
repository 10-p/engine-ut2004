# Engine/Config — tracked config sets

Two sets, split by platform (step 2.15d):

| Path | What it is | Consumed by |
|------|------------|-------------|
| `Default.ini`, `DefUser.ini` | **Platform-agnostic / native reference.** The SDL2 desktop build's boot config. | Reference; native builds read config from the game `System/` they run in. |
| `web/Default.ini`, `web/DefUser.ini` | **Web platform defaults.** Carry the web-only settings: the `[WebRTCDrv.WebRTCNetDriver]` section (`AllowDownloads=True`, 120 tick, 1000000 rate, `DownloadManagers=Engine.ChannelDownload`) and the client netspeed (`ConfiguredInternetSpeed=1000000`). | **Build-consumed:** the Emscripten link `--preload-file`s these (see `Source/SDLLaunch/CMakeLists.txt`). |

`appInit` derives `UT2004.ini` / `User.ini` from whichever pair is preloaded, so **the web set is the
single source of truth for the web boot config** — and the ufront manifest excludes the derived
`System/UT2004.ini` + `System/User.ini` so a stale copy in the asset image can never shadow them.

## Why this exists

The web config used to be sourced from the out-of-tree asset dir (`UT_ASSETS_DIR/System/*.ini`), i.e. it
lived only in the untracked game install. Rebuilding the asset image silently reverted it and broke every
WebRTC download: a game install ships a **native** config (D3DDrv/WinDrv, desktop resolutions) with no
`[WebRTCDrv.WebRTCNetDriver]` section at all, and `AllowDownloads` is a **per-class** config property — with
no section it is never set, so it stays false and the client refuses every transfer
(`Downloading 'x' not allowed`). Keeping the web set in git makes that failure mode impossible.

Platform differences that are *behavioural* rather than config (render device / viewport swaps) are still
applied in engine code via the launcher's `#ifdef __EMSCRIPTEN__` overrides; this folder only carries data.

**When the web config changes, edit `web/` — not the asset tree.** Each file must be complete (that is what
`--preload-file` ships), so shared values are duplicated between the two sets by design;
`diff Default.ini web/Default.ini` shows the platform delta.

## Deliberate departures from the retail files

- **`[GUI2K4.UT2K4ServerLoading] Backgrounds` (ufront 2.60)** — retail `DefUser.ini` lists
  `loadingscreen1, 2, 2, 4`: an Epic typo that made `loadingscreen3` unreachable (it IS exported by
  `2K4Menus.utx`, checked against the package's export table) and doubled the odds of `2`. Both sets now
  list `1, 2, 3, 4`. Do not "restore" it from a retail install. ⚠ A player whose `User.ini` already exists
  keeps the old list — `User.ini` is derived from `DefUser.ini` only when it is absent.
- **`[IpDrv.MasterServerLink] MasterServerList` (ufront 2.79n)** — retail names
  `ut2004master1|2.epicgames.com`, which no longer resolve. Both sets now list the six community masters
  that answered a real list query on 2026-09-21 (the engine's own master protocol, TCP 28902):
  `ut2004master.333networks.com` 560 servers, `ut2004master.errorist.eu` 552, `ut2004master.gonespy.com`
  542, `master.frag-net.com` 548, `master.noccer.de` 502, `utmaster.openspy.net` 599. Every entry must be
  live: `MasterServerLink.GetMasterServer` picks a RANDOM one per query. OldUnreal 3374's `Group=` field is
  omitted — our `tMasterServerEntry` has `Address` and `Port` only. The web cannot use these (a master
  query is TCP, and the browser has no sockets; its LAN tab discovers hosts over WebRTC instead); they reach
  NATIVE builds, and only new installs (a native install reads its own `System/UT2004.ini`). `UplinkToGamespy=True` is deliberately KEPT, although
  its heartbeat target is hardcoded to the dead `master0.gamespy.com`: the same switch spawns the GameSpy
  query responder (+10), which the native uplink also uses to answer a master's NAT heartbeat request
  (`MasterServerUplink.cpp`, `HB_GamespyQueryPort`) and which third-party trackers query. OldUnreal 3374
  keeps it on too.
- **`[XInterface.GUIController] MaxSimultaneousPings=35` (ufront 2.79)** — retail leaves it unset (0), which is
  harmless at retail's `ConfiguredInternetSpeed=9636`; at OUR netspeed (1000000, both sets) the UT2K4 server
  browser's `CalculateMaxConnections()` answers `Min(MaxSimultaneousPings, 35)` = 0 above 20000, so it pinged
  NOTHING: every Ping read "?", and the master's cut / '?' names (ufront 2.79o) were never replaced by each
  server's own reply. 35 is the value retail's own speed tiers give the fastest line. Measured natively
  (server + client in one network-less namespace, the server a favourite): unset → "?"; 35 → the server's
  own row with a ping. The property is `globalconfig`, so it belongs to `XInterface.GUIController`'s section,
  not the UT2K4 subclass's. On the web the in-game browser's pings are answered from the game proxy's list.
  ⚠ Like every `DefUser.ini` change it reaches only a NEW `User.ini`: an existing one keeps its 0.
