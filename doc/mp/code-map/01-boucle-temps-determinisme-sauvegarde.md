# 01 — Boucle de jeu, temps, déterminisme, sauvegarde/chargement, harnais de tests

Cartographie en lecture seule de la branche `multiplayer`. Les lignes citées correspondent à HEAD `6566d085` (M0).
Parmi les fichiers cités, seuls `src/platform/julius.c`, `src/game/game.c` (ajout en fin de fichier) et
`CMakeLists.txt` (+1 ligne avant `CORE_FILES`) diffèrent de `upstream-base` (34d1ecd5).

Méthode : lecture du code, `ctest` (36/36 verts), puis expériences avec l'exécutable `autopilot` et de petits pilotes
écrits dans un dossier temporaire et liés aux objets de `build/test`. Ces expériences n'ont modifié ni `src/` ni `test/` (§3.6).
**[NON VÉRIFIÉ]** = déduit de la lecture, sans test.

---

## 0. L'essentiel

- **Boucle** : à chaque frame, `game_run()` exécute de 0 à 20 ticks, puis `game_draw()` dessine. C'est
  **dans le dessin** que l'interface applique directement les actions du joueur à l'état de simulation, entre deux lots de ticks.
- **Temps** : 1 jour = 50 ticks, 1 mois = 16 jours, 1 an = 12 mois (9 600 ticks par an). À la vitesse par défaut (90 %),
  un tick dure 22 ms. Le nombre de ticks d'une frame dépend de l'interface : fenêtre affichée, construction en cours de
  tracé, défilement de la carte, pause. Le jeu s'arrête dès qu'un écran autre que la ville est ouvert.
- **Ordre d'un tick** : tirage du générateur aléatoire, minuterie d'annulation, une tâche choisie selon le numéro de tick
  (de 0 à 49), avancée du calendrier (traitements du jour, du mois et de l'année), puis toutes les figures par id croissant,
  puis séisme, révolte, changement d'empereur et test de victoire (§2).
- **Pas de flottant, pas de `rand()`, pas d'horloge** dans la simulation. Un seul générateur aléatoire global (`core/random.c`).
  La sauvegarde n'en garde que **8 octets** (`iv1`, `iv2`). Les valeurs courantes, la réserve de 100 valeurs et son index ne sont pas sauvegardés.
- **De l'état caché influence la simulation** (§3.4). Démontré : charger **deux fois la même sauvegarde dans le même
  processus** donne des résultats différents pour 7 sauvegardes sur 9. Remettre 5 groupes de variables à leur valeur
  initiale supprime 100 % des écarts (§3.6).
- **Reprendre une sauvegarde ne revient pas à continuer la partie** : un nouveau processus diverge dans 8 cas sur 12. Les
  recalculs faits au chargement ne sont **pas** en cause (38 cas sur 39 identiques avec un rechargement dans le même processus).
- **Réglages propres à chaque machine qui changent la simulation** : difficulté et dieux (`c3.inf`), correctifs de gameplay (`julius.ini`).
  Avec la difficulté « très facile », 14 tests sur 16 échouent ; sans les dieux, 3 sur 16.
- **L'interface et le rendu écrivent dans l'état de simulation**, y compris dans des parties sauvegardées :
  aperçus de construction, effacement, animations de l'eau et des bâtiments, phrases des figures, orientation de la vue (§3.5).
- **Sauvegarde** : format Caesar III figé, 83 parties, 1 255 017 octets avant compression. Les tailles sont codées en dur pour une grille 162×162.
  Les coordonnées sont stockées sur `u8` et `grid_offset` sur `i16` : une grille ne peut pas dépasser 181×181 sans nouveau format.

---

## 1. Boucle principale et temps

### 1.1 Chaîne d'appels
- `main` (`src/platform/julius.c:650-679`) : `setup()` (584-648) appelle `game_pre_init()` via `pre_init`, puis
  `time_set_millis(SDL_GetTicks())` (639) et `game_init()` (641) ; ensuite un premier `run_and_draw()` (668), puis
  `while (!data.quit) main_loop();` (673-675). Sous Emscripten : `emscripten_set_main_loop(main_loop, 0, 1)` (671).
- `main_loop` (391-418) : vide la file d'événements SDL (`handle_event`, 398-400). Si `data.active` est vrai, appelle
  `run_and_draw()` ; sinon `SDL_WaitEvent(NULL)` (413-417), et la simulation s'arrête quand la fenêtre est cachée.
  Il n'y a **aucun limiteur de cadence** en dehors de la synchronisation verticale (`SDL_RENDERER_PRESENTVSYNC`, `platform/screen.c:158`).
- `run_and_draw` (211-226, et 178-209 avec `DRAW_FPS`) enchaîne `time_set_millis(SDL_GetTicks())`, `game_run()`, `game_draw()`,
  `platform_screen_update/render`. Depuis M0, en mode automatisé, l'horloge est virtuelle (`platform_automation_time()`, 16 ms par frame).
- `game_run` (`src/game/game.c:168-180`) :
  `game_animation_update()` ; `n = game_speed_get_elapsed_ticks()` ; puis pour chaque tick :
  `game_tick_run()` ; `game_file_write_mission_saved_game()` ; et sortie de boucle si `window_is_invalid()`.
  L'interface interrompt donc le lot dès qu'un message invalide la fenêtre.
- `game_draw` (182-186) : `window_draw(0)` puis `sound_city_play()`. `window_draw` (`graphics/window.c:122-140`) dessine le fond
  et le premier plan, **puis** `w->handle_input(m, h)` (136). C'est là que les actions du joueur modifient directement l'état.
- `game_pre_init` (game.c:48-63) : `settings_load` (`c3.inf`), `config_load` (`julius.ini`), `hotkey_config_load`,
  `scenario_settings_init` (lit la difficulté), `game_state_unpause`, `lang_load`, `random_init` (61).
- `game_init` (77-111) : charge les images, le modèle (`c3_model.txt`), le son, puis appelle `game_state_init()` (107), qui fait `random_generate_pool()`.

### 1.2 Du temps réel aux ticks (`src/game/speed.c`)
- `core/time.c:3-13` : `static time_millis current_time`, modifié **uniquement** par la plateforme ou par un pilote de test.
- `game_speed_get_elapsed_ticks` (`game/speed.c:24-83`) renvoie 0 dans chacun des cas suivants :
  - `game_state_is_paused()` (28) ;
  - fenêtre autre que `WINDOW_CITY`, `CITY_MILITARY`, `SLIDING_SIDEBAR`, `OVERLAY_MENU`, `MILITARY_MENU`, `BUILD_MENU` ou `EDITOR_MAP` (32-57) ;
  - vitesse inférieure à 10 ;
  - `building_construction_in_progress()` (58), c'est-à-dire un tracé de route, de maisons, etc. en cours ;
  - `scroll_in_progress() && !scroll_is_smooth()` (61).

  En revenant d'une autre fenêtre ou d'une pause, un tick est forcé (`last_check_was_valid`, 68-72).
  Sinon : `ticks = diff / millis_per_tick`, plafonné à `MAX_TICKS_PER_FRAME = 20` (10, 76-82). Au-delà, le temps est perdu.
- Vitesses (`setting_game_speed`, `game/settings.c:252-296`) : de 10 à 100 par pas de 10, puis 200, 300, 400, 500. La valeur par défaut est 90 (57).

| vitesse | ms/tick (`speed.c:12-17`) | ticks/s | durée d'un jour | d'un mois | d'un an |
|---|---|---|---|---|---|
| 10 % | 502 | 2,0 | 25,1 s | 6,7 min | 80 min |
| 50 % | 112 | 8,9 | 5,6 s | 90 s | 18 min |
| 90 % (défaut) | 22 | 45 | 1,1 s | 17,6 s | 3,5 min |
| 100 % | 16 | 62,5 | 0,8 s | 12,8 s | 2,6 min |
| 200 / 300 / 400 / 500 % | 8 / 5 / 3 / 2 | ≤ 20 par frame | | | |

L'éditeur tourne à `MILLIS_PER_TICK_PER_SPEED[7]` = 57 ms (55).
`core/speed.c` sert seulement au défilement : vitesse en `double`, dépendante du temps. Il est **exclu** de l'exécutable de test (`test/CMakeLists.txt:21`).

### 1.3 Calendrier (`src/game/time.c`)
`data = {tick, day, month, year, total_days}` (3-9) est sauvegardé sur 20 octets (73-89). Les fonctions d'avancée sont aux lignes 40-71 :
`tick` de 0 à 49, `day` de 0 à 15, `month` de 0 à 11, `year` (négatif pour les années av. J.-C.).
Un nouveau scénario démarre avec `game_time_init(scenario_property_start_year())` (`game/file.c:168`).
Le temps absolu vaut `tick + 50 * total_days` (`sav_compare.c:553-566`).

---

## 2. Ordre exact des mises à jour (`src/game/tick.c`)

### 2.1 Un tick (`game_tick_run`, 168-183)
0. Éditeur (170-174) : `random_generate_next(); figure_action_handle(); return;`.
1. `random_generate_next()` (175), le **seul** tirage systématique du tick.
2. `game_undo_reduce_time_available()` (176).
3. `advance_tick()` (117-166) : `switch (game_time_tick())` exécute **une** tâche (§2.2). Ensuite, si `game_time_advance_tick()`
   fait passer le compteur de 49 à 0, `advance_day()` est appelé, et donc les traitements du jour, du mois et de l'année
   s'exécutent **dans le même tick que la tâche 49**.
4. `figure_action_handle()` (178 ; `figure/action.c:111-133`) : `city_figures_reset()`, `city_entertainment_set_hippodrome_has_race(0)`,
   puis toutes les figures de `i = 1` à `MAX_FIGURES-1` (1000), **par id croissant**, et `figure_delete` des mortes.
5. `scenario_earthquake_process()` (179) ; 6. `scenario_gladiator_revolt_process()` (180) ;
   7. `scenario_emperor_change_process()` (181) ; 8. `city_victory_check()` (182), qui peut ouvrir des fenêtres.
9. Hors de `tick.c`, après chaque tick : `game_file_write_mission_saved_game()` (`game.c:174`, campagne).
   Attention : la commande `ticks N` de l'automatisation M0 appelle `game_tick_run()` directement
   (`platform/automation.c:353-361`), sans cette étape.

### 2.2 Les 50 tâches de la journée (`advance_tick`)
Portée : **C** = agrégats de la cité (`city_data`), **B** = boucle sur les bâtiments, **M** = carte entière, **W** = monde
(ennemis, troupeaux, indigènes), **UI** = interface ou son locaux, **R** = utilise le générateur aléatoire.

| tick | appel (ligne) | portée |
|---|---|---|
| 1 | `city_gods_calculate_moods(1)` (122) | C (dieux ; `setting_gods_enabled`) R |
| 2 | `sound_music_update(0)` (123) | UI |
| 3 | `widget_minimap_invalidate()` (124) | UI |
| 4 | `city_emperor_update()` (125) | César |
| 5 | `formation_update_all(0)` (126 ; `figure/formation.c:597-610`) | W : légions, ennemis, troupeaux |
| 6 | `map_natives_check_land()` (127) | W/M |
| 7 | `map_road_network_update()` (128) | M (grille `network`, non sauvegardée) |
| 8 | `building_granaries_calculate_stocks()` (129) | B/C (`non_getting_granaries`, non sauvegardée) |
| 10 | `building_update_highest_id()` (130) | tableau des bâtiments |
| 12 | `house_service_decay_houses_covered()` (131) | B |
| 16 / 17 / 18 | `city_resource_calculate_warehouse_stocks` / `..._food_stocks_and_supply_wheat` / `..._workshop_stocks` (132-134) | C/B |
| 19 | `building_dock_update_open_water_access()` (135) | B + routage sur l'eau |
| 20 | `building_industry_update_production()` (136) | B |
| 21 | `building_maintenance_check_rome_access()` (137) | B + routage depuis le **point d'entrée de la cité** |
| 22 | `house_population_update_room()` (138) | C/B : remplit la grande liste de `building/list.c` avec les maisons |
| 23 | `house_population_update_migration()` (139) | C : migration, mise à jour annuelle de la population, messages de population ; **lit la liste du tick 22** ; R |
| 24 | `house_population_evict_overcrowded()` (140) | B : **relit la liste du tick 22** |
| 25 | `city_labor_update()` (141) | C (`static start_building_id`) |
| 27 / 28 | `map_water_supply_update_reservoir_fountain` / `..._houses` (142-143) | M/B : 27 réécrit la grande liste avec les réservoirs |
| 29 | `formation_update_all(1)` (144) | W |
| 30 | `widget_minimap_invalidate()` (145) | UI |
| 31 | `building_figure_generate()` (146) | B → création de figures (R) |
| 32 | `city_trade_update()` (147) | C/empire : génération des marchands |
| 33 | `building_count_update(); city_culture_update_coverage()` (148) | C |
| 34 | `building_government_distribute_treasury()` (149) | C |
| 35 / 36 | `house_service_decay_culture` / `house_service_calculate_culture_aggregates` (150-151) | B / C |
| 37 / 38 | `map_desirability_update` / `building_update_desirability` (152-153) | M / B |
| 39 | `building_house_process_evolve_and_consume_goods()` (154) | B/C |
| 40 | `building_update_state()` (155) | tableau des bâtiments |
| 43 / 44 | `building_maintenance_update_burning_ruins` / `..._check_fire_collapse` (156-157) | B/M, R + `fire_spread_direction` |
| 45 | `figure_generate_criminals()` (158) | C R |
| 46 | `building_industry_update_wheat_production()` (159) | B |
| 48 | `house_service_decay_tax_collector()` (160) | B |
| 49 | `city_culture_calculate()` (161) | C |

Ticks sans tâche (commentaire ligne 119) : 0, 9, 11, 13, 14, 15, 26, 41, 42, 47.

### 2.3 Jour, mois, année
- **Jour** (`advance_day`, 106-115) : `game_time_advance_day()`, et si le mois change, `advance_month()`. Ensuite,
  si `day == 0 || day == 8` (donc **après** le traitement du mois), `city_sentiment_update()`. Enfin `tutorial_on_day_tick()`.
- **Mois** (`advance_month`, 71-104), dans cet ordre :
  `city_migration_reset_newcomers`, `city_health_update` (R), `scenario_random_event_process` (R),
  `city_finance_handle_month_change`, `city_resource_consume_food`, `scenario_distant_battle_process`, `scenario_invasion_process`,
  `scenario_request_process`, `scenario_demand_change_process`, `scenario_price_change_process`, `city_victory_update_months_to_govern`,
  `formation_update_monthly_morale_at_rest`, `city_message_decrease_delays`, `map_tiles_update_all_roads`, `map_tiles_update_all_water`,
  `map_routing_update_land_citizen`, `city_message_sort_and_compact`. Ensuite `game_time_advance_month()` :
  **tout ce qui précède voit encore l'ancien mois**. Si l'année change, `advance_year()` ; sinon `city_ratings_update(0)`.
  Puis `city_population_record_monthly`, `city_festival_update`, `tutorial_on_month_tick`, et **sauvegarde automatique au milieu du tick**
  (`game_file_write_saved_game("autosave.sav")`, 101-103) si `setting_monthly_autosave()`.
- **Année** (`advance_year`, 58-69) : `scenario_empire_process_expansion`, `game_undo_disable`, `game_time_advance_year`,
  `city_population_request_yearly_update` (le traitement réel a lieu au **tick 23** suivant), `city_finance_handle_year_change` (tribut),
  `empire_city_reset_yearly_trade_amounts`, `building_maintenance_update_fire_direction` (R), `city_ratings_update(1)`,
  `city_gods_reset_neptune_blessing`.
- **Autres cadences** :
  - influence romaine (`enemy_army_calculate_roman_influence`, `figure/enemy_army.c:88-96`) recalculée 1 fois sur 5 dans
    `formation_enemy_update`, qui tourne aux ticks 5 et 29, et seulement s'il y a des ennemis ;
  - annulation disponible pendant 500 ticks (`game/undo.c:164`).

### 2.4 Ordres internes qui comptent
- Figures : par id (`figure_action_handle`). Bâtiments : boucles de 1 à `MAX_BUILDINGS-1` (2000).
  `figure_create` et `building_create` prennent **le premier emplacement libre** (`figure/figure.c:27-32`, `building/building.c:57-62`).
  Dans `building_create`, ce choix dépend de l'annulation : les ids encore présents dans `game_undo_contains_building` sont sautés.
- Le générateur n'avance qu'**une fois par tick**, plus quelques appels explicites (§3.1). Les centaines de `random_byte()`
  d'un même tick lisent **la même valeur**.

---

## 3. Déterminisme

### 3.1 Le générateur aléatoire (`src/core/random.c`)
- État (7-16) : `uint32 iv1, iv2` ; `int8 random1_7bit, random2_7bit` ; `int16 random1_15bit, random2_15bit` ; `int pool_index` ; `int32 pool[100]`.
- `random_init` (18-23) : `memset` puis graines fixes `0x54657687` / `0x72641663` (appelé dans `game_pre_init`).
  `random_generate_next` (25-47) : 31 décalages de deux LFSR. La valeur **précédente** de `random1_7bit` est écrite dans
  `pool[pool_index++]` (anneau de 100). `random_generate_pool` (49-55) appelle 100 fois `generate_next` ; on y passe depuis
  `game_state_init` (`game/state.c:23`), donc au `game_init`, au démarrage d'un scénario (`clear_scenario_data`) et dans l'éditeur.
- Lecture : `random_byte()` (7 bits), `random_byte_alt()`, `random_short()` (15 bits), `random_from_pool(i)` = `pool[(pool_index+i)%100]`.
- **Sauvegarde** : `random_save_state`/`random_load_state` (77-87) ne gardent que `iv1` et `iv2`, dans la partie `random_iv` du `.sav` et du `.map`.
  Le chargement ne recalcule **ni** les valeurs courantes **ni** la réserve : `random_byte()` renvoie l'ancienne valeur jusqu'au prochain `generate_next`.
- Appels explicites à `random_generate_next` hors du début de tick : `figure/name.c:31`, `figuretype/animal.c:50,87` (poissons, troupeaux),
  `scenario/{request,invasion,demand_change,price_change}.c` (initialisation), `map/random.c:18` (`map_random_init`, 162×162 tirages).
  Utilisateurs de `random_from_pool` : `city/population.c:91,107` (`add_to_census`/`remove_from_census`, répartition des âges).
  `random_byte_alt` : `city/labor.c:104,116` (salaires de Rome).
  `random_byte` : une quarantaine d'appels dans `building/`, `city/`, `figure*/`, `scenario/`, `map/routing_path.c:155`.
  **`figure_create` lit `random_byte()` à chaque création** (`figure.c:51`).
- Au démarrage d'un scénario, `scenario_earthquake_init`, `scenario_gladiator_revolt_init` et `scenario_emperor_change_init` lisent
  `random_byte()` sans tirage préalable. Sur une carte sans point de pêche ni troupeau, la valeur lue vient donc de
  `map_random_init`, exécuté **avant** le chargement de la graine du `.map`. Elle dépend de l'historique du processus **[NON VÉRIFIÉ par un test]**.

### 3.2 Recherches qui n'ont rien trouvé dans le code de simulation
`building city empire figure figuretype game map scenario` + `core/{random,calc,buffer}.c` :
- **aucun** `float` ni `double`, pas de `<math.h>` (`core/speed.c` est hors simulation) ;
- **aucun** `rand`/`srand`/`time()`/`clock` ; `SDL_GetTicks` n'apparaît que dans `platform/` ;
- pas de tri par adresse ni d'adresse utilisée comme clé (`qsort` seulement dans `core/encoding*.c` et `core/dir.c`) ;
- pas de `char` nu dans l'état ;
- sérialisation petit-boutiste explicite (`core/buffer.c`) ;
- `building`, `figure`, `formation` et `city_data` sont **entièrement** sauvegardés (champs du `.h` comparés aux fonctions de sauvegarde).

`time_get_millis()` est lu par la simulation uniquement pour :
- les sons et popups (`city/message.c:94,158,445`) ;
- les avertissements (`city/warning.c`) ;
- l'orientation de la porte fortifiée ou de l'arc de triomphe pendant la pose, qui alterne toutes les 1,5 s
  (`building/construction.c:736-744`) : c'est **un paramètre de commande** ;
- l'ordonnanceur (`game/speed.c`) ;
- les animations (`game/animation.c`).

Point de vigilance pour Mac + PC (H10) : `buffer_read_i32` décale un `uint8` promu en `int` de 24 bits (`buffer.c:151`).
C'est un comportement indéfini en théorie, sans conséquence sur les compilateurs courants **[NON VÉRIFIÉ sous MSVC]**.

### 3.3 Paramètres propres à la machine qui changent la simulation
| Source | Valeur | Lue par | Mesure (16 tests de parité) |
|---|---|---|---|
| `c3.inf` → `setting_difficulty()` (`settings.c:342`) | défaut `DIFFICULTY_HARD` (60) | `game/difficulty.c` → `city/sentiment.c:179`, `figure/combat.c:75` (loups), `scenario/invasion.c:201`, `city/emperor.c:41`, `city/finance.c:144,188`, trésor de départ (`city/data.c:43,49`), faveur (`scenario.c:445,451`) | « très facile » : **14 sur 16 rouges** |
| `c3.inf` → `setting_gods_enabled()` (332) | 1 | `city/gods.c:205` | désactivé : **3 sur 16 rouges** |
| `julius.ini` → `CONFIG_GP_FIX_IMMIGRATION_BUG` | 0 | `city/sentiment.c:198` (seulement si le sentiment par défaut est inférieur à 50, donc en « très difficile ») | 0 sur 16 (cas non couvert) |
| `julius.ini` → `CONFIG_GP_FIX_100_YEAR_GHOSTS` | 0 | `city/population.c:275` | 0 sur 16 (cas non couvert) |
| `c3.inf` → `setting_monthly_autosave()` | 0 | `tick.c:101`, écriture de fichier en plein tick | — |
| `CONFIG_UI_VISUAL_FEEDBACK_ON_DELETE` | 0 | `construction_clear.c:55` (aperçu de l'effacement) | — |
| Fichiers de données | — | `c3_model.txt` (coûts, désirabilité, maisons), `c3.emp`/`c32.emp` (`empire/empire.c:33-57`), table des groupes d'images (`image_group`). La simulation **lit des identifiants d'image** : `map/water_supply.c:81,104`, `map/routing_terrain.c:89`, `map/natives.c:102,165`, `map/tiles.c` | à vérifier par une somme de contrôle dans le salon |

Ni la difficulté ni les dieux ne figurent dans le `.sav`.
Les tests dépendent donc du `c3.inf` présent dans `build/test`, réécrit à chaque sortie par `game_exit()` (§6.4).

### 3.4 État caché : variables `static` modifiées par la simulation et non sauvegardées
| Fichier:ligne | Variable | Rôle | Sauvée ? | Risque pour le lockstep et la reprise |
|---|---|---|---|---|
| `core/random.c:7-16` | `random1/2_*`, `pool[100]`, `pool_index` | valeur courante, âges du recensement | **non** (seulement `iv1`/`iv2`) | **ÉLEVÉ, démontré** : `city_data.population.at_age` diverge 57 ticks après un rechargement |
| `building/maintenance.c:30` | `fire_spread_direction` | direction de propagation du feu, tirée une fois par an (32-35), lue à 74-80 | **non** | **ÉLEVÉ, démontré** : sur `inv0`, divergence de bâtiments, figures et terrain |
| `map/image_context.c:13-19,336` | `current_item_offset` des tables `terrain_images_*` | rotation des variantes d'image (eau, murs, routes, aqueducs, relief, séisme) | **non** ; remis à 0 seulement par `map_image_context_init` (`file.c:128`, éditeur) | **ÉLEVÉ, démontré** (`image_grid`). Aussi avancé par les **aperçus** de routes et de murs (`map_tiles_set_road/wall`). Une partie de ces images est relue par la simulation |
| `map/soldier_strength.c:8` | grille `strength` | influence romaine pour l'IA ennemie et les troupeaux (`formation_herd.c:30`) | **non**, et **pas vidée au chargement** | MOYEN, démontré avec les deux suivants (compteurs de routage ennemi, `edge-start`) |
| `map/point.c:3` | `last` | « dernier résultat » global, relu par les tirs (`figuretype/enemy.c:77`, `soldier.c:100`) | **non** | MOYEN |
| `building/list.c:9-23` | `small.size`, `large.size` | liste des maisons du tick 22, réutilisée aux ticks 23 et 24 | les éléments oui, **les tailles non** (98-125) | MOYEN (sauvegarde faite entre les ticks 22 et 24) |
| `city/labor.c:312` | `static int start_building_id` (locale) | tourniquet des travailleurs du service des eaux | **non** | MOYEN, aucun effet observé dans 10 sauvegardes |
| `game/undo.c:26-34` | `data` (dont `buildings[50]`, `timeout_ticks`) | annulation ; influence le choix des ids dans `building_create` (`building.c:58`) | **non** (`game_undo_disable` au chargement) | **ÉLEVÉ en multijoueur** : un seul état global, piloté par l'interface |
| `map/road_network.c:15` | grille `network` | réseaux routiers (tick 7) | **non**, recalculée au chargement (`file.c:222`) | faible (au plus 1 jour) |
| `building/granary.c:20-27` | `non_getting_granaries` | greniers (tick 8) | **non**, recalculé (`file.c:224`) | faible |
| `map/routing_data.c:3-6` (globales) | `terrain_land_citizen/noncitizen/water/walls` | grilles de routage dérivées | **non**, recalculées (`map_routing_update_all`, `file.c:218`) | faible **[NON VÉRIFIÉ]** : une partie n'est mise à jour qu'une fois par mois (`tick.c:89`) |
| `figure/route.c:9-12` | emplacements de chemins | `figure_route_clean` les libère au chargement (`file.c:221`) | oui (nettoyés) | faible : 108 octets de `route_paths` observés |
| `map/routing.c:19-36` | `routing_distance`, `queue`, `water_drag`, `state` | brouillons | — | recalculés avant chaque lecture (appelants vérifiés). `stats` est sauvegardé mais jamais lu, et l'interface l'incrémente |
| `city/victory.c:16-19` | `state`, `force_win` | victoire | non | à refaire pour le multijoueur |
| `game/state.c:9-13`, `game/speed.c:19-22`, `game/cheats.c:10-12` | pause, couches d'affichage, ordonnanceur, triche | local | non | à transformer en commandes ou à supprimer |
| `building/construction.c:50-71`, `map/bridge.c:13-18`, `construction_clear.c:20-27` | construction en cours, longueur et direction du pont **calculées par l'aperçu** (`widget/city_building_ghost.c:540`), confirmations | paramètres de la pose | non | **ÉLEVÉ** : `map_bridge_add` (`bridge.c:193`) dépend de l'aperçu |
| `city/message.c:18-52` | `queue[20]`, `consecutive_message_delay`, `last_sound_time`, `problem_*` | popups et sons | non (`is_read` l'est) | interface |
| `sound/city.c:95,115` | `channels`, `last_update_time` | sons d'ambiance | `channels` oui (partie `city_sounds`) | dépend du rendu ; à exclure de la somme de contrôle |
| `city/view.c:17-38` | orientation et caméra | vue | oui | **orientation lue par la simulation** (§3.5) |

Variables statiques vérifiées et sauvegardées :
- tableaux : `formations`, `enemy_armies`, `figure/name.c`, `figure/trader.c`, `building/storage.c` ;
- compteurs : `building/count.c`, `city/culture.c` ;
- scénario : `scenario/*.c` (séisme, révolte, empereur, invasions, `max_game_year`) ;
- empire : `empire/{city,trade_prices,trade_route}.c`.

Les objets de l'empire sont rechargés depuis `c3.emp` à chaque chargement.

### 3.5 L'interface et le rendu modifient l'état de simulation
- **Aperçus de construction** (`building/construction.c` : `start` 374, `update` 436, `place` 556 ; `construction_routed.c` ; `construction_clear.c:46`) :
  - l'aperçu **écrit dans les vraies grilles** : terrain, images, aqueducs, bits « en construction » et « supprimé », maisons créées par `place_houses` ;
  - il restaure ensuite tout à partir des **copies de la grille entière** faites pour l'annulation (`game_undo_start_build`, `undo.c:95-121` ; `game_undo_restore_map`) ;
  - en mode « effacer » sans retour visuel, il passe les **vrais bâtiments** en `BUILDING_STATE_DELETED_BY_PLAYER` (`construction_clear.c:100-122`).

  C'est pour cela que la simulation s'arrête pendant un tracé (`speed.c:58`).
- **Rendu** :
  - l'animation de l'eau réécrit `image_grid` (`widget/city_without_overlay.c:57-66,134-141`, environ toutes les 60 ms) ;
  - `building_animation_offset` (`building/animation.c:10-120`), appelé depuis les widgets de la ville, écrit `sprite_grid`
    (partie sauvegardée ; la simulation n'y lit que les ponts, `routing_terrain.c:219`) ;
  - le retour visuel de l'effacement marque des bits dans `bitfields` (`city_building_ghost.c:773-787`).
- **Fenêtres** :
  - `figure_phrase_determine` écrit `phrase_id` et `phrase_sequence_*` des figures (`window/building_info.c:302`), qui sont sauvegardés ;
  - `city_message_post` (`city/message.c:166-199`) ouvre une popup si la vue de la ville est active, ce qui met la partie en pause et interrompt le lot de ticks ;
  - `city_victory_check` ouvre des fenêtres (`victory.c:106-140`).
- **Orientation de la vue** : `game_orientation_rotate_*` → `map_orientation_change` (`map/orientation.c:40-68`) réécrit les images,
  les bits de dessin, le routage des murs, et redirige les sentinelles et les chevaux. La **simulation lit `city_view_orientation()`** :
  - `routing_terrain.c:246-272` (adjacence des murs) ;
  - `figuretype/wall.c:58` (position des sentinelles) ;
  - `figuretype/animal.c:307-318` (chevaux de l'hippodrome) ;
  - pose de l'hippodrome, du fort, de la porte fortifiée et des ponts (`construction_building.c:55,512-541`, `bridge.c:201`) ;
  - projectiles (`missile.c:167`), images des figures (`figure/image.c:65`).

  Plusieurs sauvegardes de test sont en orientation 4 : `kknight*`, `valentia57*`, `brugle-lugdunum*`.

### 3.6 Expériences (reproductibles ; pilotes jetables, hors dépôt)
Pilote : environ 40 lignes de C, liées à tous les `.o` de `build/test/CMakeFiles/autopilot.dir` sauf `run.c.o`. Il enchaîne dans **un seul processus**
des triplets « charger X, exécuter N ticks via `game_run()`, sauvegarder Y ». L'horloge avance de 2 ms par appel à la vitesse 500, comme `run.c`.
Contrôle : le premier passage est identique octet pour octet à la sortie d'`autopilot`.
- **E1 : même sauvegarde deux fois dans le même processus.** 7 cas divergent sur 9 :
  - `tower` : 67 octets d'`image_grid` ;
  - `inv0` : 15 247 octets de `route_paths`, 11 260 de figures, 3 631 de bâtiments, plus terrain et désirabilité ;
  - `edge-start`, `kknight`, `valentia57`, `curses` : `image_grid` et `city_data` ; `earthquake` : `image_grid` seule ;
  - aucun écart pour `request_start` et `brugle-palacepeaks`.

  Remise à l'état d'un processus neuf **avant chaque chargement**, groupe par groupe :
  1. générateur (`random_init` + `random_generate_pool`) : supprime tous les écarts de `city_data` ;
  2. grille `strength`, `map_point last`, tailles des listes : supprime l'écart des compteurs de routage ennemi ;
  3. `start_building_id` : aucun effet ;
  4. `fire_spread_direction` : supprime la divergence massive d'`inv0` ;
  5. `map_image_context_init()` : supprime **tous** les écarts restants d'`image_grid`.

  Pour 3 et 4, j'ai lié des **copies modifiées** de `labor.c` et `maintenance.c`.
  Avec les 5 groupes, 10 sauvegardes sur 10 sont identiques, et la parité avec les sauvegardes originales est **conservée**.
- **E2 : reprise dans un nouveau processus** : N ticks d'affilée, comparés à K ticks, puis sauvegarde, puis nouvel `autopilot` pour les N−K ticks restants.
  Il y a un écart réel dans 8 cas sur 12 (K = 1, 23 ou 300) :
  - `edge-start` (K = 300) : 13 724 octets, dont figures et formations ;
  - `kknight`, `curses` (dès K = 1) ; `valentia57`, `inv0`, `request_start`, `db-fort2`, `massilia` ;
  - aucun écart pour `earthquake`, `palacepeaks` et les deux `lugdunum`.

  `tower`, testé à part, diverge pour K ≥ 100 : `at_age`, puis `image_grid`.

  Pour `tower` avec K = 100, la première divergence apparaît 57 ticks après le chargement (`at_age[25]` et `at_age[29]`).
  `routing_counters.total_routes_calculated` vaut toujours +1 après un chargement (`check_rome_access`).
- **E3 : même chose, mais rechargement dans le même processus.** 38 cas identiques sur 39. Seul écart : `inv0` avec K = 23 (108 octets de `route_paths`).
  **Ce sont les variables cachées non sauvegardées qui empêchent une reprise exacte, pas `initialize_saved_game`.**

---

## 4. Sauvegarde et chargement

### 4.1 Format `.sav` (`src/game/file_io.c`)
- 83 parties dans un ordre fixe (`init_savegame_data`, 219-311 ; liste des noms 84-168 ; au maximum `pieces[100]`, 170-174).
  Total de 1 255 017 octets avant compression. 27 parties sont compressées (1 204 382 octets), en « implode » PKWare DCL
  avec un dictionnaire de 4 096 octets (`core/zip.c`). Chaque partie compressée est précédée de sa taille sur `i32`, ou du marqueur `UNCOMPRESSED = 0x80000000` (52).
- Parties principales :
  - 14 grilles 162×162 : 4 en `u16` de 52 488 octets (images, bâtiments, terrain, figures) et 10 en `u8` de 26 244 octets
    (bords, aqueducs, bits, sprites, aléa par case non compressé, désirabilité, relief, dégâts, copies aqueducs et sprites) ;
  - `figures` : 128 000 = 1000 × 128 ; `buildings` : 256 000 = 2000 × 128 ; `route_paths` : 300 000 = 600 × 500 ;
  - `formations` : 6 400 = 50 × 128 ; `city_data` : 36 136 ; `messages` : 16 000 ; `city_sounds` : 8 960 ;
  - `scenario` : 1 720 ; `empire_cities` : 2 706 = 41 × 66 ;
  - `end_marker` : 284 octets écrits, 280 dans les fichiers de Caesar III : « la dernière partie peut être plus courte » (613-616).
- Versions : `SAVE_GAME_VERSION = 0x66` (54). La version lue (`savegame_version`, 347) n'est **jamais testée**.
  Il n'existe **aucune extension propre à Julius** dans cette base. Toutes les sauvegardes de test sont en 0x66.
- Piège pour un format plus gros : `write_compressed_chunk` renvoie 0 au-delà de `COMPRESS_BUFFER_SIZE = 600000` (51, 588-590),
  et `savegame_write_to_file` (621-631) ignore ce retour. **La partie est alors silencieusement absente du fichier.**
  Exemple : 4 × 2 000 bâtiments × 128 octets = 1 024 000 octets.
- Ordre de chargement : `savegame_load_from_state` (345-424). C'est un simple remplissage des tableaux par `*_load_state`.
  La plupart des valeurs dérivées sont recalculées ensuite (§4.4).

### 4.2 Format `.map` (scénario, `game_file_io_read_scenario`, 507-527)
10 parties non compressées (198-217) : images, bords, terrain, bits, aléa, relief, `random_iv` (8 octets), caméra (8), `scenario` (1 720), marqueur (4).
Total 211 692 octets. **Le `.map` fournit la graine du générateur** (`scenario_load_from_state`, 322).

### 4.3 Dépendances à 162×162 et à la largeur des champs
- `GRID_SIZE = 162` (`map/grid.h:9`). Tailles codées en dur : 26 244 et 52 488 (`file_io.c:207-243`), `ROUTE_OFFSETS = {-162, 1, 162, ...}` (`map/routing.c:17`).
  Dans `sav_compare.c` : ±162 (328, 346), `8*162*162` (485), `offset_tick = 1200222` (543, 555).
  Ailleurs : `window/message_dialog.c:619` (`< 26244`), `scenario/editor.c:35-36`, `map/terrain.c:339-345`.
- Champs du format et des structures : `building.x/y` et `figure.x/y`, `destination_*`, `source_*` sont des `u8` ;
  `grid_offset` est un `short` (`building/building.h:18-20`, `figure/figure.h:32-38`, `building_state.c:122-124`, `figure.c:165-176`).
  **Une grille ne peut pas dépasser 181×181** (32 767 cases) sans élargir ces champs, donc sans nouveau format.

### 4.4 Comment démarre une partie (`src/game/file.c`)
| Chemin | Appelant | Étapes |
|---|---|---|
| Mission de campagne | `window/mission_briefing.c:53` | `start_scenario` (278-309) → `load_campaign_mission` (258-276) : **charge une sauvegarde** cachée dans `mission1.pak`, à la position lue dans une table de 4 octets par mission (247-256). Puis `initialize_saved_game`, `city_data_init_campaign_mission` (trésor ajusté selon la difficulté) |
| Scénario personnalisé (« jeu libre ») | `window/cck_selection.c:271` (`scenario_set_custom(2)` en 74 ; **aperçu** `game_file_load_scenario_data` en 263) ; `widget/top_menu.c:390` (rejouer) | `load_custom_scenario` (191-201) : `clear_scenario_data` (93-130) remet tout à zéro (dont `game_state_init`, donc `random_generate_pool` et orientation 0, `figure_name_init` (R), `map_image_context_init`, `map_random_init` (R)), puis `game_file_load_scenario_data` (graine du `.map`, prix, empire), puis `initialize_scenario_data` (132-189) : images de tuiles, indigènes, poissons, troupeaux, épaves (R), routage, entrées et sorties, calendrier, événements (R), empire, invasions, demandes, menus, `city_data_init_scenario` (trésor selon la difficulté) |
| Partie commune à ces deux chemins (284-308) | — | nom du joueur, mission et rang, `scenario_settings_init_mission` (faveur selon la difficulté), `city_emperor_init_scenario`, `tutorial_init`, `building_menu_update`, `city_message_init_scenario` |
| Charger une sauvegarde | `window/file_dialog.c:266`, `autopilot`, automatisation | `game_file_load_saved_game` (350-360) : lecture, puis `initialize_saved_game` (210-245), `building_storage_reset_building_ids`, `sound_music_update(1)` |
| Éditeur | `window/main_menu.c:117` → `game_init_editor` (`game.c:136-152`) | `game_file_editor_clear_data` + `create_scenario(2)` (`file_editor.c:51-137`) ; chargement en 139-149 ; écriture en 151-162 |

`initialize_saved_game` **recalcule et modifie** une partie de l'état :
- empire (relu dans `c3.emp`), mois de trajet des batailles lointaines, `scenario_map_init`, `city_view_init` ;
- `map_routing_update_all`, `map_orientation_update_buildings`, `figure_route_clean`, `map_road_network_update` ;
- `building_maintenance_check_rome_access` (champs des bâtiments, +1 au compteur de routage), `building_granaries_calculate_stocks` ;
- `building_menu_update`, `sound_city_init`, `game_undo_disable` ;
- les indicateurs de tutoriel `fire` et `disease` passent à 1 dans `city_data` ;
- `city_military_determine_distant_battle_city`, `map_tiles_determine_gardens`.

E3 montre que ces recalculs sont pratiquement neutres sur les cas testés.
La sauvegarde **automatique mensuelle** est écrite au milieu d'un tick (après la tâche 49 et le changement de mois, mais avant les figures).
La recharger saute donc un `figure_action_handle`.

---

## 5. Annulation et autres états de `src/game/`
- **Annulation** (`game/undo.c`) :
  - un seul état global (26-34) ;
  - `game_undo_start_build` (95-121) refuse s'il existe déjà un bâtiment `BUILDING_STATE_UNDO`, sinon copie **les 5 grilles entières** (images, terrain, aqueducs, bits, sprites) ;
  - `game_undo_finish_build` (161-167) : 500 ticks ;
  - `game_undo_perform` (200-260) : rembourse (`city_finance_process_construction(-cost)`), restaure des grilles **entières**, rend le marbre ;
  - `game_undo_reduce_time_available` (262-318) tourne à chaque tick et l'annule dès qu'un séisme survient, qu'une maison se peuple ou qu'un bâtiment change ;
  - désactivations : chargement, changement d'année, rotation de la vue, destructions, évolution des maisons.

  En multijoueur, restaurer une grille entière effacerait les constructions des autres joueurs.
- **`state.c`** : `paused` et couches d'affichage. `game_state_init` (15-26) remet aussi l'orientation à 0, place la caméra en (76, 152) et appelle `random_generate_pool`.
- **`settings.c`** (`c3.inf`, 560 octets, 117-184) : vitesse, difficulté, dieux, sauvegarde automatique, nom du joueur, économies personnelles (campagne).
  **`difficulty.c`** : tables argent, ennemis, faveur et sentiment.
- **`animation.c`** : 51 minuteries de 20 ms (`game_animation_update` au début de `game_run`) ; ne sert qu'au rendu et aux animations de l'empire.
- **`cheats.c`** : triche locale (argent, victoire, invasion). **`mission.c`**, **`tutorial.c`** (indicateurs sauvegardés dans `tutorial_part1..3`),
  **`orientation.c`** (rotation de la vue) : campagne ou interface.
- **`core/config.c`** (`julius.ini`) : seules les clés `gameplay_fix_immigration` et `gameplay_fix_100y_ghosts` (`config.h:7-8`) touchent au gameplay.
  Les autres sont des options d'interface. `CONFIG_UI_ALLOW_CYCLING_TEMPLES` et `..._VISUAL_FEEDBACK_ON_DELETE` changent le déroulement d'une commande de construction.

---

## 6. Harnais de tests

### 6.1 `autopilot` (`test/sav/run.c`, `test/CMakeLists.txt`)
- `main` (71-86) prend `input`, `output`, `expected` et `ticks`.
  `run_autopilot` (37-69) : `game_pre_init`, `game_init`, `game_file_load_saved_game`, `run_ticks`, `game_file_write_saved_game`, `game_exit`, puis `compare_files(expected, output)`.
- `run_ticks` (27-35) : vitesse 500 et `time_set_millis(2*i)`, d'où **exactement un tick par `game_run()`** (2 ms par tick ; le premier tick est forcé).
- Fichiers liés (43-66) :
  - `core` sauf `image.c`, `lang.c` et `speed.c` ; `building` sauf `model.c` ;
  - `city`, `empire`, `figure`, `figuretype`, `game`, `map`, `scenario`, `sound`, `editor`, `platform/file_manager.c`.

  Cela **définit la frontière actuelle** : `city/view.c`, `city/warning.c`, `sound/*`, `editor/*` et `game/speed.c` sont du côté simulation.
- Bouchons (`test/stub/`) :
  - `ui.c` : `window_is` et `window_get_id` répondent `WINDOW_CITY` ; `window_is_invalid` renvoie 0. `window_victory_dialog_show` **joue** le choix
    « continuer 5 ans » (`city_victory_continue_governing(60)`) : c'est un bouchon qui change l'état. Il y a aussi les popups, la minicarte et `window_building_info_get_building_type` ;
  - `input.c` : `scroll_in_progress` vaut 0 ;
  - `image.c` : table figée `image_group` ; `image_get` renvoie 0 ;
  - `model.c` : `c3_model.txt` intégré au code ;
  - `lang.c`, `sound_device.c`, `video.c`, `log.c`.

  Appels de la simulation vers l'interface couverts par ces bouchons :
  `city/message.c` (popups), `city/victory.c`, `construction_clear.c` (confirmation), `game/cheats.c`, `game/speed.c`, `game/game.c` et `tick.c` (minicarte).
- 36 tests (78-132). `add_integration_test` (71-76) copie les `.sav` **au moment de la configuration CMake** (`file(COPY)`). La sortie s'appelle `<expected>-actual.sav`.
  Avec AppleClang, l'option `--coverage` (3-6) n'est pas activée, car le test vise `"Clang"`.

### 6.2 `sav_compare.c`
- `unpack` (299-323) décompresse toutes les parties sauf `end_marker`, qui n'est donc pas comparé.
  Sa table (20-237) découpe certaines parties en sous-champs nommés, ce qui rend les messages lisibles.
  `compare` (568-578) compare **octet par octet** toutes les parties.
- Exceptions (`is_exception`, 469-511) :
  - parties ignorées en entier : `city_sounds`, `sprite_backup_grid`, `camera` ;
  - `image_grid` : eau 364-369 (« dépend de l'animation »), tente en feu, routes contre une rampe ou un grenier ;
  - `sprite_grid` sur les cases occupées par un bâtiment ;
  - `city_data` : octet 35160, valeurs 7 et 8 échangées (paix et invasions) ;
  - bâtiments de type 99 (ruine en feu), octets 0x4A-0x73 ;
  - `building_grid` des cases touchées par un séisme ;
  - `building_list_burning_totals.size` si l'un des deux vaut 0 ;
  - destination des troupeaux hors de la carte.
- `compare_game_time` (553-566) **avertit seulement** si les dates diffèrent, sans faire échouer le test.
  L'exécutable `compare` (`sav/compare.c`) compare deux fichiers à la main.

### 6.3 Ajouter un test
1. Placer `entree.sav` et `attendu.sav` (produit par Caesar III ou par une version de référence) dans `test/data/`.
2. Calculer `ticks = (tick + 50*total_days)` de l'attendu moins celui de l'entrée. Ces valeurs sont dans la partie `game_time`, à la position 1 200 222 du fichier décompressé.
3. Ajouter `add_integration_test(sav_nom entree.sav attendu.sav ticks)`, reconfigurer, puis lancer `ctest -R sav_nom`.
   En cas d'échec : `build/test/compare attendu.sav attendu-actual.sav`.

### 6.4 Fragilités
- Les tests lisent `c3.inf` et `julius.ini` dans `build/test`, puis les **réécrivent** à la sortie. Ils supposent la difficulté HARD et les dieux activés (§3.3).
- Les tests ne sont valables qu'avec **un processus neuf par test**. Enchaîner plusieurs chargements dans le même processus fait diverger les résultats (E1, §3.6).
- La commande `ticks` de M0 ne suit pas la même logique que `run.c` (§2.1, point 9).

### 6.5 Pistes d'extension (complètent `doc/mp/TESTING.md` §4)
1. **Test d'idempotence dans le même processus** (pilote E1) : charger deux fois la même sauvegarde et exiger 0 écart.
   Il échoue aujourd'hui ; c'est le test de non-régression du chantier « état caché ». Le pilote fait environ 40 lignes ; il suffit de remplacer `run.c` par une boucle de triplets.
2. **Test de reprise exacte** (E2), une fois l'état caché ajouté au format multijoueur.
3. **Somme de contrôle par tick** : hachage des 83 parties **et** de l'état caché, en excluant ce qui dépend de l'interface :
   `city_sounds`, `camera`, `empire.scroll`, `sprite_grid` sur les bâtiments, images de l'eau, bits « en construction » et « supprimé »,
   `phrase_*`, `message.is_read`, `routing_counters`. Une trace par tick sert à trouver le premier tick fautif, par dichotomie comme en E2.
4. **Rejeu de commandes** : sauvegarde + liste `(tick, joueur, commande)` appliquée au début du tick, sorties comparées à une trace de référence.
   Exécuté aussi sur Windows x64 et Linux en intégration continue.

---

## 7. Implications pour le lockstep et pour plusieurs cités (par priorité)

**P0 : prérequis au lockstep**
1. **Une seule entrée de simulation** `sim_step(tick)` : appliquer les commandes prévues pour ce tick (avant `random_generate_next`), puis `game_tick_run()`,
   puis les actions post-tick (dont la sauvegarde automatique, à **sortir** du milieu du tick).
   Le nombre de ticks d'une frame vient du réseau, plus de `game_speed_get_elapsed_ticks`. Donc :
   - pas de pause liée à la fenêtre, au tracé ou au défilement ;
   - pas de `break` sur `window_is_invalid` ;
   - pas de perte de ticks au-delà de 20 par frame ;
   - le client continue quand la fenêtre est cachée.

   Vitesse et pause deviennent des commandes de l'hôte.
2. **Couche de commandes** : toute modification par l'interface (`handle_input`, popups de confirmation, triche) devient une commande
   **complète**. Elle porte l'orientation, `road_orientation`, `sub_type` (temples), les confirmations de fort et de pont ;
   la longueur et la direction du pont sont **recalculées par la simulation**. Ordre d'application déterministe : joueur, puis numéro.
3. **Aperçus et rendu en lecture seule** : couche fantôme côté client à la place de « écrire dans la carte puis restaurer ».
   Sortir de l'état partagé :
   - les animations (`image_grid` de l'eau, `sprite_grid` des bâtiments) ;
   - les bits « en construction » et « supprimé » ;
   - les phrases, les popups et `is_read`.
4. **Orientation** : fixer l'orientation de la simulation (par exemple nord) en multijoueur. La rotation devient purement visuelle, ce qui demande un travail sur le rendu.
   Le mode classique garde le comportement actuel (tests en orientation 4).
5. **Paramètres de partie synchronisés** : difficulté, dieux, `CONFIG_GP_*` choisis dans le salon et stockés dans la sauvegarde multijoueur, jamais lus dans `c3.inf` ou `julius.ini`.
   **Sommes de contrôle** des fichiers de données (`c3_model.txt`, `c3.emp`, `.sg2` et table `image_group`, `.map`).
6. **État complet dans le format multijoueur** :
   - générateur entier (`iv1`, `iv2`, valeurs 7 et 15 bits, `pool`, `pool_index`) ;
   - `fire_spread_direction`, `current_item_offset` des contextes d'image, grille `strength`, `map_point last`, tailles des listes ;
   - `start_building_id` (labor), annulation par joueur.

   En mode classique, une remise à l'état d'un processus neuf au chargement rendrait déjà les chargements indépendants de l'historique, sans casser la parité (E1).
7. **Sauvegardes et snapshots** uniquement entre deux ticks. Le salon démarre tous les pairs par le **même** chemin :
   `random_init` puis le chemin « scénario personnalisé », de façon à ne pas dépendre de l'historique (§3.1).

**P1 : plusieurs cités**

8. **Contexte de cité** : rendre multiples les singletons `city_data`, `buildings[]`, `figures[]`, `formations`, `storages`, listes,
   messages, comptages, couverture culturelle, annulation et **générateur par cité**. Garder un générateur et un état « monde » à part
   pour la carte, l'empire, les envahisseurs, les indigènes, les troupeaux et le calendrier.
   Le test d'isolement de TESTING.md exige qu'une cité ne voie ni les tirages ni l'ordre des ids des autres. Avec un générateur
   partagé, ses `random_byte()` dépendraient des tirages des voisins.
9. **Entrelacement par tick** (la table §2.2 indique la portée de chaque tâche). Ordre proposé :
   - commandes ;
   - pour chaque cité, dans un ordre fixe : tirage, minuterie d'annulation, tâche C/B du tick ;
   - tâches M et W une seule fois ;
   - jour, mois et année par cité, puis pour le monde ;
   - figures cité par cité (par id), puis figures du monde ;
   - événements, victoire.

   Les passes sur toute la carte (désirabilité, eau, routes) parcourent aujourd'hui tous les bâtiments : il faut les parcourir cité par cité dans un ordre fixe.
   Les points d'entrée et de sortie (tick 21) et `building_maintenance_check_rome_access` deviennent propres à chaque cité.
10. **Agrandir la grille** : nouveau format avec `u16` pour x et y, et un entier 32 bits pour la position. Vérifier la taille des parties
    (`COMPRESS_BUFFER_SIZE`, `pieces[100]`) et faire échouer explicitement l'écriture en cas d'erreur.

**P2** : le mode classique (une cité, César, format 0x66) doit rester **identique au bit près**. Chaque changement de P0 et P1 n'agit qu'en mode multijoueur, ou ne change rien pour un processus neuf.

---

## 8. Non vérifié et limites
- Toutes les mesures ont été faites sur **macOS arm64** avec l'`autopilot` de `build/`, compilé avant M0 ; M0 ne touche pas la simulation.
  Rien n'a été mesuré sous Windows ou Linux.
- Le jeu n'a pas été lancé : les effets du rendu (§3.5) et des aperçus viennent de la **lecture du code**.
- `start_building_id` (labor) et `map_point last` n'ont pas été isolés un par un. Leur effet n'est pas démontré séparément.
- Dépendance de `scenario_*_init` à l'historique du générateur, au démarrage d'une carte sans troupeau : déduite du code.
- Climats différents et versions différentes de Caesar III : les identifiants d'image seraient-ils identiques (table `image_group`) ? Non vérifié.
- Mises à jour de routage seulement mensuelles : effet sur une reprise non mesuré au-delà des 13 sauvegardes testées (E3).
- Comportement de Caesar III d'origine face au même état caché : non vérifié. La parité laisse penser qu'il se comporte comme Julius depuis un processus neuf.
