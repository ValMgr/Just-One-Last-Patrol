______________________________________________

# 🤔 What is this?

### :memo: Introduction

Just One Last Patrol is a cooperative Military Simulation (MilSim) mission that aims to recreate a post war environment based on an insurgency gameplay.  
Soviets forces retreated from Kunar and NATO deployed units and vehicles to help the local population.  
A new formed group, is against the NATO intervention and is ready to fight.

### :white_check_mark: Goal

Your main task is to defeat the insurgency group and restore peace and order in Kunar.  
The militia has an unknown number of hideouts in the island that need to be destroyed.  
They have also ammo caches in various locations, destroy them to weaken its power.

### :revolving_hearts: Actions

Bad actions cause bad effects.  
Infact there's a reputation system: helping the local population, fighting the insurgency, disarming IED will rise your reputation; killing civilians, mutilate alive/dead civilian, firing near civilians for no reason, damaging/destroying buildings, breaking locked door, losing vehicles, respawns will decrease your reputation.  
At the beginning you have a very low reputation level, so civilians won't help you revealing important information about milician, they will likely lie instead.

# :sparkles: Features:

- **Persistent campaign on a dedicated server**
  - The world keeps running when players leave and come back
  - Autosave, a save when the session ends, and a manual save for admins
- Dynamic battlefield
  - Villages, towns, and cities are taken from the map
  - Milician appear only while players are nearby, then leave to save performance
  - Sentries, patrols, road checkpoints, rooftop snipers, and crewed vehicles
  - Hideouts are chosen at the start, refill their men, and send reinforcements
  - An attacked settlement can call a neighbour for help
  - At low reputation, an observation post can mortar the fight
  - Civilians fill the settlements and drive between them
  - Some stay out of it, some pick up a rifle, some wear a bomb vest
- Reputation, one score per settlement
  - Killing milician and defusing a charge raise it
  - Hurting civilians, dying in the settlement, and a standing garrison lower it
  - At low reputation, civilians lie about hideouts and inbound squads
  - At high reputation, they point at a real hideout or a squad on the road
  - Talk to them, call them over, or send them indoors
- IEDs
  - Charges are planted around villages, towns, cities, hideouts, and observation posts
  - Rubbish piles sit nearby so the real ones are harder to spot
  - Hostile places can gain another charge in the morning
- Logistics
  - Build from the command post and pay with supplies
  - Headquarters, supply caches, maintenance, bunkers, and fortifications
  - Supply production refills the stock on a timer
  - A finished headquarters clears milician from nearby settlements and shows on the map
  - Deploy another FOB from the command truck
  - Request a vehicle from the depot and pay supplies
  - Save a kit at the arsenal and draw it again for the supply difference
- Admin
  - Game Master for server admins
  - Manual save, and skip time on the in-game clock
- Under the hood
  - Garrison, civilians, IEDs, vehicles, and the build list live in config files
  - Hideout count is a mission setting
  - Saves keep towns, reputation, hideouts, IEDs, FOBs, built compositions, supplies, depot vehicles, and named loadouts