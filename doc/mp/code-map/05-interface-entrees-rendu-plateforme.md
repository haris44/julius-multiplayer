# 05 — Interface, entrées, rendu, plateforme

> Cartographie en lecture seule du code. **Les numéros de ligne renvoient au tag `upstream-base`
> (34d1ecd5).** Le commit M0 (`6566d085`, « headless automation driver ») est arrivé pendant la
> rédaction. Il a modifié `src/platform/julius.c`, `arguments.c/.h`, `src/graphics/screenshot.c/.h`,
> `src/game/game.c/.h` et `CMakeLists.txt`, et ajouté `src/platform/automation.c/.h`. Dans ces
> fichiers, les lignes de HEAD sont décalées (voir §1.4). Tous les autres fichiers sont identiques
> entre `upstream-base` et HEAD.
>
> Légende : **[SIM]** = état de simulation (doit être identique sur tous les PC en lockstep),
> **[LOCAL]** = état propre à un joueur ou à un écran, **[LOCAL-SAV]** = local mais écrit dans la
> sauvegarde.

## 0. Constats majeurs (résumé)

1. **La simulation est pilotée par l'UI.** `game_speed_get_elapsed_ticks()` (`src/game/speed.c:24`)
   renvoie 0 tick si la fenêtre courante n'est pas une vue de ville (l.32-57), si un glisser de
   construction est en cours (l.58) ou si un défilement non lissé est actif (l.61). `game_run()`
   (`src/game/game.c:168`) interrompt aussi la boucle de ticks dès que `window_is_invalid()` (l.176).
2. **Les actions de l'UI modifient l'état directement, entre deux ticks**, pendant `window_draw()`.
   Il n'existe aucune couche de commandes. Les seules exceptions sont les triches, exécutées dans la
   boucle d'événements SDL.
3. **L'orientation de la caméra (rotation) fait partie de l'état de simulation.** Elle déplace le
   point d'ancrage des bâtiments multi-tuiles (`construction_building.c:516`), décide de
   l'orientation de l'hippodrome, des forts, des ponts et des quais, et intervient dans le routage
   des murs (`map/routing_terrain.c:249`), les chevaux de l'hippodrome (`figuretype/animal.c:310`)
   et les balistes (`figuretype/wall.c:58`). `map_orientation_change()` réécrit des grilles entières.
4. **L'aperçu de construction écrit dans la vraie carte.** Il est ensuite restauré depuis les copies
   de `game_undo` (`game_undo_restore_map`). L'undo restaure des **grilles complètes**.
5. **Le rendu écrit dans l'état sauvegardé** (§2.5) : animation de l'eau dans `map_image`,
   sprites d'animation, `show_on_problem_overlay`, `building[0].type`, etc. Le placement d'un pont
   dépend même d'un calcul fait par le **rendu** du fantôme (`city_building_ghost.c:540`).
6. **Le module `building/construction.c` n'a qu'un seul état global** (`data`, l.50-69). Exécuter la
   commande d'un joueur distant écraserait le glisser en cours du joueur local.
7. **Pour l'automatisation**, le rendu est 100 % logiciel, dans un canvas `color_t` lisible
   (`graphics_canvas()`). Le temps est injectable (`time_set_millis`), le RNG est déterministe et
   les entrées peuvent être appelées directement. Piège : avec la souris en (0,0), le défilement par
   les bords **gèle les ticks** (§1.5).

---

## 1. Boucle plateforme (`src/platform/julius.c`)

### 1.1 Démarrage
`main()` (l.637) → `platform_parse_arguments()` (`arguments.c:62`), avec les options
`--display-scale`, `--cursor-scale`, `--display`, `--windowed`, `--fullscreen`, `--help` et le
dernier argument libre = dossier de données. Sur `__APPLE__`, Windows, Vita, Switch et Android, une
erreur d'argument **ne quitte pas** (l.641-644). Ensuite :
- `setup()` (l.571) : `signal(SIGSEGV)`, `init_sdl()` (l.407, `SDL_Init(AUDIO|VIDEO|JOYSTICK)`),
  `pre_init()` (l.494).
- `platform_file_manager_set_base_path()` (`file_manager.c:206`) fait un **`chdir`** dans le dossier
  C3. Tous les chemins relatifs (sauvegardes, `julius.ini`, `c3.inf`, `julius-hotkeys.ini`,
  captures, `autosave.sav`) se résolvent donc **dans le dossier de données**.
- `game_pre_init()` (`game.c:48`) : `settings_load`, `config_load`, `hotkey_config_load`,
  `lang_load`, `random_init` (graines fixes, `core/random.c:18`).
- `platform_screen_create()` (`platform/screen.c:100`), `system_init_cursors()`,
  `time_set_millis(SDL_GetTicks())` (l.626), `game_init()` (`game.c:77`, images, modèle, son,
  `game_state_init`, puis `window_logo_show()` l.108).
- `main` force ensuite `mouse_set_inside_window(1)` et `mouse_set_window_focus(1)` (l.649-650).
- Codes de sortie : -1 (SDL), 1 (pré-init ou arguments), -2 (fenêtre), 2 (`game_init`).

### 1.2 Boucle principale
`main_loop()` (l.378) : `while (SDL_PollEvent) handle_event()` (l.385). Si `data.quit`, appel de
`teardown()` (l.364 : `game_exit()` **sauvegarde `c3.inf` et `julius.ini`**). Sinon, si
`data.active`, appel de `run_and_draw()` (l.208), et sinon `SDL_WaitEvent(NULL)` (l.403, bloquant ;
`data.active` passe à 0 sur `SDL_WINDOWEVENT_HIDDEN`, l.266-268).

`run_and_draw()` (l.208-217) est la seule horloge. Elle exécute `time_set_millis(SDL_GetTicks())`,
puis `game_run()`, `game_draw()`, `platform_screen_update()` et `platform_screen_render()`.

`handle_event()` (l.273) répartit les événements :
- clavier → `platform_handle_key_down/up` (`keyboard_input.c:228/306`) ;
- texte → `platform_handle_text` (l.320, puis `keyboard_text`) ;
- souris → `mouse_set_position`, `mouse_set_left_down/right_down` et `mouse_set_scroll`, en
  **ignorant `which == SDL_TOUCH_MOUSEID`** ;
- doigts → `platform_touch_*` ; manettes → `platform_joystick_*` ;
- `SDL_QUIT` et `SDL_USEREVENT` : `USER_EVENT_QUIT/RESIZE/FULLSCREEN/WINDOWED/CENTER_WINDOW`,
  postés par `system_exit()` l.132, `system_resize()` l.137, etc.

Ordre dans une frame : (1) événements SDL appliqués à l'état d'entrée ; (2) `game_run()` exécute
0 à 20 ticks ; (3) `window_draw()` dessine **puis** traite les entrées, et les actions du joueur
s'exécutent à ce moment-là ; (4) présentation.

### 1.3 Cadence de simulation
- `game_run()` (`game.c:168`) : `game_animation_update()`, puis `num_ticks =
  game_speed_get_elapsed_ticks()` et, pour chaque tick, `game_tick_run()` +
  `game_file_write_mission_saved_game()`. La boucle s'arrête dès que `window_is_invalid()`.
  **Les ticks restants de la frame sont perdus**, car `last_update` a déjà été avancé
  (`speed.c:77`).
- `game_speed_get_elapsed_ticks()` (`speed.c:24`) compte les ticks seulement pour `WINDOW_CITY`,
  `CITY_MILITARY`, `SLIDING_SIDEBAR`, `OVERLAY_MENU`, `MILITARY_MENU` et `BUILD_MENU` ; l'éditeur
  tourne à 70 % pour les drapeaux. Pause : 0 tick. Délais par tick : `MILLIS_PER_TICK_PER_SPEED`
  (702 à 16 ms, vitesses 10 à 100 %) et `MILLIS_PER_HYPER_SPEED` (100 à 500 % : 16, 8, 5, 3 et
  2 ms). Plafond : `MAX_TICKS_PER_FRAME` = 20. Le premier appel valide force 1 tick.
- `game_tick_run()` (`tick.c:168`) appelle aussi l'UI : `widget_minimap_invalidate()` (ticks 3 et
  30) et `sound_music_update()` (tick 2). L'autosave mensuelle écrit `autosave.sav` depuis le tick
  (`tick.c:101-103`).
- Le harnais `test/sav/run.c` contourne l'UI : les stubs `test/stub/ui.c` et `input.c` renvoient
  `window_get_id() == WINDOW_CITY` et `scroll_in_progress() == 0`. Le harnais fait
  `time_set_millis(2*i)` avec une vitesse de 500 %, soit 1 tick par `game_run()`. **La liste des
  stubs donne exactement les symboles d'UI dont dépend la simulation** : `window_is`,
  `window_invalidate`, `window_get_id`, `window_is_invalid`, `window_message_dialog_show_city_message`,
  `window_popup_dialog_show`, `window_victory_dialog_show`, `window_mission_end_show_*`,
  `widget_minimap_invalidate`, `window_building_info_get_building_type`, `scroll_in_progress`,
  `scroll_is_smooth` et `mouse_reset_up_state`.

### 1.4 Où brancher l'automatisation (et ce que M0 a fait)
Points d'insertion les moins intrusifs (lignes `upstream-base`) :

| Besoin | Point d'accroche | Remarque |
|---|---|---|
| Horloge virtuelle | remplacer `time_set_millis(SDL_GetTicks())` (l.210, l.626) | Tout le code passe par `time_get_millis()` (`core/time.c`) : vitesse, double-clic, animations, répétitions des flèches. |
| Entrées synthétiques | avant `game_run()` dans `run_and_draw` | Soit `SDL_PushEvent`, soit des appels directs (§5.4). |
| Capture à la frame N | après `game_draw()`, avant `platform_screen_update()` (l.213-215) | Canvas complet, sans curseur. |
| Charger une partie au démarrage | après `setup()` (l.647), ou à la 1re frame | `game_file_load_saved_game(path)` (`file.c:350`) puis `window_city_show()`, comme `file_dialog.c:265-268`. Pour une carte `.map` : `scenario_set_custom(2)` puis `game_file_start_scenario(file)` (`file.c:330`) et `window_city_show()`, comme `cck_selection.c:74,269-273`. |
| Quitter après N frames | `system_exit()` (l.132) ou `SDL_QUIT` | `teardown` appelle `game_exit()`, qui réécrit `c3.inf` et `julius.ini` dans le dossier de données. |
| Ne pas bloquer | `data.active` / `SDL_WaitEvent` (l.400-404) | La fenêtre factice peut ne jamais envoyer `SHOWN`. |

**État au commit M0** (observé, non testé ici) : `--automation SCRIPT` (`arguments.c`). Le module
`src/platform/automation.c` fournit une horloge virtuelle de 16 ms par frame, injecte les entrées
via `SDL_PushEvent`, et propose les commandes `wait`, `mouse`, `click`, `rclick`, `drag`, `key`,
`text`, `load`, `save`, `ticks` (boucle directe sur `game_tick_run()`), `speed`, `screenshot`,
`cityshot` et `quit`. Il ajoute aussi `graphics_save_screenshot_to_file()` et
`game_exit_without_saving_settings()`. Ses accroches sont celles du tableau ci-dessus.

### 1.5 Pièges pour une exécution headless (déduits du code, non vérifiés à l'exécution)
- **Souris en (0,0) = défilement par les bords = simulation gelée.** `mouse` est statique (0,0) et
  `main` force `inside_window = focus = 1`. Dans la vue de ville, `scroll_map()` (`widget/city.c:242`)
  passe par `get_direction()` (`input/scroll.c:339`) avec `MOUSE_BORDER` = 5 et obtient
  `DIR_7_TOP_LEFT`, donc `is_scrolling = 1`. Avec `ui_smooth_scrolling=0` (cas du `julius.ini` du
  dossier de données), `game_speed_get_elapsed_ticks` renvoie 0. La caméra glisse en plus vers le
  coin haut-gauche. Parades : placer la souris au centre, ou `ui_disable_mouse_edge_scrolling=1`,
  ou avancer par `game_tick_run()` direct.
- Un événement poussé pendant `run_and_draw` n'est lu qu'au `SDL_PollEvent` de l'itération
  suivante (latence d'une frame).
- Double-clic : deux relâchements au même endroit à moins de 300 ms (`DOUBLE_CLICK_TIME`,
  `input/mouse.c:13`), soit environ 18 frames de 16 ms. Dans le dialogue de fichiers, un double-clic
  valide le fichier.
- Un clic droit sur la carte avec une alerte affichée ne fait qu'effacer l'alerte
  (`widget/city.c:197-200`).
- Les popups (messages, confirmations) remplacent `WINDOW_CITY` et **arrêtent les ticks** jusqu'à
  leur fermeture. `city_message_post()` affiche le popup seulement si `window_is(WINDOW_CITY)` ;
  sinon le message va dans une file (`city/message.c:190-195`). Des `game_tick_run()` en boucle ne
  reproduisent donc ni l'arrêt sur popup ni la coupure par `window_is_invalid`.
- `create_full_city_screenshot` modifie temporairement la résolution et la caméra, puis appelle
  `screen_set_resolution`, ce qui efface les alertes et invalide la fenêtre (perte de ticks à la
  frame suivante).

---

## 2. Rendu

### 2.1 Canvas et présentation
- `color_t` = `uint32_t` en 0xAARRGGBB (`graphics/color.h:6`). Constantes : `COLOR_*`,
  `ALPHA_OPAQUE` = 0xff000000.
- Canvas unique : `graphics.c:9-13`, alloué par `graphics_init_canvas()` (l.29) via
  `system_create_framebuffer()` (`platform/screen.c:446`, un `malloc`). Accès :
  `graphics_canvas()` (l.39) et `graphics_get_pixel()`. Le clip et la translation de dialogue
  utilisent `graphics_in_dialog()` / `graphics_reset_dialog()` (décalage `(w-640)/2`,
  `(h-480)/2` : `graphics/screen.c:21-22`).
- `screen_set_resolution(w,h)` (`graphics/screen.c:17`) réalloue le canvas, recalcule la vue
  (`city_view_set_viewport`), **efface les alertes** et invalide la fenêtre.
- Présentation : `platform_screen_resize()` (`platform/screen.c:197`) appelle
  `SDL_RenderSetLogicalSize` et crée une texture `SDL_PIXELFORMAT_ARGB8888` STREAMING de la taille
  logique. `platform_screen_update()` (l.395) fait `SDL_UpdateTexture(canvas, pitch = w*4)` puis
  `RenderCopy` ; `platform_screen_render()` (l.407) fait `RenderPresent`. Le renderer est demandé
  avec `PRESENTVSYNC`, avec repli logiciel (l.158-166). L'échelle d'affichage (`scale.percentage`)
  ne touche que SDL : le jeu ne voit que des coordonnées logiques.

### 2.2 Images, polices, texte
- Données : `core/image.c` lit les fichiers `.sg2`/`.555` d'origine (`C3.sg2`, variantes
  nord/sud, `C3map*` pour l'éditeur, ennemis, polices). Climat chargé par
  `image_load_climate(climate, is_editor, force)`. API : `image_group(GROUP_*)`, `image_get(id)`,
  `image_data(id)`.
- Dessin (`graphics/image.h`) : `image_draw`, `image_draw_masked` (masques `COLOR_MASK_RED/GREEN`
  pour le fantôme), `image_draw_isometric_footprint(_from_draw_tile)`,
  `image_draw_isometric_top(_from_draw_tile)`, `image_draw_fullscreen_background`,
  `image_draw_scaled_down`.
- Polices : `graphics/font.h`, 10 `font_t` (`FONT_NORMAL_PLAIN` … `FONT_NORMAL_BROWN`).
  `font_set_encoding()` gère les encodages CJK multi-octets. Texte : `graphics/text.c`
  (`text_draw`, `text_draw_number`, `text_get_width`), `lang_text.c` (chaînes du fichier
  d'origine `c3.eng` : `lang_text_draw(group, id, …)`), `rich_text.c` (messages).
- Widgets génériques : `generic_button` (déclenche sur `went_up`), `image_button`
  (`went_up`, ou défilement continu `IB_SCROLL`), `arrow_button` (déclenche sur `went_down`, avec
  répétition temporelle `REPEAT_MILLIS`), `scrollbar`, `panel`, `menu`, `tooltip`, `warning`.

### 2.3 Vue isométrique, caméra, orientation (`src/city/view.c`)
- Tuile : 60×30 px, demi-tuile 30×15. La vue utilise des coordonnées « view tile » `(x_view,
  y_view)` dans `[0,VIEW_X_MAX=165) × [0,VIEW_Y_MAX=325)` (`view.h:7-8`, « TODO get rid of these » ;
  soit `GRID_SIZE+3` et `2*GRID_SIZE+1`).
- Table `view_to_grid_offset_lookup[165][325]` (l.40), recalculée par `calculate_lookup()` (l.74)
  à chaque rotation. Une tuile dont `map_image_at() < 6` est marquée hors carte.
- Caméra : `camera.tile` (vue, y pair) + `camera.pixel` (l.22-25). Fonctions :
  `city_view_get/set_camera` (l.175/229), `city_view_set_camera_from_pixel_position` (l.247),
  `city_view_scroll` (l.259), `city_view_go_to_grid_offset` (l.343, centre la vue),
  `check_camera_boundaries` (l.42).
- Viewport : `x = 0`, `y = TOP_MENU_HEIGHT` (24), largeur `écran-160` (barre latérale dépliée) ou
  `écran-40` (repliée) (l.406-414).
- Orientation `data.orientation` ∈ {0, 2, 4, 6} (`DIR_0_TOP`… `DIR_6_LEFT`), modifiée par
  `city_view_rotate_left/right` (l.360/378). **Elle est écrite dans la sauvegarde** avec la caméra
  (`city_view_save_state`, l.465).
- Conversions :
  - écran → vue : `city_view_pixels_to_view_tile(x, y, &tile)` (l.287 ; 0 hors viewport) ;
  - vue → offset : `city_view_tile_to_grid_offset` (l.337, renvoie 0 pour « aucune ») ;
  - offset ↔ (x,y) : `map_grid_offset(x,y) = start_offset + x + y*GRID_SIZE`
    (`map/grid.c:52-65`) ;
  - offset → vue : `city_view_grid_offset_to_xy_view` (l.267, **recherche linéaire** sur
    165×325) ;
  - vue → écran : `city_view_set_selected_view_tile` (l.325) puis
    `city_view_get_selected_tile_pixels`.
  - Formule déduite (non testée) du **centre** de la tuile `(xv, yv)` :
    `X = 60*(xv-cam.x) - cam.px + 30 - (yv impair ? 30 : 0)`,
    `Y = 24 + 15*(yv-cam.y) - cam.py`.
- Chaîne de saisie de la tuile courante : `update_city_view_coords()` (`widget/city.c:173`) remplit
  `map_tile {x, y, grid_offset}`.

### 2.4 Passes de rendu de la ville
`widget_city_draw()` (`widget/city.c:47`) appelle `city_with_overlay_draw()` si
`game_state_overlay()`, sinon `city_without_overlay_draw()` (`city_without_overlay.c:505`). Ordre :
1. `city_building_ghost_mark_deleting` ;
2. `city_view_foreach_map_tile(draw_footprint)` (`view.c:498`) ;
3. par rangée : `draw_top`, `draw_figures` et `draw_animation` (`foreach_valid_map_tile_row`) ;
4. fantôme `city_building_ghost_draw` (`city_building_ghost.c:790`) ;
5. figures surélevées et ornements de l'hippodrome. Le mode suppression utilise trois passes
   `deletion_*`.

Les overlays sont dans `city_overlay_*.c`. Le rendu dépend de `GRID_SIZE` par la macro `OFFSET`
(`city_without_overlay.c:30`, `city_with_overlay.c:33`, `city_building_ghost.c:48`) ; la minimap
(`widget/minimap.c:81`) et `scenario_minimap.c:89` dépendent de `VIEW_X_MAX/Y_MAX`.

### 2.5 Écritures dans l'état « simulation » faites par le rendu ou les fenêtres
Ces écritures portent sur des données **sérialisées** (sauvegarde). Elles divergeraient entre PC
selon ce que chacun regarde : à exclure de toute somme de contrôle, ou à déplacer côté rendu.

| Écriture | Où | Donnée |
|---|---|---|
| Animation de l'eau (+1 image toutes les 60 ms, horloge) | `city_without_overlay.c:134-141` | `map_image_set` (grille d'images) |
| Sprites d'animation des bâtiments | `building/animation.c:33-120` (appelé par le rendu) | `map_sprite_animation_set` |
| Bâtiment 0 utilisé comme « jardin » | `city_without_overlay.c:126-127` | `building_get(0)->type` |
| Overlay « problèmes » | `city_overlay_risks.c:29-39` (via `city_with_overlay.c:377`) | `b->show_on_problem_overlay` |
| Marquage de suppression (clic gauche sur « dégager ») | `city_building_ghost.c:784-786` | bits `property` constructing/deleted |
| Effacement de sprite de pont hors eau | `widget/city_bridge.c:11` | `map_sprite_clear_tile` |
| Pont : longueur et direction calculées par le fantôme | `city_building_ghost.c:540` → `map/bridge.c:30` | struct statique `bridge`, **lue par `map_bridge_add`** (`bridge.c:193`) |
| Bascule de l'orientation des portes et arcs (toutes les 1,5 s) | `city_building_ghost.c:274` → `construction.c:736` | `data.road_orientation`, **lue au placement** |
| Coût prévisionnel | `city_building_ghost.c:581,594` | `building_construction_set_cost` |
| Animation de la carte de l'empire | `window/empire.c:375` → `empire/object.c:372` | `obj.animation_index` |
| Phrase d'une figure (panneau d'info) | `window/building_info.c:302` → `figure/phrase.c:630` | `phrase_id`, `phrase_sequence_*` |
| Texte d'évolution d'une maison (panneau d'info) | `building_info.c:241` | `data.house.evolve_text_id` |

### 2.6 Captures d'écran (`src/graphics/screenshot.c`, `upstream-base`)
- API : `graphics_save_screenshot(int full_city)` (l.280). Déclenchée uniquement par les hotkeys
  globales (`hotkey_handle_global_keys`, `input/hotkey.c:462-467`) : F12 (ou Alt+F12 sur Mac)
  pour l'écran, Ctrl+F12 pour la ville entière. **Elle s'exécute dans `update_input_before`**,
  donc avant le dessin de la frame, et capture la frame précédente.
- Fichier : nom horodaté `"city %Y-%m-%d %H.%M.%S.png"` / `"full city …png"`
  (`generate_filename`, l.98), écrit dans le dossier courant (dossier C3). PNG RGB 8 bits via la
  libpng embarquée (`ext/png`), compression 3. Ensuite `show_saved_notice()` ajoute une alerte
  visible sur les frames suivantes (l.183).
- Ville entière (`create_full_city_screenshot`, l.223) : uniquement en `WINDOW_CITY` ou
  `CITY_MILITARY`. Image de `largeur×60` × `(hauteur+1)×30` px (environ 9600×4830 pour 160×160),
  rendue par bandes de 30 px. Chaque bande redimensionne le canvas, déplace la caméra puis appelle
  `city_without_overlay_draw` ; la résolution et la caméra sont restaurées à la fin. Les bandes
  dépendent de `GRID_SIZE`.
- M0 a ajouté `graphics_save_screenshot_to_file(filename, full_city)`, sans alerte.

### 2.7 Dépendances aux dimensions de carte (UI)
`GRID_SIZE` = 162 (`map/grid.h:9`), `VIEW_X_MAX/VIEW_Y_MAX` (vue, minimaps),
`view_to_grid_offset_lookup` (statique), calcul de la ville entière dans `screenshot.c`, tailles de
l'éditeur (`scenario/editor.c:14-21`). Agrandir les cartes impose de dériver `VIEW_*` de
`GRID_SIZE` et de vérifier la recherche linéaire (l.267, coût en O(GRID²) par appel).

---

## 3. Architecture de l'UI

### 3.1 Fenêtres (`src/graphics/window.c`)
- `window_type {id, draw_background, draw_foreground, handle_input, get_tooltip}`
  (`window.h:79-85`), avec une soixantaine d'identifiants `WINDOW_*` (`window.h:8-77`).
- **Pas de vraie pile** : une file circulaire de `MAX_QUEUE` = 3 (l.11). `window_show()` (l.78)
  avance l'indice ; `window_go_back()` (l.96) recule. Toutes deux remettent à zéro la souris, le
  tactile et le défilement. `window_draw_underlying_window()` redessine la fenêtre précédente sous
  un dialogue modal.
- `window_draw(force)` (l.122) :
  1. `update_input_before()` (l.104) : touch→souris, manette, `mouse_determine_button_state()`,
     `hotkey_handle_global_keys()` ;
  2. `draw_background()` si invalidée ;
  3. `draw_foreground()` ;
  4. `handle_input(mouse, hotkeys)` ;
  5. tooltip, `warning_draw()` ;
  6. `update_input_after()` (l.114) : reset du scroll et des hotkeys, mise à jour du curseur.
- `window_invalidate()` redessine le fond **et** coupe la boucle de ticks.
  `window_request_refresh()` redessine sans couper.

### 3.2 Fenêtres de la ville
- `window/city.c` : `window_city_show()` (l.341) et `window_city_military_show(formation_id)`
  (l.359). Dessin : barre latérale, menu du haut, `widget_city_draw()`, puis
  `city_message_process_queue()` (l.120, popups en file). Entrées : `handle_hotkeys()` (l.202), puis
  le menu du haut (`widget/top_menu.c`), la barre latérale (`widget/sidebar/city.c`) et enfin
  `widget_city_handle_input()` (`widget/city.c:498`).
- Pendant un glisser de construction, le menu et la barre latérale sont ignorés (l.264).
- Autres fenêtres de jeu :
  - `build_menu.c` (choix du type) ;
  - `building_info.c` (clic droit sur une tuile ; sous-fenêtres `window/building/*.c`) ;
  - `advisors.c` et les 12 conseillers `window/advisor/*.c` ;
  - dialogues `labor_priority`, `set_salary`, `donate_to_city`, `gift_to_emperor`, `hold_festival`,
    `resource_settings`, `trade_prices` ;
  - `empire.c` et `trade_opened.c` ;
  - `message_dialog` et `message_list` ;
  - utilitaires `popup_dialog` (confirmation avec callback `(int accepted)`),
    `plain_message_dialog`, `select_list` et `numeric_input`.

### 3.3 Du menu principal à une partie
- `window_logo_show()` (`logo.c:39`) : un clic va au menu ; vidéo d'intro si
  `ui_show_intro_video` ; popup si le patch ou les polices manquent.
- `window_main_menu_show()` (`main_menu.c:130`). Les 6 `generic_button` sont en coordonnées de
  dialogue 640×480 : x 192-448, y = 100, 140, 180, 220, 260, 300 (l.30-37). Il faut ajouter le
  décalage de dialogue, soit (+80, +60) en 800×600. `button_click()` (l.108) :
  1. **Nouvelle carrière** (`new_career.c`, campagne) : nom → `mission_selection.c` →
     `mission_briefing.c` (`init` l.43 : `game_file_start_scenario_by_name`) → `button_start_mission`
     (l.175) → `window_city_show` ;
  2. **Charger** : `window_file_dialog_show(FILE_TYPE_SAVED_GAME, LOAD)`, validé par
     `button_ok_cancel` (`file_dialog.c:250`) ;
  3. **Scénario personnalisé** : `cck_selection.c` (`init` l.72 avec `scenario_set_custom(2)`,
     liste des `*.map` ; la sélection exécute `game_file_load_scenario_data` pour l'aperçu ;
     `button_start_scenario` l.269) ;
  4. **Éditeur** : `game_init_editor()` (`game.c:136`), seulement si `editor_is_present()` ;
  5. **Options Julius** (`window_config_show`) ;
  6. **Quitter**.
- Démarrage effectif : `start_scenario()` (`game/file.c:278`, static) puis
  `initialize_saved_game()` (l.210). Elle recalcule entre autres `city_view_init`, le routage,
  `map_orientation_update_buildings` et `building_menu_update`, puis appelle `game_state_unpause`.

### 3.4 Réglages et fichiers de configuration
| Fichier | Code | Contenu | Effet sur la simulation |
|---|---|---|---|
| `c3.inf` (560 octets, format d'origine) | `game/settings.c:117/140` | plein écran et taille, sons et volumes, **vitesse de jeu** (90 par défaut), vitesse de défilement, **difficulté** (`DIFFICULTY_HARD` par défaut), **dieux activés**, tooltips, alertes, autosave mensuelle, dernier conseiller, nom du joueur, économies personnelles par mission | difficulté (`game/difficulty.c:21-41`), dieux (`city/gods.c:205`), vitesse |
| `julius.ini` | `core/config.c:11` (clés l.14-36, `config.h:6-25`) | `gameplay_fix_immigration`, `gameplay_fix_100y_ghosts` (**[SIM]**, lus dans `city/sentiment.c:198` et `city/population.c:275`), plus 15 options `ui_*` et `screen_*` | les 2 options `gameplay_*` |
| `julius-hotkeys.ini` | `core/hotkey_config.c:14` | raccourcis (§5.3) | aucun |

En lockstep, la difficulté, les dieux, les options `gameplay_*` et la vitesse doivent venir de
l'hôte (salon) et non du fichier local.

### 3.5 Chaînes traduites propres à Julius (`src/translation/`)
- Énumération `translation_key` (`TR_*`, environ 95 clés, `translation.h:8`). Il y a un tableau
  `translation_string all_strings[]` par langue (`french.c`, `english.c`… environ 119 lignes
  chacun). `translation_load()` charge la langue puis **complète avec l'anglais**, en journalisant
  « Translation key not found ». Accès : `translation_for(key)`, en encodage interne, avec un tampon
  de 100 000 octets. Le test `test/translation/check.c` vérifie seulement que le chargement passe.
- Presque tout le texte de l'UI vient des fichiers d'origine : `c3.eng` / `c3_mm.eng` via
  `lang_get_string(group, id)`, et `c3_map.eng` pour l'éditeur. Les données du dossier sont en
  français. Toute nouvelle chaîne multijoueur passe par une clé `TR_*` + `english.c` + `french.c`.

---

## 4. Inventaire des actions joueur qui modifient l'état

### 4.0 Mécanique commune (à connaître pour la couche de commandes)
- L'UI appelle directement la logique **dans `handle_input`**, entre deux ticks. Aucune action ne
  porte d'identifiant de joueur, car l'état de cité est le singleton `city_data`.
- Beaucoup d'actions sont **relatives** (bascules, cycles, ±1). Une commande rejouable doit plutôt
  transmettre la **valeur absolue** visée.
- Plusieurs dialogues stockent la **sélection** dans `city_data` (sauvegardée) avant de la valider :
  fête, cadeau, don.
- `city_warning_show()` (`city/warning.c:31`) est appelé depuis la logique (échecs de
  construction…). En multijoueur, il ne doit s'afficher que pour l'émetteur de la commande.
- Certaines fonctions consomment le RNG de la simulation, par exemple `scenario_request_dispatch`
  (`random_byte`, `request.c:105`). Les commandes doivent donc s'exécuter au même point du tick
  sur tous les PC.

### 4.1 Construction, démolition, undo
Flux souris (`widget/city.c`) :
- `handle_mouse()` (l.463) : sur `left.went_down`, `handle_legion_click()` (l.204), sinon
  `build_start()` (l.216) qui appelle `building_construction_start(x,y,off)` (`construction.c:374`) ;
- tant que le bouton est enfoncé : `build_move()` → `building_construction_update()` (l.436) ;
- sur `went_up` : `build_end()` (l.231) → `building_construction_place()` (l.556) ;
- clic droit ou Échap : `building_construction_cancel()` (l.423).

Le type est choisi au préalable, de façon **[LOCAL]**, par `building_construction_set_type()`
(l.271), appelé depuis `build_menu.c:290`, les hotkeys (`window/city.c:193-200`, contrôle
`scenario_building_allowed` + `building_menu_is_enabled`) et le clonage
(`building_clone_type_from_grid_offset`).

| Action | Déclencheur UI | Logique | État modifié | Paramètres pour rejouer |
|---|---|---|---|---|
| Placer un bâtiment (1 clic) | `widget/city.c:463` → `build_end` | `building_construction_place` → `building_construction_place_building(type,x,y)` (`construction_building.c:498`) | `building_create`, grilles (terrain, building, images, property), trésor (`city_finance_process_construction`), `formation_move_herds_away`, marbre (grands temples, oracle), undo | type **résolu** (sous-type des temples cyclé : `data.sub_type`, l.679-690), x et y de **fin**, orientation de vue (ancrage l.516-520, hippodrome, fort, quais), `road_orientation` (porte, arc, l.528-548), et pour un pont l'état `bridge` recalculé |
| Tracer une route, un mur, un aqueduc ou réservoir + aqueduc | glisser (même flux) | `building_construction_place_road/wall/aqueduct` (`construction_routed.c:78/106/133`), `place_reservoir_and_aqueducts` (`construction.c:169`) | terrain, images, routage (`map_routing_update_land/walls`) | type, (x0,y0) départ, (x1,y1) fin ; le chemin vient du routage depuis le départ |
| Zones (maisons, jardins, places) | glisser | `place_houses` (l.80), `place_garden` (l.148), `place_plaza` (l.122) | bâtiments « terrain vague », terrain jardin, propriétés de place | type et rectangle (x0,y0,x1,y1) |
| Pont (bas ou pour navires) | clic | `map_bridge_add(x,y,is_ship)` (`bridge.c:193`) | terrain, sprites | (x,y). **Dépend de `map_bridge_calculate_length_direction` exécuté par le rendu** (l.540) et de l'orientation (`bridge.c:201`) |
| Dégager, démolir | glisser avec `BUILDING_CLEAR_LAND` | `building_construction_clear_land(0,…)` (`construction_clear.c:189`) | bâtiments `DELETED_BY_PLAYER`, sans-abri (`figure_create_homeless`), terrain, aqueducs, ponts | rectangle + **confirmation fort/pont** : un popup asynchrone (l.221-226) rappelle `clear_land_confirmed` via `confirm_delete_fort/bridge` (l.169/179) |
| Annuler un glisser | clic droit, Échap, bouton tactile | `building_construction_cancel` | **restaure les grilles** (`game_undo_restore_building_state`, `restore_map(1)`) | aucun (local en principe, mais touche la carte partagée) |
| Undo | `button_undo` (`widget/sidebar/city.c:294`) | `game_undo_perform` (`undo.c:200`) | trésor remboursé, **restauration de grilles entières** (terrain, aqueduc, sprite, image, property) ou bâtiments passés en `BUILDING_STATE_UNDO` | aucun ; valable 500 ticks (`undo.c:164`), désactivé par rotation, nouvelle année, séisme… |

Constats propres à la construction :
- **Aperçu sur la vraie carte.** `building_construction_start` appelle `game_undo_start_build()`
  (`undo.c:95`), qui sauvegarde les grilles image, terrain, aqueduc, property et sprite. Chaque
  `update` commence par `game_undo_restore_map()` puis trace réellement. Par exemple, en mesure,
  `map_tiles_set_road` est appelé (`construction_routed.c:43`). Comme les ticks sont gelés pendant
  le glisser (`speed.c:58`), c'est sans conséquence en solo. **En lockstep, c'est incompatible** :
  la simulation tourne pendant le glisser et les commandes distantes passent par le même `data` et
  le même tampon d'undo.
- **Orientation :** `city_view_orientation()` intervient dans le placement
  (`construction.c:495`, `construction_building.c:55,516`, `map/water.c`, `map/bridge.c:201`,
  `map/building_tiles.c`).
- **Horloge murale :** l'orientation des portes et arcs bascule toutes les 1500 ms
  (`construction.c:739`).
- La confirmation de démolition d'un fort ou d'un pont est un rappel différé. Le bug d'origine est
  conservé : le coût est payé même en cas de refus (`construction.c:601-604`).
- Les limites « un seul par cité » (sénat, caserne, hippodrome) et le compteur de légions sont
  globaux (`construction_building.c:576-604`). En multijoueur, il faudra un compteur par joueur.

### 4.2 Économie et gestion de la cité
| Action | Déclencheur UI | Logique | État [SIM] | Paramètres |
|---|---|---|---|---|
| Impôts ±1 % | `button_change_taxes` (`advisor/financial.c:111`) | `city_finance_change_tax_percentage(±1)` (`finance.c:27`), puis `estimate_taxes` et `calculate_totals` | `city_data.finance.tax_percentage` (0-25) | taux absolu |
| Salaires ±1 | `arrow_button_wages` (`advisor/labor.c:108`) | `city_labor_change_wages` (`labor.c:88`) | `city_data.labor.wages` (0-100) | valeur absolue |
| Priorité de main-d'œuvre | `button_priority` (`labor.c:116`) → `button_set_priority` (`labor_priority.c:109`) | `city_labor_set_priority(cat, prio)` (`labor.c:457`) : décale les autres catégories puis `city_labor_allocate_workers()` | `labor.categories[].priority` | catégorie 0-8, priorité 0-9 |
| Fête | `button_god`, `button_size` (`hold_festival.c:120/126`), puis `button_hold_festival` (l.145) | `city_festival_select_god/size` (`festival.c:50/60`, sélection sauvegardée) puis `city_festival_schedule` (l.69) | `festival.planned`, trésor (`process_sundry`), vin retiré (grande fête) | dieu 0-4, taille. Le refus d'une grande fête faute de vin est contrôlé à la sélection. |
| Entrepôt ou grenier : état d'une ressource | `toggle_resource_state` (`building/distribution.c:472`) | `building_storage_cycle_resource_state(storage_id, res)` (`storage.c:84`) : cycle accepter → refuser → chercher | `storages[].resource_state[]` | `storage_id` (ou `building_id`) + ressource + **état visé**. L'indice UI se traduit via `city_resource_get_available()` |
| Vider tout, n'accepter rien | `granary_orders` / `warehouse_orders` (l.485/496) | `building_storage_toggle_empty_all` (`storage.c:79`), `building_storage_accept_none` (l.97) | `storage.empty_all`, états | `storage_id` |
| Centre de commerce | `warehouse_orders(1)` | `city_buildings_set_trade_center` (`buildings.c:92`) | `building.trade_center_building_id` | `building_id` |
| Ouvrir une route commerciale | `button_open_trade` (`empire.c:664`), popup, puis `confirmed_open_trade` (l.655) | `empire_city_open_trade(city_id)` (`empire/city.c:289`) + `building_menu_update` | `cities[].is_open`, trésor (`cost_to_open`) | `city_id` (empire) |
| Import / export / aucun | `button_toggle_trade` (`resource_settings.c:221`) | `city_resource_cycle_trade_status` (`resource.c:84`, cycle avec contrôles empire, réinitialise le stock) | `resource.trade_status[]`, `stockpiled[]` | ressource + statut visé |
| Seuil d'export (« exporter au-delà de ») | `button_export_up_down` (l.209) | `city_resource_change_export_over(res,±1)` (l.109) | `export_over[]` (0-100) | ressource + valeur |
| Stocker (ne pas distribuer) | `button_toggle_stockpile` (l.226) | `city_resource_toggle_stockpiled` (l.119) | `stockpiled[]` (annule l'export) | ressource + 0/1 |
| Industrie à l'arrêt | `button_toggle_industry` (l.214, si l'industrie existe) | `city_resource_toggle_mothballed` (l.136) | `mothballed[]` | ressource + 0/1 |

### 4.3 Militaire
| Action | Déclencheur UI | Logique | État | Paramètres |
|---|---|---|---|---|
| Sélectionner une légion | clic sur un soldat (`widget/city.c:204`), touche L (`cycle_legion`, `window/city.c:160`), menu militaire, barre latérale militaire | `formation_set_selected` (`formation.c:145`) + `window_city_military_show` | [LOCAL] (non sauvegardé, réinitialisé au chargement l.686) | — |
| Déplacer | clic gauche en `WINDOW_CITY_MILITARY` → `military_map_click` (`widget/city.c:518`) | `formation_legion_move_to(m,x,y)` (`formation_legion.c:111`) ; clic sur son propre fort → retour | `standard_x/y`, `is_at_fort`, `action_state` des soldats, moral, routage | `formation_id`, x, y |
| Retour au fort | `button_return_to_fort` (`advisor/military.c:174`, `building/military.c:390`, `sidebar/military.c:599`) | `formation_legion_return_home` (l.144) | `is_at_fort`, restauration de la formation | `formation_id` |
| Formation (y compris **« nettoyage » `FORMATION_MOP_UP`, l'équivalent d'une attaque libre**) | `button_layout` (`building/military.c:399`), `button_select_formation_layout` (`sidebar/military.c:565`, indice traduit selon l'**orientation**) | `formation_legion_change_layout(m, layout)` (l.85) | `layout`, `prev.layout` | `formation_id` + enum `FORMATION_*` **final** |
| Service de l'empire | `button_empire_service` (`advisor/military.c:183`, `sidebar/military.c:607`) | `formation_toggle_empire_service` (`formation.c:150`) + `formation_calculate_figures` | `empire_service` | `formation_id` + 0/1 |

Il n'y a pas d'ordre d'attaque explicite : l'engagement est automatique (portée, formation
`MOP_UP`).

### 4.4 César et campagne (à supprimer d'après la vision, à recenser quand même)
| Action | Déclencheur UI | Logique |
|---|---|---|
| Salaire du gouverneur | `button_set_salary` (`set_salary.c:99`) | `city_emperor_set_salary_rank` (`emperor.c:275`) + `city_finance_update_salary` |
| Don de ses économies à la cité | `button_set_amount` / `arrow_button_amount` / `button_donate` (`donate_to_city.c:110/136/125`) | `city_emperor_set/change_donation_amount` (sélection dans `city_data.emperor.donate_amount`), puis `city_emperor_donate_savings_to_city` (`emperor.c:318`) |
| Cadeau à César | `button_set_gift` / `button_send_gift` (`gift_to_emperor.c:100/107`) | `city_emperor_set_gift_size` (`emperor.c:172`), puis `city_emperor_send_gift` (l.205, faveur, économies) |
| Envoyer des biens (demande impériale) | `button_request` (`advisor/imperial.c:245`), popup, `confirm_send_goods` (l.238) | `scenario_request_dispatch(id)` (`request.c:98`, **RNG**, entrepôts, trésor) ; `city_military_clear_empire_service_legions` dès le clic |
| Envoyer des troupes (bataille lointaine) | `confirm_send_troops` (l.230) | `formation_legions_dispatch_to_distant_battle` (`formation_legion.c:189`) |
| Victoire : continuer à gouverner | `button_continue_governing` (`victory_dialog.c:105`) | `city_victory_continue_governing(months)` (`victory.c:149`), `city_victory_reset` |
| Fin de mission (renvoi ou suite) | `button_fired`, `advance_to_next_mission` (`mission_end.c:160/122`) | `city_victory_stop_governing`, `setting_set_personal_savings_for_mission`, rang de campagne |
| Démarrer une mission | `button_start_mission` (`mission_briefing.c:175`) | `city_mission_reset_save_start` |

### 4.5 Contrôle global de la partie
| Action | Déclencheur UI | Logique | Classement en lockstep |
|---|---|---|---|
| Pause | touche P (`toggle_pause`, `window/city.c:187`), bouton tactile (`widget/city.c:337`) | `game_state_toggle_paused` (`state.c:38`) | **global** (commande collective) |
| Vitesse | `[` `]` PgUp PgDn (`window/city.c:207-212`), `button_game_speed` (`sidebar/extra.c:278`), `speed_options.c:95` | `setting_increase/decrease_game_speed` (`settings.c:257/268`) | **global** |
| Rotation de la carte | Début / Fin (`window/city.c:228-235`), boutons (`sidebar/city.c:342/348`) | `game_orientation_rotate_left/right/north` (`game/orientation.c:9-45`) → `map_orientation_change` (`map/orientation.c:40`) + `game_undo_disable` | **[SIM] aujourd'hui.** Il faut l'imposer globalement ou découpler rendu et simulation (§0.3) |
| Difficulté, dieux | `difficulty_options.c:50/59` | `setting_increase/decrease_difficulty`, `setting_toggle_gods_enabled` | réglage d'hôte |
| Options gameplay Julius | `window/config.c:121-122,576` | `config_set(CONFIG_GP_*)` | réglage d'hôte |
| Autosave mensuelle | `menu_options_autosave` (`top_menu.c:466`) | `setting_toggle_monthly_autosave` | local (écriture de fichier depuis le tick) |
| Nouvelle partie, rejouer, charger, sauvegarder, supprimer | `top_menu.c:374/398/405/413/420`, Ctrl+O / Ctrl+S (`window/city.c:244-249`) | `game_file_*` (`file.c`), `game_undo_disable`, `game_state_reset_overlay` | à orchestrer (sauvegarde réseau) |

### 4.6 Triche (`platform/keyboard_input.c:291-302`, layout clavier dépendant, sans `repeat`)
- **Alt+K** → `game_cheat_activate()` (`cheats.c:14`). La triche s'arme si la fenêtre d'info d'un
  **puits** est ouverte. Si elle est déjà armée et qu'un dialogue de message est ouvert, Alt+K
  appelle `scenario_invasion_start_from_cheat()` (`invasion.c:425`).
- **Alt+C** → `city_finance_process_cheat()` (`finance.c:63`) : +1000 si le trésor est inférieur à
  5000, compté dans `cheated_money`.
- **Alt+V** → `city_victory_force_win()`.
- Ces triches s'exécutent dans `handle_event`, **hors `window_draw`**.

### 4.7 Actions locales mais sérialisées dans la sauvegarde [LOCAL-SAV]
Caméra et orientation (`city_view_save_state`), signets (`map_bookmark_save` / `_go_to`,
Ctrl/Alt+F1..F4 et F1..F4, `bookmark.c:19/26`), défilement et objet sélectionné de la carte de
l'empire (`empire_save_state`), messages lus ou supprimés (`city_message_mark_read/delete`,
`message.c:404/409`, via `message_list.c:194/208`), ordre du graphique de population
(`city_population_set_graph_order`, `advisor/population.c:437`), note sélectionnée
(`city_rating_select`, `advisor/ratings.c:163`), sélections de fête, cadeau et don (§4.2, §4.4).
En multijoueur, il faut les rendre propres à chaque joueur et les exclure de la somme de contrôle.

### 4.8 Effets de bord implicites (ouvrir une fenêtre modifie la simulation)
- `window/advisors.c:115` (`init`, à **chaque ouverture** de conseiller) appelle
  `city_labor_allocate_workers()`, `city_finance_estimate_taxes/wages`,
  `city_finance_update_interest/salary`, `city_finance_calculate_totals`,
  `city_migration_determine_no_immigration_cause`, `city_houses_calculate_culture_demands`,
  `city_culture_update_coverage`, `city_resource_calculate_food_stocks_and_supply_wheat()` (qui
  remet `market.inventory[WHEAT] = 200` si Rome fournit le blé, `resource.c:297-307`),
  `formation_calculate_figures` et `city_ratings_update_explanations`. **Danger de
  désynchronisation** si un seul PC ouvre un conseiller.
- `window/building_info.c:165` (`init`) : `city_resource_determine_available()` (liste locale),
  `building_house_determine_evolve_text`, `figure_phrase_determine` (§2.5),
  `formation_legion_recruits_needed`.
- Changer de vitesse ou d'impôt recalcule aussi des estimations (§4.2). Les sons (`sound_*`) sont
  locaux.

### 4.9 Synthèse SIMULATION / LOCAL
- **[SIM]** : bâtiments, figures, formations, grilles de carte, `city_data` (finances, main-d'œuvre,
  ressources, fêtes, empereur), stockage, empire (routes ouvertes), scénario (demandes),
  RNG, temps de jeu, pause et vitesse (en lockstep), **orientation (en l'état)**, difficulté,
  dieux, `gameplay_*`, état d'undo (par joueur).
- **[LOCAL]** :
  - fenêtres (`graphics/window.c`) ;
  - entrées (`input/*`) ;
  - type de construction et glisser (`building/construction.c` `data`, qui modifie pourtant la
    carte) ;
  - overlay (`game/state.c` `current_overlay`) ;
  - légion sélectionnée ;
  - barre latérale repliée et viewport ;
  - alertes (`city/warning.c`, horodatées) ;
  - file de popups de messages (`message.c` `queue`, `consecutive_message_delay`) ;
  - curseur de « zone à problème » ;
  - tooltips, son, musique, vidéo ;
  - réglages d'affichage, de son, de défilement, de tooltips et d'alertes ;
  - raccourcis ;
  - dernier conseiller (`setting_set_last_advisor`).
- **[LOCAL-SAV]** : §4.7 et les écritures du rendu (§2.5).

---

## 5. Entrées (`src/input/`, `src/platform/`)

### 5.1 Chaîne de traitement
1. SDL (`julius.c:handle_event`).
2. Fonctions d'entrée selon la source :
   - souris : `input/mouse.c` ;
   - clavier : `platform/keyboard_input.c`, puis `hotkey_key_pressed/released`
     (`input/hotkey.c:389/417`) et `keyboard_*` pour l'édition de texte (`input/keyboard.c`) ;
   - tactile : `input/touch.c` ; manette : `input/joystick.c`.
3. Dans `window_draw`, `update_input_before` convertit le tactile et la manette en souris et
   clavier, puis appelle `mouse_determine_button_state()` (`mouse.c:118`).
4. `handle_input(m, h)` reçoit les entrées. `mouse_in_dialog()` (l.149) translate les coordonnées
   dans le repère 640×480 des dialogues.
5. `update_input_after` remet `hotkey_state` et le scroll à zéro.

### 5.2 Souris
- `mouse_button {is_down, went_down, went_up, double_click, system_change}` (`mouse.h:14-20`).
- `mouse_set_left_down(d)` (l.75) ajoute un drapeau `system_change`. Down et up dans la même
  frame donnent `went_down = went_up = 1` et `is_down = 0` : un clic complet en une frame est
  possible.
- Le double-clic dépend de `time_get_millis()` et se réinitialise si la souris bouge (l.64-67).
- Molette : `mouse_set_scroll`.
- Clic droit + glisser sur la carte = défilement (`scroll_drag_start`, désactivable).

### 5.3 Clavier et raccourcis
- `get_key_from_scancode()` (`keyboard_input.c:15`) suit le layout physique ; les noms de
  touches passent par `SDL_GetKeyName`.
- Modificateurs : `KEY_MOD_SHIFT/CTRL/ALT/GUI`.
- `hotkey_install_mapping()` (`hotkey.c:335`) fixe Entrée et Échap, puis ajoute les définitions
  de `julius-hotkeys.ini` ou les valeurs par défaut (`hotkey_config.c:114-180`) :

| Touches | Action |
|---|---|
| flèches | défilement |
| P | pause |
| Espace | bascule d'overlay |
| L | légion suivante |
| `[` `]` PgDn PgUp | vitesse |
| Début / Fin | rotation |
| 1-0, pavé num. | conseillers |
| `-` | finances |
| `=` | chef |
| W F D C T | overlays eau, feu, dégâts, crime, problèmes |
| Ctrl+O / Ctrl+S | charger / sauvegarder |
| F1-F4 | aller au signet |
| Ctrl+F1-F4 ou Alt+F1-F4 | poser un signet |
| F5 | centrer |
| F6 ou Alt+Entrée | plein écran |
| F7, F8, F9 | 640, 800, 1024 |
| F12 / Ctrl+F12 | captures |

- Les actions `HOTKEY_BUILD_*` et `BUILD_CLONE` n'ont **pas** de touche par défaut.
- Les hotkeys globales (centrer, plein écran, taille, captures) sont traitées dans
  `hotkey_handle_global_keys()` (l.447), les autres par chaque fenêtre via `const hotkeys *h`
  (`hotkey.h:7-28`).
- La saisie de texte a lieu si une capture est active (`keyboard_start_capture`, l.105 ; `input_box`)
  et ne passe que par `SDL_TEXTINPUT` → `keyboard_text()` (l.363). Entrée et Retour arrière sont
  traités par `platform_handle_key_down` (l.231-240).

### 5.4 Injecter des entrées synthétiques
1. **`SDL_PushEvent`** (choix de M0) : passe par le même code que le vrai matériel, y compris le
   mapping des touches et les triches. Il faut fournir `keysym.scancode`, `sym` et `mod`, et des
   `SDL_TEXTINPUT` pour le texte. Inconvénients : une frame de latence (§1.5) et des coordonnées
   **logiques**. Avec `windowID = 0`, l'observateur de rendu SDL ne devrait pas convertir les
   coordonnées ; à l'échelle 100 % il n'y a de toute façon aucune différence (non vérifié).
2. **Appels directs**, dans une accroche « avant frame » :
   - souris : `mouse_set_position`, `mouse_set_left_down` / `mouse_set_right_down`,
     `mouse_set_scroll` ;
   - clavier : `hotkey_key_pressed(key, mod, 0)` et `hotkey_key_released` ;
   - **`hotkey_set_value_for_action(action, 1)`** (`hotkey.c:470`, déjà utilisé par la manette
     `joystick.c:678`) pour déclencher un raccourci sans touche (utile pour `HOTKEY_BUILD_*`) ;
   - texte : `keyboard_text("…")`, `keyboard_return()`.
   Ces appels sont synchrones, sans latence ni dépendance au layout.
3. **Appels de logique** (sans UI, idéal pour l'IA et futur format des commandes) :
   `building_construction_set_type` / `start` / `update` / `place` et les fonctions des tableaux
   §4.x. Attention : les contrôles de l'UI sont contournés (`scenario_building_allowed`,
   `building_menu_is_enabled`, confirmations), le fantôme aussi (pont, orientation des portes), et
   ces appels dépendent de l'orientation courante.

Pour viser une tuile, voir la formule du §2.3 ou faire un helper qui appelle
`city_view_go_to_grid_offset` puis `city_view_grid_offset_to_xy_view`.

---

## 6. Éditeur de cartes (`src/editor/`, `src/window/editor/`)

### 6.1 Capacités
- Entrée par le menu principal : `game_init_editor()` (`game.c:136`) recharge la langue
  (`c3_map.eng`) et les images `C3map*`, puis `game_file_editor_create_scenario(2)` (80×80) et
  `editor_set_active(1)`. Le dossier de données contient les fichiers requis par
  `editor_is_present()` (`editor.c:7-17`). Dans l'éditeur, `game_tick_run()` n'anime que les
  drapeaux (`tick.c:170-173`) et l'orientation reste à 0.
- Outils (`editor/tool.h:6-28`, `editor/tool.c`) :
  - pinceaux avec `editor_tool_set_brush_size` : herbe, arbres, eau, arbustes, rochers, prairie,
    relever et abaisser le terrain ;
  - rampe d'accès ;
  - route (glisser, utilise `game_undo_start_build(BUILDING_ROAD)`, l.117) ;
  - drapeaux : entrée et sortie terrestres, entrée et sortie fluviales, points d'invasion (8),
    points de pêche (8), troupeaux (4), point de séisme ;
  - bâtiments indigènes : hutte, centre, champ.
- Flux souris : `widget_map_editor_handle_input` (`widget/map_editor.c:300`) →
  `editor_tool_start_use` / `update_use` / `end_use` (`tool.c:108/230/411`). Les contraintes sont
  dans `editor/tool_restriction.c` (bord de carte, eau profonde…).
- Menu du haut (`widget/top_menu_editor.c`) :
  - nouvelle carte : `window_select_list_show` puis `map_size_selected` (l.174), 6 tailles
    40/60/80/100/120/160 (`scenario/editor.c:14-21`) ;
  - charger et sauvegarder un `.map` (`file_dialog`, `FILE_TYPE_SCENARIO`) ;
  - réinitialiser troupeaux, poissons et invasions ;
  - choix de l'empire ; quitter (`game_exit_editor`).
- Attributs (`window/editor/attributes.c`) : description, climat, image, ennemi, conditions de
  départ (année, fonds, prêt de secours, blé de Rome, épaves, jalons), demandes, invasions,
  bâtiments autorisés, critères de victoire, événements spéciaux (séisme, révolte, changement
  d'empereur, événements aléatoires), variations de prix et de demande. Fonctions :
  `scenario/editor.c`, `editor_events.c`, `editor_map.c`.

### 6.2 Format `.map`
`game_file_editor_write_scenario()` (`file_editor.c:151`) → `game_file_io_write_scenario()`.
Pièces de taille fixe (`file_io.c:207-216`) : images (52488 = 2×162²), bord, terrain (52488),
bitfields, aléa, élévation (26244 = 162²), graine du RNG (8), caméra (8) et struct `scenario`
(1720 octets) : **un seul** `entry_point`, `exit_point`, `river_entry_point` et
`river_exit_point` (`scenario/data.h:211-218`). Les 12 cartes `.map` du dossier de données
(Caesarea, Lugdunum…) sont utilisables comme bases de test.

### 6.3 Ce qu'il faudrait pour des cartes multijoueur
- **Points de départ par joueur (2 à 4)** : nouveaux outils (`TOOL_PLAYER_START_n`), nouveaux
  champs de scénario (au minimum une position de départ, et sans doute une entrée et une sortie
  terrestres par joueur, puisque immigrants et marchands utilisent `entry_point` / `exit_point`),
  rendu des drapeaux (`figure_create_editor_flags`) et contrôles de validité. À défaut,
  réutiliser provisoirement les 8 `invasion_points` sans changer de format.
- **Cartes plus grandes** : `MAP_SIZES`, `GRID_SIZE`, `VIEW_X_MAX/Y_MAX`, tailles des pièces du
  fichier (§6.2, donc nouveau format versionné), lookup de vue, minimap et capture de ville entière.
- Retirer ou masquer de l'éditeur ce qui relève de César et de la campagne (demandes, critères de
  victoire, rang, prêt de secours, changement d'empereur) et le remplacer par des réglages de
  partie dans le salon.

---

## 7. Non vérifié, limites
- **Rien n'a été exécuté.** Les comportements dynamiques sont déduits du code : gel par défilement
  en (0,0), latence de `SDL_PushEvent`, conversion des coordonnées pour `windowID = 0`, absence
  d'événements `SHOWN`/`FOCUS` avec le pilote vidéo factice.
- **Inventaire construit par grep** des appels de `src/window` et `src/widget` vers la logique,
  complété par la lecture des gestionnaires. Les chemins tactile et manette, ainsi que les
  plateformes Vita, Switch, Android et iOS, ont seulement été survolés. Le menu militaire et le
  « clonage » ont été vérifiés comme locaux.
- Non vérifié : si les bits `property` constructing/deleted et les sprites écrits par le rendu
  sont relus par la logique **pendant les ticks** (lecteurs repérés : `map/image_context.c`,
  `map/bridge.c`, `map/building_tiles.c`, `construction_clear.c`).
- Le détail des algorithmes d'élévation et de rampe de l'éditeur n'a pas été lu.
- Le module d'automatisation de M0 a été lu rapidement et n'a pas été testé.
