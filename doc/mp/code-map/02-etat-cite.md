# 02 — État global « de la cité » et faisabilité de N cités sur une carte

Base analysée : branche `multiplayer` = upstream `34d1ecd5` (tag `upstream-base`), lecture seule.
Conventions : `fichier:ligne` relatif à la racine du dépôt. Classes d'état :
**(a)** par cité/joueur, **(b)** monde/carte partagé, **(c)** UI/local, **[C]** César/campagne
(à neutraliser en MP), **[T]** tutoriel. Dans le tableau de `tick.c` : **P** = fonction par cité,
**M** = monde, **M+ctx** = boucle monde qui lit/écrit l'état de la cité du propriétaire de chaque entité.

## 0. Résumé

- Tout l'état « cité » central est une seule variable globale, `city_data` (`struct city_data_t`,
  `src/city/data_private.h:25-426`, définie `src/city/data_private.c:3`), 36 232 octets en mémoire
  (mesuré, arm64/clang), dont 24 464 octets de champs `unused` et 9 608 octets d'historique mensuel
  de population.
- `city_data.` n'apparaît **que** dans `src/city/` (23 fichiers, 2 111 lignes, dont 832 dans
  `data.c` = sauvegarde/chargement). Tout le reste du code passe par les ~410 fonctions publiques
  déclarées dans `src/city/*.h` (dont ~326 pour l'état de cité proprement dit).
- À côté de `city_data`, au moins 12 états « par cité » vivent dans des `static` dispersés
  (couvertures culturelles, compteurs de bâtiments, curseur d'allocation de l'eau, greniers,
  légions…), dont un `static` local non sauvegardé (`src/city/labor.c:312`).
- ≥102 boucles sur le tableau de bâtiments, 23 sur les figures, 29 sur les formations ; environ
  70 d'entre elles devront filtrer par propriétaire (+8 selon des choix de design).
- Le format de sauvegarde C3 réserve déjà **2 enregistrements de cité de 18 068 octets**
  (36 136 = 2 × 18 068) : le premier (`other_player`) est nul dans les 54 sauvegardes de test.
- Recommandation : **option A** (pointeur de « cité courante » basculé, `city_data` devenant une
  macro sur `city_datas[i]`) + identifiants explicites seulement pour les interactions entre cités.
- Pièges majeurs (indépendants de l'option) : code UI qui modifie l'état simulé (conseillers,
  rotation de la vue), *scratch* partagés hérités de C3 (grille de distances de routage, listes de
  bâtiments, `map_point` « last »), aléa constant pendant un tick, valeurs mises en cache avant
  les boucles.

## 1. La structure `city_data`

### 1.1 Déclaration et exposition

- `src/city/data_private.h:25` : `extern struct city_data_t { ... } city_data;` (le type et la variable sont
  déclarés dans le même énoncé) ; `src/city/data_private.c:3` : `struct city_data_t city_data;`.
- `data_private.h` est inclus **uniquement** par les 24 `.c` de `src/city/` (vérifié :
  `grep -rl "city/data_private.h\|\"data_private.h\"" src`). Les autres modules n'ont accès qu'aux
  accesseurs des `src/city/*.h`. API générale : `src/city/data.h` (`city_data_init`,
  `city_data_init_scenario`, `city_data_init_campaign_mission`, `city_data_save_state`,
  `city_data_load_state`).
- Neuf accesseurs renvoient un **pointeur dans `city_data`** : `city_houses_demands()`
  (`src/city/houses.c:33`, non-`const`), `city_finance_overview_last_year/this_year()`
  (`src/city/finance.c:403,408`), `city_emperor_get_gift()` (`src/city/emperor.c:189`),
  `city_labor_category()` (`src/city/labor.c:122`), `city_map_entry_point/exit_point/entry_flag/exit_flag()`
  (`src/city/map.c:6-24`). Aucun appelant ne les stocke dans un `static` (vérifié), mais
  `src/building/house_evolution.c:509` garde `demands` pendant toute la boucle sur les maisons.
- Initialisation : `city_data_init()` (`src/city/data.c:11`, `memset` + valeurs par défaut,
  appelée par `clear_scenario_data`, `src/game/file.c:98`), `city_data_init_scenario()`
  (`data.c:38`, trésor initial, `faction_id = 1`), `city_emperor_init_scenario(rank)` (`file.c:302`).

### 1.2 Sous-structures (tailles mesurées avec `offsetof`/`sizeof`, macOS arm64, clang)

| Sous-structure | Lignes `.h` | Octets | Contenu / remarque |
|---|---|---|---|
| `building` | 26-53 | 92 | sénat, caserne, centre de distribution, centre de commerce, hippodrome, arcs de triomphe, docks/quais actifs (`working_dock_ids[10]`), mission, rencontre indigène. Coordonnées en `uint8_t`/`int8_t`, offsets de grille en `int16_t` |
| `figure` | 54-62 | 28 | compteurs par tick : animaux, indigènes attaquants, ennemis, soldats impériaux, émeutiers, soldats, `security_breach_duration` |
| `houses` (`house_demands`) | 63 | 100 | maisons manquant de puits/fontaine/nourriture/temples… (conseillers) |
| `emperor` [C] | 64-87 | 92 | cadeaux, dette, rang, salaire, épargne personnelle, dons, invasion par César |
| `military` | 88-94 | 12 | totaux de légions/soldats, légions au service de l'empire, attaque indigène |
| `distant_battle` [C] | 95-107 | 11 | bataille lointaine |
| `finance` | 108-125 | 164 | trésor, taux d'impôt, `finance_overview` année courante/précédente, intérêts, salaire, vols, tribut [C] |
| `taxes` | 126-146 | 60 | contribuables plébéiens/patriciens, collecté/non collecté mensuel et annuel |
| `population` | 147-184 | 9 992 | population, `monthly.values[2400]` (9 600 o), `at_age[100]`, `at_level[20]`, naissances/décès, curseurs `last_used_house_add/remove`, `graph_order` (choix UI) |
| `labor` | 185-195 | 232 | salaires (cité/Rome), travailleurs, chômage, `categories[10]` (besoin, alloué, priorité) |
| `migration` | 196-210 | 52 | durées/lots/files d'immigration et d'émigration, cause de non-immigration |
| `sentiment` | 211-223 | 36 | humeur, causes, protestataires, criminels |
| `health` | 224-228 | 12 | santé, cible, travailleurs d'hôpital |
| `ratings` | 229-259 | 104 | culture/prospérité/paix/faveur [C pour faveur], points et explications |
| `culture` | 260-266 | 20 | moyennes divertissement/religion/éducation/santé par maison |
| `religion` | 267-275 | 104 | `god_status gods[5]`, malédictions/bénédictions actives |
| `entertainment` | 276-289 | 48 | spectacles par type de salle, course à l'hippodrome |
| `festival` | 290-309 | 52 | fête planifiée/choisie, coûts, effets |
| `resource` | 310-336 | 356 | stocks entrepôts/ateliers/greniers, `trade_status`, `export_over`, `stockpiled`, `mothballed`, nourriture, `last_used_warehouse` |
| `sound` (c) | 337-350 | 12 | compteurs anti-répétition des sons de combat |
| `trade` | 351-360 | 24 | nb de routes, problèmes terrestres/maritimes, rotation des ressources importées |
| `map` | 361-370 | 128 | points d'entrée/sortie, drapeaux, **top 10 des réseaux routiers (calculé sur toute la carte)** |
| `mission` [C/T] | 371-381 | 36 | victoire, gouvernance prolongée, drapeaux de messages et tutoriel |
| `unused` | 382-425 | 24 464 | `other_player[18068]`, `unknown_2c20[1400]` (5 600 o), `faction_id`, octets inconnus relus/réécrits tels quels |
| **Total** | | **36 232** | partie « utile » 11 768 o ; 4 cités ≈ 145 Ko : la mémoire n'est pas un sujet |

Couplage entre modules (sous-structures écrites/lues par fichier, `grep -o "city_data\.[a-z_]*"`) :
`ratings.c` touche 10 familles (ratings 165, finance 14, population 10, labor 10, emperor 9…),
`finance.c` 6 (finance 80, taxes 50, population 9, labor 5, emperor 4, religion 1), `emperor.c`
4, `gods.c` 4, `victory.c` 5, `migration.c` 3, `sentiment.c` 4 ; les autres touchent
essentiellement leur propre famille. Le découpage en modules ne correspond donc pas à un
découpage des données : un découpage par « sous-structure par cité » n'apporte rien, il faut
démultiplier la structure entière.

### 1.3 Accès directs : commande et chiffres

```sh
grep -rc "city_data\." src | grep -v ":0$" | sort -t: -k2 -nr   # lignes par fichier
grep -ro "city_data\." src | wc -l                               # occurrences : 2241
grep -rn "city_data\." src | wc -l                               # lignes : 2111
```

Lignes (occurrences) par fichier — tous dans `src/city/` : `data.c` 832 (834), `ratings.c` 193
(219), `finance.c` 127 (149), `emperor.c` 103 (111), `military.c` 90 (95), `resource.c` 80 (86),
`migration.c` 72 (77), `population.c` 71 (81), `labor.c` 71 (84), `buildings.c` 66 (68),
`houses.c` 54 (58), `festival.c` 51 (59), `gods.c` 49 (49), `entertainment.c` 43 (43),
`trade.c` 36 (36), `sentiment.c` 36 (43), `victory.c` 25 (26), `figures.c` 25 (26),
`health.c` 23 (31), `sound.c` 21 (21), `culture.c` 18 (18), `map.c` 16 (18), `mission.c` 9 (9).
Hors `data.c` : 1 279 lignes dans 22 fichiers. `message.c`, `view.c`, `warning.c` n'utilisent
pas `city_data` (état propre en `static`).

Appels aux accesseurs de cité hors de leur propre module (regex sur les noms déclarés dans
`src/city/*.h`, hors `view`, `message`, `warning`, `sound`, `data`) : **685 sites** — 66 entre
modules de `src/city`, 298 dans le reste de la simulation (`building` 118, `figuretype` 53,
`game` 50, `figure` 31, `scenario` 20, `empire` 14, `map` 12), 321 dans l'UI (`window` 272,
`widget` 39, `graphics` 7, `sound` 3). En ajoutant `view` (173), `message` (163), `warning` (50),
`sound` (14) et `data` (7), on obtient 1 092 sites.

## 2. Rôle des modules de `src/city/`

| Module | Rôle | Mise à jour (directe dans `tick.c` / indirecte) | Classe |
|---|---|---|---|
| `buildings` | Registre des bâtiments uniques et compteurs : sénat, caserne, centre de commerce, hippodrome (1 max), arcs, docks/quais actifs, mission | construction/destruction ; `building_count_update` (tick 33) remet docks/quais à zéro et fixe la caserne | a |
| `constants.h` | énumérations (`advisor_type`, `low_mood_cause`, `festival_size`, `god_type`…) | — | — |
| `culture` | couverture par type de service dans le `static coverage` (`culture.c:12`), moyennes par maison | tick 33 `city_culture_update_coverage`, tick 49 `city_culture_calculate` (appelle `city_entertainment_calculate_shows`, `city_festival_calculate_costs`) | a |
| `data`/`data_private` | définition, initialisation, sérialisation de `city_data` | chargement/sauvegarde | a |
| `emperor` | cadeaux, rang/salaire, épargne, dons, renflouement de dette, invasion par César | tick 4 `city_emperor_update` (`update_debt_state` + `process_caesar_invasion`) | C (la dette est aussi un flux de trésorerie) |
| `entertainment` | spectacles par salle, salles en manque, course de chars | via tick 49 | a |
| `festival` | planification, coût et exécution des fêtes | mois : `city_festival_update` ; coûts via tick 49 | a |
| `figures` | compteurs de figures de la cité, brèche de sécurité | remis à zéro par `figure_action_handle` (`src/figure/action.c:113`) puis incrémentés par les actions | a (M+ctx) |
| `finance` | trésor, impôts, salaires, intérêts, bilans, tribut | mois : `city_finance_handle_month_change` ; année : `city_finance_handle_year_change` (`pay_tribute`, `finance.c:333`) | a (+C) |
| `gods` | humeurs des 5 dieux, colère, bénédictions/malédictions (fermes, greniers, entrepôts, navires) | tick 1 `city_gods_calculate_moods(1)` ; année : `city_gods_reset_neptune_blessing` | a |
| `health` | santé, travailleurs d'hôpital, épidémies | mois : `city_health_update` | a |
| `houses` | demandes non satisfaites des maisons | via tick 39 (`city_houses_reset_demands`, `house_evolution.c:508`) | a |
| `labor` | catégories, priorités, allocation des travailleurs aux bâtiments (`b->num_workers`), salaires, chômage | tick 25 `city_labor_update` ; via tick 23 (`city_labor_calculate_workers`) | a |
| `map` | entrée/sortie et drapeaux ; top 10 des réseaux routiers | tick 7 via `map_road_network_update` (`src/map/road_network.c:76`) | a (entrée/sortie) / b (réseaux) |
| `message` | journal (1 000 messages), file de popups, délais/compteurs par catégorie, jalons de population, zones à problème | mois : `city_message_decrease_delays`, `city_message_sort_and_compact` | a + c |
| `migration` | taux et lots d'immigration/émigration selon l'humeur | via tick 23 (`house_population.c:187`) | a |
| `military` | totaux de légions, attaque indigène, bataille lointaine | via `formation_calculate_figures` (ticks 5/29) ; mois : `scenario_distant_battle_process` | a (+C) |
| `mission` | drapeaux tutoriel, « sauvegarde de départ » | — | C/T |
| `population` | population, recensement par âge/niveau, historique mensuel, capacité | ticks 22/23 ; mois : `city_population_record_monthly` ; année : `city_population_request_yearly_update` | a |
| `ratings` | notes et explications | mois : `city_ratings_update(0)` ; année : `city_ratings_update(1)` | a (faveur = C ; déjà figée à 50 en jeu libre, `ratings.c:499`) |
| `resource` | stocks, statut commercial par ressource, nourriture, consommation | ticks 16/17/18 ; mois : `city_resource_consume_food` | a |
| `sentiment` | humeur, protestataires, criminels | jour 0 et 8 : `city_sentiment_update` | a |
| `sound` | anti-répétition des sons de combat (dans `city_data.sound`) | appelé par `src/figure/sound.c` et les figures | c (mais sauvegardé) |
| `trade` | nb de routes, problèmes de commerce, rotation des imports | tick 32 `city_trade_update` (+ `empire_city_generate_trader`) | a |
| `victory` | victoire/défaite, gouvernance prolongée (`static data`, `victory.c:16`) | chaque tick `city_victory_check` (sort tout de suite en jeu libre, `victory.c:108`) ; mois : `city_victory_update_months_to_govern` | C/a (+UI) |
| `view` | caméra, orientation, viewport, conversions écran/grille | — | c (fuit dans la simulation, §3.2) |
| `warning` | avertissements textuels temporisés (`TIMEOUT_MS 15000`) | — | c |

### 2.1 Ordonnancement (`src/game/tick.c`)

| Où | Appels |
|---|---|
| `advance_year` (58-69) | `scenario_empire_process_expansion` M/C · `game_undo_disable` c · `game_time_advance_year` M · `city_population_request_yearly_update` P · `city_finance_handle_year_change` P · `empire_city_reset_yearly_trade_amounts` M (P si quotas par joueur) · `building_maintenance_update_fire_direction` M · `city_ratings_update(1)` P · `city_gods_reset_neptune_blessing` P |
| `advance_month` (71-104) | `city_migration_reset_newcomers` P · `city_health_update` P · `scenario_random_event_process` M→P (frappe « la » cité) · `city_finance_handle_month_change` P · `city_resource_consume_food` P · `scenario_distant_battle_process` C · `scenario_invasion_process` C/M · `scenario_request_process` C · `scenario_demand_change_process` M · `scenario_price_change_process` M · `city_victory_update_months_to_govern` C · `formation_update_monthly_morale_at_rest` M · `city_message_decrease_delays` P · `map_tiles_update_all_roads/water` M · `map_routing_update_land_citizen` M · `city_message_sort_and_compact` P · `city_ratings_update(0)` P · `city_population_record_monthly` P · `city_festival_update` P · `tutorial_on_month_tick` T · autosave c |
| `advance_day` (106-115) | `city_sentiment_update` P (jours 0 et 8) · `tutorial_on_day_tick` T |
| `advance_tick` (117-166) | 1 `city_gods_calculate_moods` P · 2 `sound_music_update` c · 3/30 `widget_minimap_invalidate` c · 4 `city_emperor_update` C/P · 5/29 `formation_update_all` M+ctx · 6 `map_natives_check_land` M · 7 `map_road_network_update` M (écrit `city_data.map`) · 8 `building_granaries_calculate_stocks` P · 10 `building_update_highest_id` M · 12 `house_service_decay_houses_covered` M · 16/17/18 `city_resource_calculate_*` P · 19 `building_dock_update_open_water_access` M · 20 `building_industry_update_production` M · 21 `building_maintenance_check_rome_access` P · 22 `house_population_update_room` P · 23 `house_population_update_migration` P · 24 `house_population_evict_overcrowded` M+ctx · 25 `city_labor_update` P · 27/28 `map_water_supply_*` M · 31 `building_figure_generate` M+ctx · 32 `city_trade_update` P · 33 `building_count_update` + `city_culture_update_coverage` P · 34 `building_government_distribute_treasury` P · 35 `house_service_decay_culture` M · 36 `house_service_calculate_culture_aggregates` M+ctx · 37/38 désirabilité M · 39 `building_house_process_evolve_and_consume_goods` M+ctx · 40 `building_update_state` M+ctx · 43 `building_maintenance_update_burning_ruins` M · 44 `building_maintenance_check_fire_collapse` M+ctx · 45 `figure_generate_criminals` P · 46 `building_industry_update_wheat_production` M · 48 `house_service_decay_tax_collector` M · 49 `city_culture_calculate` P |
| `game_tick_run` (168-183) | `random_generate_next` M · `game_undo_reduce_time_available` c · `figure_action_handle` M+ctx · `scenario_earthquake_process` M · `scenario_gladiator_revolt_process` P/M · `scenario_emperor_change_process` C · `city_victory_check` C/P |

Bilan : ~30 appels P (à exécuter pour chaque cité active), ~9 M+ctx (bascule de contexte par
entité), ~20 M, ~7 C, 2 T. L'ordre des ticks 1-49 est significatif (ex. tick 22 remplit la
liste `building_list_large`, ticks 23 et 24 la consomment) : pour N cités, les appels P d'un
même numéro de tick doivent être exécutés pour toutes les cités avant de passer au tick suivant,
dans un ordre fixe (index de cité croissant).

## 3. États globaux hors `city_data` et classement

### 3.1 Tableau de classement

| État | Emplacement | Classe | Sauvegardé (partie) | Remarque MP |
|---|---|---|---|---|
| `city_data` (hors `sound`, `unused`, `map.largest_road_networks`) | `src/city/data_private.h:25` | a | `city_data` | cœur de l'état cité |
| entrée/sortie + drapeaux | `data_private.h:362-365` | a | `city_data`, `city_entry_exit_*` | « route de Rome » : immigrants (`src/figuretype/migrant.c:17`), caravanes (`src/empire/city.c:281`), soldats et marchands repartent par la sortie (`src/figuretype/soldier.c:369`, `trader.c:319`) |
| `coverage` | `src/city/culture.c:12` | a | `culture_coverage` (60) | hôpital écrit 2 fois (`culture.c:192`) |
| `available` (listes de ressources) | `src/city/resource.c:14` | a (dérivé) | non | dépend de l'empire du joueur |
| `data.state/force_win` | `src/city/victory.c:16` | a | non | |
| `start_building_id` (`static` locale) | `src/city/labor.c:312` | a | **non** | curseur de l'allocation des porteurs d'eau, non restauré au chargement |
| journal de messages, compteurs/délais, jalons | `src/city/message.c:18` | a | `messages`, `message_extra`, `message_counts`, `message_delays`, `population_messages` | parties UI mêlées (§3.2) |
| `data.buildings[]`, `data.industry[]` | `src/building/count.c:15` | a | `building_count_*` (372) | recalcul complet au tick 33 |
| `non_getting_granaries` | `src/building/granary.c:20` | a (scratch) | non | `MAX_GRANARIES` 100 |
| `tower_sentry_request` | `src/building/barracks.c:17` | a | `building_barracks_tower_sentry` | |
| `id_last_legion`, `num_legions` | `src/figure/formation.c:18` | a | `formation_totals` (12) | `MAX_LEGIONS` 6 (`formation.h:9`) à rendre par joueur |
| `totals.legion_*` / `enemy_*` | `src/figure/enemy_army.c:10` | a / b | `enemy_army_totals` (20) | |
| `is_open`, `trader_figure_ids`, `trader_entry_delay` | `src/empire/city.c:18` (`cities[MAX_CITIES=41]`) | a si commerce par joueur | `empire_cities` (2706) | « city » y désigne une **cité de l'empire** |
| quotas `limit`/`traded` par route | `src/empire/trade_route.c:10` | a ou b | `trade_route_limit/traded` | choix de design |
| départ, nom du joueur, critères, fonds initiaux | `src/scenario/data.h:126-240` | a (à démultiplier) | `scenario`, `player_name` | `entry_point/exit_point` uniques par scénario |
| révolte des gladiateurs | `src/scenario/gladiator_revolt.c:9` | a/b | `gladiator_revolt` | événement interne à une cité |
| `city_data.emperor`, `distant_battle`, faveur, tribut, `mission` | `data_private.h` | C | `city_data` | à neutraliser par mode, pas à supprimer (tests) |
| demandes de César | `scenario.requests[]` (`data.h:171`), `src/scenario/request.c` | C | `scenario` | |
| changement d'empereur | `src/scenario/emperor_change.c:10` | C | `emperor_change_*` | |
| invasions | `src/scenario/invasion.c:86` | C/b | `invasion_warnings`, `last_invasion_id` | |
| tutoriel | `src/game/tutorial.c:14` | T | `tutorial_part1..3` | |
| grilles de carte | `src/map/terrain.c:7`, `building.c:6-8`, `figure.c:5`, `image.c:5`, `aqueduct.c:11`, `sprite.c:5`, `property.c:31-35`, `random.c:6`, `desirability.c:12`, `elevation.c:6`, `soldier_strength.c:8`, `routing.c:19-34`, `road_network.c:15` | b | `*_grid` | `GRID_SIZE` 162 en dur (`src/map/grid.h:9`) |
| `map_data` | `src/map/grid.c:9` | b | via `scenario` | |
| `all_buildings[MAX_BUILDINGS=2000]`, `extra` | `src/building/building.c:23-31` | b (+ propriétaire) | `buildings`, `building_extra_*` | `faction_id` existe mais n'est jamais lu |
| listes `small/large/burning` | `src/building/list.c:9` | b (scratch) | `building_list_*` | 500/2000/500 entrées |
| `storages[MAX_STORAGES=200]` | `src/building/storage.c:15` | b (par bâtiment) | `building_storages` | |
| `figures[MAX_FIGURES=1000]`, `created_sequence` | `src/figure/figure.c:15` | b (+ propriétaire) | `figures`, `figure_sequence` | |
| `formations[MAX_FORMATIONS=50]`, `id_last_in_use` | `src/figure/formation.c:16-18` | b | `formations` | |
| armées ennemies, routes, marchands, noms | `src/figure/enemy_army.c:8`, `route.c:9`, `trader.c:19`, `name.c:5` | b | parties homonymes | |
| temps, aléa | `src/game/time.c:3`, `src/core/random.c:7` | b | `game_time`, `random_iv` | `random_byte()` constant pendant un tick |
| `fire_spread_direction` | `src/building/maintenance.c:30` | b | non | |
| `last` (émulation des globales C3) | `src/map/point.c:3` | b (scratch) | non | |
| objets de l'empire, prix | `src/empire/object.c:27`, `trade_prices.c:15` | b | `trade_prices` | |
| `scenario` (carte, rivière, troupeaux, pêche, invasions, bâtiments autorisés, climat, prix/demande) | `src/scenario/scenario.c:7` | b | `scenario` (1720) | |
| tremblement de terre, année max | `src/scenario/earthquake.c:18`, `criteria.c:5` | b | `earthquake`, `max_game_year` | |
| `city_data.map.largest_road_networks` | `data_private.h:366-369` | **b** | `city_data` | calculé sur toute la carte |
| `paused` | `src/game/state.c:9` | b (commande globale) | non | |
| vue : caméra, orientation, viewport | `src/city/view.c:17,40` | c | `city_view_orientation`, `camera` | l'orientation fuit (§3.2) |
| avertissements | `src/city/warning.c:19` | c | non | |
| UI des messages (`current_message_id`, file, défilement, zones à problème, `last_sound_time`, `should_play_sound`) | `src/city/message.c:18,52` | c | `message_extra` | |
| `city_data.sound` | `data_private.h:337` | c | `city_data` (comparé) | |
| overlay, undo, construction en cours, menus | `src/game/state.c:9`, `src/game/undo.c:26`, `src/building/construction.c:50`, `menu.c:41` | c | non | undo écrit dans la simulation |
| signets, défilement de l'empire, légion sélectionnée | `src/map/bookmark.c:9`, `src/empire/empire.c:23`, `formation.c:18` | c | `bookmarks`, `empire` | |
| sons d'ambiance | `src/sound/city.c:95` | c | `city_sounds` (ignoré par le comparateur) | |

### 3.2 Fuites UI → simulation (bloquantes pour le lockstep, quelle que soit l'option)

1. Ouvrir les conseillers recalcule l'état simulé : `init()` dans `src/window/advisors.c:115-135`
   appelle `city_labor_allocate_workers` (réécrit `b->num_workers` et le `static` de
   `labor.c:312`), `city_finance_*`, `city_culture_update_coverage`,
   `city_resource_calculate_food_stocks_and_supply_wheat` (remplit l'inventaire de blé des
   marchés si Rome fournit le blé), `formation_calculate_figures` (peut appeler `formation_clear`).
   Idem `src/window/advisor/entertainment.c:83-84` (`city_gods_calculate_moods(0)`,
   `city_culture_calculate`), `window/advisor/military.c:187`, `widget/sidebar/military.c:610`.
   Fidèle à C3 en solo, mais en lockstep un seul PC exécuterait ces écritures : il faut en faire
   des commandes réseau (fidèle) ou des calculs sur copie (non fidèle).
2. L'orientation de la vue (locale) modifie la simulation : `game_orientation_rotate_*`
   (`src/game/orientation.c:9,17,25`) → `map_orientation_change` (`src/map/orientation.c:40-68`) :
   images, `map_routing_update_walls`, `figure_tower_sentry_reroute`,
   `figure_hippodrome_horse_reroute`. `count_adjacent_wall_tiles` (`src/map/routing_terrain.c:249`)
   et le calcul des images de figures (`src/figure/image.c:65`) dépendent de `city_view_orientation()`.
3. `city_message_post` (`src/city/message.c:166-199`) : `is_read` et le délai de popup dépendent de
   `window_is(WINDOW_CITY)` ; `building_maintenance_check_rome_access` déplace la caméra
   (`src/building/maintenance.c:336-338`) ; `city_victory_check` ouvre des fenêtres.
4. `game_run` arrête la boucle de ticks si une fenêtre est invalidée (`src/game/game.c:176`) :
   le nombre de ticks par image dépend de l'UI.

## 4. Boucles « de cité » à filtrer par propriétaire

Comptage : `grep -rnE "< MAX_BUILDINGS|<= MAX_BUILDINGS|building_get_highest_id\(\)|building_list_(small|large|burning)_size\(\)" src --include='*.c'`
→ 102 boucles (hors `list.c` ; la regex rate les variantes comme `4 * MAX_BUILDINGS`,
`house_population.c:43`), 23 sur `MAX_FIGURES`, 29 sur `MAX_FORMATIONS`/`MAX_LEGIONS` ;
aucune dans `src/window` ni `src/widget`.

**4.1 Agrégats vers l'état de la cité (filtre propriétaire obligatoire, ~40)**
- `src/city/culture.c:169` `city_culture_calculate` ; `entertainment.c:53` `city_entertainment_calculate_shows`.
- `src/city/finance.c:140` `city_finance_estimate_taxes`, `:180` `collect_monthly_taxes`, `:285` `reset_taxes`.
- `src/city/health.c:101` `city_health_update`, `:54/67/80` `cause_disease`.
- `src/city/labor.c:177` `calculate_workers_needed_per_category`, `:291` `set_building_worker_weight`,
  `:327` `allocate_workers_to_water`, `:367/409` `allocate_workers_to_non_water_buildings`.
- `src/city/population.c:319` `calculate_people_per_house_type` ; `ratings.c:453` `calculate_max_prosperity`.
- `src/city/resource.c:174/185` `city_resource_calculate_warehouse_stocks`, `:243` `calculate_available_food`,
  `:301` `..._supply_wheat`, `:316` `city_resource_calculate_workshop_stocks`, `:341` `city_resource_consume_food`.
- `src/city/sentiment.c:180/253` `city_sentiment_update` ; `src/city/military.c:54` `city_military_update_totals`.
- `src/building/count.c:57` `building_count_update` ; `government.c:25` `building_government_distribute_treasury` ;
  `granary.c:180` `building_granaries_calculate_stocks`.
- `src/building/house_population.c:17` `house_population_add_to_city` (curseur par cité), `:43` `house_population_remove_from_city`,
  `:62` `fill_building_list_with_houses`, `:75` `house_population_update_room`, `:99` `create_immigrants`,
  `:140` `create_emigrants`, `:170` `calculate_working_population`.
- `src/figure/formation.c:219` `formation_calculate_legion_totals`, `:242` `formation_get_num_legions`,
  `:253` `formation_for_legion` ; `src/figuretype/crime.c:114` `figure_generate_criminals`.

**4.2 Recherches de cible (sinon on se sert dans les stocks du voisin dès que les routes se touchent, ~20)**
- `src/building/warehouse.c:256` `building_warehouse_for_storing`, `:305` `building_warehouse_for_getting`,
  `:351` `determine_granary_accept_foods`, `:381` `determine_granary_get_foods`.
- `src/building/granary.c:229` `building_granary_for_storing`, `:278` `building_getting_granary_for_storing`.
- `src/building/industry.c:168/206` `building_get_workshop_for_raw_material(_with_room)` ;
  `market.c:73` `building_market_get_storage_destination`.
- `src/building/barracks.c:51` `get_closest_legion_needing_soldiers`, `:78` `get_closest_military_academy`.
- `src/figuretype/docker.c:93/159`, `trader.c:247` `get_closest_warehouse`, `entertainer.c:22/37`
  `determine_destination`, `migrant.c:65` `closest_house_with_room`, `src/map/water.c:211`
  `map_water_get_wharf_for_new_fishing_boat`.
- À trancher (design) : `src/building/maintenance.c:113` `building_maintenance_get_closest_burning_ruin`
  (préfets du voisin), `src/map/water_supply.c:50/153/190` (aqueducs/puits reliés entre joueurs),
  `src/figure/formation_enemy.c:143/181/203/239` (cibles des émeutiers, envahisseurs, indigènes).

**4.3 Effets de masse sur « toute la cité » (~13)**
- `src/building/warehouse.c:202/222/236` `building_warehouses_add/remove_resource` (fêtes `festival.c:88`, demandes [C], undo).
- `src/building/industry.c:125/138` `building_bless/curse_farms`, `granary.c:377/411` (dieux, `gods.c:50-124`).
- `src/building/destruction.c:165` `building_destroy_first_of_type` (événements aléatoires), `:184`
  `building_destroy_last_placed` (plus grand `created_sequence` **de toute la carte**).
- `src/city/sentiment.c:32/42` (fêtes, dieux) ; `src/building/barracks.c:131` `building_barracks_create_tower_sentry`.
- `src/building/maintenance.c:228` `building_maintenance_check_rome_access` : distances depuis
  l'entrée de la cité → `b->distance_from_entry`, utilisé ensuite par les greniers, migrants, etc.

**4.4 Boucles monde avec bascule de contexte par entité (M+ctx, ~9)**
`src/building/figure.c:1171` `building_figure_generate` · `src/figure/action.c:111`
`figure_action_handle` · `src/building/house_evolution.c:506` · `src/building/house_service.c:67` ·
`src/building/maintenance.c:160` `check_fire_collapse` · `src/building/building.c:205`
`building_update_state` · `house_population.c:222` `evict_overcrowded` ·
`src/figure/formation.c:597` `formation_update_all` ·
`src/map/road_network.c:76`.

**4.5 Pièges dans ces boucles : valeurs lues une fois avant la boucle**
- `house_evolution.c:508-509` : `city_houses_reset_demands()` + pointeur `demands` gardé pour toutes les maisons.
- `house_service.c:69` : `base_entertainment` = couverture de la cité, appliquée à toutes les maisons.
- `government.c:16` : `treasury` lu une fois ; `labor.c:313` : `water_cat` pointeur local.
- `building/figure.c:1173` : `patrician_generated` limite à **un** patricien par appel pour toute la carte.
- `maintenance.c:162` : `city_sentiment_reset_protesters_criminals()` avant la boucle monde.
- `action.c:113-114` : `city_figures_reset()` et `city_entertainment_set_hippodrome_has_race(0)` avant la boucle.

**4.6 Scratch partagés hérités de C3** : la grille `routing_distance` (`src/map/routing.c:19`) est
recalculée par `map_routing_calculate_distances` (entrée de cité, légions, construction) et relue
plus tard par d'autres (`src/map/road_access.c:123-204`, `road_aqueduct.c`, `terrain.c:240`) ;
`building_list_large` est remplie au tick 22 et relue aux ticks 23-24 ; `map_point_store_result`
(`src/map/point.c:3`) émule une globale C3. Avec N cités, ce qu'une cité lit peut provenir du calcul
d'une autre cité : la fidélité « exacte » par cité suppose de recalculer avant chaque lecture ou de
rendre ces scratch propres à chaque cité. Enfin `random_byte()` (`src/core/random.c:57`) est constant
pendant un tick : exécuter N fois une fonction P ne consomme pas d'aléa (la séquence solo est
préservée), mais toutes les cités tirent les mêmes valeurs ce tick-là (épidémies `health.c:27`, dieu
choisi `gods.c:174`, recensement via `random_from_pool`).

## 5. Options d'architecture pour N cités

### 5.1 Option A — contexte « cité courante » par pointeur

`struct city_data_t city_datas[MAX_MP_CITIES]; struct city_data_t *city_data_current;` et, dans
`data_private.h` seulement, `#define city_data (*city_data_current)`. Aucune des 2 111 lignes
n'est modifiée. Une structure compagnon par cité (`city_extra`) regroupe les `static` du §3 classe
(a), sélectionnée par le même index. API : `city_context_set(i)`, `city_context_push/pop`,
`for_each_active_city(fn)`.

Sites à toucher (estimation) : 2 déclarations ; ~12 `static` à déplacer ; ~30 appels P dans
`tick.c` ; ~9 boucles M+ctx ; ~70 boucles à filtrer (§4.1-4.3, communes aux deux options) ; 3-5
points d'entrée (début de `game_run`/`game_draw`, gestion des entrées, futur exécuteur de
commandes réseau : contexte = cité de l'émetteur). Total **~120-150 sites**.

Pièges : oubli de bascule (accès silencieux à la mauvaise cité, déterministe donc sans
désynchronisation mais faux) ; valeurs/pointeurs mis en cache (§4.5) ; bascules imbriquées
(une figure de A agit sur un bâtiment de B : commerce, combat, incendie) → pile push/pop avec
assertion d'équilibre ; il faut restaurer le contexte local après chaque tick pour l'UI ; la macro
casserait tout fichier qui inclurait `data_private.h` et utiliserait `city_data` comme autre
identifiant (ex. le membre `buffer *city_data` de `src/game/file_io.c:106`, qui n'inclut pas ce
header aujourd'hui). Alternative sans macro : `sed` mécanique `city_data.` → `city->` sur 2 111 lignes.

Tests autopilot : avec N=1 et `city_data_current = &city_datas[0]` fixe, le chemin d'exécution
est identique par construction ; l'écriture au format C3 reste `save_main_data` sur la cité 0.

### 5.2 Option B — identifiant de cité passé partout

Signatures : ~326 fonctions publiques (410 déclarées dans `src/city/*.h`, moins `view` 34,
`message` 28, `sound` 12, `warning` 5, `data` 5) + 82 fonctions `static` de `src/city` ;
1 279 lignes `city_data.` → `c->` ; 832 lignes de `data.c` paramétrées ; **685 sites d'appel**
(§1.3) ; plus la propagation de l'identifiant dans les fonctions intermédiaires qui n'en ont pas
(ex. `map_has_road_access` → `city_map_road_network_index`, actions de figures → helpers) : non
chiffré, plusieurs centaines. Total estimé **2 500-3 500 lignes**.

Pièges : diff massif (fusions avec Julius amont difficiles) ; collision de vocabulaire —
`city_id` désigne déjà une cité **de l'empire** (`empire_city_get(int city_id)`,
`figure_create_trade_caravan(x, y, city_id)`), il faudra un autre nom (`player_id`, `owner`) ;
mêmes boucles à filtrer et mêmes scratch partagés qu'en A.

Tests autopilot : compatibles si la refactorisation est purement mécanique ; le risque
d'erreur est proportionnel au diff, mais les 36 tests le détecteraient.

### 5.3 Comparaison et recommandation

| Critère | A (contexte) | B (identifiant explicite) |
|---|---|---|
| Taille du diff | ~150 sites | ~3 000 lignes |
| Fidélité solo / autopilot | identique par construction (N=1) | identique si refacto sans erreur |
| Vérification par le compilateur | non (dépendance implicite) | oui |
| Interactions entre cités | push/pop explicites | naturelles |
| Fusions avec l'amont | faciles | difficiles |
| Incrémental | oui, testable à chaque étape | difficilement |

**Recommandation : A**, avec une API explicite (identifiant de joueur) limitée aux échanges entre
cités (commerce entre joueurs, combats, dégâts, incendies chez le voisin). Raisons : l'encapsulation
existante (`city_data` confinée à `src/city/`) rend la bascule de pointeur peu coûteuse ; avec N=1
le comportement solo est inchangé ; chaque étape se valide avec `ctest`. Plan :
1. indirection par pointeur, N=1 → ctest ;
2. `city_extra` (statics du §3 a, y compris `labor.c:312`) → ctest ;
3. champ `owner` en mémoire sur `building`/`figure`/`formation`, **non sérialisé au format C3**
   (ne pas réutiliser `faction_id`, sauvegardé et comparé octet par octet), 0 par défaut → ctest ;
4. filtres §4.1-4.3 + bascules §4.4 + boucle `for_each_active_city` dans `tick.c`, N=1 → ctest ;
5. **test d'invariance d'index** : charger une sauvegarde de test dans la cité k≠0 (toutes les
   entités `owner = k`, les autres cités vides), simuler, réécrire la cité k au format C3 et
   comparer à la référence : détecte les accès implicites à la cité 0 et les scratch partagés (§4.6) ;
6. N>1 et format de sauvegarde MP.

## 6. Correspondance avec le format de sauvegarde

### 6.1 Le bloc `city_data` (36 136 octets)

- Taille déclarée : `src/game/file_io.c:249` (`create_savegame_piece(36136, 1)`, compressé) et
  `test/sav/sav_compare.c:42` (`{1, 36136, "city_data", 17908}` ; 17908 ne sert qu'à l'affichage
  des différences).
- Contenu (`src/city/data.c:52-527`, `save_main_data`) : `buffer_write_raw(main,
  city_data.unused.other_player, 18068)` (`data.c:54`) puis 18 068 octets de champs. Total
  recalculé en sommant les écritures de `save_main_data` : 36 136. Donc **36 136 = 2 × 18 068** :
  deux enregistrements de cité.
- L'ordre des champs sauvegardés n'a rien à voir avec l'ordre de la structure (ex. trésor à
  l'octet 12 de l'enregistrement, `data.c:64`) : on peut réorganiser ou étendre la structure en
  mémoire tant que `save_main_data`/`load_main_data` (`data.c:529`) restent identiques.
- Vérifié sur les 54 `.sav` de `test/data` (programme jetable hors dépôt, décompression avec
  `src/core/zip.c`) : les 18 068 premiers octets sont **tous nuls** et `city_faction` vaut 1.
  Historique : le commit `a6918914` (2013, « Loading of real saved games ») remplace l'assertion
  `sizeof(Data_CityInfo) == 18068` par `2*18068` et ajoute `__otherPlayer[18068]` en tête.
  Hypothèse (non vérifiable sans désassembler C3) : C3 stocke `city_info[2]` indexé par faction
  (0 = « autre joueur », 1 = joueur), reste d'un multijoueur abandonné ; `faction_id` des bâtiments
  (`building.c:73`), figures (`figure.c:39`, 0 pour les envahisseurs `invasion.c:305`) et formations
  (`formation.c:60/86`) va dans ce sens et n'est jamais lu par la logique (vérifié par grep).
- Seule exception du comparateur dans ce bloc : octet 35 160 (`sav_compare.c:373-386`) =
  octet 17 092 de l'enregistrement = `ratings.peace_explanation` (`data.c:328`), valeurs 7/8 inversées.

### 6.2 Autres parties liées à la cité

| Partie (octets) | Source | Classe |
|---|---|---|
| `city_faction_unknown` (2), `city_faction` (4) | `unused.faction_bytes`, `unused.faction_id` (`data.c:1033-1035`) | a (constantes) |
| `player_name` (64) | `scenario.settings.player_name` | a |
| `city_graph_order` (8) | `population.graph_order` (choix UI) + `unused.unknown_order` (`data.c:1036-1037`) | c/a |
| `city_entry_exit_xy` (16), `city_entry_exit_grid_offset` (8) | drapeaux d'entrée/sortie (`data.c:1006-1026`) | a |
| `culture_coverage` (60) | `culture.c:190` | a |
| `building_count_industry` (128), `_culture1` (132), `_culture2` (32), `_culture3` (40), `_military` (16), `_support` (24) | `src/building/count.c:233` (lecture `:316`) | a |
| `messages` (16 000), `message_extra` (12), `population_messages` (10), `message_counts` (80), `message_delays` (80) | `src/city/message.c:511` (`city_message_save_state`) | a + c |
| `building_barracks_tower_sentry` (4), `formation_totals` (12), `enemy_army_totals` (20) | `barracks.c`, `formation.c`, `enemy_army.c` | a / b |
| `empire_cities` (2706), `trade_route_limit`/`traded` (1280 + 1280), `trade_prices` (128) | `src/empire/` | a/b selon design |
| `emperor_change_*` (8 + 4), `invasion_warnings` (3232), `last_invasion_id` (2), `tutorial_part1..3` | `src/scenario/`, `src/game/tutorial.c` | C / T |
| `city_view_orientation` (4), `camera` (8), `city_sounds` (8960) | `view.c`, `sound/city.c` | c (`camera` et `city_sounds` ignorés par le comparateur, pas l'orientation) |

### 6.3 Conséquences

- Le format C3 doit rester **inchangé** pour le solo et les tests : en option A il suffit de
  sérialiser `city_datas[0]` (ou la cité choisie) avec le code actuel.
- Le slot `other_player` ne peut pas accueillir le multijoueur : 2 enregistrements seulement,
  champs étroits (`senate_x` en `uint8_t`, `barracks_x`/`distribution_center_x` en `int8_t`,
  offsets de grille en `int16_t` dans `city_data.building` et dans l'enregistrement sauvegardé :
  entrée/sortie en `u8`/`i16`, `data.c:120-125`), insuffisants au-delà de ~181×181 tuiles ; et
  les grilles du format font 162×162.
- Il faut un **format MP versionné** distinct (`SAVE_GAME_VERSION` vaut 0x66, `file_io.c:54`) :
  N enregistrements de cité (champs élargis) + `city_extra` par cité + propriétaire des entités +
  état par joueur de l'empire. `is_read` et `current_message_id` (dépendants de l'UI) doivent
  sortir de l'état comparé en lockstep.

## 7. Non vérifié / limites

- Sémantique d'origine de `other_player` et de `faction_id` dans C3 : hypothèse (§6.1).
- Tailles `sizeof` mesurées sur macOS arm64/clang uniquement (programme jetable dans le scratchpad).
  Rien n'est `packed` : MSVC ou 32 bits peuvent donner d'autres paddings.
- Comptages par regex : commentaires inclus, appels via pointeurs de fonction ou macros non
  détectés (ordre de grandeur ±5 %). Les estimations de sites des options A et B sont des ordres de
  grandeur, pas un inventaire.
- Je n'ai lancé ni `ctest` ni le jeu : « 36/36 verts » vient du brief (36 appels
  `add_integration_test` dans `test/CMakeLists.txt`). Environ 14 de ces tests (requests, caesar,
  invasion, distantbattle) exercent du code [C] : César doit être neutralisé par mode, pas supprimé.
- Les fonctions d'action de `src/figuretype/*.c` n'ont pas été auditées une à une pour les
  interactions entre cités : le §4 recense les boucles explicites, pas les effets indirects.
- Je n'ai pas mesuré l'effet des recalculs déclenchés par l'UI (§3.2) sur une partie solo.
- Classement (a)/(b) non tranché pour : aqueducs/puits reliés, préfets du voisin, indigènes et
  mission, quotas de routes commerciales, révolte des gladiateurs. Ce sont des choix de design.
- Contradiction avec la contrainte n°1 à arbitrer : tribut (`finance.c:333`), salaire et dette
  (`emperor.c:33`), salaires de Rome (`wages_rome`) et « route de Rome » font partie de l'économie
  interne d'une cité. Les supprimer change le gameplay interne ; les garder garde du « César ».
