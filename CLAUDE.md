# OpenCircle: Co-Op Bus Racing Simulator

UE 5.8 (engine at `D:\UE_5.8`). Hybrid C++/Blueprint. Duos (Driver + Conductor) share one bus and race A→B through a city, earning fares and keeping passengers happy.

## Rules
- Work the **current phase only**. No procedural city, traffic AI or progression until driving and co-op are fun.
- Unreal naming (`U/A/F/E/I`, `b` bools), components/subsystems over monoliths, tunables as `UPROPERTY(EditAnywhere)` or DataAssets, no hardcoded values.
- Gameplay logic server-authoritative from day one (Phase 2 adds multiplayer).
- Scoring weights live in config/DataAsset, not code.
- Keep replies and output short.

## Roadmap (current: Phase 1)
0. Pre-prod: engine, networking (listen server first), EOS/Steam, design doc.
1. **Bus**: Chaos Vehicles bus (weight, wide turns, long braking, gears, doors, indicators, horn); walkable interior; cameras (cockpit, chase, conductor, mirrors/map); greybox town (2–3 intersections, 4–6 stops).
2. Multiplayer: replicated bus, players move in bus local space, lobby (Driver/Conductor), voice, 2–4 buses.
3. Conductor + NPC passengers: board/pay/exit, ticketing, mood meter, 3–4 data-driven events, shared HUD.
4. Seeded procedural roads/city with 3 route types (fast, busy, risky).
5. Traffic AI + pedestrians (server sim, LOD far away).
6. Race loop: pre-race screen, timer, scoring, penalties, results.
7. Bus-to-bus walkie-talkie and phone; mute/block/report.
8. Progression, polish, release.

## Layout
- `Source/OpenCircle/{Core,Player,Components,UI}`: C++. `Variant_*` = unused template code.
- `Content/`: assets (empty start; FluidNinjaLive is a plugin pack).

## Editor automation
Editor open + Remote Execution enabled, then:
`python Tools/ue.py "unreal.log('hi')"` or `python Tools/ue.py -f script.py`
