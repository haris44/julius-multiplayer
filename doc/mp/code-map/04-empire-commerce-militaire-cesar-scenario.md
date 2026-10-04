# 04 — Empire et commerce, militaire et combats, César, scénario, campagne et modes de jeu

Cartographie en lecture seule du code de la branche `multiplayer` (base `upstream-base` = 34d1ecd5).
Les références sont données sous la forme `chemin:ligne`. Les points marqués **[NON VÉRIFIÉ]** viennent de la lecture du code ; ils n'ont été vérifiés ni en jeu ni contre l'exécutable original.

---

## 0. Faits transverses (à lire en premier)

**Ordonnancement dans la simulation** (`src/game/tick.c`) : 50 ticks/jour, 16 jours/mois, 12 mois/an (`src/game/time.c:4-6`).

| Fréquence | Appel (fichier:ligne) | Système |
|---|---|---|
| chaque tick | `random_generate_next` (tick.c:175), `figure_action_handle` (178), `scenario_earthquake_process` (179), `scenario_gladiator_revolt_process` (180), `scenario_emperor_change_process` (181), `city_victory_check` (182) | RNG global, figures, événements, victoire |
| jour, tick 4 | `city_emperor_update` (125) | dette et colère de César |
| jour, ticks 5 et 29 | `formation_update_all(0/1)` (126, 144) | légions, ennemis, troupeaux |
| jour, tick 6 | `map_natives_check_land` (127) | colère des indigènes |
| jour, tick 19 | `building_dock_update_open_water_access` (135) | docks |
| jour, tick 21 | `building_maintenance_check_rome_access` (137) | « route de Rome » |
| jour, tick 32 | `city_trade_update` (147), qui appelle `empire_city_generate_trader` | génération des marchands |
| mois | `scenario_random_event_process` (75), `city_finance_handle_month_change` (76), `scenario_distant_battle_process` (78), `scenario_invasion_process` (79), `scenario_request_process` (80), `scenario_demand_change_process` (81), `scenario_price_change_process` (82), `city_victory_update_months_to_govern` (83), `formation_update_monthly_morale_at_rest` (84), `city_ratings_update(0)` (95) | événements de scénario |
| année | `scenario_empire_process_expansion` (60), `city_finance_handle_year_change` (64, tribut), `empire_city_reset_yearly_trade_amounts` (65), `city_ratings_update(1)` (67) | empire, tribut, quotas |

- **Une seule cité** : tout l'état décrit ici (finances, faveur, dette, bataille lointaine, commerce, compteurs militaires, victoire) vit dans le singleton `extern struct city_data_t city_data` (`src/city/data_private.h:25`). Les structures `emperor` (64-87), `military` (88-94), `distant_battle` (95-107), `finance` (108-125, dont `tribute_not_paid_*` 121-122), `trade` (351-360), `map` (361-370, points d'entrée et de sortie) et `mission` (371-381) en font partie.
- **Vestiges de « factions »** dans le format original : `figure.faction_id` (« 1 = city, 0 = enemy », `src/figure/figure.h:27`), `figure.is_friendly` (`figure.h:25`), `formation.faction_id` (« 1 = player, 0 = everyone else », `src/figure/formation.h:55`), `building.faction_id` (toujours 1, `src/building/building.c:73`), `city_data.unused.faction_id` (`data_private.h:423`) et un bloc de 18068 octets `city_data.unused.other_player` (`data_private.h:383`, sauvegardé tel quel par `src/city/data.c:54/531`). Ces champs ressemblent aux restes d'un multijoueur prévu puis abandonné dans Caesar III **[NON VÉRIFIÉ]**. **Aucune logique de combat ne lit `faction_id`.**
- **Couverture par les tests** (`test/CMakeLists.txt:78-132`) : les 36 tests autopilot exercent justement ces systèmes : `sav_request1/2` (demandes de César), `sav_caesar1-4` (invasion de César et baliste), `sav_invasion1-5`, `sav_distantbattle1-3`, `sav_curses1/2` (Mars), `sav_earthquake0-5`, `sav_native1/2` (le 2ᵉ porte sur le commerce, `cicero-lugdunum-trade`) et `sav_edge1-4` (bataille). `test/sav/sav_compare.c:20-45` compare la sauvegarde **octet par octet sur une disposition fixe** (par exemple `formations` = 6400 octets = 50 × 128, `figures` = 128000 octets). **Conséquence** : rien ne peut être supprimé du chemin solo. La suppression de César doit se faire par un *gating* propre au mode multijoueur, sans modifier le format solo, l'ordre des boucles ni la consommation du RNG.

---

## 1. Empire et commerce

### 1.1 Données de l'empire
- Chargement : `empire_load` (`src/empire/empire.c:33-57`) lit `c3.emp` (campagne) ou `c32.emp` (scénario personnalisé). Pour chaque `empire_id`, le fichier contient un en-tête de 32 octets et un bloc de 12800 octets (`EMPIRE_HEADER_SIZE`/`EMPIRE_DATA_SIZE`, 16-21). L'empire est donc **un préréglage figé du fichier** : le code ne permet pas de créer des villes à la volée.
- Objets : `full_empire_object objects[MAX_OBJECTS=200]` (`src/empire/object.c:11-27`), lus par `empire_object_load` (60-102). Leurs types sont définis dans `src/empire/type.h:4-12` (`EMPIRE_OBJECT_CITY`, `_BATTLE_ICON`, `_LAND/SEA_TRADE_ROUTE`, `_ROMAN_ARMY`, `_ENEMY_ARMY`).
- Villes : `empire_city cities[MAX_CITIES=41]` (`src/empire/city.c:16-18`). La structure est définie dans `src/empire/city.h:7-20` (`type`, `route_id`, `is_open`, `buys_resource[]`, `sells_resource[]`, `cost_to_open`, `trader_entry_delay`, `is_sea_trade`, `trader_figure_ids[3]`). L'index 0 n'est jamais une vraie ville : `city_id = 0` désigne les marchands indigènes. Les types de ville (`type.h:14-22`) sont `EMPIRE_CITY_DISTANT_ROMAN`, `_OURS`, `_TRADE`, `_FUTURE_TRADE`, `_DISTANT_FOREIGN`, `_VULNERABLE_ROMAN` et `_FUTURE_ROMAN`.
- `empire_object_init_cities` (`object.c:104-158`) crée les villes à partir des objets : `route_id` est borné à 0..19, `is_sea_trade` est déduit du type de l'objet route (313-326) et les quotas sont codés sur 3 bits (`trade40/25/15`, 295-311) puis traduits en 15, 25 ou 40 chargements par an (144-150). Les villes romaines et étrangères n'achètent et ne vendent rien (131-136).
- Production autorisée : `empire_can_produce_resource` (`city.c:107-116`) n'autorise une industrie que si la ville `EMPIRE_CITY_OURS` « vend » la matière première dans le fichier d'empire, ou si une ville commerçante ouverte l'exporte. Ce test conditionne le menu de construction (`src/building/menu.c:86,93`). **L'unique ville « OURS » de l'empire décide donc des ressources exploitables de la cité.**
- Expansion : `scenario_empire_process_expansion` (`src/scenario/empire.c:18-31`, appelée chaque année) fait passer `FUTURE_TRADE` à `TRADE` et `FUTURE_ROMAN` à `DISTANT_ROMAN` (`city.c:194-209`).

### 1.2 Quotas, prix et ouverture d'une route
- Quotas : `struct route_resource data[MAX_ROUTES=20][RESOURCE_MAX]` contient `{limit, traded}` (`src/empire/trade_route.c:3-10`). Le quota est atteint quand `traded >= limit` (62-65). Le compteur est remis à zéro chaque année, mais seulement pour les routes ouvertes (`city.c:159-166`). Les variations de demande déplacent la limite d'un cran dans l'échelle 0/15/25/40 (`trade_route.c:28-48`). **Chaque achat et chaque vente comptent tous deux dans `traded`** (`figuretype/trader.c:384,396`, `figuretype/docker.c:34,46,67`).
- Prix : il n'existe **qu'une table globale** `prices[RESOURCE_MAX]` de paires `{buy, sell}`. Ses valeurs par défaut sont `DEFAULT_PRICES` (`src/empire/trade_prices.c:8-13`), par exemple vin 215/160 et armes 250/180. Elle est remise à zéro au chargement d'un scénario (`src/game/file.c:344`) et modifiée par `trade_price_change` (34-48), qui ajoute ou retire le même montant à l'achat et à la vente (plancher : achat 2, vente 0).
- Ouverture : l'interface appelle `confirmed_open_trade`, puis `empire_city_open_trade` (`src/window/empire.c:655-662`). La fonction impute `cost_to_open` en dépense de construction (`city_finance_process_construction`) et pose `is_open = 1` (`city.c:289-294`). Il n'y a pas de commande : l'appel est direct depuis l'interface.

### 1.3 Génération des marchands
- `city_trade_update` (`src/city/trade.c:8-29`, une fois par jour) décrémente les durées de « problèmes de commerce », puis appelle `empire_city_generate_trader` (`city.c:296-319`). Cette dernière parcourt les villes ouvertes et **s'arrête au premier marchand créé** : au plus un marchand par jour, toutes villes confondues. Pour une route maritime, il faut un dock en service (sinon `MESSAGE_NO_WORKING_DOCK` avec un délai de 384) et un point d'entrée fluvial.
- `generate_trader` (`city.c:211-287`) : le nombre de marchands simultanés (1 à 3) est la moyenne arrondie au supérieur de la règle 15→1, 25→2, 40→3 sur les ressources de la ville. `trader_entry_delay` vaut 30 pour la mer et 4 pour la terre, en jours. Un navire apparaît à `scenario_map_river_entry()` ; une caravane apparaît à `city_map_entry_point()` avec un chef et deux ânes (`figuretype/trader.c:28-43`). Aucun des deux n'apparaît pendant un « problème de commerce ».

### 1.4 Une caravane terrestre, pas à pas (`figure_trade_caravan_action`, `figuretype/trader.c:326-429`)
1. `FIGURE_ACTION_100_TRADE_CARAVAN_CREATED` : la caravane est un fantôme. Après plus de 20 ticks, elle appelle `go_to_next_warehouse`, qui cherche l'entrepôt le plus proche à partir du **trade center**. Le trade center est le premier entrepôt rencontré (`city_buildings_get_trade_center`, fixé dans `building/maintenance.c:267-269`).
2. `get_closest_warehouse` (218-307) ne retient que les entrepôts reliés par la route (`distance_from_entry > 0`). Le choix est pondéré par des pénalités selon les ressources exportables, les emplacements vides et l'import courant. La caravane n'importe jamais rien d'un marchand indigène (`city_id == 0`, 232-233).
3. `_101_ARRIVING`, puis `_102_TRADING` : tous les 10 ticks, elle tente **un achat** (`figure_trade_caravan_can_buy`, puis `trader_get_buy_resource` : la cité exporte un chargement et encaisse `trade_price_sell`, 133-162), puis **une vente** (`figure_trade_caravan_can_sell`, puis `trader_get_sell_resource` : la cité importe et paie `trade_price_buy` via `building_warehouse_space_add_import`, 164-216). Quand les deux échouent, elle passe à l'entrepôt suivant ou repart vers `city_map_exit_point()`.
4. Capacité : 8 achats (`trader_amount_bought >= 8`) et 8 ventes (`loads_sold_or_carrying >= 8`). La ressource importée suit une rotation globale (`city_trade_current/next_caravan_import_resource`, `city/trade.c:71-92`).
5. Les ânes suivent leur chef ou meurent avec lui (431-458). Les caravanes et les ânes sont de catégorie CITIZEN, donc **attaquables** par des ennemis (§2.5).

### 1.5 Un navire et ses dockers, pas à pas
- `figure_trade_ship_action` (`trader.c:613-746`) : le navire est créé avec `loads_sold_or_carrying = 12` à vendre. Il cherche un dock libre (`building_dock_get_free_destination`, `src/building/dock.c:52-81` ; bogue d'origine avec 10 docks, l.66) ou rejoint la file d'attente. Il reste amarré (`_112_MOORED`) jusqu'à ce que les dockers soient inactifs au moins 10 fois de suite (`trade_ship_done_trading`, 591-611). Il repart **vers `river_entry`** (677, 684) ; il ne part vers `river_exit` (667) que si son dock disparaît pendant l'approche.
- `figure_docker_action` (`figuretype/docker.c:278-498`). Import : `deliver_import_resource` (214-241) décrémente le stock du navire **avant** la livraison, attend 80 ticks en file, puis `try_import_resource` (20-53) paie `trade_price_buy` à l'entrepôt. Export : `fetch_export_resource` (243-270), puis `try_export_resource` (55-74) encaisse `trade_price_sell`. Les entrepôts doivent être sur le **même réseau routier** que le dock (101, 167). Capacité d'achat : 12 (`trader_has_traded_max`, `src/figure/trader.c:71-74`).

### 1.6 Règles d'achat et de vente, et flux d'argent
- Exporter vers la ville `city_id` (`empire_can_export_resource_to_city`, `empire.c:135-151`) suppose que le quota n'est pas atteint, que le stock dépasse `export_over`, que la ville achète la ressource et que la cité l'a en `TRADE_STATUS_EXPORT`.
- Importer (`empire_can_import_resource_from_city`, 167-221) suppose que la ville vend la ressource, que la cité l'a en `TRADE_STATUS_IMPORT` et que le quota n'est pas atteint. Le stock plafond est de 10, 20, 30 ou 40 pour la nourriture et les produits finis, selon la population (<2000, <4000, <6000, au-delà ; 153-165). Il est de 10 pour le marbre et les armes, et de `2 + 2 × ateliers actifs` pour les matières premières.
- Les statuts d'échange et `export_over` sont des réglages du joueur (`src/city/resource.c:79-112`).
- Argent : `city_finance_process_export(prix)` crédite le trésor et l'est **doublé** si la bénédiction de Neptune est active (`src/city/finance.c:53-61`). `city_finance_process_import(prix)` débite le trésor (47-51). Les appels se trouvent dans `figuretype/trader.c:154` et `building/warehouse.c:179-180,193-194`. **L'argent ne va nulle part ailleurs** : aucune contrepartie n'est créditée ou débitée.
- Statistiques des marchands : `traders[MAX_TRADERS=100]` (`src/figure/trader.c:7-22`). Les valeurs y sont inversées par un bogue volontairement conservé (commentaires `BUG` l.44 et l.52). Ces statistiques sont sauvegardées et comparées par les tests.
- Marchands indigènes (`figure_native_trader_action`, `trader.c:460-552`) : ils apparaissent aux centres de réunion quand la mission est opérationnelle (`src/building/figure.c:1106-1112`). Ils **achètent seulement**, avec `city_id = 0`, sans quota, en comptant +3 par chargement.

### 1.7 Points d'entrée et de sortie, et « route de Rome »
- Le scénario ne contient qu'**un seul** `entry_point`, `exit_point`, `river_entry_point` et `river_exit_point` (`src/scenario/data.h:211-214`), plus 8 `invasion_points` et un `earthquake_point`. Ils sont recopiés dans `city_data.map` (`game/file.c:160-166`, `scenario_map_init_entry_exit`, `scenario/map.c:18-28`).
- Leurs consommateurs sont les migrants (`figuretype/migrant.c:17,201,255`), les caravanes (`empire/city.c:281`, `trader.c:319`), les navires et l'eau libre (`empire/city.c:273`, `building/dock.c:27,43`, `figuretype/water.c`), les légions envoyées en bataille lointaine (`soldier.c:369`), l'invasion de César (`scenario/invasion.c:229`) et l'invasion de repli (260).
- `building_maintenance_check_rome_access` (`src/building/maintenance.c:223-339`, tous les jours) calcule `distance_from_entry` de chaque bâtiment à partir de l'entrée. Une maison qui ne peut pas rejoindre l'entrée est **détruite** au bout de 8 passages (254-264). Un entrepôt sans route reste à distance 0 et n'est jamais choisi par les marchands. Si la sortie n'est pas joignable depuis l'entrée, le jeu **démolit jusqu'à 15 murs ou aqueducs**, puis **le dernier bâtiment posé** (`building_destroy_last_placed`, 308-333). Il déplace aussi la caméra (`city_view_go_to_grid_offset`, 338).
- **Pour le multijoueur, c'est un point dur.** Sur une carte partagée, ces règles doivent être calculées par joueur, avec un point d'entrée et de sortie propre à chacun. Sinon un adversaire peut murer le chemin et déclencher des destructions chez les autres.

---

## 2. Militaire et combats

### 2.1 Structures et limites
- `formation formations[MAX_FORMATIONS=50]` (`src/figure/formation.c:16`, `formation.h:7`) ; une formation compte au plus 16 figures (`MAX_FORMATION_FIGURES`). **Les plages d'identifiants sont codées en dur.** Les légions sont cherchées à partir de l'index 1 (`formation_create_legion`, 53-77, avec `faction_id = 1` et `legion_id = id - 1`). Les ennemis et les troupeaux sont cherchés à partir de 10 (`formation_create`, 79-104, avec `faction_id = 0`). La formation 0 sert de pseudo-formation à l'attaque indigène (`formation_enemy.c:594`, `figuretype/native.c:63`).
- `MAX_LEGIONS = 6` (`formation.h:9`) est appliqué à la construction (`src/building/construction_building.c:581-584`) et par le fantôme de construction (`widget/city_building_ghost.c:602`). Il est aussi **supposé par plusieurs boucles** : `formation_legion_update` ne parcourt que les ids 1..6 (`formation_legion.c:326`), `formation_legion_curse` les ids 1..6 (273, commentaire `BUG`) et `cycle_legion` 1..`MAX_LEGIONS` (`window/city.c:162-182`).
- Les coordonnées des figures sont des `unsigned char` et `grid_offset` un `short` (`figure.h:32-43`). Une carte de plus de 255 tuiles est donc impossible sans changer le format ; `GRID_SIZE` vaut 162 (`map/grid.h:9`).

### 2.2 Forts, recrutement et ordres
- Un fort crée sa légion à la pose (`construction_building.c:37`, puis `formation_legion_create_for_fort`, `formation_legion.c:16-30`, avec un étendard `FIGURE_FORT_STANDARD`).
- La caserne crée les soldats (`building_barracks_create_soldier`, `src/building/barracks.c:92-123`) pour **la légion de n'importe quel fort** qui a besoin de recrues (`get_closest_legion_needing_soldiers`, 46-71) et les envoie à **n'importe quelle académie** (74-90). Ces deux recherches parcourent tout le monde, sans filtre de propriétaire. Il existe aussi un état global caché, `static tower_sentry_request` (l.17).
- Les ordres sont des appels directs, sans commande :
  - `formation_legion_move_to` (`formation_legion.c:111-142`, qui appelle `map_routing_calculate_distances` et `city_warning_show`) ;
  - `formation_legion_return_home` (144-166) ;
  - `formation_legion_change_layout` (85-91) ;
  - `formation_toggle_empire_service` (`formation.c:150-153`) ;
  - `formation_legions_dispatch_to_distant_battle` (`formation_legion.c:189-203`).
- Ces ordres sont appelés depuis `widget/city.c:518-535` (clic sur la carte en mode militaire), `widget/sidebar/military.c:583,603,609`, `window/building/military.c:394,430`, `window/advisor/military.c:178,186` et `window/advisor/imperial.c:233`.

### 2.3 Mise à jour, moral et états des soldats
- `formation_update_all` (`formation.c:597-610`) enchaîne :
  - `formation_calculate_legion_totals` ;
  - `formation_calculate_figures`, qui reconstruit les listes de figures et les totaux d'armée, puis `city_military_update_totals` ;
  - `formation_legion_decrease_damage` (les soldats au repos guérissent) ;
  - le moral « deployed » (en réalité **quotidien**, 328-357) ;
  - `formation_legion_update`, `formation_enemy_update` et `formation_herd_update`.
- Le moral est **global par camp**. Quand une légion tombe à 20 ou moins, le moral de toutes les légions baisse de 10 et celui de tous les ennemis monte de 10, et inversement (`change_all_morale`, 314-326). Un moral bas provoque la fuite (`formation_legion.c:340-350` ; `formation_enemy.c:523-533`).
- États des soldats (`figure_soldier_action`, `figuretype/soldier.c:225-407`) : au repos, aller à l'étendard, à l'étendard (les javeliniers tirent, 84-110 ; les légionnaires attaquent les cases adjacentes, 112-117), *mop-up* (poursuite dans un rayon de 20, 119-142) et bataille lointaine (368-403). Le *mop-up* ne reste actif que s'il y a des formations ennemies, des émeutiers ou des indigènes en attaque (`formation_legion.c:351-366`).

### 2.4 Ennemis IA (invasions)
- Types d'invasion (`src/scenario/types.h:4-9`) : `LOCAL_UPRISING = 1`, `ENEMY_ARMY = 2`, `CAESAR = 3`, `DISTANT_BATTLE = 4`.
- `scenario_invasion_init` (`scenario/invasion.c:96-140`) tire le mois au hasard (2 à 9). Il crée des avertissements, jusqu'à 101, à partir des icônes de bataille de l'empire. Leur compte à rebours `months_to_go` déclenche les messages « ennemis proches ».
- `scenario_invasion_process` (319-396, chaque mois) lance `start_invasion` (191-317) :
  - la taille est ajustée par la difficulté et plafonnée à 150 (201-204) ;
  - les soldats sont répartis en 1 à 3 formations par type selon `ENEMY_PROPERTIES` (50-69, la ligne 11 étant César) ;
  - le point d'invasion est tiré au hasard si `from == 8` (239-247) ;
  - **le bâtiment présent sur la case d'apparition est détruit** (285-287) ;
  - les figures sont créées avec `faction_id = 0`, `is_friendly = 0` et `FIGURE_ACTION_151_ENEMY_INITIAL` (303-312).
- Il n'existe **qu'une nation ennemie par scénario** (`scenario.enemy_id`, `ENEMY_ID_TO_ENEMY_TYPE` 23-44) et un seul jeu de sprites ennemis est chargé (`image_load_enemy`, `game/file.c:185`).
- IA de cible (`src/figure/formation_enemy.c`) :
  - Si une légion est proche (carte `map_soldier_strength`, bâtie par `enemy_army_calculate_roman_influence`, `src/figure/enemy_army.c:88-121`), l'armée marche sur elle.
  - Sinon elle choisit un bâtiment avec `set_enemy_target_building` (172-231), selon `ENEMY_ATTACK_PRIORITY[attack_type]` (22-48 : chaîne alimentaire, or [sénat, forum], plus beaux bâtiments, troupes). À défaut, elle prend `RIOTER_ATTACK_PRIORITY` (50-56). Les bâtiments protégés par une légion sont ignorés.
  - Quand l'ennemi est plus de deux fois plus fort que les légions, il ignore les soldats (`enemy_army_is_stronger_than_legions`, `enemy_army.c:123-126`).
- **Destruction des bâtiments** : une figure en `TERRAIN_USAGE_ENEMY` qui avance sur une case « destructible » incrémente `map_building_damage`. Le seuil est de 10 pour un bâtiment, 200 pour un mur et 150 pour une porte (`src/figure/movement.c:155-186`). Au-delà, `building_destroy_by_enemy` (`src/building/destruction.c:201-229`) détruit le bâtiment et pénalise la **paix de la cité** (`city_ratings_peace_building_destroyed`).
- Mars : `mars_kill_enemies` (`formation_enemy.c:344-366`). La révolte de gladiateurs transforme `FIGURE_GLADIATOR` en `FIGURE_ENEMY54_GLADIATOR` (`figuretype/entertainer.c:154-164`) ; ces figures restent `is_friendly = 1` et ciblent selon la priorité des émeutiers (`figuretype/enemy.c:563-630`).

### 2.5 Combat : qui est ennemi de qui
Les décisions de combat ne dépendent **jamais du propriétaire**. Elles dépendent uniquement du **type** et de la **catégorie** de figure.
- **Catégories** (`src/figure/properties.h:6-14`, table `properties.c:3-85`, une ligne = un `figure_type`) :
  - ARMED (2) : préfet, les 3 types de soldats, gladiateur, dompteur et sentinelle de tour ;
  - HOSTILE (3) : types 43 à 57 (ennemis et légionnaires de César) **et le loup** ;
  - CITIZEN (1) : marchands, caravanes, ânes, travailleurs et autres citadins ;
  - CRIMINAL (4) : manifestants, criminels et émeutiers ;
  - NATIVE (5) : indigènes et marchands indigènes ;
  - ANIMAL (6) : moutons et zèbres.
- **Mêlée au contact** : `figure_combat_attack_figure_at` (`src/figure/combat.c:349-431`) est appelée à chaque changement de case de **toute** figure (`movement.c:123`) et par les légionnaires sur les 8 cases adjacentes. La matrice des lignes 374-392 donne :
  - ARMED attaque NATIVE (seulement en attaque), CRIMINAL, HOSTILE et ANIMAL ;
  - HOSTILE attaque CITIZEN, ARMED, CRIMINAL et ANIMAL.
  - Une figure accepte au plus 2 attaquants (393-395). L'attaquant doit avoir une catégorie entre 1 et 3 (352).
- **Dégâts** (`hit_opponent`, 56-114) : attaque moins défense, avec +4 dans le dos, +4 pour une légion arrêtée en formation, et en défense +7 en colonne et +4 en double ligne pour `FIGURE_FORT_LEGIONARY` et `FIGURE_ENEMY_CAESAR_LEGIONARY` arrêtés. La figure meurt quand `damage > max_damage`, puis le moral est mis à jour.
- **Recherche de cible** :
  - soldats : `figure_is_enemy || RIOTER || indigène en attaque` (151-186) ;
  - ennemis : `figure_is_legion` uniquement (236-267) ;
  - projectiles des soldats et des tours : ennemi, troupeau ou indigène en attaque (269-294) ;
  - projectiles ennemis : légion, ou tout `is_friendly` à +5 de distance si la cité a moins de 4 soldats (296-347, `enemy.c:55`) ;
  - loups (188-234).
- **Projectiles** (`src/figuretype/missile.c`) : le camp dépend **du type du projectile**. `FIGURE_ARROW` et `FIGURE_SPEAR`, tirés par les ennemis, touchent tout « citizen » dont le type est inférieur à `FIGURE_INDIGENOUS_NATIVE` (67-76, 152-188). `FIGURE_JAVELIN` et `FIGURE_BOLT`, tirés par le joueur, touchent `figure_is_enemy`, les indigènes en attaque et les animaux (83-98, 190-246).
- **Prédicats de type** : `figure_is_enemy` (types 43 à 57), `figure_is_legion` (11 à 13) et `figure_is_herd` (`src/figure/figure.c:122-135`).
- **Routage** : `is_friendly` sert seulement à `has_fighting_friendly` et `has_fighting_enemy` (`src/map/routing.c:363-381`). Les soldats routent comme des citoyens (`TERRAIN_USAGE_ANY`, puis `map_routing_citizen_can_travel_over_land`, `figure/route.c:103-106`) : ils **traversent les portes** et ne cassent rien. Les grilles de routage sont globales.

### 2.6 Ce qu'il faut changer pour le joueur contre joueur
1. **Propriétaire** : stocker un `owner` sur chaque figure, formation et bâtiment. On peut réutiliser `faction_id` en gardant la valeur 1 pour le joueur 0 afin que les sauvegardes solo restent identiques. Toutes les créations doivent le propager : `figure_create` le pose aujourd'hui à 1 (`figure.c:39`), et la caserne, les marchands et les missiles doivent hériter du tireur (`missile.c:58`, où `f->building_id` contient déjà l'id du tireur).
2. **Hostilité** : remplacer `figure_is_enemy(f)` et les tests de catégorie par une fonction du type `figure_is_hostile_to(f, owner)` dans `combat.c` (151-347, 374-392), `missile.c` (67-98), `formation_legion_update` (351-366), `enemy_army_*` et `city_figures_*`. Il faut décider si les soldats de A attaquent les citoyens de B (la mêlée ARMED contre CITIZEN n'existe pas aujourd'hui) ; aller vers un style AoE2 l'imposerait.
3. **Attaque de bâtiments** : ajouter un ordre « attaquer un bâtiment ». Il réutiliserait le mécanisme `TERRAIN_USAGE_ENEMY` et `building_destroy_increase_enemy_damage`, filtré par propriétaire. La pénalité de paix et `map_soldier_strength` devraient aussi être tenues par joueur.
4. **Routage** : les portes et les murs doivent bloquer l'adversaire mais pas leur propriétaire, ce qui demande une grille ou un test par joueur.
5. **Indices des formations** : il faut de 6 à 24 légions et `MAX_FORMATIONS`, les plages 1..6 et 10+ ainsi que `legion_id` doivent être revus. Ce changement de format doit être réservé à la sauvegarde multijoueur.
6. **Moral et totaux par camp** : `change_all_morale`, `enemy_army` totals, `city_military_update_totals`, `city_figures_*` et les recherches de caserne et d'académie doivent tenir compte du joueur.
7. Le ciblage des projectiles et des sentinelles de tour doit inclure les soldats adverses. Il faut aussi distinguer visuellement les camps (couleur, ou les sprites `GROUP_FIGURE_CAESAR_LEGIONARY` **[NON VÉRIFIÉ]**).

### 2.7 Indigènes et batailles lointaines
- **Indigènes** : `map_natives_check_land` (`src/map/natives.c:197-226`) fait monter `native_anger` jusqu'à 100. Ensuite, toute construction sur leur terre lance une attaque **globale** (`city_military_start_native_attack`, `city/military.c:71-74`). La cible est le bâtiment le plus proche du centre de réunion principal (`set_native_target_building`, `formation_enemy.c:233-264`) ; les indigènes cassent en mode `TERRAIN_USAGE_ENEMY` (`figuretype/native.c:58-81`). En multijoueur, il faudra savoir **qui** a provoqué l'attaque et **qui** est visé.
- **Bataille lointaine** : `scenario_distant_battle_process` (`scenario/distant_battle.c:39-56`) déclenche un événement `INVASION_TYPE_DISTANT_BATTLE` si les distances de l'empire dépassent 4 mois. Il est annoncé par `MESSAGE_CAESAR_REQUESTS_ARMY`, puis `city_military_init_distant_battle` donne 24 mois (`city/military.c:148-157`).
  - Le joueur coche des légions « au service de l'empire » et les envoie depuis le conseiller impérial. Les soldats marchent jusqu'à la sortie et deviennent des fantômes (`soldier.c:368-403`).
  - Le combat est résolu de façon abstraite par la force : `dispatch_soldiers` (`formation_legion.c:168-187`) calcule la force et `player_has_won` (`military.c:205-235`) les pertes.
  - Résultats (`fight_distant_battle`, 237-266) : faveur -50, -25 ou -10, ou bien +25 avec un **arc de triomphe** (`city_buildings_earn_triumphal_arch`, **seule source d'arcs**) et un retour (268-291). En cas d'échec, la ville de l'empire devient étrangère pendant 24 mois.

---

## 3. César : liste exhaustive des interventions (à supprimer ou neutraliser en multijoueur)

| # | Fichier : fonction | Déclencheur | Effet |
|---|---|---|---|
| 1 | `city/emperor.c:16-31` `city_emperor_init_scenario` (appelée en `game/file.c:302`) | début de partie | faveur initiale (`scenario.settings.starting_favor`, issue de la difficulté, `scenario.c:445,451`), épargne personnelle, rang du joueur et rang de salaire |
| 2 | `city/emperor.c:33-86` `update_debt_state` | chaque jour, trésor < 0 | 1ʳᵉ dette : **prêt de secours** `scenario_rescue_loan` ajusté par la difficulté, prospérité -3 (`ratings.c:58-65`) ; 2ᵉ : faveur -5 ; après 12 mois : faveur -10 ; après 24 mois : faveur plafonnée à 10 |
| 3 | `city/emperor.c:88-154` `process_caesar_invasion` | chaque jour, faveur ≤ 10 | avertissement, puis invasion 192 jours plus tard (**sans revérifier la faveur**), de taille 32, 64, 96 puis 144 ; pause si faveur ≥ 35, retraite si ≥ 22 ; +10 de faveur après une victoire |
| 4 | `scenario/invasion.c:366-376` et `415-423` | invasion de scénario `INVASION_TYPE_CAESAR`, ou la colère du #3 | légionnaires de César au point d'entrée, attaque « plus beaux bâtiments » (id 24) |
| 5 | `figure/figure.c:80-82`, `figuretype/enemy.c:632-671`, `figure/formation.c:186-202,268-269`, `figure/combat.c:87-89`, `figuretype/missile.c:200` | armée de César en jeu | décompte des pertes, comptage `imperial_soldiers`, pause et retraite, moral max 100, défense de formation, javelots presque sans effet contre la colonne |
| 6 | `city/figures.c:34-37,87-90` | soldats impériaux présents | comptés comme envahisseurs : défaite (`victory.c:90-99`), immigration bloquée (`migration.c:36-40`), conseillers, musique, sons d'ambiance (`sound/city.c:213`) |
| 7 | `scenario/request.c:15-116` | chaque mois, à l'année et au mois tirés | demande de biens, d'argent ou de troupes ; rappel à 12 mois ; refus : faveur -3, puis -5 après 24 mois de retard ; envoi par l'interface (`imperial.c:241`) qui retire les biens, l'argent ou la population et les armes ; reçu 1 à 4 mois plus tard : faveur `+favor`, ou `+favor/2` en retard |
| 8 | `scenario/distant_battle.c` et `city/military.c:148-305` | invasion de type 4 | voir §2.7 : faveur de -50 à +25, arc de triomphe, soldats tués |
| 9 | `city/finance.c:258-265` `pay_monthly_salary` | chaque mois, trésor > -5000 | le salaire passe du trésor à l'épargne personnelle |
| 10 | `city/finance.c:333-392` `pay_tribute` | chaque année | tribut fixe selon la population ou 25 % du bénéfice ; s'il n'est pas payé (trésor ≤ 0) : `tribute_not_paid_*`, faveur -3, -5 ou -8 (`ratings.c:515-523`) et **prospérité -1** (`ratings.c:433-435`) |
| 11 | `city/ratings.c:497-596` `update_favor_rating` | mois et année | -2 par an hors tutoriels ; malus de tribut ; salaire trop élevé par rapport au rang (524-538) ; jalons 25, 50 et 75 % : +5 ou -2 ; `open_play` force 50 (499-502) |
| 12 | `city/emperor.c:162-263` cadeaux, 301-328 dons | interface (`window/gift_to_emperor.c:110`, `donate_to_city.c:127`) | faveur +3, 5 ou 10, dégressive (remise à zéro après 12 mois, `ratings.c:503-506`) ; l'épargne passe au trésor |
| 13 | `city/emperor.c:14,275-279`, `window/set_salary.c:102` | interface | barème de salaire `SALARY_FOR_RANK` |
| 14 | `scenario/emperor_change.c:16-34` | année et mois tirés | **simple message** ; `city_ratings_reset_favor_emperor_change` (`ratings.c:105-108`) n'est jamais appelée (code mort) |
| 15 | `scenario/random_event.c:34-50`, `city/labor.c:99-118` | événement aléatoire mensuel | « Rome augmente ou baisse les salaires » (`wages_rome`, qui sert de référence au sentiment `sentiment.c:132` et à la prospérité `ratings.c:420-424`) |
| 16 | `city/resource.c:291-307,350-354` (+ granary, warehouse, market…) | option de scénario `rome_supplies_wheat` | blé gratuit dans les marchés et les maisons |
| 17 | `city/victory.c:60-65,121-127,149-157` | chaque tick | critère de faveur ; renvoi (`MESSAGE_FIRED`, puis `window_mission_end_show_fired`) ; « continuer à gouverner » remet le salaire à 0 |
| 18 | `window/mission_end.c:122-144`, `window/victory_dialog.c:26-40`, `game/settings.c:391-398` | fin de mission | promotion `campaign_rank+1` et report de l'épargne dans les réglages |
| 19 | Interface : `window/advisor/imperial.c`, `widget/sidebar/extra.c:111`, `graphics/tooltip.c:289`, `window/advisor/ratings.c:92`, `widget/city_without_overlay.c:251` (drapeau de faveur du sénat), `city/message.c:306-322` | affichage | à masquer en multijoueur |
| 20 | Empire : villes `*_ROMAN`, objets `ROMAN_ARMY`, expansion de l'empire | données d'empire | contexte romain ; neutre pour la jouabilité hors bataille lointaine |

La fiche « route de Rome » (§1.7) dépend de l'entrée de carte et non de César : **à adapter, pas à supprimer**.

---

## 4. Scénario : données, événements et victoire

### 4.1 Structure
`struct scenario_t scenario` (`src/scenario/data.h:126-240`, instance dans `scenario.c:7`). Elle contient :
- le nom, l'année de départ, le climat, `player_rank`, `initial_funds` et `rescue_loan` ;
- `rome_supplies_wheat`, `enemy_id`, `is_open_play` et `open_play_scenario_id` ;
- `win_criteria` : population, culture, prospérité, paix, faveur, `time_limit`, `survival_time` et les jalons 25/50/75 ;
- `empire` : id, expansion, durées de trajet des batailles lointaines ;
- `requests[20]`, `demand_changes[20]`, `price_changes[20]` et `invasions[20]` ;
- `earthquake`, `emperor_change` et `gladiator_revolt` ;
- `random_events` : 7 drapeaux ;
- `map` : taille et `grid_start`, avec les points d'entrée, de sortie, fluviaux, de séisme, de troupeaux (4), de pêche (8) et d'invasion (8) ;
- `allowed_buildings[50]` et `native_images` ;
- `settings` (rang et mission de campagne, `is_custom`, faveur et épargne de départ, nom du joueur).

La structure est sérialisée champ par champ (`scenario_save_state` et `scenario_load_state`, `scenario.c:14-438`).

### 4.2 Événements

| Événement | Code | Déclencheur | Effet |
|---|---|---|---|
| Séisme | `scenario/earthquake.c:32-148` | année du scénario, mois aléatoire de 2 à 9 ; durée et cadence selon la gravité | se propage depuis `earthquake_point`, détruit et met le feu, change le terrain, recalcule le routage |
| Révolte des gladiateurs | `scenario/gladiator_revolt.c:16-44` | année, mois de 3 à 6, dure 3 mois, seulement si une école de gladiateurs est active | gladiateurs hostiles, tués à la fin (`enemy.c:568-575`) |
| Changement d'empereur | `scenario/emperor_change.c` | année et mois aléatoire | message seulement |
| Événements aléatoires | `scenario/random_event.c:115-141` | chaque mois, `RANDOM_EVENT_PROBABILITY[random_byte()]` (23-32), filtré par les drapeaux | salaires de Rome ; perturbation terrestre ou maritime de 48 jours (`city_trade_start_*`) ; eau contaminée (santé -25 à -50) ; effondrement d'une mine de fer ; inondation d'une glaisière |
| Variation de demande | `scenario/demand_change.c:20-48` | année, mois de 2 à 9 | quota de la route ±1 cran, avec message |
| Variation de prix | `scenario/price_change.c:19-41` | année, mois de 2 à 9 | prix global ± montant |
| Invasions et soulèvements | `scenario/invasion.c:319-396` | année et mois, avertissements sur la carte de l'empire | §2.4 |
| Demandes de César | `scenario/request.c` | §3 #7 | — |
| Bataille lointaine | `scenario/distant_battle.c` | §2.7 | — |
| Expansion de l'empire | `scenario/empire.c:18-31` | `expansion_year` | nouvelles villes commerçantes |

Les fonctions d'initialisation (`scenario_request_init`, `_demand_change_init`, `_price_change_init`, `_invasion_init`) consomment chacune **20 tirages RNG** au démarrage (`random_generate_next` dans la boucle). Retirer ou réordonner ces appels change toute la suite aléatoire du solo.

### 4.3 Victoire, défaite et limites de temps
- `city_victory_check` (`city/victory.c:106-140`) est appelée **à chaque tick**. Elle sort immédiatement en `open_play` (108-110). Sinon `determine_victory_state` (37-104) déclare la victoire quand tous les critères actifs sont atteints. La défaite survient :
  - à `max_year` si `time_limit` est actif (la victoire si c'est `survival`) ;
  - si les envahisseurs sont plus de 2 + le nombre de soldats et que la population est tombée sous le quart de son maximum ;
  - si des envahisseurs sont présents et que la population est nulle.
- Il existe un bogue d'origine : la survie ne fonctionne que sans autre critère (76).
- **La simulation ouvre directement des fenêtres** : `window_victory_dialog_show` et `window_mission_end_show_*` (123, 132, 136).
- `scenario_criteria_init_max_year` (`scenario/criteria.c:91-100`) calcule `start + time_limit`, ou `start + survival`, sinon `start + 1000000`. Les jalons 25/50/75 ne servent qu'à la faveur (§3 #11).
- « Continuer à gouverner » donne 24 ou 60 mois (`victory_dialog.c:18-22,105-111` ; `victory.c:142-157`).

---

## 5. Campagne et modes de jeu

- **Menu principal** (`window/main_menu.c:108-128`) : nouvelle carrière, chargement, scénario personnalisé (CCK), éditeur et options.
- **Campagne** :
  - `window_new_career_show` réinitialise les réglages (`new_career.c:34-39`), puis on passe à `window_mission_selection_show`.
  - Pour chaque rang, `game/mission.c:5-40` (`MISSION_IDS[12]`, `RANK_CHOICE`) propose le choix entre mission pacifique et militaire (`mission_selection.c:128,138`). Vient ensuite le briefing, puis `game_file_start_scenario_by_name` (`mission_briefing.c:53`).
  - `start_scenario` (`game/file.c:278-309`) appelle `load_campaign_mission` (258-276), qui lit **une sauvegarde** dans `mission1.pak` via une table d'offsets.
  - Une partie gagnée passe par `window_mission_end_show_won`, puis `advance_to_next_mission` (`mission_end.c:122-144`), qui incrémente le rang et revient au menu au-delà de 11. En cas de renvoi, le joueur revient à la sélection de mission (160-171).
  - Les tutoriels correspondent aux rangs 0 à 2 hors scénario personnalisé (`scenario/property.c:36-49`, `game/tutorial.c`).
  - Une sauvegarde de début de mission est écrite par rang (`game/file.c:78-91,372-391`).
- **Scénario personnalisé** : `window_cck_selection_show` appelle `scenario_set_custom(2)` (`cck_selection.c:74` ; la valeur 2 n'est jamais distinguée de 1) et liste les fichiers `*.map`. Le chargement passe par `game_file_start_scenario`, puis `load_custom_scenario` (`game/file.c:191-201`), avec `clear_scenario_data` et `initialize_scenario_data` (93-189). Dans ce mode, le rang vient de `scenario.player_rank` et l'épargne est remise à 0 (`emperor.c:22-26`). `campaign_mission` n'est pas remis à zéro : la taille de l'insurrection de Mars dépend alors de la dernière mission de campagne jouée (`invasion.c:398-413`) **[NON VÉRIFIÉ en jeu]**.
- **Éditeur** : `scenario_editor_create` (`scenario/editor.c:29-97`) fixe par défaut 4 critères à 10, toutes les constructions autorisées, 1000 de fonds et 500 de prêt. Il offre une API complète pour les demandes, invasions, prix et demandes de commerce (106-292).
- **« Jeu libre »** : il n'y a **pas de mode dédié**. Le seul mécanisme est l'option d'éditeur **open play** (`scenario_editor_toggle_open_play`, `editor.c:431-438`, bouton `window/editor/win_criteria.c:70-75`, id forcé à 12). Ses effets se limitent à :
  - aucune victoire ni défaite (`victory.c:108`) ;
  - faveur remise à 50 chaque mois (`ratings.c:499-501`) ;
  - objectifs masqués (`sidebar/extra.c:111`, `tooltip.c:267-291`, `advisor/ratings.c:44-93`, `cck_selection.c:161-165`).

  **Demandes, invasions (y compris de César), batailles lointaines, dette, tribut et salaire restent actifs.** Un cas limite **[NON VÉRIFIÉ]** : après environ 24 mois de dette, `city_ratings_limit_favor(10)` est suivie dans le même appel de `process_caesar_invasion`. Cela arme une invasion de César 192 jours plus tard, même si la faveur revient ensuite à 50.

---

## 6. Proposition : jeu libre multijoueur sans César

### 6.1 Principe
Il faut introduire un objet « règles de partie » (mode multijoueur, nombre de joueurs, points d'entrée et de sortie par joueur, difficulté commune) et **neutraliser par condition**, au lieu de supprimer. C'est le seul moyen de garder les 36 tests verts et le format solo octet pour octet (§0). Tout l'état multijoueur (propriétaires, cités 2 à 4, routes par joueur) va dans un format de sauvegarde multijoueur distinct.

### 6.2 Garder, supprimer ou adapter

| Système | Décision | Points d'entrée à toucher |
|---|---|---|
| Faveur, cadeaux, salaire, dons, rangs, promotions | **Supprimer** (mode multijoueur) | `city_emperor_update` (tick.c:125), `update_favor_rating`, `pay_monthly_salary`, `window/advisor/imperial.c` et fenêtres associées, conseillers et barres latérales |
| Demandes de César | **Supprimer** | `scenario_request_process` (tick.c:80), `scenario_request_init` |
| Invasions de César et colère | **Supprimer** | `process_caesar_invasion`, branche `INVASION_TYPE_CAESAR` (invasion.c:366) |
| Bataille lointaine et service de l'empire | **Supprimer** ; prévoir une nouvelle source d'arcs de triomphe | `scenario_distant_battle_process` (tick.c:78), boutons d'*empire service* |
| Changement d'empereur | **Supprimer** | tick.c:181 |
| Tribut | **À décider** : sa disparition change l'équilibre interne (trésor, prospérité via `has_made_money`, `ratings.c:153-157`) | `pay_tribute` (finance.c:333) |
| Prêt de secours et dette | **Adapter** : une règle de faillite multijoueur sans faveur | `update_debt_state` |
| Salaires de Rome | **Garder** comme référence économique (sentiment et prospérité en dépendent) ; événements aléatoires optionnels | random_event.c:34-50 |
| Séisme, révolte de gladiateurs, eau contaminée, mines | **Garder**, à rendre propres à chaque joueur (point de séisme, école) | earthquake.c, gladiator_revolt.c, random_event.c |
| Commerce avec les villes IA, prix, quotas, variations | **Garder et rendre propre à chaque joueur** (`is_open`, quotas et compteurs par joueur ; prix globaux possibles) | `empire_city_*`, `trade_route_*`, `city_trade_update` |
| Invasions IA, soulèvements, Mars | **Optionnel (« PvE »)** : il faut désigner une cité cible | `start_invasion`, `set_enemy_target_building` (filtre par propriétaire) |
| Indigènes | **Garder neutres** ; colère et cible propres au joueur fautif | `map_natives_check_land`, `set_native_target_building` |
| Victoire et défaite | **Remplacer** par des conditions multijoueur globales (dernier survivant, score à partir de culture, prospérité, paix et population, limite de temps) | `city_victory_check` (tick.c:182) |
| Campagne, tutoriels, CCK | **Hors multijoueur** : point d'entrée lobby, puis chargement de carte, sans `mission1.pak` | `window/main_menu.c`, `game/file.c:start_scenario` |

### 6.3 Commerce entre joueurs (options)
- **A, minimal** : chaque joueur commerce seulement avec les villes IA, avec un état de route propre à chaque joueur et un point d'entrée propre. Les joueurs n'interagissent qu'indirectement, par exemple en attaquant les caravanes adverses.
- **B, joueur à joueur virtuel** : chaque autre joueur apparaît comme une `EMPIRE_CITY_TRADE` dont `buys` et `sells` reprennent ses statuts `TRADE_STATUS_IMPORT/EXPORT` et son `export_over`. La transaction est atomique dans le tick : le vendeur retire les biens de ses entrepôts et encaisse le prix de vente, l'acheteur les reçoit et paie le prix d'achat ; l'écart sert de taxe ou de puits d'argent. En lockstep, tout l'état est sur tous les pairs : **aucun message réseau n'est nécessaire hormis les commandes** (ouvrir une route, régler les statuts).
- **C, physique à la AoE2** : une caravane charge chez A, marche sur la carte partagée jusqu'aux entrepôts de B et peut être interceptée. Cette option réutilise `figure_trade_caravan_action`, mais exige des entrepôts filtrés par propriétaire, une origine qui n'est plus le point d'entrée et une logique de chargement et de déchargement en deux temps.

### 6.4 Guerre entre joueurs : étapes
Les étapes sont détaillées au §2.6 : ajouter le propriétaire, définir l'hostilité, ajouter l'ordre d'attaque de bâtiment, rendre le routage des portes dépendant du propriétaire, réindexer les formations, puis rendre moral et totaux propres à chaque camp. Il faut aussi rendre propres à chaque cité : la pénalité de paix, l'arrêt de l'immigration (`migration.c:36-40`), `city_figures_total_invading_enemies` et les sons et musiques de combat.

### 6.5 Pièges lockstep propres à ces systèmes
- **Interface qui modifie l'état de la simulation** :
  - `city_emperor_calculate_gift_costs` appelée dans `draw_background` (`imperial.c:92`) ;
  - `city_military_clear_empire_service_legions` au clic (`imperial.c:249`) ;
  - `city_emperor_init_selected_gift` et `city_emperor_init_donation_amount` à l'ouverture des fenêtres ;
  - les ordres de légion, l'ouverture de route et les réglages d'échange, tous appelés directement.

  Tout cela doit devenir des commandes exécutées à un point fixe du tick, ou des fonctions en lecture seule.
- **Simulation qui appelle l'interface** : fenêtres de victoire, `city_warning_show` (`formation_legion.c:128`, `maintenance.c:336-337`), `city_view_go_to_grid_offset` (`maintenance.c:338`), `city_message_post` (messages à router par joueur), et les sons joués depuis la simulation (`enemy.c:30-34`).
- **Réglages locaux lus par la simulation** : `setting_difficulty()` (`game/difficulty.c`) et `config_get(CONFIG_GP_*)`. Ils doivent être identiques sur tous les pairs et fixés dans les règles de partie. La sauvegarde automatique mensuelle se fait dans le tick (tick.c:101-103).
- **RNG global unique** (`core/random.c`), lu par les invasions, événements, demandes, ciblage aléatoire et la création de figures. Dès qu'il y aura plusieurs cités, l'ordre de parcours des joueurs fera partie de l'état déterministe.

### 6.6 Questions ouvertes
1. Faut-il garder le tribut et le prêt de secours, sous un autre nom, pour conserver l'équilibre économique « exact » de la cité ?
2. Quel modèle de commerce entre joueurs choisir (A, B ou C) ? Prix globaux ou par joueur ?
3. Les soldats peuvent-ils tuer les citoyens adverses et casser n'importe quel bâtiment ? Que deviennent les tours et les portes ?
4. Comment gagner un arc de triomphe sans bataille lointaine ?
5. Que fait la colère de Mars en multijoueur, alors que la taille du soulèvement est indexée sur la mission de campagne ?
6. Faut-il conserver les invasions IA ? Si oui, contre quel joueur ?
7. Un ou plusieurs fichiers d'empire ? Faut-il générer des objets d'empire en code (`MAX_CITIES = 41`, `MAX_ROUTES = 20`) ?
8. Pour une carte de plus de 162 tuiles, les coordonnées `u8` des figures et `grid_offset` en `short` bloquent : il faut un nouveau format (à croiser avec la cartographie « carte »).

---

## 7. Non vérifié et limites de cette cartographie
- Que les champs `faction_id` et `other_player` soient des vestiges d'un multijoueur prévu dans Caesar III est une hypothèse.
- Le comportement de l'original pour le changement d'empereur (remise de la faveur à 50) n'a pas pu être vérifié : dans Julius, la fonction n'est jamais appelée.
- Le cas limite de l'open play (invasion de César après une dette prolongée) et la dépendance de Mars à `campaign_mission` en scénario personnalisé ne sont établis que par lecture du code.
- Le contenu des fichiers `c3.emp`/`c32.emp` et `mission1.pak` (nombre d'empires, positions) n'a pas été inspecté, et la sémantique exacte de `is_custom = 2` n'est pas connue.
- La présence de sprites utilisables pour distinguer les légions adverses n'a pas été vérifiée, et le jeu n'a pas été lancé.
