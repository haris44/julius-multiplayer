# 06 — Ordonnancement d'un tick à N cités : audit des tâches de `game/tick.c`

Périmètre : chaque appel de `src/game/tick.c` (tâches 0-49, jour, mois, année, fin de tick) et les sous-fonctions qui
touchent des grilles ou des tableaux globaux. Base : branche `multiplayer`, après M3.5 (tranches d'ids). Lecture seule :
rien n'a été compilé ni exécuté. Les références `fichier:ligne` sont relatives à `src/`. **[NV]** = non vérifié.

Ordonnancement audité (proposition M3.6) : à chaque tick, pour chaque joueur `p` croissant,
`player_context_switch(p)` puis le `game_tick_run` d'origine en entier (`game/tick.c:169-186`), le calendrier
(`game/time.c:3-9`) étant enregistré comme état par cité.

Deux exigences :
- **parité** : avec 1 joueur, rien ne change ;
- **jumeaux** : une cité non branchée se comporte exactement comme si elle était seule sur la carte.

## 0. L'essentiel

1. **L'ordonnancement « tick d'origine entier, cité par cité » est le bon squelette.** Il garde l'ordre interne exact de
   chaque cité (tâche, puis temps, puis jour/mois/année, puis figures, puis événements), et avec un joueur il se réduit à
   l'original. Comme `random_byte()` ne fait que lire le générateur (`core/random.c:58-61`) et que seul
   `random_generate_next` l'avance, une tâche « monde » exécutée dans le contexte de la cité 0 ne perturbe pas ses tirages.
2. **Mais 14 tâches ne peuvent pas tourner telles quelles N fois.** Elles se rangent en trois familles :
   - **Grille effacée puis remplie par la tranche courante** (la cité suivante efface le travail de la précédente) :
     - eau, tâche 27 (`map/water_supply.c:149-151`) ;
     - désirabilité, tâche 37 (`map/desirability.c:125-130`) ;
     - terres indigènes, tâche 6 (`map/natives.c:199`) ;
     - influence romaine, tâches 5 et 29 (`figure/enemy_army.c:95`).
   - **Boucle qui déborde sur les tranches des autres** (double traitement) :
     - `building_figure_generate`, tâche 31 (`building/figure.c:1176`) ;
     - `building_maintenance_check_fire_collapse`, tâche 44 (`building/maintenance.c:183`) ;
     - `formation_calculate_figures`, tâches 5 et 29 (`figure/formation.c:482`) ;
     - `formation_legion_decrease_damage`, tâches 5 et 29 (`figure/formation_legion.c:373`) ;
     - `figure_sink_all_ships`, tâche 1 (`figuretype/water.c:305`).
   - **État global non enregistré, avancé ou écrasé N fois** :
     - calendrier ;
     - `fire_spread_direction` (`building/maintenance.c:30`) ;
     - compteur d'influence romaine (`figure/enemy_army.c:16`) ;
     - délais des marchands de l'empire (`empire/city.c:264`) ;
     - changements de prix et de demande, plusieurs fois appliqués (`scenario/price_change.c:32`,
       `scenario/demand_change.c:35`) ;
     - séisme, révolte, changement d'empereur, invasions, demandes ;
     - extension de l'empire, rotation des images de terrain.
3. **Le réseau routier (tâche 7) casse les jumeaux en silence.** La grille est globale et c'est voulu. En revanche, le
   classement des 10 plus grands réseaux, écrit dans `city_data` (`city/map.c:73-86`), compte les routes de toute la
   carte. Il est relu pour choisir la case d'accès à la route (`map/road_access.c:20,154,190`). Les réseaux d'un voisin
   peuvent donc évincer ceux d'une cité isolée du classement.
4. **Les recalculs de carte entière déclenchés par une cité** (`map_routing_update_land`, `map_tiles_update_all_*`)
   rafraîchissent aussi les zones des autres cités, plus tôt qu'elles ne l'auraient fait seules. Or le routage peut être
   « en retard » sur le terrain : la peste et les émeutiers détruisent sans recalcul (`building/destruction.c:153-161`).
   C'est un risque réel pour le test jumeaux, à confirmer par le test.
5. **Pour que le test jumeaux (M3.7) soit exact sur les 36 sauvegardes de référence**, il faut que chaque cité ait son
   propre exemplaire de tout ce qui est « scénario » et « empire » : demandes, invasions, séisme, révolte, empereur,
   commerce, prix. Ces sauvegardes contiennent César et des invasions. Le plus simple est de l'enregistrer par cité en
   M3.6. Les règles du jeu libre (M4 et M8) décideront ensuite ce qui redevient commun au monde.

## 1. Légende

- **État de cité** : enregistré dans `player_context` (`game/player_context.c:63-77`) :
  - `city_data`, couvertures culturelles, compteurs de bâtiments, greniers (`non_getting_granaries`) ;
  - sentinelles, totaux de formations, victoire, ressources disponibles, messages ;
  - curseur des travailleurs du service des eaux, générateur aléatoire ;
  - listes de travail (`building/list.c:10-24`, petite, grande et incendies) ;
  - `building extra` (dont `highest_id_in_use`), séquence des figures, noms.
- **Grille** : grille de carte partagée.
- **Brouillon** : état temporaire global, recalculé avant usage :
  - `routing_distance` et la file de `map/routing.c` ;
  - les files de `road_network.c` et de `water_supply.c`.
- **Global NE** : état global, non enregistré (voir §7).
- Colonne « N fois OK ? » : peut-on l'exécuter une fois par cité, dans son contexte et sans modification ?
  - **oui** ;
  - **idem** : idempotent, mais coût multiplié par N ;
  - **NON**.

## 2. Début de tick (`game_tick_run`, `game/tick.c:169-186`)

| Tâche | Lit / écrit | N fois OK ? | Problème | Recommandation |
|---|---|---|---|---|
| `mp_command_run_scheduled` (:177) | commandes de tous les joueurs | — | déjà hors cités | **Une seule fois, avant la boucle des cités.** Chaque commande doit basculer vers le contexte de son joueur. |
| `random_generate_next` (:178 ; `core/random.c:26-48`) | générateur, enregistré par cité (`random.c:120-123`) | oui | — | **Par cité tel quel.** C'est ce qui rend chaque cité indépendante. |
| `game_undo_reduce_time_available` (:179 ; `game/undo.c:267-319`) | `undo.c:27-35` (Global NE) ; lit les bâtiments annulables | NON en théorie, inactif en réseau | `game_can_undo()` est faux en réseau (`undo.c:37-41`). Hors réseau à N cités (tests locaux), `timeout_ticks--` serait appliqué N fois. | **Une seule fois**, dans le tour de la cité 0 (`if (p == 0)`). Parité conservée. |

## 3. Les 50 tâches de la journée (`advance_tick`, `game/tick.c:118-167`)

Les tâches 0, 9, 11, 13, 14, 15, 26, 41, 42 et 47 sont vides.

| Tâche | Lit / écrit | N fois OK ? | Problème | Recommandation |
|---|---|---|---|---|
| 1 `city_gods_calculate_moods(1)` (`city/gods.c:250-339`) | `city_data.religion`, couverture, compteurs, générateur. Sous-fonctions : bénédictions et malédictions | oui pour l'humeur ; **NON** pour 2 sous-fonctions | (a) `figure_sink_all_ships` (`figuretype/water.c:303-321`) parcourt `FIGURE_ALL_END` : la colère de Neptune d'une cité coule les navires de **toutes** les cités. (b) `formation_legion_curse` (`figure/formation_legion.c:273`) vise les formations absolues 1 à 6, donc les légions de la cité 0 quelle que soit la cité maudite. | Par cité, après deux corrections : (a) boucle `FIGURE_FIRST..FIGURE_END` ; (b) `FORMATION_BASE + 1 .. FORMATION_BASE + 6`. Les ids absolus et relatifs coïncident avec 1 joueur, la parité est donc conservée. |
| 2 `sound_music_update(0)` (`sound/music.c:85-115`) | interface : `data.next_check`, population et ennemis de la cité courante | NON (interface) | `next_check` serait décrémenté N fois, et la musique suivrait la dernière cité. | **Une seule fois, dans le contexte du joueur local** (`p == mp_session_local_player_id()`). Avec 1 joueur, c'est p = 0. |
| 3, 30 `widget_minimap_invalidate` (`widget/minimap.c:60-63`) | drapeau d'interface | idem | — | Une seule fois, ou laisser tel quel : coût nul. |
| 4 `city_emperor_update` (`city/emperor.c:33-160`) | `city_data.emperor`, `ratings`, `finance`. `formation_caesar_pause` et `_retreat` parcourent la tranche (`figure/formation.c:187-203`). `scenario_invasion_start_from_caesar` | oui pour la cité, **NON** pour l'invasion | L'invasion de César écrit `scenario/invasion.c:86-89` et `enemy_armies` (Global NE). | **Par cité tel quel**, si l'état d'invasion est enregistré par cité (§7). En MP, César est neutralisé (DESIGN §5.1). |
| 5, 29 `formation_update_all` (`figure/formation.c:598-610`) | `formations` (tranche), figures, `enemy_armies` et `totals` (Global NE), grille `strength`, `city_data.figure`, `city_data.military` | **NON** | (a) `formation_calculate_figures` (:479-530) remet à zéro les formations de la tranche, puis ajoute **toutes** les figures (`FIGURE_ALL_END`, :482) à leur formation. Les formations des autres tranches sont doublées et leur `index_in_formation` est réécrit. (b) `formation_legion_decrease_damage` (`formation_legion.c:371-383`) parcourt toutes les figures : les blessures guérissent N fois plus vite. (c) `enemy_army_calculate_roman_influence` (`enemy_army.c:88-123`) incrémente un compteur global (cadence 1/5 faussée) et efface toute la grille `strength`. Les ennemis d'une autre cité la relisent à chaque tick (`soldier_strength.c:46-60`, `formation_herd.c:30`). (d) `totals` (`enemy_army.c:10-17`) est global et reflète la dernière cité. (e) `set_native_target_building(formation_get(0))` (`formation_enemy.c:594`) écrit la formation indigène **absolue** 0, lue par `figuretype/native.c:63` : la dernière cité écrase la cible des autres. | Par cité, après corrections : (a) n'ajouter une figure que si `FORMATION_OWNER(f->formation_id) == courant`. Les figures d'une formation sont créées dans la même tranche [NV : à confirmer pour les ennemis créés par `start_invasion`]. (b) `FIGURE_FIRST..FIGURE_END`. (c, d) Enregistrer `totals` par cité, ainsi que `enemy_armies` si les invasions sont par cité ; une grille `strength` **par cité** (`strength[p]`), car son recalcul périodique, effacement compris, n'a de sens que pour une cité. (e) `formation_get(FORMATION_BASE)` aux deux endroits. |
| 6 `map_natives_check_land` (`map/natives.c:197-223`) | efface tout le drapeau « terre indigène » de la grille propriétés (:199), puis le remet pour les huttes de la tranche. `native_anger` des huttes, `city_data.military` | **NON** (effacement) | La cité p+1 efface les terres indigènes marquées par p. Seule l'interface les lit (`widget/city_overlay_risks.c:272`), et `has_building_on_native_land` les relit aussitôt dans le même appel : l'effet sur la simulation est donc nul. En revanche, `has_building_on_native_land` compte les bâtiments **de tous les joueurs**, ce qui est voulu par DESIGN §5.3. | Par cité, mais **effacer une seule fois** : `map_property_clear_all_native_land()` dans le tour de la cité 0, le marquage dans chaque cité. Plus tard (DESIGN §5.3), les indigènes iront dans la tranche « monde ». |
| 7 `map_road_network_update` (`map/road_network.c:76-91`) | grille `network` et file (Brouillon) ; `city_data.map.largest_road_networks` | idem pour la grille, **NON** pour le classement | La grille est recalculée sur toute la carte, et c'est voulu (les routes se branchent, D-018). Mais (a) le classement des 10 plus grands réseaux (`city/map.c:65-86`) inclut les réseaux des autres. Une cité isolée peut en être évincée : son index tombe à 11, et `find_minimum_road_tile` choisit une autre case d'accès (`map/road_access.c:20,154,190`). (b) La numérotation dépend des routes des autres : les `road_network_id` sauvegardés diffèrent de la référence, alors que seules les égalités sont utilisées. (c) L'id est un `uint8_t` (:33,80) : au-delà de 255 réseaux sur une grande carte, des réseaux distincts partagent le même id. | **Scinder.** Monde, une seule fois, dans le tour de la cité 0 : grille `network` et liste (id, taille) de tous les réseaux. Par cité : `largest_road_networks` restreint aux réseaux de la cité. Pour être exact pour les jumeaux, il faut les réseaux dont toutes les cases sont au joueur (grille `owner`, D-018) ou, à défaut, ceux qui touchent un bâtiment de la tranche. L'ordre relatif est conservé, car le balayage est en ordre de trame. Le test jumeaux doit normaliser `road_network_id` (comparer les partitions, pas les numéros). Passer l'id en 16 bits avant M3.2. |
| 8 `building_granaries_calculate_stocks` (`building/granary.c:170-212`) | `non_getting_granaries` (cité), tranche | oui | — | Par cité tel quel. |
| 10 `building_update_highest_id` (`building/building.c:293-304`) | `extra.highest_id_in_use` (cité), en id **absolu** | oui | La valeur est absolue : p·2000 + local. Ses lecteurs doivent donc partir de `BUILDING_FIRST` (voir tâches 31, 37 et 44). | Par cité tel quel. |
| 12 `house_service_decay_houses_covered` (`building/house_service.c:53-65`) | tranche | oui | — | Par cité tel quel. |
| 16 `city_resource_calculate_warehouse_stocks` (`city/resource.c:169-203`) | tranche, `city_data.resource` ; `map_has_road_access` | oui (sous réserve) | dépend du classement des réseaux (tâche 7) | Par cité tel quel, une fois la tâche 7 scindée. |
| 17 `city_resource_calculate_food_stocks_and_supply_wheat` (`resource.c:236-297`) | tranche, `city_data` | oui | — | Par cité tel quel. |
| 18 `city_resource_calculate_workshop_stocks` (`resource.c:299-321`) | tranche, `city_data`, accès route | oui (sous réserve) | idem 16 | Par cité tel quel. |
| 19 `building_dock_update_open_water_access` (`building/dock.c:34-48`) | BFS sur l'eau depuis l'entrée de la rivière, commune (`routing_distance`, Brouillon), tranche | idem | Le même BFS est refait N fois. Il est relu tout de suite (`map/terrain.c:235-241`). | Par cité tel quel. Optimisation possible : un seul BFS par tick si plusieurs cités ont des quais. |
| 20 `building_industry_update_production` (`building/industry.c:37-74`) | tranche, images des fermes | oui | — | Par cité tel quel. |
| 21 `building_maintenance_check_rome_access` (`building/maintenance.c:238-357`) | BFS citoyen depuis `city_map_entry_point()` de la cité (`routing_distance`) ; tranche ; démolition de murs et d'aqueducs ; `map_tiles_update_all_*` ; `map_routing_update_land` et `_walls` | oui pour la boucle ; **à surveiller** | (a) Le BFS traverse aussi les terrains dégagés jusque chez le voisin. Une cité « non branchée par la route » mais voisine peut recevoir des distances différentes si l'autre cité offre un raccourci ou fait obstacle. Les jumeaux doivent être séparés par une barrière (eau, falaises) ou assez éloignés [NV]. (b) Si la sortie est bloquée, `map_routing_delete_first_wall_or_aqueduct` (:330-331) peut démolir les murs d'un autre joueur, contraire à D-018. (c) `building_destroy_last_placed` [NV : quelle tranche ?]. (d) Recalculs de carte entière : voir le §6. (e) `map_road_to_largest_network` dépend du classement (tâche 7). | Par cité tel quel pour l'ordonnancement. Filtrer par propriétaire en M4 : murs et aqueducs du joueur bloqué seulement. |
| 22 `house_population_update_room` (`building/house_population.c:68-90`) | grande liste (cité), tranche, `city_data.population` | oui | La liste est enregistrée, donc le passage 22 → 23 → 24 est conservé par cité. | Par cité tel quel. |
| 23 `house_population_update_migration` (`house_population.c:182-215` ; `city/migration.c`) | `city_data` ; grande liste ; création de migrants (tranche) ; `city_figures_total_invading_enemies` | oui | Les ennemis ne sont comptés que dans la tranche de la cité (`city/figures.c`, calculé dans `figure_action_handle`). | Par cité tel quel. |
| 24 `house_population_evict_overcrowded` (:217-233) | grande liste, création de sans-abri | oui | — | Par cité tel quel. |
| 25 `city_labor_update` (`city/labor.c`) | tranche, `city_data.labor`, curseur `water_start_building_id` (enregistré, `labor.c:538`) | oui | Le curseur est remis à 1 et non à `BUILDING_FIRST` (`labor.c:93-95` relatif), mais il est recadré à la lecture (:344-346). | Par cité tel quel. |
| 27 `map_water_supply_update_reservoir_fountain` (`map/water_supply.c:147-217`) | **efface** `TERRAIN_FOUNTAIN_RANGE` et `RESERVOIR_RANGE` sur toute la carte (:149) ; remet **tous** les aqueducs à sec (:151, :74-89) ; remplit depuis les réservoirs de la tranche ; grande liste | **NON** | La cité p+1 efface les zones d'eau et assèche les aqueducs de p. Au tick 28 suivant, les maisons de p n'ont plus d'eau. De plus, `fill_aqueducts_from_offset` (:91-145) suit tout aqueduc adjacent et marque `has_water_access = 2` sur un réservoir d'un autre joueur (:117-118), contraire à DESIGN §3.4 (eau par propriétaire). | **Scinder.** Monde, dans le tour de la cité 0, à la même place : `map_terrain_remove_all(...)` et `set_all_aqueducts_to_no_water()`. Par cité, sans effacer : réservoirs de la tranche, remplissage limité aux aqueducs du joueur (grille `owner`) et aux réservoirs de la tranche, zones, fontaines. Avec 1 joueur, l'ordre est identique. Pour des jumeaux non branchés, les réseaux d'aqueducs sont disjoints, donc le résultat est exact. |
| 28 `map_water_supply_update_houses` (:48-72) | maisons de la tranche ; lit les zones d'eau ; puits de la tranche ; petite liste | oui, après 27 | `mark_well_access` (:32-46) met `has_well_access` sur **n'importe quel** bâtiment à 2 cases d'un puits, y compris chez le voisin, et cet état persiste jusqu'au tick 28 suivant. | Par cité, en filtrant `BUILDING_OWNER(building_id) == courant` dans `mark_well_access`. Sans effet pour des jumeaux éloignés. |
| 31 `building_figure_generate` (`building/figure.c:1171-1260`) | **boucle `i = 1 .. max_id`** (:1176), avec `max_id` absolu ; création de figures dans la tranche courante | **NON** | Pour p ≥ 1, la boucle repasse sur toutes les tranches 0..p-1. Les bâtiments des autres génèrent des figures dans la tranche de p, et leurs délais avancent plusieurs fois. | Par cité, avec `for (i = BUILDING_FIRST; i <= max_id; ...)`. C'est identique avec 1 joueur. |
| 32 `city_trade_update` (`city/trade.c:8-29`) → `empire_city_generate_trader` (`empire/city.c:296-319`) | `city_data.trade` ; **`cities[]` de l'empire** (Global NE : `is_open`, `trader_entry_delay`, `trader_figure_ids`) ; `trade_route` (Global NE) ; `figure/trader.c:19` (Global NE, anneau `next_index`) | **NON** | `trader_entry_delay--` (:264) est appliqué N fois par tick. Un marchand créé par une cité occupe l'emplacement `trader_figure_ids` pour toutes. Les ids des marchands s'entrelacent dans `traders[]`. | Par cité, après avoir enregistré par cité `empire/city.c:18` (`cities[]`), `empire/trade_route.c:10` et `figure/trader.c` (tant que D-019/M8 n'a pas tranché). Avec 1 joueur, rien ne change. |
| 33 `building_count_update` + `city_culture_update_coverage` (`building/count.c:52` ; `city/culture.c`) | tranche, compteurs (cité), `city_data` | oui | — | Par cité tel quel. |
| 34 `building_government_distribute_treasury` (`building/government.c:25`) | tranche, `city_data` | oui | — | Par cité tel quel. |
| 35 `house_service_decay_culture` (`house_service.c:15-41`) | tranche | oui | — | Par cité tel quel. |
| 36 `house_service_calculate_culture_aggregates` (:67-…) | tranche, couverture (cité) | oui | — | Par cité tel quel. |
| 37 `map_desirability_update` (`map/desirability.c:125-130`) | **efface** la grille de désirabilité (:14-17), ajoute les bâtiments `1 .. max_id` (:69-84, `max_id` absolu de la cité courante), puis le terrain de toute la carte (:86-123) | **NON** | Chaque cité efface puis recalcule. Pour p = 0, seuls les bâtiments de la tranche 0 sont comptés. Pour la dernière cité, toutes les tranches le sont, par accident. Le résultat final dépend donc de l'ordre, et les cités 0 à N-2 relisent jusqu'au tick 37 suivant une grille calculée avec des voisins partiels. La désirabilité doit traverser les cités (D-018). | **Une seule fois (monde)**, dans le tour de la cité 0, à la même place, en parcourant toutes les tranches : `for (i = 1; i < BUILDING_ALL_END; i++)`. Les ids au-delà de `highest_id_in_use` sont vides, donc c'est identique avec 1 joueur. Le `calc_bound` à chaque ajout dépend de l'ordre, mais seulement sur les cases touchées par les deux cités, donc les jumeaux éloignés sont exacts. |
| 38 `building_update_desirability` (`building/building.c:253-274`) | tranche, lit la grille | oui | — | Par cité tel quel. |
| 39 `building_house_process_evolve_and_consume_goods` (`building/house_evolution.c:506-524`) | tranche, `city_data.houses` ; fusion et agrandissement (`building/house.c:111,158,185…`, `map_building_at`) ; `map_tiles_update_all_gardens` ; `map_routing_update_land` | oui pour l'ordonnancement | (a) L'agrandissement peut absorber la maison adjacente d'un autre joueur : filtre par propriétaire à ajouter en M4. (b) Recalculs de carte entière (§6). | Par cité tel quel. |
| 40 `building_update_state` (`building/building.c:206-251`) | tranche ; `map_building_tiles_remove` ; puis, selon le cas, `map_tiles_update_all_walls`, `_aqueducts`, `map_routing_update_land`, `map_tiles_update_all_roads` | oui pour la boucle | Recalculs de toute la carte, déclenchés par une cité (§6). | Par cité tel quel. Voir le §6 pour la portée des recalculs. |
| 43 `building_maintenance_update_burning_ruins` (`building/maintenance.c:53-117`) | tranche ; liste des incendies (cité) ; `fire_spread_direction` (**Global NE**, :30) ; propage le feu au bâtiment voisin, **quel que soit son propriétaire** (:83-104) | **NON** (`fire_spread_direction`) | `fire_spread_direction` est tiré chaque année par `building_maintenance_update_fire_direction` dans le contexte de chaque cité, avec son générateur. La dernière cité l'emporte et toutes l'utilisent. Le feu qui passe chez le voisin crée une ruine dans la tranche de **l'incendiaire** (`destruction.c:79`), et la population retirée est celle du mauvais `city_data` (`destruction.c:27`). | Par cité, en **enregistrant `fire_spread_direction` par cité**. Pour le feu transfrontalier (M4) : basculer vers le contexte du propriétaire de la victime (`BUILDING_OWNER`) avant `building_destroy_by_fire`. |
| 44 `building_maintenance_check_fire_collapse` (`maintenance.c:175-236`) | **boucle `i = 1 .. max_id`** (:183) ; `random_byte` ; `map_random_get` ; `city_sentiment_reset_protesters_criminals` | **NON** | Pour p ≥ 1, les tranches 0..p-1 sont reparcourues : risques d'incendie et d'effondrement ajoutés plusieurs fois, effondrements et incendies déclenchés dans le mauvais contexte. | Par cité, avec `for (i = BUILDING_FIRST; i <= max_id; ...)`. NB : `random_building = (i + map_random) & 7` reste exact, car 2000 est divisible par 8. |
| 45 `figure_generate_criminals` (`figuretype/crime.c:110-157`) | `BUILDING_FIRST .. max_id` (déjà correct, :115), `random_byte`, `city_data` | oui | — | Par cité tel quel. |
| 46 `building_industry_update_wheat_production` (`industry.c:76-100`) | tranche | oui | — | Par cité tel quel. |
| 48 `house_service_decay_tax_collector` (`house_service.c:43-51`) | tranche | oui | — | Par cité tel quel. |
| 49 `city_culture_calculate` (`city/culture.c:162-189`) | tranche, `city_data`, spectacles, coûts des fêtes | oui | — | Par cité tel quel. |

`game_time_advance_tick` (`game/tick.c:164` ; `game/time.c:45-52`) : avec le calendrier enregistré par cité, chaque cité
avance sa copie, et toutes restent égales. Voir le §8, étape 1, pour l'assertion de cohérence.

## 4. Jour (`advance_day`, `game/tick.c:107-116`)

| Tâche | Lit / écrit | N fois OK ? | Problème | Recommandation |
|---|---|---|---|---|
| `game_time_advance_day` (`time.c:54-62`) | calendrier | oui si le calendrier est par cité | sinon, N jours par jour | Enregistrer `game/time.c:3-9` par cité (prévu). |
| `city_sentiment_update` (jours 0 et 8 ; `city/sentiment.c`) | tranche, `city_data` | oui | — | Par cité tel quel. |
| `tutorial_on_day_tick` (`game/tutorial.c:221-256`) | `tutorial.c` (Global NE) | idem | inerte hors tutoriel | Tel quel. |

## 5. Mois (`advance_month`, `game/tick.c:72-105`) et année (`advance_year`, :59-70)

| Tâche | Lit / écrit | N fois OK ? | Problème | Recommandation |
|---|---|---|---|---|
| `city_migration_reset_newcomers` | `city_data` | oui | — | Par cité. |
| `city_health_update` (`city/health.c:92-136`) | tranche, `city_data`, peste (`building_destroy_by_plague`, sans recalcul du routage) | oui | voir le §6 (routage en retard) | Par cité. |
| `scenario_random_event_process` (`scenario/random_event.c`) | `random_byte` (cité) ; `scenario.random_events` (lecture) ; `city_data` (salaires de Rome, commerce, santé) ; `building_destroy_first_of_type` (tranche, `destruction.c:163-178`) | oui | Chaque cité tire ses propres événements avec son générateur, ce qui est voulu. | Par cité tel quel. |
| `city_finance_handle_month_change` (`city/finance.c`) | tranche, `city_data` | oui | — | Par cité. |
| `city_resource_consume_food` (`resource.c:323-…`) | tranche, `city_data` | oui | — | Par cité. |
| `scenario_distant_battle_process` (`scenario/distant_battle.c:39-56`) | `scenario.invasions` (lecture), `city_data.distant_battle` ; `city_military_process_distant_battle` [NV en détail : écrit aussi l'état de l'empire ?] | oui pour le déclenchement | César, neutralisé en MP. Pour les jumeaux, la lecture seule de `scenario` est sans danger. | Par cité. |
| `scenario_invasion_process` (`scenario/invasion.c:319-…`) | `data.warnings`, `last_internal_invasion_id` (Global NE, :86-89) ; `start_invasion` crée formation et ennemis dans la tranche courante ; `enemy_armies` | **NON** | `months_to_go--` est appliqué N fois. L'invasion est lancée dans la tranche de la première cité qui l'atteint, et les autres ne la voient pas. | Enregistrer par cité `invasion.c` `data` et `enemy_army.c` (pour les jumeaux et la parité multi-sauvegardes). En jeu libre (DESIGN §5.3), une « menace neutre » deviendra une tâche monde, dans la tranche monde, qui désigne sa cible. |
| `scenario_request_process` (`scenario/request.c:26-…`) | `scenario.requests[]` (Global NE), `city_data.ratings` | **NON** | `months_to_comply--` est appliqué N fois. L'état reçu ou refusé est commun. | Enregistrer par cité. César est neutralisé en MP, mais c'est nécessaire au test jumeaux sur les 36 sauvegardes. |
| `scenario_demand_change_process` (`demand_change.c:20-48`) | `trade_route` (Global NE) | **NON** | `trade_route_increase_limit` est appliqué **N fois** au même mois. | Une seule fois (monde) si `trade_route` reste commun ; par cité s'il est enregistré par cité (recommandé jusqu'à M8). Message à chaque cité concernée. |
| `scenario_price_change_process` (`price_change.c:19-41`) | `trade_prices` (Global NE, `empire/trade_prices.c:15`) | **NON** | Le prix change **N fois**. | Prix communs au monde (D-019) : **une seule fois**, dans le tour de la cité 0, puis un message dans chaque cité. Pour le test jumeaux : prix par cité (enregistrés), ou scénarios sans changement de prix. |
| `city_victory_update_months_to_govern` | `city_data.mission` | oui | — | Par cité. |
| `formation_update_monthly_morale_at_rest` (`formation.c:360-…`) | tranche de formations | oui | — | Par cité. |
| `city_message_decrease_delays` (`city/message.c:364-371`) | messages (cité) | oui | — | Par cité. |
| `map_tiles_update_all_roads` (`map/tiles.c:728-731`) | images de toute la carte ; compteurs de rotation `image_context` (Global, sauvegardé, `map/image_context.c:18,354-360`) | **NON en toute rigueur** | Le résultat dépend des compteurs de rotation : N passes en ordre de trame les font avancer N fois, et une cité jumelle reçoit d'autres variantes d'images que seule. [NV] La simulation relit-elle ces variantes ? `routing_terrain.c:87-107` relit les images d'**aqueduc** ; pour les routes, probablement pas. | Voir §6 : une seule fois par mois (monde) **ou** compteurs de rotation par cité et passe limitée aux cases du joueur. |
| `map_tiles_update_all_water` (`tiles.c:901-904`) | images du rivage ; `image_context` (eau) | **NON en toute rigueur** | idem | Une seule fois (monde). L'eau n'a pas de propriétaire. |
| `map_routing_update_land_citizen` (`map/routing_terrain.c:109-140`) | grille `terrain_land_citizen` de toute la carte, fonction pure du terrain (sauf la réparation « shouldn't happen », :119-127) | idem en apparence ; **risque** | voir §6 : rafraîchit en avance la zone d'une autre cité dont le routage était en retard | §6. |
| `city_message_sort_and_compact` (`message.c:264-…`) | messages (cité) ; tri en O(10⁶) | oui | coût ×N, mensuel : négligeable | Par cité. |
| `game_time_advance_month` | calendrier (cité) | oui | — | Par cité (calendrier enregistré). |
| `city_ratings_update(0/1)` (`city/ratings.c`) | tranche (:453), `city_data`, `scenario_criteria_*` (lecture) | oui | — | Par cité. |
| `city_population_record_monthly`, `city_festival_update` (`city/festival.c:120-135`) | `city_data` | oui | — | Par cité. |
| `tutorial_on_month_tick` | tutoriel | idem | — | Tel quel. |
| autosauvegarde `game_file_write_saved_game("autosave.sav")` (:102-104) | toute la partie | **NON** | Elle serait écrite N fois, **au milieu de la boucle des cités**, avec le contexte de p chargé : les emplacements des autres seraient à jour, mais pas celui de p (`player_context_flush` absent). | Avec 1 joueur : à sa place (parité). Avec N : la reporter **après** la boucle des cités, une seule fois, au format `.mpsav` (M3.8). |
| **Année** : `scenario_empire_process_expansion` (`scenario/empire.c:18-31`) | `scenario.empire.is_expanded`, `empire/city.c` (`cities[].type`), `empire/object.c` (Global NE) | **NON** | Seule la première cité fait l'extension et reçoit le message : `is_expanded` est déjà vrai pour les suivantes. | Monde, une seule fois (cité 0), **plus** le message dans chaque cité. Ou tout enregistré par cité, si l'empire est par cité jusqu'à M8. |
| `game_undo_disable` (`undo.c:43-46`) | annulation (Global NE) | idem | idempotent | Tel quel. |
| `game_time_advance_year` | calendrier | oui | — | Par cité. |
| `city_population_request_yearly_update`, `city_finance_handle_year_change` | `city_data` | oui | — | Par cité. |
| `empire_city_reset_yearly_trade_amounts` (`empire/city.c:159-166`) | `trade_route` traded (Global NE) | idem | remise à zéro idempotente | Tel quel si commun ; par cité s'il est enregistré. |
| `building_maintenance_update_fire_direction` (`maintenance.c:32-35`) | `fire_spread_direction` (Global NE) ← `random_byte` de la cité | **NON** | la dernière cité l'emporte (voir 43) | **Enregistrer par cité.** |
| `city_ratings_update(1)`, `city_gods_reset_neptune_blessing` | `city_data` | oui | — | Par cité. |

## 6. Recalculs de carte entière déclenchés par une cité

Ces fonctions appellent un recalcul de **toute** la carte :
- tâche 21 (rome access, :333-339) ;
- tâche 39 (agrandissement d'une maison) ;
- tâche 40 (bâtiment démoli, rendu, magasin, réservoir) ;
- tâches 43 et 44 (incendie, effondrement) ;
- mois (`tick.c:88-90`) ;
- séisme.

| Fonction | Nature | Idempotente ? | Risque pour les jumeaux |
|---|---|---|---|
| `map_routing_update_land` / `_citizen` / `_noncitizen` (`routing_terrain.c:25-29,109-200`) | grilles de routage, fonction du terrain et du type de bâtiment de chaque case | oui | **Oui.** Le routage peut être en retard sur le terrain : `building_destroy_by_plague` et `_by_rioter` (`destruction.c:153-161`) ne le recalculent pas. Seuls le prochain recalcul déclenché (tâches 40, 43, 44, 39) ou celui du mois le mettent à jour. Si la cité B déclenche un recalcul global, la zone de A est rafraîchie plus tôt qu'en partie solo. Le même problème vient du recalcul mensuel fait N fois, car les émeutiers de A détruisent entre le tour de A et celui de B. |
| `map_routing_update_walls`, `_water` | idem | oui | idem, plus faible |
| `map_tiles_update_all_walls`, `_aqueducts`, `_roads`, `_gardens`, `_plazas`, `_empty_land`, `_meadow`, `_water` (`map/tiles.c`) | images, via les compteurs globaux de `image_context` | **non** (rotation) | Oui pour `image_grid`. Pour la simulation, seulement si une variante d'image est relue [NV : aqueducs, `routing_terrain.c:87-107`]. |

Recommandation, au choix. La décision appartient à M3.6/M3.7, à noter dans DECISIONS.md :
- **A (exacte, recommandée)** : borner ces recalculs à la « zone » de la cité qui les déclenche.
  - La zone regroupe les cases dont le bâtiment est de la tranche p, celles dont la grille `owner` vaut p, et les cases
    sans propriétaire (rivage, arbres, prairie).
  - Il faut donc que la grille `owner` de D-018 garde aussi la trace des ruines et des décombres laissés par p.
  - Avec 1 joueur, la zone couvre toute la carte et l'ordre de trame est identique, donc la parité est conservée.
  - Les compteurs `image_context` deviennent un état par cité (environ 250 octets à enregistrer).
- **B (provisoire)** : garder les recalculs globaux et vérifier empiriquement, avec le test jumeaux, qu'aucune des
  36 sauvegardes ne diverge, en excluant `image_grid` de la comparaison si la simulation ne relit pas les variantes. Il
  faut le documenter comme une limite connue (peste ou émeute plus recalcul déclenché par le voisin).

Dans tous les cas : une démolition d'un bâtiment d'un autre joueur doit basculer vers le contexte du propriétaire. C'est le
cas du feu transfrontalier (tâche 43), du séisme et de `map_routing_delete_first_wall_or_aqueduct`. Sinon, la population
retirée (`destruction.c:27`) et la ruine créée (`destruction.c:79`) vont à la mauvaise cité.

## 7. Fin de tick (`game/tick.c:181-185`)

| Tâche | Lit / écrit | N fois OK ? | Problème | Recommandation |
|---|---|---|---|---|
| `figure_action_handle` (`figure/action.c:111-133`) | figures de la tranche (`FIGURE_FIRST..FIGURE_END`) ; `city_figures_reset` (`city_data.figure`) ; grilles de figures et de routage ; `route_paths` (tranches, `figure/route.c:10-12`) | oui | Les compteurs d'ennemis, d'animaux et d'émeutiers (`city_data.figure`) ne comptent que la tranche courante, ce qui est voulu par cité. Sous-fonctions qui parcourent `FIGURE_ALL_END` (voulu pour le combat, `figure/combat.c`) : `figure_hippodrome_horse_reroute` (`figuretype/animal.c:448-460`) relance les chevaux de **toutes** les cités, mais il n'est pas appelé dans le tick [NV]. `map_point last` (`map/point.c:3`, brouillon relu par `figuretype/enemy.c:77` et `soldier.c:100`) peut transmettre une valeur d'une cité à l'autre. | **Par cité tel quel.** Enregistrer `map/point.c:last` par cité (8 octets) pour fermer la fuite. |
| `scenario_earthquake_process` (`scenario/earthquake.c:95-…`) | `data` (Global NE) ; `scenario.earthquake` ; `random_byte` ; détruit **tout** bâtiment sur le trajet (:69-80) ; recalculs d'images et de routage | **NON** | `data.delay++` est appliqué N fois, d'où un séisme N fois plus rapide. Le message ne va qu'à la cité qui le déclenche. | Jeu libre : **une seule fois (monde)**, dans le tour de la cité 0 (lire son générateur ne le perturbe pas), avec bascule vers le propriétaire pour chaque destruction et un message à chaque cité. Test jumeaux : `data` et `scenario.earthquake` par cité, faute de quoi chaque sauvegarde à séisme diverge. |
| `scenario_gladiator_revolt_process` (`scenario/gladiator_revolt.c:24-44`) | `data.state` (Global NE) ; `building_count_active` (cité) | **NON** | La cité 0 décide de l'état pour tout le monde (révolte ou « terminé » selon **ses** écoles). | **Par cité**, en enregistrant `data` (DESIGN §3.3 point 6). |
| `scenario_emperor_change_process` (`scenario/emperor_change.c:23-34`) | `data.state` (Global NE) | **NON** | Seule la cité 0 reçoit le message. | César : neutralisé en MP. Pour les jumeaux : enregistrer par cité. |
| `city_victory_check` (`city/victory.c:90-…`) | état de victoire (cité, enregistré) ; fenêtres | oui | retourne tout de suite en MP (`game_rules_is_multiplayer`) | Par cité tel quel. La victoire multijoueur est une tâche monde, en fin de tick (DESIGN §5.4). |

## 8. État statique non enregistré lu ou écrit par la simulation

Il s'agit des zones absentes de `player_context_init()` (`game/player_context.c:58-78`).

| Fichier:variable | Rôle | Portée recommandée |
|---|---|---|
| `game/time.c:3-9` `data` | calendrier | **par cité**, comme prévu (copies égales) ; ou global avec un découpage du tick (§9, variante) |
| `building/maintenance.c:30` `fire_spread_direction` | direction du feu, tirée chaque année | **par cité** |
| `figure/enemy_army.c:8` `enemy_armies[25]` | armées ennemies par invasion | par cité tant que les invasions sont par cité (jumeaux) ; monde ensuite (DESIGN §5.3) |
| `figure/enemy_army.c:10-17` `totals` (dont `days_since_roman_influence_calculation`) | forces en présence, cadence de l'influence | **par cité** |
| `map/soldier_strength.c:8` grille `strength` | influence romaine | **par cité** (`strength[p]`) : effacée et recalculée par cité |
| `scenario/invasion.c:86-89` `data` | alertes, dernier id d'invasion | par cité (jumeaux) ; monde en jeu libre |
| `scenario/earthquake.c` `data` | état du séisme | par cité (jumeaux) ou monde une seule fois (jeu libre) |
| `scenario/gladiator_revolt.c` `data` | révolte | **par cité** |
| `scenario/emperor_change.c` `data` | changement d'empereur | par cité (César neutralisé en MP) |
| `scenario/data.h:240` `scenario` : `requests[]`, `empire.is_expanded`, `invasions`, `earthquake`, `demand_changes`, `price_changes` | événements du scénario, en partie écrits | au minimum `requests[]` et `empire.is_expanded` **par cité** pour les jumeaux ; le reste est en lecture seule |
| `empire/city.c:18` `cities[]` | routes ouvertes, délais et ids des marchands, type (extension) | **par cité** jusqu'à M8 (D-019 décidera) |
| `empire/trade_route.c:10` `data` | quotas et quantités échangées | par cité (jusqu'à M8) |
| `empire/trade_prices.c:15` `prices` | prix | monde (D-019) ; par cité seulement pour le test jumeaux, si les sauvegardes ont des changements de prix |
| `empire/object.c:28` `objects` | carte de l'empire (drapeau « étendu ») | suit `cities[]` |
| `figure/trader.c:19` `data` (`traders[100]`, `next_index`) | cargaisons des marchands | par cité (ou tranches de 100) |
| `game/undo.c:27-35` `data` | annulation | monde, une seule fois ; inactif en réseau |
| `map/image_context.c:25-260` `current_item_offset` | rotation des variantes d'images | par cité si option A du §6 ; sinon monde |
| `map/point.c:3` `last` | dernier point calculé, relu par les tirs | par cité (fuite entre cités sinon) |
| `map/road_network.c:15` `network` | grille des réseaux routiers | monde (grille), calculée une seule fois |
| `map/routing.c:19-36` `routing_distance`, files | brouillons | recalculés avant chaque lecture dans un même appel : partagés |
| `map/water_supply.c:26-30`, `road_network.c:17-21` `queue` | brouillons | partagés |
| `map/routing.c` `stats` (sauvegardé, jamais lu) | compteurs de routes calculées | monde ; à exclure de la somme de contrôle |
| `building/house.c:48` `merge_data` | brouillon de fusion | partagé |
| `game/tutorial.c` `data` | tutoriels | inerte en MP |
| `sound/music.c` `data` | musique | interface, joueur local |
| `figure/figure.c:27-33`, `figure/formation.c:58` | repli sur `figures[0]` ou `formations[0]` quand la tranche est pleine | partagé : id 0 « poubelle », commun aux tranches. À remplacer par l'id local 0 de la tranche (`FIGURE_FIRST-1`, `FORMATION_BASE`) |

Les autres états de simulation repérés sont déjà enregistrés. Les tranches de bâtiments, figures, formations, chemins
et stockages (`building/storage.c:10-12`) sont en place.

## 9. Plan d'implémentation recommandé pour M3.6

Principe : **chaque cité déroule le tick d'origine dans son contexte.** La part « monde » d'une tâche s'exécute **une
seule fois, dans le tour de la cité 0, à la place exacte de la tâche d'origine**. Avec 1 joueur, l'ordre est donc
identique, et la parité tient par construction.

Pour les jumeaux, c'est exact : entre le tour de la cité 0 et celui de la cité k, rien ne touche à une cité non
branchée. Les commandes sont appliquées avant la boucle.

Pourquoi ne pas mettre la part monde dans une phase avant ou après toutes les cités : elle s'exécuterait avant
`random_generate_next` et `undo`, ou après les figures, ce qui change l'ordre d'origine.

### Étape 0 — Corrections préalables, chacune vérifiée par la parité

Elles ne changent rien avec 1 joueur.
1. Bornes de boucle :
   - `building/figure.c:1176` et `building/maintenance.c:183` : `i = BUILDING_FIRST` au lieu de `1` ;
   - `map/desirability.c:71-72` : `for (i = 1; i < BUILDING_ALL_END; i++)` (tâche monde).
2. Formations :
   - `figure/formation.c:482` : ne garder que `FORMATION_OWNER(f->formation_id) == player_context_current_player` ;
   - `figure/formation_legion.c:373` : `FIGURE_FIRST..FIGURE_END` ;
   - `figure/formation_legion.c:273` : `FORMATION_BASE + 1 .. + 6` ;
   - `figure/formation_enemy.c:594` et `figuretype/native.c:63` : `formation_get(FORMATION_BASE)` ;
   - `figure/formation.c:58` et `figure/figure.c:33` : repli sur l'id 0 local.
3. Divers :
   - `figuretype/water.c:305` (`figure_sink_all_ships`) : `FIGURE_FIRST..FIGURE_END` ;
   - `map/water_supply.c:40-42` : filtre `BUILDING_OWNER`.
4. Enregistrer par cité, avec un fichier `*_register_player_state` par module :
   - `game/time.c` ;
   - `fire_spread_direction` ;
   - `enemy_army.c` (`enemy_armies`, `totals`) ;
   - `scenario/invasion.c`, `earthquake.c`, `gladiator_revolt.c`, `emperor_change.c` (`data`) ;
   - les champs écrits de `scenario` (`requests`, `empire.is_expanded`) ou toute la structure ;
   - `empire/city.c`, `trade_route.c`, `object.c`, `figure/trader.c`, `map/point.c` ;
   - éventuellement `trade_prices.c` et `image_context`.

   Vérifier que `MAX_REGIONS 32` (`player_context.c:23`) suffit. Il y en a 16 aujourd'hui, il faudra en compter
   environ 30 : passer à 48.
5. Grille `strength` par cité : `strength[PLAYER_CONTEXT_MAX_PLAYERS]`, indexée par le joueur courant.

### Étape 1 — Découper `game_tick_run`

```c
void game_tick_run(void)
{
    if (editor_is_active()) { ... inchangé ... }
    mp_command_run_scheduled();               // once, switches context per command
    int n = player_context_num_players();
    int back = player_context_current();
    for (int p = 0; p < n; p++) {
        player_context_switch(p);             // no-op with 1 player
        city_tick_run(p == 0);                // the original tick, 'world' parts when first
    }
    player_context_switch(back);              // local player for the UI
    if (n > 1) { /* assert all calendars equal; MP victory; deferred autosave (M3.8) */ }
}

static void city_tick_run(int is_world_turn)
{
    random_generate_next();
    if (is_world_turn) game_undo_reduce_time_available();
    advance_tick(is_world_turn);              // flag passed down to day/month/year
    figure_action_handle();
    if (is_world_turn) scenario_earthquake_process();   // or per city for the twin test (§7)
    scenario_gladiator_revolt_process();      // per city (state registered)
    scenario_emperor_change_process();        // per city (state registered)
    city_victory_check();
}
```

### Étape 2 — Dans `advance_tick`, scinder les tâches « monde » à leur place

| Tick | Partie monde (`is_world_turn` seulement) | Partie par cité |
|---|---|---|
| 2 | — | `sound_music_update` seulement si `p == joueur local` |
| 3, 30 | `widget_minimap_invalidate` | — |
| 6 | `map_property_clear_all_native_land` | `map_natives_check_land_marks()`, la fonction moins l'effacement |
| 7 | `map_road_network_update_grid()` : grille plus liste (id, taille) en ordre de trame | `city_map_update_largest_road_networks()` : filtrer les réseaux de la cité, puis le classement d'origine |
| 27 | `map_terrain_remove_all(FOUNTAIN\|RESERVOIR)` et `set_all_aqueducts_to_no_water` | réservoirs et fontaines de la tranche, remplissage limité aux aqueducs du joueur |
| 37 | `map_desirability_update` (toutes les tranches) | — |
| mois | `map_tiles_update_all_roads`, `_water`, `map_routing_update_land_citizen` (option B du §6), ou leur version « zone » par cité (option A) | tout le reste |
| mois | `scenario_price_change_process` (prix monde), message dans chaque cité | — |
| année | `scenario_empire_process_expansion` (si l'empire est monde), message dans chaque cité | le reste |
| mois | autosauvegarde : à sa place si `n == 1`, sinon reportée après la boucle | — |

Toutes les autres tâches restent **par cité, telles quelles** : elles ont été vérifiées dans les tableaux des §3 à §5.

### Étape 3 — Tests

1. Parité : `tools/check.sh` doit être vert après chaque étape. Avec 1 joueur, `is_world_turn` est toujours vrai et il n'y
   a aucun échange.
2. Test minimal à 2 cités : la même sauvegarde est chargée dans les deux contextes, avec la cité 1 vide (aucun
   bâtiment). La cité 0 doit donner exactement la référence. Ce test détecte les fuites des §3 et §5 sans attendre l'outil
   de composition (M3.3).
3. Test jumeaux (M3.7) :
   - deux cités séparées (barrière d'eau ou de relief) ;
   - comparer chaque cité à sa référence en normalisant `road_network_id` ;
   - comparer `image_grid` seulement avec l'option A du §6.
4. Mesurer le coût des échanges : environ 60 Ko × 2 par cité et par tick, plus les tâches idem 19 et 21 (BFS ×N).

### Variante écartée : calendrier global

Le calendrier global, avancé une seule fois après la boucle, éviterait N copies. Mais `advance_tick` et `advance_month`
intercalent le changement de date **au milieu** des tâches d'une cité : `game_time_advance_month` au milieu du mois, et
les figures qui voient la nouvelle date. Il faudrait découper le tick en 5 phases synchronisées sur toutes les cités. Le
calendrier par cité, avec une assertion d'égalité en fin de tick, est plus simple et exact.

## 10. Ce qui n'a pas été vérifié

- Que les figures d'une formation sont toujours dans la tranche de cette formation (ennemis de `start_invasion`,
  `scenario/invasion.c:296-304` ; troupeaux et indigènes créés au chargement dans le contexte 0 : tous en tranche 0
  aujourd'hui, faute de tranche « monde »).
- `city_military_process_distant_battle` et `building_destroy_last_placed` (tranche et état global éventuels).
- Si la simulation relit des variantes d'images de route, d'eau ou de mur. Seules les images d'aqueduc le sont, de façon
  certaine.
- La liste exhaustive des cas où le routage est en retard sur le terrain (peste et émeutiers vérifiés ; ennemis :
  `building_destroy_by_enemy` recalcule, `destruction.c:227`).
- Les actions de figures, en détail : seules les boucles `FIGURE_ALL_END` ont été listées (`combat.c`, `wall.c`,
  `figuretype/maintenance.c:99` (préfets contre ennemis), `animal.c:453`). Elles sont voulues ou hors tick.
- Comment poser deux jumeaux complets sur une grille de 162 avant M3.1 à M3.3 : l'ordre M3.6 → M3.7 de la roadmap
  suppose une carte assez grande.
- Les effets d'interface de `city_message_post` dans le contexte d'une cité non locale (popup `message.c:191-197`,
  sons). Ils sont hors simulation, mais à filtrer sur le joueur local.
