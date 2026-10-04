# 03 — Bâtiments, figures, carte et grilles

Périmètre : `src/building`, `src/figure`, `src/figuretype`, `src/map`, avec les points d'appui nécessaires dans
`src/city`, `src/game`, `src/scenario` et `test/sav`. Base : branche `multiplayer` = tag `upstream-base` (34d1ecd5).
Lecture seule, rien n'a été compilé ni exécuté. Les références `fichier:ligne` sont relatives à `src/` sauf mention
contraire. **[NV]** = non vérifié (détail au §7).

## 0. L'essentiel

1. Tout l'état de simulation tient dans des **tableaux statiques de taille fixe** : `all_buildings[2000]`, `figures[1000]`,
   600 routes × 500 pas, 50 formations, 200 stockages, etc. Il n'y a aucun tableau dynamique : pas de `src/core/array.h`
   dans ce Julius, et aucun `malloc` dans la simulation en dehors de `game/file_io.c` et `building/model.c`.
2. Une seule grille `GRID_SIZE = 162` (map/grid.h:9) contient la carte jouable (≤ 160×160), centrée, avec une bordure d'au
   moins 1 tuile. Cette bordure sert de garde-fou au routage par offsets.
3. Les types des champs plafonnent la taille de la carte. `x,y` sont en `unsigned char` (≤ 255). Surtout, `grid_offset` est
   en `short` dans `building` et `figure` (≤ 32767), donc **GRID_SIZE ≤ 181**, soit une carte d'environ 179² au plus.
   Viser 300 à 500 tuiles oblige à élargir les structs **et** le format de sauvegarde.
4. Le routage est un BFS sur une grille de distances **unique, globale et partagée** (`routing_distance`). D'autres fonctions
   relisent cette grille plus tard, sans la recalculer. Au-delà de 500 pas ou d'une distance ≥ 998, aucun chemin n'est rendu.
5. La simulation ne lit aucune notion de propriétaire. `faction_id` existe sur les bâtiments, les figures et les formations,
   et il est sauvegardé, mais il vaut toujours 1 (sauf pour les envahisseurs) et n'est jamais lu. On peut donc le réutiliser
   comme `owner` sans toucher au format legacy.
6. Routes, murs, aqueducs, jardins, places et ponts **ne sont pas des bâtiments** : ce sont des bits de `terrain_grid`. Leur
   donner un propriétaire demande une nouvelle grille par tuile (les grilles existantes n'ont plus aucun bit libre).
7. Les recherches de destination (marché, charrettes, entrepôts, sans-abri, préfets, émeutiers…) parcourent **tous** les
   bâtiments et ne filtrent que par `road_network_id`, distance et `distance_from_entry`. Si deux villes sont reliées par une
   route, leurs économies se mélangent.
8. Trois états d'interface agissent sur la simulation et bloquent le lockstep tant qu'ils ne sont pas découplés :
   l'orientation de la caméra (passabilité des murs, baliste, chevaux, route sous aqueduc), la prévisualisation de
   construction (qui écrit dans les grilles partagées) et l'undo (qui restaure des grilles entières).
9. Les tests autopilot comparent des sauvegardes au format C3 (grilles 162, 2000×128 o, 1000×128 o…), et
   `test/sav/sav_compare.c` code 162 en dur. Il faudra conserver un codec « legacy 162 ».

## 1. Bâtiments

### 1.1 Stockage, identifiants, cycle de vie

- `static building all_buildings[MAX_BUILDINGS]` (building/building.c:23), `#define MAX_BUILDINGS 2000` (building/building.h:7).
  L'id d'un bâtiment est son index ; le slot 0 est la sentinelle « aucun bâtiment » et n'est jamais parcouru (les boucles partent de 1).
- `building_create(type, x, y)` (building.c:54) prend le **premier** slot `BUILDING_STATE_UNUSED` à partir de 1 qui n'est
  pas dans le buffer d'undo (`game_undo_contains_building`, game/undo.c:73). Si tout est plein, elle appelle
  `city_warning_show(WARNING_DATA_LIMIT_REACHED)` et renvoie `&all_buildings[0]`. Certains appelants écrivent ensuite dans le
  slot 0 (par exemple les ruines dans destruction.c:79). Initialisations (building.c:72-167) : `faction_id = 1`,
  `created_sequence` (u16, compteur global), taille (`building_properties_for_type`), `house_size`, `subtype.house_level`,
  `output_resource_id`/`workshop_type`, grenier `resource_stored[RESOURCE_NONE] = 2400` (capacité libre), `grid_offset`,
  `house_figure_generation_delay = map_random_get(grid_offset) & 0x7f`, `figure_roam_direction = delay & 6`,
  `is_adjacent_to_water`.
- États (building/type.h) : `UNUSED 0, IN_USE 1, UNDO 2, CREATED 3, RUBBLE 4, DELETED_BY_GAME 5, DELETED_BY_PLAYER 6`.
  `building_update_state` (building.c:206, tick 40) fait passer CREATED à IN_USE et libère (`memset` dans `building_delete`,
  building.c:172) les slots UNDO, DELETED_* et RUBBLE. Elle relance ensuite les recalculs plein-carte : murs, aqueducs,
  `map_routing_update_land` et routes. Les ids sont réutilisés immédiatement.
- `extra.highest_id_in_use` n'est recalculé qu'au tick 10 (`building_update_highest_id`, building.c:293). Selon les boucles, on
  s'arrête à `building_get_highest_id()` (génération de figures, feu/effondrement, désirabilité, migrants) ou on va jusqu'à
  `MAX_BUILDINGS`.
- Bâtiments multi-parties, chaînés par `prev_part_building_id` / `next_part_building_id` (`building_main`, `building_next`) :
  un entrepôt occupe **9 slots** (1 + 8 `BUILDING_WAREHOUSE_SPACE`, construction_building.c:131-158), un hippodrome 3
  (:49), un fort 2 (fort + `BUILDING_FORT_GROUND`, :25). Un incendie crée **un `BUILDING_BURNING_RUIN` par tuile**
  (destruction.c:21-96) et la scission d'une grande maison en recrée jusqu'à 4 (house.c:260-363). La limite de 2000 se
  consomme donc bien plus vite que le nombre de bâtiments visibles.
- Routes, murs, aqueducs, jardins, places et ponts = bits de `terrain_grid`, sans enregistrement `building`. Sont en revanche
  des bâtiments : tours, portes (`BUILDING_GATEHOUSE`), réservoirs, fontaines, puits.

### 1.2 `struct building` (building/building.h:9-138), champs structurants

| Groupe | Champs (type) | Remarques MP |
|---|---|---|
| Identité | `id`, `state`, `type` (short), `subtype` (union `house_level`/`warehouse_resource_id`/`orientation`/…), `size`, `house_size`, `house_is_merged` | — |
| Propriétaire | `faction_id` (u8, toujours 1) | jamais lu, candidat `owner` |
| Position | `x`, `y` (**u8**), `grid_offset` (**short**), `road_access_x/y` (u8) | plafonds §4 |
| Réseau | `road_network_id` (**u8**), `has_road_access`, `distance_from_entry` (short, distance BFS depuis l'entrée) | dépend de l'entrée unique |
| Liens figures | `figure_id` (walker principal), `figure_id2` (recruteur ou acheteuse), `immigrant_figure_id`, `figure_id4` (baliste ou préfet sur une ruine), `formation_id` | §2.8 |
| Emploi | `num_workers`, `labor_category`, `houses_covered` (≤ 300), `percentage_houses_covered` | §2.6 |
| Risques | `damage_risk`, `fire_risk`, `fire_duration`, `fire_proof`, `ruin_has_plague` | §1.5 |
| Maison | `house_population`, `_room`, `house_highest_population`, `house_unreachable_ticks`, `house_tax_coverage`, `house_days_without_food`, `desirability` (i8), `sentiment.house_happiness` | — |
| Union `data` | `house` (inventaire[8] + 18 compteurs de couverture + agrégats), `market` (inventaire, `*_demand`, `fetch_inventory_id`), `granary.resource_stored[16]`, `industry` (`progress`, bénédiction/malédiction…), `dock`, `entertainment` | sérialisée selon le type |
| Stock | `loads_stored`, `storage_id` (**u8**), `tax_income_or_storage` | — |

Sauvegarde : 128 octets par bâtiment (`building_state_save_to_buffer`, building/building_state.c:114 ; union écrite selon le
type, :11-112). La pièce `buildings` fait 256000 octets (game/file_io.c:253), plus `highest_id`, `highest_id_ever`,
`sequence` et `corrupt_houses` (building.c:327).

### 1.3 Types et calendrier de mise à jour

`building_type` (building/type.h) compte 115 valeurs : maisons 10-29, spectacles 30-37, forts 40/44/45/57, temples 60-69,
marché et stockage 70-73, industrie 100-114, natifs 88/89/93, `BUILDING_BURNING_RUIN` 99. Une journée compte 50 ticks.
`advance_tick` (game/tick.c:117-166) appelle une seule fonction par numéro de tick, puis `figure_action_handle` s'exécute à
**chaque** tick (tick.c:178).

| Tick | Fonction | Tick | Fonction |
|---|---|---|---|
| 5/29 | `formation_update_all` | 25 | `city_labor_update` (city/labor.c:450) |
| 6 | `map_natives_check_land` | 27 | `map_water_supply_update_reservoir_fountain` |
| 7 | `map_road_network_update` | 28 | `map_water_supply_update_houses` |
| 8 | `building_granaries_calculate_stocks` | 31 | `building_figure_generate` (building/figure.c:1171) |
| 10 | `building_update_highest_id` | 33 | `building_count_update` + couverture culturelle |
| 12 | `house_service_decay_houses_covered` | 35/36 | décroissance / agrégats culture des maisons |
| 16-18 | stocks entrepôts / nourriture / ateliers (city/resource.c) | 37 | `map_desirability_update` |
| 19 | `building_dock_update_open_water_access` | 38 | `building_update_desirability` |
| 20 | `building_industry_update_production` | 39 | `building_house_process_evolve_and_consume_goods` |
| 21 | `building_maintenance_check_rome_access` | 40 | `building_update_state` |
| 22-24 | place / migration / expulsions des maisons | 43/44 | ruines en feu / `check_fire_collapse` |

Chaque mois, il faut ajouter `map_tiles_update_all_roads`, `map_tiles_update_all_water` et `map_routing_update_land_citizen`
(tick.c:87-89).

### 1.4 Maisons

- **Évolution** : `building_house_process_evolve_and_consume_goods` (house_evolution.c:506) appelle un callback par niveau
  (`evolve_callback`, :498). Chaque callback teste la désirabilité contre le modèle (`check_evolve_desirability`, :22) et
  les biens/services (`has_required_goods_and_services`, :43), avec un `devolve_delay`. La consommation a lieu les jours 0 et
  7. Les demandes sont agrégées dans `city_houses_demands()`, qui est **global à la cité**.
- **Désirabilité d'une maison** : `building_update_desirability` (building.c:254) prend le maximum de la grille de
  désirabilité sur l'emprise, ajoute +10 si le bâtiment touche l'eau et un bonus d'altitude.
- **Fusion et agrandissement** : `building_house_merge` (house.c:144), `building_house_can_expand` (:174) et
  `prepare_for_merge` (:99) absorbent les maisons voisines de même niveau **sans aucun test de propriétaire**.
- **Population** : au tick 22, `house_population_update_room` (house_population.c:70) remplit `building_list_large` avec
  les maisons et marque pour expulsion celles où `distance_from_entry == 0`. Suivent `house_population_update_migration`
  (:185, tick 23) et `house_population_evict_overcrowded` (:222, tick 24). `house_population_add_to_city` et
  `_remove_from_city` (:13, :39) parcourent les maisons en anneau depuis les curseurs globaux `last_used_house_add/remove`.
- **Services** : les walkers placent les compteurs de couverture à `MAX_COVERAGE = 96` (figure/service.c:10), et
  `house_service_decay_culture` (house_service.c:15) les décrémente d'un point par jour. `house_tax_coverage` est placé à
  50 puis décrémenté au tick 48. `house_service_calculate_culture_aggregates` (:67) utilise des moyennes **de cité**.

### 1.5 Incendies, effondrements, destruction

- `building_maintenance_check_fire_collapse` (building/maintenance.c:160, tick 44), pour chaque bâtiment non `fire_proof` :
  `damage_risk += 1`, ou `+= 3` si `(i + map_random_get(grid_offset)) & 7 == random_byte() & 7` ; sur le même tirage,
  `fire_risk` de +2 à +10 selon le niveau et le climat. Au-delà de 200 le bâtiment s'effondre
  (`building_destroy_by_collapse`), au-delà de 100 il prend feu (`building_destroy_by_fire`). Messages et sons (UI).
- `building_maintenance_update_burning_ruins` (:37, tick 43) : `fire_duration++`. Au-delà de 32 la ruine devient RUBBLE.
  La propagation est testée tous les 4 appels (désert) ou 8 (sinon), vers le bâtiment voisin dans la direction
  `fire_spread_direction` (±1). Elle ignore le propriétaire. `fire_spread_direction` est une **variable statique non
  sauvegardée** (:30), retirée chaque année.
- L'ingénieur remet `damage_risk` à 0 et le préfet `fire_risk` à 0, pour **tout** bâtiment dans un rayon de 2
  (`engineer_coverage` et `prefect_coverage`, figure/service.c:183-203). Le préfet se dirige vers une ruine en feu à
  distance ≤ 25 (`fight_fire`, figuretype/maintenance.c:163) et vers un ennemi à distance ≤ 30 (`get_nearest_enemy`, :95,
  qui parcourt toutes les figures).

### 1.6 Entrepôts, greniers, industries, marchés, modèle

- **Stockage** : `building_storage` (building/storage.c, `MAX_STORAGES 200`, référencé par `storage_id` en u8). Un espace
  d'entrepôt contient `loads_stored` ≤ 4 de `subtype.warehouse_resource_id`. Un grenier stocke ses ressources par unités de
  100 dans `data.granary.resource_stored[]`.
- **Greniers** : `building_granaries_calculate_stocks` (granary.c:169) construit la liste globale `non_getting_granaries`
  (`MAX_GRANARIES 100`, 98 utiles).
- **Industries** : `building_industry_update_production` (industry.c:37) fait `progress += num_workers` si
  `houses_covered > 0`. Le plafond est 200 pour une matière brute, 400 pour un atelier. Les bénédictions et malédictions
  divines (`building_bless_farms` et autres) s'appliquent à toute la cité.
- **Marchés** : `spawn_figure_market` (building/figure.c:583) gère le recruteur (si `houses_covered ≤ 50`), le marchand
  itinérant (`figure_id`) et l'acheteuse (`figure_id2`).
- **Modèle** : `model_load` (building/model.c:75) lit `c3_model.txt` (130 lignes de bâtiments, 20 de maisons) vers
  `model_building` (coût, désirabilité, `laborers`) et `model_house` (seuils, besoins, `max_people`, `tax_multiplier`).
  C'est une donnée globale en lecture seule, valable pour tous les joueurs. Les tests la remplacent par
  `test/stub/model.c` (test/CMakeLists.txt:22).

## 2. Figures

### 2.1 Stockage et cycle de vie

- `static struct { int created_sequence; figure figures[MAX_FIGURES]; } data` (figure/figure.c:15-18),
  `MAX_FIGURES 1000` (figure.h:9). Le slot 0 est la sentinelle.
- `figure_create` (figure.c:25) prend le premier slot avec `state == 0`. Si tout est plein, elle renvoie `&figures[0]`
  **sans avertissement** ; l'appelant écrit alors dans la figure 0 (par exemple `b->figure_id = 0`). À la création :
  `faction_id = 1`, `is_friendly = 1`, `created_sequence` (u16), `cross_country_x/y = 15·x/y`, `progress_on_tile = 15`,
  ajout à la grille des figures, et `trader_create()` pour les caravanes et les navires.
- `figure_delete` (figure.c:60) nettoie les références arrière dans le bâtiment d'origine (`figure_id`, `figure_id2`,
  `figure_id4`, `docker_ids`, `immigrant_figure_id`), le marchand de l'empire, la route et la grille, puis fait un `memset`.
- `figure_action_handle` (figure/action.c:111) parcourt les ids de 1 à 999 dans l'ordre et appelle
  `figure_action_callbacks[f->type]` (table de 80 entrées, action.c:28-109). Les figures DEAD sont supprimées dans la même
  passe. `city_figures_reset()` est appelé avant (compteurs **de cité**).
- 54 sites appellent `figure_create`, dont 32 dans building/figure.c. Le propriétaire pourra presque toujours se déduire du
  bâtiment source.

### 2.2 `struct figure` (figure/figure.h:11-117), 128 octets sauvegardés (figure.c:146)

| Groupe | Champs |
|---|---|
| Position | `x`, `y`, `previous_tile_x/y`, `source_x/y`, `destination_x/y` (**u8**), `grid_offset`, `destination_grid_offset` (**short**), `cross_country_x/y` (short = 15·x + décalage) |
| Mouvement | `direction` (0-7, ou 8 `AT_DESTINATION`, 9 `REROUTE`, 10 `LOST`, 11 `ATTACK`, core/direction.h), `progress_on_tile` (0..15), `routing_path_id/current_tile/length`, `terrain_usage`, `is_boat`, `use_cross_country`, `speed_multiplier`, `is_ghost` |
| Errance | `max_roam_length`, `roam_length`, `roam_choose_destination`, `roam_random_counter`, `roam_turn_direction`, `roam_ticks_until_next_turn` |
| Liens | `building_id`, `immigrant_building_id`, `destination_building_id`, `formation_id`, `index_in_formation`, `leading_figure_id`, `target_figure_id`, `targeted_by_figure_id`, `opponent_id`, `attacker_id1/2`, `trader_id`, `empire_city_id`, `next_figure_id_on_same_tile` |
| Camp / combat | `faction_id` (« 1 = city, 0 = enemy », jamais lu), `is_friendly`, `damage`, `num_attackers`, `action_state_before_attack` |
| Charge | `resource_id`, `loads_sold_or_carrying`, `collecting_item_id`, `migrant_num_people`, `min_max_seen` |
| Rendu (sauvegardé) | `image_id`, `cart_image_id`, `image_offset`, `phrase_*`, `name` |

### 2.3 Mouvement et routage d'une figure

- `walk_ticks` (figure/movement.c:214) fait `progress_on_tile++`. Au centre de la tuile (valeur 15), il enchaîne :
  `figure_service_provide_coverage(f)` (la couverture s'applique donc aussi pendant les trajets routés), `figure_route_add`
  si la figure n'a pas de chemin, calcul de la direction, puis `advance_route_tile` (:145). Ce dernier contrôle la
  passabilité selon `terrain_usage` : tuile bâtiment refusée sauf entrepôt, grenier, arc et terrain de fort ; porte refusée
  en errance ; un ennemi endommage l'obstacle (`building_destroy_increase_enemy_damage`, :183). Enfin `move_to_next_tile`
  (:80) met à jour x/y, `grid_offset` et la grille des figures, puis appelle `figure_combat_attack_figure_at`.
- `figure_route_add` (figure/route.c:47) prend un slot libre parmi `MAX_ROUTES 600` (le slot 0 est inutilisé), puis calcule
  un BFS adapté à `terrain_usage` :

  | `terrain_usage` | Calcul |
  |---|---|
  | `ROADS` | routes, jardins et terrains passables |
  | `PREFER_ROADS` | `ROADS`, puis terrain citoyen en repli |
  | `ANY` | terrain citoyen (soldats, migrants) |
  | `ENEMY` | non-citoyen limité à 5000 puis 25000 tuiles, puis « à travers tout » (:72-80) |
  | `ANIMAL` | non-citoyen limité à 5000 tuiles |
  | `WALLS` | chemin de ronde |
  | bateaux | BFS eau avec `GUARD 50000` |

  Ensuite, `map_routing_get_path` (map/routing_path.c:48) reconstruit le chemin à l'envers en suivant les distances
  décroissantes ; en cas d'égalité, il préfère la direction générale vers la source. Il renvoie 0 si la distance est ≤ 0 ou
  ≥ **998** (:52), ou si le chemin atteint **500** pas (`MAX_PATH`, :9 et :89). Si la table est pleine ou le chemin vide,
  la figure n'a pas de route et passe en LOST/REROUTE au pas suivant (`set_next_route_tile_direction`, movement.c:128).

### 2.4 Walkers de service : choix de route (errance)

1. **Apparition** : le walker apparaît sur la tuile renvoyée par `map_has_road_access` (map/road_access.c:30), qui préfère
   la route du plus grand réseau (`city_map_road_network_index`).
2. **Destination initiale** : `figure_movement_init_roaming` (movement.c:241) vise un point à 8 tuiles dans la direction
   `b->figure_roam_direction`. Cette direction tourne N→E→S→O (+2) à chaque nouveau walker du bâtiment. Le point est
   recalé sur la route la plus proche dans un rayon de 6 (`map_closest_road_within_radius`) ; si aucune route n'est
   trouvée, l'errance démarre aussitôt.
3. **Errance libre** : `figure_movement_roam_ticks` (:354). Au centre de chaque tuile, le walker lit les routes adjacentes
   (`map_get_adjacent_road_tiles_for_roaming`, road_access.c:263, portes exclues). Avec 1 route il la prend ; avec 2 il
   continue ou tourne selon `roam_turn_direction` ; avec plus de 2 il prend
   `(roam_random_counter + map_random_get(grid_offset)) & 6` en évitant le demi-tour ; avec 0 l'errance s'arrête. Des cas
   spéciaux gardent la ligne droite sur les routes doubles. Toute tuile `TERRAIN_ROAD` est empruntée, quel que soit son
   propriétaire.
4. **Retour** : `roamer_action` (figuretype/service.c:13) incrémente `roam_length` à chaque tick. Quand
   `roam_length ≥ max_roam_length`, le walker revient par un chemin routé vers une route à 2 tuiles de son bâtiment, puis
   meurt. Valeurs de `max_roam_length` : 96 (écolier), 128 (patricien), 192 (missionnaire), 384 (culture, recruteur,
   marchande), 480 (émeutier), 512 (percepteur, artiste), 640 (ingénieur, préfet), 800 (natif, acheteuse, sentinelle de tour).
5. **Validité** : à chaque tick, le walker vérifie `b->state == IN_USE && b->figure_id == f->id` (`figure_id2` pour le
   recruteur), sinon il meurt (figuretype/service.c:50-60, 150-160).

### 2.5 Application de la couverture (figure/service.c)

- `figure_service_provide_coverage` (:310) balaie une zone de **5×5 (rayon 2) autour de la figure** à chaque centre de
  tuile : `provide_culture` (:12, maisons habitées, compteur à 96) ; `provide_service` (:162, **tout** bâtiment :
  ingénieur, préfet, percepteur avec `house_tax_coverage = 50`) ; `provide_market_goods` (:278, distribution de
  l'inventaire du marché) ; `provide_missionary_coverage` (:144, rayon 4).
- Le nombre de maisons servies s'ajoute à `b->houses_covered` du bâtiment d'origine, plafonné à 300 (:433-439).
  `house_service_decay_houses_covered` le décrémente d'un point par jour (tick 12).
- Aucun test de propriétaire : un walker de A sert les maisons de B. Un marchand de A vide son stock chez B. Un percepteur
  de A active `house_tax_coverage` chez B.

### 2.6 Recruteurs de main-d'œuvre

- `spawn_labor_seeker` (building/figure.c:58) produit un recruteur si `houses_covered ≤ 50` (19 appels) ou `≤ 100` (6 appels).
  `generate_labor_seeker` (:39) exige `city_population() > 0` (**cité globale**) et utilise `figure_id2`.
- Le recruteur erre 384 ticks. Son callback de couverture ne fait rien, mais le nombre de maisons habitées croisées alimente
  `houses_covered`.
- `city_labor_update` (city/labor.c:450) range chaque bâtiment dans une catégorie (`CATEGORY_FOR_BUILDING_TYPE`, :27) ;
  `should_have_workers` (:144) exige `houses_covered > 0`, sauf pour l'ingénierie et l'eau. Le pool unique
  `city_data.labor` est réparti par priorité de catégorie, puis selon `percentage_houses_covered` (:288).
- `allocate_workers_to_water` (:310) conserve un `static int start_building_id` dans la fonction, **non sauvegardé**.

### 2.7 Charrettes, entrepôts, acheteuses

- **Charretier** : produit par `spawn_figure_industry` (building/figure.c:949) quand la production est faite.
  `figure_cartpusher_action` (figuretype/cartpusher.c:150) lit `road_network_id = map_road_network_get(f->grid_offset)`,
  puis `determine_cartpusher_destination[_food]` (:36, :89) essaie dans l'ordre `building_warehouse_for_storing`
  (warehouse.c:250), `building_granary_for_storing` (granary.c:215) et `building_get_workshop_for_raw_material_with_room`
  (industry.c:156). Filtres : même `road_network_id`, `distance_from_entry > 0`, `has_road_access`, 100 % de personnel
  pour un stockage, réglages de stockage. Coût : `calc_distance_with_penalty` (core/calc.c:38), soit la distance de
  Chebyshev plus l'écart absolu de distance à l'entrée.
- **Entrepositaire** : `figure_warehouseman_action` (cartpusher.c:408) appelle `building_warehouse_determine_worker_task`
  (warehouse.c:423) et `building_granary_determine_worker_task` (granary.c:131). Destinations : caserne **singleton**
  (`building_get_barracks_for_weapon` → `city_buildings_get_barracks()`, barracks.c:19), ateliers, greniers « getting »
  (granary.c:264, 309), autres entrepôts. `building_warehouse_for_getting` (warehouse.c:301) **ne filtre même pas par
  réseau routier**.
- **Acheteuse** : quand `figure_id2` est libre, `building_market_get_storage_destination` (market.c:65) parcourt tous les
  greniers et entrepôts `IN_USE` avec `has_road_access`, `distance_from_entry > 0`, même `road_network_id` et distance de
  Chebyshev < 40. Priorités : nourriture manquante, puis biens manquants (seulement si la demande, remise à 10 par les
  marchandes, n'est pas épuisée), puis plus petit stock < 50, puis nourriture < 600/400. L'acheteuse (`figure_market_buyer_action`, figuretype/market.c:102) se rend à l'accès routier du stockage, prélève les
  biens (`take_food_from_granary` :22, `take_resource_from_warehouse` :71) et rentre suivie de `FIGURE_DELIVERY_BOY`
  (`leading_figure_id`).

### 2.8 Liens figure ↔ bâtiment

| Bâtiment → figure | Figure → bâtiment / figure |
|---|---|
| `figure_id` : walker, charretier, entrepositaire, marchande… | `building_id` : bâtiment d'origine, vérifié à chaque tick |
| `figure_id2` : recruteur ou acheteuse | `destination_building_id` : stockage, salle de spectacle, cible |
| `figure_id4` : baliste ou préfet sur une ruine | `immigrant_building_id` : maison visée |
| `immigrant_figure_id` | `formation_id`, `leading_figure_id` (ânes, livreurs) |
| `data.dock.docker_ids[3]`, `trade_ship_id`, `data.industry.fishing_boat_id`, `formation_id` (fort) | `target_figure_id` + `target_figure_created_sequence`, `opponent_id`, `attacker_id1/2` |

La cohérence est vérifiée dans les deux sens. Le bâtiment utilise `has_figure_of_types` (building/figure.c:65). La figure
teste `b->figure_id != f->id` et meurt si le lien est rompu. Tous les ids sont stockés en `short`.

## 3. Carte et grilles

### 3.1 Géométrie

- `enum { GRID_SIZE = 162 }` (map/grid.h:9). Les grilles typées `grid_u8/i8/u16/i16` (grid.h:12-26) sont toutes des
  tableaux de `GRID_SIZE²` éléments.
- `map_data` (map/data.h) contient `width`, `height`, `start_offset` et `border_size`. Il est rempli par `map_grid_init`
  (scenario/map.c:9) à partir de `scenario.map`, sauvegardé en i32 (scenario/scenario.c:59-62).
- L'éditeur propose 40 à **160** tuiles de côté (`MAP_SIZES`, scenario/editor.c:14-21). La carte est centrée :
  `grid_start = (162-h)/2·162 + (162-w)/2` et `grid_border_size = 162-w` (editor.c:35-36). Une carte de 160 a donc une
  bordure d'une tuile.
- `map_grid_offset(x,y) = start_offset + x + y·GRID_SIZE` (grid.c:52). Le parcours canonique fait
  `grid_offset += border_size` à chaque ligne (par exemple routing_terrain.c:112). Hors carte, le terrain vaut `TREE|WATER`
  (`map_terrain_init_outside_map`, terrain.c:335) et les grilles de routage valent -1 (bloqué). Seule cette bordure
  empêche les offsets ±1 de « déborder » sur la ligne voisine : `map_grid_is_valid_offset` (grid.c:47) et
  `map_grid_add_delta` (:72) bornent par la **grille** et non par la carte.
- Tables d'offsets précalculées : `DIRECTION_DELTA` (grid.c:11), `ADJACENT_OFFSETS[1..5]` (grid.c:15), macros `OFFSET(x,y)`
  (grid.c:7, tiles.c:22, water.c:13, water_supply.c:19, house.c:15, editor/tool_restriction.c:9, trois widgets), `map_ring`
  calculée à l'initialisation (map/ring.c:51). Une seule exception est **écrite en dur** :
  `ROUTE_OFFSETS = {-162, 1, 162, -1, -161, 163, 161, -163}` (map/routing.c:17).

### 3.2 Inventaire des grilles (27, plus 6 pièces de scénario)

| Variable (fichier:ligne) | Type | Octets @162 | Sauvegarde | Rôle |
|---|---|---|---|---|
| `images` (map/image.c:5) | u16 | 52488 | `image_grid` | image par tuile ; lue aussi par la simulation (aqueducs : routing_terrain.c:87 ; route sous aqueduc) ; dépend de l'orientation |
| `images_backup` (image.c:6) | u16 | — | non | undo |
| `edge_grid` (map/property.c:31) | u8 | 26244 | `edge_grid` | x/y dans un multi-tuiles (6 bits), bit « tuile de dessin » (dépend de l'orientation), `EDGE_NATIVE_LAND` |
| `bitfields_grid` (property.c:32) | u8 | 26244 | `bitfields_grid` | taille multi-tuiles, `CONSTRUCTION` et `DELETED` (prévisualisation), alternate terrain, plaza/earthquake |
| `edge_backup`, `bitfields_backup` (property.c:34-35) | u8 | — | non | undo |
| `buildings_grid` (map/building.c:6) | u16 | 52488 | `building_grid` | id du bâtiment |
| `damage_grid` (building.c:7) | u8 | 26244 | `building_damage_grid` | dégâts ennemis |
| `rubble_type_grid` (building.c:8) | u8 | — | **non** | type ruiné (UI `building_info` seulement) |
| `terrain_grid` (map/terrain.c:7) | u16 | 52488 | `terrain_grid` | 16 bits `TERRAIN_*` (map/terrain.h), **tous utilisés** |
| `terrain_grid_backup` (terrain.c:8) | u16 | — | non | undo |
| `aqueduct` (map/aqueduct.c:11) | u8 | 26244 | `aqueduct_grid` | eau 0/1 **et** variantes d'image (double usage) |
| `aqueduct_backup` (aqueduct.c:12) | u8 | 26244 | `aqueduct_backup_grid` | undo, sauvegardé |
| `figures` (map/figure.c:5) | u16 | 52488 | `figure_grid` | tête de liste chaînée des figures (`next_figure_id_on_same_tile`) |
| `sprite` / `sprite_backup` (map/sprite.c:5-6) | u8 | 2×26244 | oui | frame d'animation ou pièce de pont |
| `random` (map/random.c:6) | u8 | 26244 | `random_grid` (non compressée) | aléa statique par tuile |
| `desirability_grid` (map/desirability.c:12) | i8 | 26244 | oui | −100..100 |
| `elevation` (map/elevation.c:6) | u8 | 26244 | oui | altitude 0..5 |
| `routing_distance` (map/routing.c:19) | i16 | — | non | distances du **dernier** BFS |
| `water_drag` (routing.c:32) | u8 | — | non | BFS bateaux |
| `terrain_land_citizen`, `_noncitizen`, `terrain_water`, `terrain_walls` (map/routing_data.c:3-6) | i8 | — | non (recalcul) | passabilité par usage |
| `network` (map/road_network.c:15) | u8 | — | non (tick 7 et au chargement) | id de réseau routier |
| `strength` (map/soldier_strength.c:8) | u8 | — | **non** | influence romaine, recalculée tous les 5 jours (figure/enemy_army.c:88) et lue entre-temps |

Fichier de scénario `.map` : `graphic_ids`, `terrain` (52488), `edge`, `bitfields`, `random`, `elevation` (26244)
(game/file_io.c:207-212), plus l'état du RNG (`random_iv`).

### 3.3 Routage (map/routing.c, routing_path.c, routing_terrain.c)

- **Algorithme** : BFS en 4 directions (`route_queue`, routing.c:57) ; 8 directions pour les épaves (`route_queue_dir8`),
  traînée de bord de carte pour les bateaux (`route_queue_boat`). La file circulaire `queue.items[MAX_QUEUE = GRID_SIZE²]`
  (:11, :26-30) est globale. Chaque appel commence par un `memset` de toute la grille `routing_distance`
  (`clear_distances`, :38) ; la source vaut 1, 0 signifie « non atteint ». Avec une destination, la recherche s'arrête quand
  celle-ci sort de la file ; sinon elle couvre toute la composante atteignable.
- **Passabilité** : quatre grilles reconstruites en plein-carte. Citoyen (routing_terrain.c:109) et non-citoyen (:174)
  sont recalculés à chaque destruction, incendie, agrandissement de maison et chaque mois ; eau (:210) ; murs (:274).
  `CITIZEN_0_ROAD` couvre routes, entrepôts, centre du grenier et **portes** (:35-38) ; `CITIZEN_2_PASSABLE_TERRAIN` les
  jardins, ruines, rampes et terrains de fort ; viennent ensuite `CITIZEN_4_CLEAR_TERRAIN`, `CITIZEN_N1_BLOCKED` et
  `CITIZEN_N3_AQUEDUCT`. Côté non-citoyen, `NONCITIZEN_4_GATEHOUSE` (la porte bloque les ennemis) et `NONCITIZEN_5_FORT`.
- **Grille partagée relue après coup** : `map_routing_distance()` (routing.c:499) renvoie le résultat du **dernier**
  calcul, quel qu'il soit. S'en servent sans recalculer : `building_maintenance_check_rome_access` (maintenance.c:226-305),
  `map_road_to_largest_network` (road_access.c:146), `map_closest_reachable_road_within_radius` (:123),
  `map_can_place_road_under_aqueduct` et `..._aqueduct_on_road` (map/road_aqueduct.c:45-84),
  `map_terrain_is_adjacent_to_open_water` (terrain.c:240), `map_soldier_strength_get_max` (soldier_strength.c:48) et le
  placement des formations ennemies (formation_enemy.c:323).
- **Compteurs** sauvegardés : `total_routes_calculated` et `enemy_routes_calculated` (routing.c:504).

### 3.4 Réseaux routiers, accès route, distance à l'entrée

- `map_road_network_update` (road_network.c:76, tick 7 et au chargement) parcourt toute la carte puis remplit par
  inondation (`mark_road_network`, :33, file circulaire `MAX_QUEUE 1000` sans détection de débordement). L'id est un
  `uint8_t` : au 256e réseau il retombe à 0, ce qui casse le marquage. Les 10 plus grands réseaux sont conservés dans
  `city_data.map.largest_road_networks[10]` (city/map.c:73).
- `map_has_road_access` (road_access.c:30) accepte toute route adjacente et préfère le plus grand réseau.
  `map_road_to_largest_network` (:146) exige en plus `routing_distance > 0`, c'est-à-dire une route atteinte depuis l'entrée.
- `building_maintenance_check_rome_access` (maintenance.c:223, tick 21) lance un BFS depuis `city_map_entry_point()`
  (entrée **unique**) et calcule `distance_from_entry`, `road_network_id` et `road_access_x/y` de chaque bâtiment. Une
  maison sans route dans un rayon de 2 voit ses habitants expulsés puis passe en UNDO. Si la sortie n'est plus atteignable,
  la fonction **supprime murs et aqueducs** sur le chemin (`map_routing_delete_first_wall_or_aqueduct`, 15 essais), puis
  appelle `building_destroy_last_placed()`. Elle appelle aussi `city_warning_show` et `city_view_go_to_grid_offset`
  (:336-338) : de l'UI dans la simulation.

### 3.5 Eau

- `map_water_supply_update_reservoir_fountain` (water_supply.c:146, tick 27) efface les bits
  `TERRAIN_FOUNTAIN_RANGE | TERRAIN_RESERVOIR_RANGE` sur toute la carte et passe tous les aqueducs « sans eau » (image et
  grille `aqueduct`). Un réservoir est plein s'il a de l'eau dans un carré 5×5 ; le remplissage se propage par les
  aqueducs vers d'autres réservoirs (`fill_aqueducts_from_offset`, :90, `MAX_QUEUE 1000`). Chaque réservoir plein ajoute
  `RESERVOIR_RANGE` (rayon 10) ; une fontaine avec employés posée sur cette portée ajoute `FOUNTAIN_RANGE` (rayon 4, 3 en
  désert).
- `map_water_supply_update_houses` (:47, tick 28) : une maison reçoit `has_water_access` si une tuile `FOUNTAIN_RANGE` touche
  son emprise. Les puits sont rangés dans `building_list_small` (limitée à 500, tronquée silencieusement) et marquent
  `has_well_access` sur **tout** bâtiment dans un rayon de 2.

Rien de tout cela ne tient compte du propriétaire : bits de terrain partagés, aqueducs interconnectables.

### 3.6 Désirabilité, grilles bâtiment/figure, propriétés, aléa

- **Désirabilité** : `map_desirability_update` (desirability.c:124, tick 37) remet la grille à zéro, ajoute pour chaque
  bâtiment des anneaux (`map_ring`, portée ≤ 6, paramètres du modèle), puis fait une passe plein-carte pour les places,
  jardins et ruines. Les anneaux débordent jusqu'à x = -1 et x = width (:22-46, avec un bug d'origine conservé, :37). C'est
  une donnée « physique » partagée entre voisins.
- **Grille des figures** : `map_figure_add`/`delete` (map/figure.c:24, :65) gèrent une liste chaînée par tuile, et
  `figures_on_same_tile_index` est plafonné à 20.
- **Aléa** : `map_random_init` (random.c:13) consomme le RNG global sur **les GRID² tuiles**. Il n'est utilisé que pour une
  nouvelle carte de l'éditeur ; les scénarios et les sauvegardes chargent la grille d'aléa.

### 3.7 Couplages UI → simulation (bloquants pour le lockstep)

- **Orientation de la vue** (`city_view_orientation()`), lue par la simulation pour : la passabilité du chemin de ronde
  (`count_adjacent_wall_tiles`, routing_terrain.c:246), la tuile de la baliste (figuretype/wall.c:58), les chevaux de
  l'hippodrome (animal.c:310), les routes sous aqueduc (road_aqueduct.c:38, :74), le placement des bâtiments
  (construction_building.c:55, :516 ; construction.c:495), le bit « tuile de dessin » et les images.
  `map_orientation_change` (map/orientation.c:40) recalcule les murs (`map_routing_update_walls`), redirige les sentinelles
  et les chevaux, et modifie les grilles d'images et de bords. Chaque pair aurait sa propre caméra : désynchronisation
  garantie si l'orientation n'est pas découplée.
- **Prévisualisation de construction** : `building_construction_start/update` sont appelées par widget/city.c:219-237 pendant
  le survol et le glissé. Elles écrivent dans `routing_distance` (construction.c:385-394, construction_routed.c:96, :159),
  dans les bits `CONSTRUCTION`/`DELETED`, dans les images d'aqueducs et dans le terrain mesuré. `building_construction_place`
  modifie la carte en dehors du tick.
- **Undo** (game/undo.c) : sauvegarde et restaure des grilles **entières** (image, terrain, aqueduct, property, sprite).
  `building_create` évite les ids présents dans l'undo (building.c:58).

## 4. Dépendances à la taille 162

### 4.1 Valeurs écrites en dur ou dérivées

| Où | Quoi |
|---|---|
| map/grid.h:9 | `GRID_SIZE = 162` : dimension de toutes les grilles, de `OFFSET`, de `MAX_QUEUE` du routage (routing.c:11), des boucles plein-grille (grid.c:180-260, random.c:16, terrain.c:341, city/view.c:119) |
| map/routing.c:17 | littéral `ROUTE_OFFSETS {-162, 1, 162, -1, -161, 163, 161, -163}` (le seul offset non dérivé de `GRID_SIZE`) |
| game/file_io.c:207-212, 230-243 | tailles de pièces 26244 / 52488 (scénario et sauvegarde) |
| test/sav/sav_compare.c:20-50, 328, 332, 346, 350, 485 | tailles de pièces, `-162/162`, `162*162`, `8*162*162` ; `GRID_SIZE` (:463) |
| city/view.h:7-8 | `VIEW_X_MAX 165`, `VIEW_Y_MAX 325` (table de vue isométrique, UI) ; graphics/screenshot.c:251 |
| scenario/editor.c:14-21 | tailles de carte proposées, de 40 à 160 |

### 4.2 Plafonds de types

| Champ (type) | Plafond | Où |
|---|---|---|
| `building.x/y`, `road_access_x/y` ; `figure.x/y`, `previous_tile_*`, `source_*`, `destination_*` (u8) | coordonnée ≤ 255 | building.h:18-19, :39-40 ; figure.h:32-43 |
| `formation.x/y/x_home/standard_x/destination_x` sauvegardés en u8 | ≤ 255 | figure/formation.c:629-636 |
| `city_data` : `entry_point`/`exit_point`/`senate_x,y` (u8), `barracks_x,y` et `distribution_center_x,y` (**int8**) | ≤ 255, voire **≤ 127** | city/data_private.h:28-40 ; city/data.c:120-128, 362, 445 |
| `building.grid_offset`, `figure.grid_offset`, `destination_grid_offset` (short) ; `*_grid_offset` de `city_data` (int16) ; `param2` des messages (i16, city/message.c:517) | offset ≤ 32767, donc **GRID_SIZE ≤ 181** | building.h:20, figure.h:38-41 |
| `routing_distance` (i16) | 32767, mais chemins refusés dès 998 (routing_path.c:52) et à partir de 500 pas | routing.c:19 |
| `network` et `building.road_network_id` (u8) | 255 réseaux (retour à 0 au-delà) | road_network.c:15, building.h:30 |
| `building.storage_id` (u8) | 255 stockages | building.h:134 |
| ids sauvegardés en i16 (`figure.building_id`, listes, `route_figures`, `formation.figures[]`) ; grilles u16 | id ≤ 32767 | — |
| `cross_country_x/y` (short = 15·x) | x ≤ 2184 | figure.h:67-68 |

### 4.3 Format de sauvegarde

- Les pièces ont des tailles fixes : grilles 162², 2000×128 o, 1000×128 o, 600×2 o + 600×500 o, 50×128 o, et le stockage
  200×32 o (6400 o au total).
- `COMPRESS_BUFFER_SIZE 600000` (game/file_io.c:51) : `read_compressed_chunk` et `write_compressed_chunk` **refusent** toute
  pièce plus grande (:569, :588). Une grille u16 dépasse cette taille dès GRID > 547 ; les bâtiments dès 4688 slots ; les
  chemins dès 1200 routes.
- Les tests rechargent des `.sav` d'origine et comparent octet par octet au format C3.

### 4.4 À changer pour des cartes de 300 à 500 tuiles

1. Fixer `GRID_SIZE` à au moins `largeur + 2`, soit par une constante plus grande à la compilation (504 ou 512 ; c'est le
   plus simple, `OFFSET` et `ADJACENT_OFFSETS` suivent), soit par une grille dynamique, qui obligerait à initialiser au
   démarrage toutes les tables `static const` d'offsets.
2. Réécrire `ROUTE_OFFSETS` à partir de `GRID_SIZE`, **dans le même ordre** : l'ordre conditionne les égalités du BFS.
3. Élargir les types : `x/y` en int16 ; tous les `grid_offset` en int32 (building, figure, `city_data`, messages, drapeaux) ;
   `road_network_id` et `network` en u16 ; `storage_id` en u16.
4. Rendre paramétrables les plafonds de recherche : `MAX_PATH`/`MAX_PATH_LENGTH 500` et le seuil 998, `GUARD 50000`
   (routing.c:12), `max_tiles` 5000/25000 (route.c:73-89), `MAX_QUEUE 1000` des réseaux et de l'eau. Il faut garder les
   valeurs d'origine en mode legacy : sur une carte de 160, un chemin tortueux peut déjà dépasser 500 pas, et ce
   comportement est celui de C3.
5. Créer un nouveau format de sauvegarde versionné (au-delà de 600 000 o par pièce) tout en **gardant un codec legacy 162**.
   À la conversion, il faut remapper chaque grille par (x,y) et non par offset, ainsi que tous les offsets sauvegardés :
   `building.grid_offset` (building_state.c:124), `figure.grid_offset` et `destination_grid_offset` (figure.c:171-174),
   entrée, sortie, sénat, caserne et centre de distribution (city/data.c:122-128, 364, 447), drapeaux (data.c:1013-1014),
   messages (`param2`). La bordure doit être reproduite (`TREE|WATER`, images de bord).
6. Adapter le format de carte et l'éditeur (pièces de 26244/52488 octets, `MAP_SIZES`), la vue isométrique et la minicarte
   (UI, hors périmètre).
7. Risque **[NV]** : sur une carte de 160 (bordure d'une tuile), une lecture à 3 tuiles ou plus hors de la carte « déborde »
   sur la ligne voisine dans la grille 162, mais tomberait dans la bordure avec une grille plus large. Exemple suspect :
   les `figure_offsets` de formation_enemy.c:314-315, protégés seulement par `map_grid_is_valid_offset`. Cela peut changer un
   résultat legacy.

### 4.5 Performance

- Nombre de tuiles par grille : 162² = 26 244 ; 302² = 91 204 (×3,5) ; 502² = 252 004 (×9,6).
- Chaque `figure_route_add` fait un `memset` de `routing_distance` (52 Ko à 162, 504 Ko à 502) puis un BFS jusqu'à la
  destination. La file seule pèse 1 Mo d'int à 502.
- Passes plein-carte quotidiennes : réseaux routiers (tick 7) ; BFS depuis l'entrée et boucle sur les bâtiments (tick 21,
  à multiplier par le nombre de joueurs s'il y a une entrée par joueur) ; eau (tick 27, `map_grid_and_u16` et
  `set_all_aqueducts_to_no_water`) ; désirabilité (tick 37) ; `map_routing_update_land` aux ticks 40/43/44, soit deux
  passes à chaque destruction. Chaque mois s'ajoutent routes, eau et passabilité. En 500², le coût quotidien est
  multiplié par environ 10, ce qui devrait rester acceptable en C **[NV, non profilé]**.
- Coûts quadratiques : O(B) par recherche (marché, charrettes, spectacles, docks), O(F) par figure (`get_nearest_enemy`,
  les 7 boucles de combat.c). Les termes O(F·B) et O(F²) croissent d'environ 16× avec 4 joueurs.
- Les plafonds `GUARD` et `max_tiles` tronquent les recherches sur une grande carte, ce qui change les décisions.

## 5. Limites techniques actuelles (aucune n'est dynamique dans Julius)

| Constante | Valeur | Où | Effet au dépassement |
|---|---|---|---|
| `MAX_BUILDINGS` | 2000 (slot 0 réservé) | building/building.h:7 | `WARNING_DATA_LIMIT_REACHED`, bâtiment 0 |
| `MAX_FIGURES` | 1000 | figure/figure.h:9 | figure 0 sans avertissement |
| `MAX_ROUTES` / `MAX_PATH_LENGTH` (`MAX_PATH`) | 600 / 500 (+ seuil 998) | figure/route.c:6-7, map/routing_path.c:9 | pas de chemin (LOST) ; testé par `sav_routing_full` **[NV]** |
| `MAX_FORMATIONS` / `MAX_LEGIONS` / `MAX_FORMATION_FIGURES` | 50 / 6 / 16 | figure/formation.h:7-10 | slots 1-9 pour les légions (`legion_id = id-1`), 10-49 pour les troupeaux et ennemis (formation.c:53, :79) ; 6 légions vérifiées à la construction (construction_building.c:581) |
| `MAX_STORAGES` | 200 | building/storage.c:7 | entrepôt ou grenier sans réglages |
| `MAX_TRADERS` | 100 (anneau) | figure/trader.c:7 | écrasement (statistiques) |
| `MAX_ENEMY_ARMIES` | 25 | figure/enemy_army.c:3 | — |
| `MAX_SMALL` / `MAX_LARGE` / `MAX_BURNING` | 500 / 2000 / 500 | building/list.c:5-7 | **troncature silencieuse** (puits, spectacles, maisons, réservoirs, ruines) |
| `MAX_GRANARIES` | 100 (98 utiles) | building/granary.c:14 | greniers ignorés |
| docks actifs | 10 | city/data_private.h (`working_dock_ids[10]`) | — |
| plus grands réseaux | 10 | data_private.h:366-369 | — |
| ids de réseau routier | 255 (u8) | road_network.c:15 | marquage cassé |
| files de réseau et d'eau | 1000 (circulaires) | road_network.c:11, water_supply.c:21 | débordement non détecté |
| `GUARD` / `max_tiles` | 50000 / 5000-25000 | routing.c:12, route.c:73-89 | recherche tronquée |
| `MAX_UNDO_BUILDINGS` | 50 | game/undo.c:24 | undo désactivé |
| `MAX_INVASION_POINTS` / `HERD` / `FISH` | 8 / 4 / 8 | scenario/data.h:14-16 | format de carte |
| `created_sequence` | u16 | bâtiment, figure | rebouclage |

## 6. Propriétaire (joueur)

### 6.1 Où le stocker

- **Bâtiments** : `building.faction_id` (u8, building.h:13). Il est sauvegardé (building_state.c:117), vaut toujours 1
  (building.c:73) et n'est jamais lu. Proposition : `owner = faction_id`, avec 1 à 4 pour les joueurs et 0 pour le neutre.
  Les sauvegardes legacy deviennent alors « joueur 1 » sans changement de format. **Piège** : les natifs sont créés par
  `building_create` et ont donc eux aussi `faction_id = 1` (map/natives.c:122).
- **Figures** : `figure.faction_id` (u8, figure.h:27, sauvegardé figure.c:160). Il ne vaut 0 que pour les envahisseurs
  (scenario/invasion.c:305) et n'est jamais lu. Il faut le propager depuis le bâtiment source dans les 54 appels à
  `figure_create` (immigrants : maison visée ; missiles : tireur ; animaux, épaves, envahisseurs : neutre).
- **Formations** : `formation.faction_id` (int, sauvegardé en u8, formation.c:617). Les slots de légions 1 à 9 ne suffisent
  pas pour 4 × 6 légions.
- **Tuiles** : il faut une **nouvelle grille** `grid_u8 owner`, car `terrain_grid` (16 bits), `bitfields` (8) et `edge` (8)
  sont pleins. Elle est indispensable pour les routes, murs, aqueducs, jardins, places et ponts, et utile pour une notion de
  territoire. L'analogue existant est `EDGE_NATIVE_LAND` (property.c).
- **État de cité** : `city_data` (city/data_private.h) est une structure globale unique : entrée et sortie, plus grands
  réseaux, singletons (sénat, caserne, hippodrome, docks), curseurs `last_used_*`, main-d'œuvre, population, finances,
  ressources. Elle doit passer à une instance par joueur ; presque tout le §6.2 en dépend.

### 6.2 Logiques à adapter (fichier:fonction)

| Domaine | Fonctions | Comportement actuel |
|---|---|---|
| Réseau, accès, entrée | map/road_network.c:76 `map_road_network_update` ; city/map.c:73 ; map/road_access.c:30 `map_has_road_access`, :146 `map_road_to_largest_network` ; building/maintenance.c:223 `check_rome_access` | réseaux et top 10 globaux, entrée et sortie uniques, destruction de murs et d'aqueducs |
| Errance | figure/movement.c:241, :354 ; road_access.c:263 | toute route est empruntable |
| Couverture | figure/service.c:310 et `provide_culture`/`provide_service`/`provide_market_goods`/`provide_missionary_coverage` (:12, :162, :278, :144) | sert les maisons de tout joueur |
| Main-d'œuvre | building/figure.c:39 `generate_labor_seeker` ; city/labor.c:144-450 ; house_population.c:166 `calculate_working_population` | pool et population uniques |
| Maisons, migrants | house.c:99, :144, :174 (fusion et agrandissement) ; figuretype/migrant.c:15 (immigrant depuis l'entrée), :61 `closest_house_with_room` ; house_population.c:13, 39, 70, 97, 222 | absorbe les voisines ; un sans-abri entre dans n'importe quelle maison |
| Eau | map/water_supply.c:47, :90, :146 | bits de terrain partagés, puits rayon 2 sur tout bâtiment |
| Stockage, commerce | building/market.c:65 ; warehouse.c:199, 217 (curseur global), 250, 301 (sans réseau), 423 ; granary.c:169, 215, 264, 309 ; industry.c:156, 194 ; barracks.c:19, 46, 74, 125 ; figuretype/cartpusher.c:36, 89, 276, 334 ; entertainer.c:16 ; docker.c:76, 142 ; figuretype/trader.c:218 ; map/water.c:211 ; building/dock.c:52 | filtre réseau et distance seulement |
| Feu, préfets | maintenance.c:37 (propagation), :106 (ruine la plus proche), :160 ; service.c:183-203 ; figuretype/maintenance.c:95, 163 | interventions entre joueurs |
| Combat, guerre | figure/combat.c:349 `figure_combat_attack_figure_at` (matrice de catégories `ARMED`/`HOSTILE`/…), :151 et suivantes ; routing.c:363-381 (`is_friendly`) ; routing_terrain.c:35-38 (porte passable pour tout citoyen) ; soldats en `TERRAIN_USAGE_ANY` | aucune hostilité entre deux « villes » |
| Émeutes, crime, invasions | figure/formation_enemy.c:139 (meilleur bâtiment de **toute** la carte), :172 ; figuretype/crime.c:110 | cible n'importe quel joueur |
| Agrégats de cité | count.c:51 ; city/resource.c:174-341 ; finance.c:166 `collect_monthly_taxes` ; health.c:54-101 ; sentiment.c ; culture.c:169 ; entertainment.c:53 ; ratings.c:453 ; government.c:7 ; house_service.c:67 ; house_evolution.c:506 ; building/figure.c:1173 (un patricien par tick pour toute la carte) | sommes sur tous les bâtiments |
| Désirabilité | desirability.c:124 | physique partagée (décision de conception) |
| Undo | game/undo.c:95, 73 | restaure des grilles entières |

### 6.3 Pièges

1. **Filtrer correctement** : pour les entités neutres qui doivent rester traitées (natifs, ruines, animaux), filtrer par
   « appartient à un autre joueur » et non par « appartient à moi ».
2. **Ordre et exactitude** : les égalités sont tranchées par l'ordre des ids (premier trouvé) et par des curseurs
   circulaires (`last_used_house_add`, `last_used_warehouse`, `start_building_id`). Partitionner les ids par joueur, par
   exemple `[p·2000+1, (p+1)·2000)`, et les routes de la même manière, reproduit exactement chaque cité, y compris le
   comportement quand le tableau est plein. Un tableau commun avec des compteurs par joueur ne le fait pas. Avec un seul
   joueur, la partition est l'original : les tests ne bougent pas.
3. **Raccordement de routes** : dès que deux villes se raccordent, `road_network_id` cesse d'isoler. Le filtre de
   propriétaire est indispensable dans les recherches du §6.2.
4. **États « froids »** : `routing_distance`, `map/point.c` (`last`, relu par enemy.c:77 et soldier.c:100) et la grille
   `strength` sont relus après coup. Toute écriture locale (UI) désynchronise. Il faut une grille séparée pour la
   prévisualisation, et des commandes de construction exécutées au tick.
5. **État non sauvegardé** : `fire_spread_direction` (maintenance.c:30), `start_building_id` (labor.c:312), `last`
   (point.c), `strength`, `rubble_type_grid`. Gênant pour une resynchronisation par sauvegarde et pour un checksum.
6. **Effets UI dans la simulation** : avertissements, messages, sons, déplacement de caméra (maintenance.c:336-338,
   building.c:64). Ils doivent être routés vers le seul propriétaire.
7. **Sentinelles partagées** : les bâtiments et figures 0 sont écrits lors d'un dépassement (destruction.c:79,
   building/figure.c:102). Ils sont communs à tous les joueurs.
8. **Guerre** : la porte est passable pour tout citoyen (soldats adverses compris), et seul l'usage `ENEMY` endommage les
   bâtiments. L'hostilité doit dépendre du propriétaire, pas du type de figure.

## 7. Non vérifié, questions ouvertes

**Non vérifié** :

- Lus partiellement : house_evolution.c (besoins par niveau), formation_legion.c, figuretype/soldier.c, enemy.c, docker.c,
  trader.c, crime.c, et city/*.c en dehors des extraits cités.
- Aucun profilage ni aucune exécution : les coûts du §4.5 sont des estimations.
- Contenu exact de `sav_routing_full` (saturation de la table de routes ?).
- Recensement exhaustif des lectures hors carte qui « débordent » (§4.4 point 7).
- Augustus (tableaux dynamiques via `core/array.h`) n'a pas été examiné.
- Sauvegarde ou non de `fire_spread_direction` par C3.

**Questions ouvertes** :

1. Routes, aqueducs et murs : propriété par tuile, ou infrastructure partagée parcourable par tous ?
2. Eau, désirabilité, incendies, ingénieurs et préfets : traversent-ils les frontières ?
3. Faut-il une entrée et une sortie par joueur dans le format de carte ? Que devient la règle « route vers Rome », qui
   détruit des murs ?
4. Limites par joueur : partition d'ids ou simples compteurs ? Combien de routes et de pas par joueur ?
5. Faut-il un `GRID_SIZE` fixe et plus grand avec un codec legacy 162, ou une grille dynamique ?
6. Faut-il une orientation de simulation canonique (le nord), la rotation ne touchant plus que le rendu ? Cela modifierait
   la passabilité des murs en solo si l'orientation sauvegardée n'est pas le nord : à arbitrer face aux tests.
7. Natifs, animaux et envahisseurs IA : sont-ils conservés comme entités neutres ?
