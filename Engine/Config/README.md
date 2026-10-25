# Engine/Config — the tracked boot inis

| Path | What it is | Read by |
|------|------------|---------|
| `Default.ini`, `DefUser.ini` | The native set: the boot config of the SDL2 desktop and Android builds. | Copied into a game's `System/` folder for a native run; `appInit` derives `UT2004.ini` / `User.ini` from them when those are absent. |
| `web/Default.ini`, `web/DefUser.ini` | The web set: the native set plus the browser's settings. | The Emscripten link embeds them as `/System/Default.ini` and `/System/DefUser.ini` (`Source/SDLLaunch/CMakeLists.txt`); `appInit` derives `UT2004.ini` / `User.ini` from them. |

Each file is complete (the link embeds it as it is), so the values the two sets share are written in both;
`diff Default.ini web/Default.ini` shows the web's delta. A game install's own `System/*.ini` is a Windows config
(D3DDrv, WinDrv) without the sections below; the web build never reads it, and the frontend's asset manifest
leaves out the derived `UT2004.ini` / `User.ini` so an install copy cannot shadow them.

## Sections of this port

| Section | What it holds |
|---------|---------------|
| `[Engine.Engine]` | `ViewportManager=SDL2Drv.SDL2Client`; `RenderDevice` is `OpenGLDrv.OpenGLRenderDevice` in the native set (desktop OpenGL on Linux and Windows; Android has no OpenGLDrv, so startup replaces it with GL4ESDrv) and `GL4ESDrv.GL4ESRenderDevice` in the web set. The launcher's `-*RENDERER` switches pick another and persist it. |
| `[SDL2Drv.SDL2Client]` | The client settings (window sizes, brightness, detail levels), `UseJoystick` (game pads, on by default), `PadCursorSeconds` (the seconds a pad's left stick at full tilt takes the menu cursor across the viewport's width, 3.0), `PadWalkTilt` (the move stick tilted out of its deadzone but less than this walks, 0.5; 0 never); web: `WebGLVersion` (1 or 2). |
| `[GL4ESDrv.GL4ESRenderDevice]` | Every GL4ESDrv option, listed in full. `NumAASamples` is the MSAA sample count (0 = off): native 0, web 4. Web: `UseVBO=True` and `DefaultTexMipBias`. |
| `[OpenGLDrv.OpenGLRenderDevice]` | The desktop GL renderer's options, matching GL4ESDrv's where shared: `NumAASamples=0`, `UseRenderTargets=True`, `LevelOfAnisotropy=16.0`. |
| `[PixoDrv.PixoRenderDevice]` | The Pixomatic software renderer's options. |
| `[ALAudio.ALAudioSubsystem]` | OpenAL Soft: `UseEAX=True` (its effects), `UseHRTF=False` (headphone virtualisation, the player's choice). |
| `[WebRTCDrv.WebRTCNetDriver]` | The browser-to-browser net driver: downloads allowed, 1000000 rates, 120 Hz tick, 256 MB download cap. |
| `[ProxyNetDrv.ProxyNetDriver]` (web) | The proxy net driver: `[IpDrv.TcpNetDriver]`'s keys, with `IpDrv.HTTPDownload` then `Engine.ChannelDownload` as download managers. |
| `[ufront.AutoFOV]` | Aspect-correct field of view (`AutoFOV` 0 off, 1 Hor+, 2 adaptive) and its reference aspect; the ini comments describe each key. |
| `[ufront.Shadows]` | Pawn-shadow `CullDistance` (native 1200, the retail value; web 4800) and `FadeExponent` (2.0). |

## Departures from the retail values

- `[Core.System] Paths` adds `../QuickScreenshots/*.utx`. The map lists draw a map's preview from its quick package
  only when a `Paths=` entry lists that folder; with an ini that has none (a retail one; Android reads the game
  folder's own inis) they load it from the map, as retail does.
- `[IpDrv.TcpNetDriver]` `NetServerMaxTickRate` / `LanServerMaxTickRate` are 120 (retail 20 / 35).
- `[IpDrv.MasterServerLink] MasterServerList` names the community master servers; retail's
  `ut2004master1|2.epicgames.com` do not resolve. Every entry must be live: `GetMasterServer` picks one at random per
  query. `UplinkToGamespy=True` stays: the same switch spawns the GameSpy query responder that answers a master's NAT
  heartbeat and third-party trackers. The web cannot query a master (no sockets); its browser lists servers through
  WebRTC discovery or the game proxy.
- `[Engine.LevelInfo]`: `PhysicsDetailLevel=PDL_High`, `MeshLODDetailLevel=MDL_Ultra`, `DecalStayScale=2.0`,
  `MaxClientFrameRate=120`. `[Engine.GameInfo] bWeaponShouldViewShake=False`. `ReduceMouseLag=False` for the GL and
  D3D devices.
- `DefUser.ini`: `[Engine.PlayerInput] MouseSmoothingMode=0` (the mouse turns the view 1:1; the script's default
  1 repeats and rescales movements by the frame time); `ConfiguredInternetSpeed` / `ConfiguredLanSpeed` 1000000;
  `[XInterface.GUIController] MaxSimultaneousPings=35` (without it the server browser pings nothing at that
  netspeed); pad bindings
  `Joy5`–`Joy16` for an SDL game controller; `[UnrealGame.UnrealPawn]` real player shadows; `[XGame.xPlayer]
  InputClass=Engine.PlayerInput`; `[GUI2K4.UT2K4ServerLoading] Backgrounds` lists loading screens 1 to 4 (retail
  lists 2 twice and never 3, which `2K4Menus.utx` has). Web: `[GUI2K4.UT2k4ServerBrowser] bHideNetworkMessage=True`
  (no first-run network-options prompt).
- A `DefUser.ini` change reaches only a new `User.ini`: `appInit` derives it only when it is absent.
