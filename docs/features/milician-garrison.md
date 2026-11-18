# Milician Location Garrison (v2)

Proximity spawn / despawn of milician packages from stored garrisons on towns, OPs, and hideouts.

## Location types

| Type | How authored | When players near | Special |
|------|--------------|-------------------|---------|
| **TOWN** | Workbench town generator / map seed | ~75% of garrison as Sentry / Patrol / Checkpoint / Reinforcement | Suppressed if inside a player FOB |
| **OP** | Hand-placed `JOLP_LocationController` | Same planner as towns | Suppressed if inside a player FOB |
| **HIDEOUT** | **Auto-selected at mission start** from village-sized settlements | Defense only (sentries) | Regenerates men, then every **30 min** reinforces weakest nearby settlement; HQ prop must be destroyed |
| **FOB** | Hand-placed / spawned `JOLP_FOBController` | — | Disables milician on settlements whose centre is in its radius |

## Generate towns

### Recommended: runtime auto-seed (works today)

Kunar is a **SubScene** of the base map. World Editor often cannot see parent-map town markers, so the generator tool may fail even with the world open.

1. Leave `AI_SpawnArea` empty (or only hand-placed OPs). **Any authored TOWN skips auto-seed entirely** — delete leftover `Town_0` stubs before Play if you want map-descriptor seed.
2. **Host / Play** the mission.
3. Watch the log for `[JOLP] Map-descriptor seed: scanned=N seeded=M` and `[JOLP] Seeded town 'Asadabad'…` (city names, not `Town_N`).
4. Walk toward a seeded town to test milician spawn.

Hand-placed `JOLP_LocationController` towns still win: if any authored TOWN exists, auto-seed is skipped. The Workbench generator names entities from the map display name (`Asadabad`, not `Town_0`).

### Optional: World Editor tool

1. Open `JustOneLastPatrol_kunar.ent` (not Prefab Edit).
2. Select **AI_SpawnArea**.
3. **Plugins → JOLP Generate Town Locations** (`Ctrl+Shift+G`).
4. If log says `GetWorld()=0` / `candidates=0`, ignore the tool and use runtime auto-seed above.

## Auto hideouts

On `PostGameStart`, the location manager:

1. Reads mission param **`m_iHideoutCount`** (default **5**, `Missions/JOLP.conf` / mission header slider).
2. Collects locations with type **TOWN** and settlement size **VILLAGE** (not FOB-suppressed).
3. Shuffles and promotes that many to **HIDEOUT**.
4. Spawns HQ prefab [`JOLP_HideoutHQ.et`](../../Prefabs/Props/Military/JOLP_HideoutHQ.et) (FIA equipment crate placeholder) at the location.
5. Sets garrison from milician config (`m_iHideoutInitialGarrison` / max / regen).

Each reinforce tick (default 30 min): hideout **regenerates** `m_iHideoutRegenPerTick` men (up to max), then may send a Reinforcement package to the weakest eligible nearby settlement.

Destroying the HQ crate marks the hideout dead: local packs despawn, reinforce/regen stop, `GetAliveHideoutCount()` drops.

Log lines: `[JOLP] Promoted 'Hideout_Asadabad'…`, `[JOLP] Hideout selection: promoted N / wanted 5`.

### Debug mode

`JOLP_Debug` / `JOLP_Global.IsDebug()` is **ON** automatically unless the session is a **dedicated server** (`RplMode.Dedicated` / console app).

When debug is on, each generated hideout gets a **map marker** labelled with its name (removed when the HQ is destroyed).

## Player FOBs

Suppression is driven by `JOLP_FOBControllerComponent` (standalone marker or HQ composition).

- Starter: Thunderbolt HQ in `FOB.layer` (`JOLP_Headquarters_Thunderbolt.et`, 300 m).
- Player-built: complete a `JOLP_Headquarters_USMC` composition — unfinished outlines do **not** suppress.
- Standalone marker: [`Prefabs/Controllers/JOLP_FOBController.et`](../../Prefabs/Controllers/JOLP_FOBController.et).

Any **TOWN/OP** whose centre is inside the FOB radius is marked `SUPPRESSED_BY_FOB` (no milician spawn / reinforce target; live packs despawn on the next tick). Removing the FOB clears suppression.

See also: [`fob-building.md`](fob-building.md).

Hideouts are **not** suppressed by FOBs.

Defaults from descriptors:

| Descriptor | Radius | Garrison |
|------------|--------|----------|
| Village / Settlement / Local / Generic | 250 | 20 |
| Town | 400 | 20 |
| City | 600 | 40 |

Active garrison ratio default: **0.65**.

Kunar often labels hamlets as `MDT_NAME_LOCAL` rather than `MDT_NAME_VILLAGE` — both are seeded.

## Editor-tunable params

### Mission header (`Missions/JOLP.conf`)
- `m_iCivilianPopulationPercent`
- `m_iHideoutCount` (default 5)

### Location controller (instance / prefab)
Name, type (TOWN/OP/HIDEOUT), settlement size, activation radius, initial garrison (default 20), active ratio (default 0.65), max-active override, role allow toggles, hideout reinforce interval / group size / search range / defense reserve.

### Milician manager (`Manager.layer` → `JOLP_MilicianManager`)
Select the entity near Faction/Loadout managers. Edit `m_Config`:

- **Group Prefab**: sentry, patrol, checkpoint, reinforcement, and reinforcement vehicle lists. One entry is picked at random per spawn
- Faction key (default `USSR`, the OPFOR side, so milician stay distinct from FIA partisans)
- Checkpoint **composition** prefab (fortifications + static weapon)
- Waypoint prefabs (Defend / Patrol / Move / Cycle)
- Tick, despawn hysteresis, patrol max duration, reinforce arrive absorb delay (120 s)
- **Hideout reinforce interval** (default 1 800 000 ms = 30 min)
- Hideout initial / max garrison, regen per tick, **HQ prefab**

Config defaults: [`Configs/Milician/MilicianGarrison.conf`](../../Configs/Milician/MilicianGarrison.conf).

## Package behaviours

- **Sentry** — Defend or tight cycle patrol (50/50); despawns with location.
- **Patrol** — Wide cycle (~1.2× radius); after max duration, survivors return to stored (location stays active).
- **Checkpoint** — Snap to nearest road, spawn composition + Defend infantry; both removed on location despawn.
- **Reinforcement** — Vehicle leaves from a clear stretch of road, crew rides, and the move order ends on a road inside the target. No clear road means the squad walks. Exempt from source despawn; after arrival + 2 min, living count added to **target** stored.

Hideout timer regenerates men, then deducts from hideout stored (keeps `m_iHideoutDefenseReserve`, default 4).

## Win-condition hook

```
JOLP_Global.GetLocations().GetHideouts();
JOLP_Global.GetLocations().GetAliveHideoutCount(); // HQ not destroyed
```

Destroying all hideout HQs is the intended campaign end-state hook (UI / victory not wired yet).

## Verification checklist

1. Host mission → console lists towns and `Hideout selection: promoted N`.
2. Find a `Hideout_*` ammo crate in a village-sized settlement.
3. Set hideout interval to ~60 000 ms for testing → regen log + wave to weakest town.
4. Destroy the HQ crate → `Hideout HQ destroyed` → no more spawn/reinforce from that site; alive count drops.
5. Place `JOLP_FOBController` on a normal town → `SUPPRESSED_BY_FOB` → no milician spawn there.
6. `GetAliveHideoutCount()` matches undestroyed hideouts.
