# FOB Logistics & Building

Vanilla Conflict-style free-roam building for Just One Last Patrol.

## Player flow

1. **FOB Thunderbolt** starts with a placed HQ (`JOLP_Headquarters_Thunderbolt.et`).
2. At the command-post **Building Service** table, use **Show build preview** to open BUILDING mode.
3. Spend **supplies** from the HQ `Storage_Supplies` pile to place catalog compositions.
4. Additional FOBs are created only when a **completed** player-built `JOLP_Headquarters_USMC` finishes. Unfinished outlines do **not** suppress milician towns.

## Content browser (not missing)

`SCR_ContentBrowserEditorComponent` already lives on vanilla `EditorModeBuilding.et`. Tabs are filtered by the active provider’s `m_aAvailableTraits`:

| Provider trait | Content-browser tab |
|----------------|---------------------|
| `TRAIT_SERVICE` | Support / services |
| `TRAIT_FORTIFICATION` | Fortifications |
| `THEME_MILITARY` | Single objects |
| `SERVICE_HQ` | Headquarters |

Thunderbolt / player HQs use `JOLP_BuildingService_US` (master provider) with those four traits. Catalog labels must include the matching trait or the card stays empty.

## Wiring

| Piece | Role |
|-------|------|
| `GameMode_JOLP` → `SCR_CampaignBuildingManagerComponent` | Building mode; catalog = JOLP registry |
| `JOLP_PlayerController` | `SCR_CampaignBuildingNetworkComponent` |
| `JOLP_Tent_CommandPost_US_01` → `JOLP_BuildingService_US` | Master provider + Show build preview |
| `JOLP_Headquarters_*` | US HQ + supply seed (+ FOB controller on buildable) |
| `JOLP_SupplyIncomeComponent` | One-shot seed / 15-minute grant into SUPPLIES |
| `JOLP_FOBControllerComponent` | Suppression radius + faction map marker |

## Supplies

- Currency is vanilla `EResourceType.SUPPLIES`.
- Thunderbolt seeds **1000** into HQ `Storage_Supplies` (world override + `DoStartGame` / `JOLP_SupplyIncomeComponent`).
- Seed prefers the BuildingService generator (encapsulator → child crate meshes), then the Storage_Supplies virtual container via `SetResourceValue`.
- Supply Production grants **150** every **15** in-game minutes.
- Costs / refunds use CAMPAIGN budget (100% refund default).

## Build catalog

Registry → [`Configs/Building/JOLP_Compositions_FreeRoamBuilding.conf`](../../Configs/Building/JOLP_Compositions_FreeRoamBuilding.conf).

| Buildable | Source |
|-----------|--------|
| **Headquarters** | Vanilla `E_Headquarters_S_Conflict_US_01` |
| **Supply cache** | Vanilla `E_SupplyCache_S_US_01` |
| **Ammo / vehicle maint / bunker / MG / sandbag position** | Vanilla Conflict / US FreeRoam editables |
| **Sandbags / camo / dragonteeth** | Vanilla FreeRoamBuilding |

Catalog uses **vanilla `E_*` only** — custom `E_JOLP_*` wrappers previously collided GUID-wise with BuildingService/Tent and failed with “Error when creating entity from prefab”. ContentBrowser faction AND-filter is no-op’d (`JOLP_BuildingCatalogDebug`) so auto-label `FACTION_US` placeables still show. Dig layouts come from vanilla outline manager.

## Map marker

Armed FOBs place a server static `SCR_MapMarkerBase` (PLACED_CUSTOM) with the FOB name so the whole faction can see it on the map.

## Prefabs

- Catalog: `Configs/Building/JOLP_Compositions_FreeRoamBuilding.conf`
- Provider: `Prefabs/Props/Military/Compositions/US/JOLP_BuildingService_US.et`
- Tent: `Prefabs/Compositions/Misc/SubCompositions/Tents/JOLP_Tent_CommandPost_US_01.et`
- Editable wrappers: `PrefabsEditable/Auto/Compositions/.../E_JOLP_*.et` (services, forts, FreeRoam props)
- Starter: `.../JOLP_Headquarters_Thunderbolt.et`

## Out of scope (this pass)

- Persistence of built compositions across restart
- Multi-resource logistics (timber/steel/…)
- Conflict capture / XP / commander features tied to `SCR_GameModeCampaign`
