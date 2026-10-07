# Conception technique — Caesar III multijoueur

> Le **comment**. Les exigences sont dans [VISION.md](VISION.md), le découpage dans [ROADMAP.md](ROADMAP.md),
> l'historique des choix dans [DECISIONS.md](DECISIONS.md). Les références `fichier:ligne` détaillées sont dans
> [code-map/](code-map/). Ce document se modifie quand une décision change, en ajoutant une entrée dans DECISIONS.md.

## 0. Principes directeurs

1. **Le mode classique ne bouge pas d'un bit.** Une cité, César, format de sauvegarde 0x66 : c'est la référence,
   vérifiée par les tests de parité avec le jeu original. Toute nouveauté est **inactive en mode classique**. On
   **neutralise** le code de César et de la campagne par une condition, on ne le supprime pas.
2. **Lockstep déterministe.** Chaque machine simule toute la partie. Seules les **commandes** des joueurs
   circulent sur le réseau. La simulation est une fonction pure de l'état initial, des règles et des commandes.
3. **Une cité = le code original, exécuté dans le contexte de cette cité.** On ne réécrit pas la logique interne :
   on la fait tourner une fois par cité (exigence E7).
4. **Isolement économique.** Chaque cité a son état, ses identifiants et son générateur aléatoire. Même branchées
   par la route, les cités n'agissent l'une sur l'autre que par des mécanismes multijoueur explicites : commerce,
   guerre et voisinage (désirabilité).
5. **Petits pas vérifiés.** Chaque étape garde `ctest` vert et ajoute son propre test (voir [TESTING.md](TESTING.md)).

## 1. Vue d'ensemble

```
┌──────────────────────────── Simulation (identique sur tous les PC) ────────────────────────────┐
│ Règles de partie (game_rules) : mode, joueurs, difficulté, dieux, correctifs, victoire…         │
│                                                                                                  │
│ MONDE (partagé)                          CITÉ p  (p = 0..3, une par joueur)                      │
│  · grilles de la carte (taille variable)  · city_data (actuel singleton, une instance par cité)  │
│  · grille de propriété des infrastructures · city_extra : les états `static` « par cité »         │
│  · calendrier (tick, jour, mois, année)   · générateur aléatoire de la cité                       │
│  · empire (définition), prix             · tranche d'ids : bâtiments, figures, formations,       │
│  · générateur aléatoire du monde            chemins, stockages                                    │
│  · entités neutres (tranche « monde ») :  · messages, compteurs, routes commerciales             │
│    indigènes, animaux, envahisseurs PvE   · point d'arrivée, autorisations d'exploiter          │
└──────────────────────────────────────────────────────────────────────────────────────────────────┘
┌──────── Local (propre à chaque PC, hors simulation, hors somme de contrôle) ────────┐
│ caméra, orientation de la vue, fenêtres, aperçu de construction, popups, sons…      │
└─────────────────────────────────────────────────────────────────────────────────────┘
```

## 2. Modes de jeu et règles

- **Classique** : campagne, scénario personnalisé, éditeur, comme Julius. Conservé pour les tests et comme
  référence. Il sera masqué de l'interface et accessible seulement par une option de développement (`--classic`).
- **Multijoueur** : de 1 à 4 joueurs. Une partie à 1 joueur est la « partie libre solo », utile pour s'entraîner
  et tester. Un même chemin de code sert à toutes les tailles de partie.
- `game_rules` (nouveau, état de simulation, sauvegardé) contient tout réglage que la simulation lit :
  difficulté, dieux, correctifs de gameplay, fonds de départ, condition de victoire, menaces neutres, vitesse.
  - En mode classique, ses accesseurs renvoient les réglages locaux (`c3.inf`, `julius.ini`), donc rien ne change.
  - En multijoueur, ils renvoient les valeurs choisies dans le salon. Ainsi un `c3.inf` local ne peut plus
    désynchroniser : une difficulté « très facile » sur une seule machine fait échouer 14 tests de parité sur 16.
    `tools/check-determinism.sh` refuse toute autre lecture des réglages locaux que les règles remplacent
    (difficulté, dieux, correctifs, D-073).
  - Les règles sont écrites dans la pièce `mp_header` du `.mpsav`, avec la taille de la carte et le nombre de
    joueurs. Elles comprennent aussi la carte préparée (`prepared_map`, D-069), les invasions de l'IA, le brouillard,
    les territoires et la fin de partie (score de César).

## 3. Moteur multi-cités

### 3.1 Contexte de cité (option A de code-map/02)

- `city_data` devient `#define city_data (*city_current)` dans `src/city/data_private.h`, avec
  `city_datas[MAX_PLAYERS]`. Les 2 111 lignes qui l'utilisent ne changent pas.
- Les ~12 états `static` « par cité » dispersés (couverture culturelle, compteurs de bâtiments, greniers,
  sentinelles, totaux de formations, messages, `labor.c:312`…) passent dans une structure `city_extra[p]`
  sélectionnée par le même contexte.
- Le **générateur aléatoire** devient propre à chaque cité (contexte), plus un générateur « monde ». C'est
  indispensable au test d'isolement : une cité ne doit pas voir les tirages de ses voisines.
- API : `sim_context_set(p)`, `sim_context_push(p)` / `sim_context_pop()` avec contrôle d'équilibre, et
  `sim_context_world()`. Les interactions entre cités (commerce, combat) utilisent une API explicite avec
  identifiant de joueur.
- Vocabulaire : **joueur / `player_id` / `owner`**. « city » et `city_id` désignent déjà les villes de l'empire.

### 3.2 Tranches d'identifiants (exigence E5 : limites par joueur)

Chaque tableau d'entités est découpé en tranches de la taille d'origine :

| Tableau | Taille d'origine = tranche | Tranches |
|---------|----------------------------|----------|
| bâtiments | 2 000 | une par joueur + une « monde » |
| figures | 1 000 | idem |
| formations | 50 (légions dans les ids 1 à 9, ennemis à partir de 10) | idem, avec plages relatives à la tranche |
| chemins (`route_paths`) | 600 × 500 pas | idem |
| stockages | 200 | idem |
| listes de travail (`building/list.c`) | 500 / 2 000 / 500 | une par contexte |

- La tranche `p` occupe les ids `[p·N, (p+1)·N)`. L'id local 0 de chaque tranche est réservé, comme l'id 0
  d'origine. L'allocation prend le premier emplacement libre **dans la tranche du contexte**. Les boucles
  « de cité » parcourent la tranche courante ; les boucles « monde » (rendu, minicarte) parcourent tout.
- Avec un seul joueur, il n'y a qu'une tranche qui **est** le tableau d'origine : ordre, allocation et limites
  sont identiques au bit près.
- Chaque joueur dispose donc des limites d'origine (2 000 bâtiments, 1 000 figures…), indépendamment des autres.
  On pourra les relever plus tard en agrandissant les tranches.
- Les ids restent globaux et uniques : les grilles (`building_grid`, `figure_grid`) et les références croisées
  n'ont pas besoin de savoir à qui appartient l'entité. Le propriétaire se déduit de l'id :
  `owner = id / taille_tranche`.

### 3.3 Ordonnancement d'un tick à N cités

La table de code-map/01 §2.2 indique la portée de chaque tâche : C (agrégats de la cité), B (boucle de
bâtiments), M (carte), W (monde). Ordre d'un tick :

1. Appliquer les commandes prévues pour ce tick, triées par (joueur, numéro de séquence).
2. Pour chaque cité active, par ordre d'index croissant, dans son contexte : tirage de son générateur, puis la tâche
   C/B du numéro de tick.
3. Les tâches M/W, une seule fois, dans le contexte « monde » ou en parcourant les tranches dans l'ordre.
4. Jour, mois et année : d'abord pour chaque cité, puis pour le monde. Le calendrier est commun.
5. Les figures, tranche par tranche (cité 0, cité 1, …, monde), chacune dans le contexte de sa tranche.
6. Événements (séisme : monde ; révolte des gladiateurs : par cité), puis victoire multijoueur.

Avec une seule cité, cet ordre se réduit exactement à l'ordre d'origine.

### 3.4 Propriété et construction partout (D-018)

- **Construction libre** sur toute case libre de la carte. Ce que l'on pose appartient à celui qui le pose :
  - bâtiments, figures et formations : propriétaire déduit de la tranche d'ids (§3.2) ;
  - infrastructures de terrain (routes, murs, aqueducs, jardins, places, ponts), qui ne sont pas des bâtiments :
    nouvelle grille `owner` (u8 par case, 0xFF = personne), sauvegardée dans `.mpsav`.
- On ne démolit que ce qui nous appartient. Détruire chez l'autre passe par la guerre (§7).
- **Branchements** : les routes de plusieurs joueurs forment un même réseau. Les personnages y circulent librement,
  mais **n'agissent que sur les bâtiments de leur propriétaire** :
  - services : la couverture n'est appliquée qu'aux maisons du propriétaire (`figure/service.c`) ;
  - main-d'œuvre : les recruteurs ne comptent que les maisons du propriétaire ;
  - marchés, charrettes, entrepôts, greniers, ateliers, migrants, pompiers : chaque recherche de cible filtre par
    propriétaire (liste : code-map/03 §6.2 et code-map/02 §4.2).

  Seuls les caravanes (§6) et les soldats (§7) agissent chez les autres.
- **Eau** : un réservoir n'est alimenté que par les aqueducs de son propriétaire, et ne dessert que ses fontaines
  et ses maisons.
- **Désirabilité** : elle traverse les cités. Un beau quartier voisin profite à vos maisons, un atelier voisin leur
  nuit.
- **Route de Rome** (`building_maintenance_check_rome_access`) : calculée pour chaque cité depuis son point
  d'arrivée (§5.2), sur tout le réseau routier. La démolition automatique en cas de blocage ne touche que les murs,
  aqueducs et bâtiments du joueur bloqué.
- Fidélité : une cité **non branchée** se comporte exactement comme l'original (test « jumeaux », M3.7). Branchée,
  ses personnages peuvent errer chez le voisin : c'est l'effet voulu du branchement. Un test vérifie que rien ne
  traverse en dehors des déplacements : aucune couverture, aucun ouvrier, aucune marchandise ni aucun incendie
  éteint chez l'autre.

### 3.5 Grille de taille variable (exigence E6)

- `GRID_SIZE` devient une valeur d'exécution (`map_grid_size()`) fixée au chargement de la carte : 162 pour une
  partie classique, plus grande pour une carte multijoueur (objectif de 400 de côté, plafond technique 512).
  Les sauvegardes classiques gardent leur pas de 162 : **aucune conversion**, les tests de parité restent valables.
- Les grilles sont dimensionnées pour le plafond. Les tables d'offsets écrites avec ±162 (`ROUTE_OFFSETS`…) sont
  calculées au chargement.
- On élargit les types en mémoire : coordonnées `u8` → `u16`, `grid_offset` `short` → `int32`. Le format
  classique continue de les écrire sur leur largeur d'origine.
- Les limites de routage (500 pas, distance 998) restent celles d'origine en mode classique. Elles sont
  proportionnelles à la taille de la carte en multijoueur, pour les longs trajets (marchands, armées).
- À surveiller : la performance du routage (BFS sur toute la grille, `memset` de `routing_distance` à chaque
  appel), à mesurer sur les grandes cartes.

### 3.6 Déterminisme et état caché

- **Remise à zéro de l'état caché** à chaque démarrage ou chargement de partie : générateur entier,
  `fire_spread_direction`, compteurs d'images, grille `strength`, `map_point last`, tailles des listes,
  `start_building_id`. Un chargement ne dépend alors plus de l'historique du processus, sans casser la parité
  (expérience E1, code-map/01 §3.6).
- La **sauvegarde multijoueur** contient tout l'état caché : reprendre une partie équivaut exactement à la
  continuer.
- La simulation n'utilise aucun flottant, aucune horloge ni `rand()`, ce qui est déjà vrai aujourd'hui. Un
  script de vérification dans `tools/check.sh` le maintient.
- Les **sommes de contrôle des fichiers de données** qui influencent la simulation (`c3_model.txt`,
  `c3.emp`/`c32.emp`, table des groupes d'images, carte) sont comparées dans le salon. Les fichiers de texte
  (`.eng`) n'en font pas partie : des joueurs avec des langues différentes peuvent jouer ensemble.
- Les sauvegardes, automatiques comprises, ont lieu **entre deux ticks** en multijoueur.

### 3.7 Couche de commandes et séparation interface / simulation

- Toute action du joueur qui modifie la simulation devient une **commande** : type, joueur, paramètres complets
  en valeurs absolues (type de bâtiment résolu, rectangle, orientation de pose, confirmations, longueur du pont
  recalculée par la simulation). Elles sont sérialisées en petit-boutiste. L'inventaire complet des actions est
  dans code-map/05 §4.
- Exécution : en solo local, immédiatement (comme aujourd'hui, la construction reste possible pendant la
  pause). En réseau, au tick `T + délai`, identique pour tous.
- Les commandes sont **enregistrables et rejouables** : sauvegarde + journal de commandes = rejeu exact. C'est
  l'outil de test et de débogage des désynchronisations.
- **Ce que l'interface ne doit plus faire en multijoueur** (code-map/01 §3.5 et 05) :
  - modifier la simulation depuis l'interface. L'aperçu de construction ne doit plus écrire dans la carte ou
    doit être annulé avant les ticks, l'effacement ne doit plus marquer les vrais bâtiments, le rendu ne doit
    plus réécrire les animations dans des grilles sauvegardées. Ouvrir un conseiller ne doit plus recalculer
    l'état : ce recalcul devient une commande, ce qui reste fidèle à C3 ;
  - mettre la simulation en pause parce qu'une fenêtre est ouverte, pendant un tracé ou un défilement. En
    multijoueur, le temps ne s'arrête que par une pause commune, comme dans AoE2 ;
  - dépendre de l'orientation de la vue. La simulation utilise une **orientation canonique** (nord) et la rotation
    devient purement visuelle ;
  - annuler (undo). L'annulation est désactivée en multijoueur : elle restaure des grilles entières et
    effacerait les constructions des voisins.
- Les messages et popups émis par la simulation sont adressés à un joueur et ne s'affichent que chez lui.

### 3.8 Somme de contrôle d'état

Hachage stable (FNV-1a 64 bits) de tout l'état de simulation, état caché compris. On en exclut ce qui dépend de
l'interface : caméra, sons, animations de l'eau, `sprite_grid` des bâtiments, phrases des figures, `is_read`
des messages, compteurs de routage (code-map/01 §6.5). Il sert à détecter les désynchronisations à chaque tour
réseau et à trouver par dichotomie le premier tick fautif dans les tests.

## 4. Réseau local (lockstep)

- Topologie en **étoile, TCP** : l'hôte est le joueur 0 et relaie tout. On est sur un réseau local avec au plus
  4 joueurs : la simplicité prime.
- **Tours** de K ticks (environ 100 ms de jeu : K = 6 à 100 %). Une commande émise pendant le tour T est exécutée
  au tour T+2. Chaque client envoie ses commandes du tour, éventuellement vides, avec la somme de contrôle du
  tour. L'hôte diffuse l'ensemble quand il a tout reçu. Un client n'exécute le tour T que s'il dispose de toutes
  les commandes de T.
- Désynchronisation : sommes de contrôle différentes. On met le jeu en pause, chaque pair écrit une sauvegarde
  multijoueur, puis on les compare avec l'outil `compare`.
- **Salon** : l'hôte choisit la carte et les règles, les clients rejoignent par découverte UDP sur le réseau local
  ou par adresse IP. On vérifie les versions (`PROTOCOL_VERSION` de `mp/lockstep.c`, 18 depuis T4.1 ; il change à
  chaque changement de l'état ou des messages) et les sommes de contrôle des données. Tous démarrent par le même
  chemin déterministe (remise à zéro de l'état caché, chargement de la carte, application des règles).
- **Règles du salon** (D-063, D-069, D-073) :
  - les réglages affichés sont un état de l'interface (`mp/lobby`) : carte (« Carte 1 », « Carte 2 », « Carte au
    hasard », par défaut au hasard), difficulté (« facile » par défaut), dieux, fin, invasions de l'IA, brouillard ;
  - l'hôte peut les changer jusqu'au lancement. Chaque changement passe par `mp_lockstep_set_rules` et part chez
    les clients (message `MSG_RULES`), qui les affichent sans pouvoir les changer. « Lancer la partie » les relit
    (`mp_lobby_start_game`). Les règles partent aussi avec l'accueil (`MSG_WELCOME`) ;
  - reçues du réseau, elles sont vérifiées (`game_rules_settings_valid`) : hors bornes, elles sont ignorées dans le
    salon, et l'accueil est refusé ;
  - « Carte au hasard » : l'hôte tire la carte avec la graine du salon (`mp_mapgen_choose_prepared_map`). La règle
    garde la carte tirée, et les clients reçoivent la carte toute faite dans la sauvegarde ;
  - une partie reprise d'un `.mpsav` garde les règles de sa pièce `mp_header` : le salon les montre grisées
    (`mp_lobby_rules_editable`). Une carte `.mpmap` garde ses territoires ; le reste vient du salon ;
  - les règles par défaut des tests et de la ligne de commande (`--mp-host`) restent en difficile, sur la carte 1.
- Vitesse et pause sont des commandes de l'hôte. En cas de déconnexion, le jeu se met en pause et l'hôte peut
  continuer sans le joueur parti. La cité de ce joueur reste figée ou est retirée : à décider plus tard.
- Implémentation : une petite couche `src/platform/net*.c` (sockets POSIX et Winsock), sans nouvelle dépendance.

## 5. Jeu libre multijoueur et César

> **Revu par D-050 (2026-10-06)** : César revient comme arbitre (lauriers, faveur, campagnes, colère). Le plan
> complet est dans [CESAR.md](CESAR.md) ; la liste ci-dessous décrit l'état actuel du code, qui sera rouvert
> mécanique par mécanique aux jalons M9 et M10.
>
> Depuis M9.1, `mp/caesar` tient les lauriers de chaque cité (en dixièmes, détaillés par source : les cinq notes,
> puis fêtes, dons, campagnes, demandes, guerres, armée, punition) et la jauge commune de colère avec la part de
> chacun. C'est une pièce commune du `.mpsav` (`mp_caesar`), comptée dans la somme de contrôle ; une partie
> classique repart toujours d'un état vide. Tous les réglages chiffrés sont dans `mp/caesar_rules.h`, la seule
> table à modifier (CESAR §10.1).
>
> Depuis D-057 : chaque mois, chaque cité calcule ses cinq notes (`mp_caesar_update_city_month`, après les fêtes
> dans `advance_month`) et en reçoit les lauriers. Les restes en centièmes sont gardés pour ne rien perdre. La fin
> `GAME_END_CAESAR` (`mp/endgame`) s'arrête quand une cité atteint `caesar_score` (règles du salon). Interface :
> `window/mp_imperial` (conseiller impérial), `window/mp_caesar_letter` (lettres, état d'affichage non sauvegardé),
> bandeau `widget/mp_status`.
>
> Depuis D-067 : cadeau, salaire et don à la cité sont trois commandes (`MP_ACTION_SEND_GIFT`, `MP_ACTION_SET_SALARY`,
> `MP_ACTION_DONATE`), appliquées dans la cité de l'expéditeur avec le code d'origine (`city/emperor.c`). La taille du
> cadeau et le montant du don choisis dans la fenêtre restent un état de la fenêtre. `mp_caesar_gift_sent` donne les
> lauriers du cadeau et lance l'attente de 12 mois (compteur par joueur dans `mp_caesar`, version 3) ; la faveur
> d'origine n'est jamais touchée (D-073). Le salaire est de nouveau versé, au plus le rang : la commande refuse un
> rang plus haut, et `mp_caesar_limit_salary` le rabaisse chaque mois (`city/finance.c`).
>
> Depuis D-071 : l'estime de César n'est qu'une lecture des lauriers (`mp_caesar_esteem` : lauriers rapportés au
> score, ou au rang suivant sans score). `mp_caesar` garde les lauriers de chaque cité à la fin de ses 12 derniers
> mois (version 4), d'où le gain du mois et la tendance. Le 4e pilier de l'évaluation (`window/mp_ratings`), le sénat
> et la barre latérale montrent les lauriers ; la faveur n'est plus affichée en multijoueur.

### 5.1 Interventions de César neutralisées (liste complète : code-map/04 §3)

Désactivées quand `game_rules.mode == MP` :
- faveur ;
- demandes impériales ;
- colère et invasions de César ;
- batailles lointaines et service de l'empire ;
- changement d'empereur ;
- rangs et promotions ;
- victoire et renvoi de la campagne ;
- blé fourni par Rome.

Leurs écrans et indicateurs sont masqués.

Conservés, car ils relèvent de l'économie ou de la vie interne de la cité :
- salaires de Rome, qui servent de référence au sentiment et à la prospérité ;
- événements aléatoires, désactivables ;
- dieux ;
- séismes ;
- révolte des gladiateurs ;
- épidémies ;
- règle du trésor à −5 000 (plus de construction possible) ;
- tribut annuel et prêt de secours (D-026 ; la dette fait encore baisser la faveur, `city/emperor.c` `update_debt_state`) ;
- cadeaux, salaire, épargne et don à la cité, par commandes (D-067), avec des lauriers au lieu de faveur ;
  boutons du conseiller impérial visibles.

Impact assumé (H2) : le salaire du gouverneur, de nouveau versé (D-067), est limité par le rang ; au rang 0, il
est nul.

### 5.2 Arrivée des joueurs

Chaque joueur a un **point d'arrivée** sur la carte : c'est l'endroit où il « atterrit ». Ce point sert d'entrée et
de sortie de sa cité :
- les immigrants et les caravanes des villes de l'empire y arrivent ;
- la règle de la « route de Rome » s'y rapporte.

Les **autorisations d'exploiter** du joueur sont dans l'empire de sa cité (§6.2). L'arrivée des navires est propre à
chaque cité (point d'entrée de la rivière par cité) : sur les cartes préparées, le bout du bras de mer le plus
proche.

### 5.3 Entités neutres

Indigènes, animaux et envahisseurs IA (option « menaces neutres ») appartiennent à la tranche « monde ».
- La colère des indigènes et ses cibles visent le joueur fautif.
- Les invasions IA visent un joueur désigné par la règle, par exemple à tour de rôle.

### 5.4 Fin de partie

> Remplacé à M9.5 par le jugement de César (CESAR.md §4.3) ; ci-dessous, l'état actuel.

Les conditions de fin se choisissent dans le salon :
- **sans fin** : la partie s'arrête par décision commune ;
- **conquête** : est éliminé le joueur qui a perdu son sénat ou toute sa population ;
- **score à durée limitée** : le score combine population, culture, prospérité et paix.

## 6. Commerce et autorisations (exigences E3 et E12)

### 6.1 Commerce avec l'empire (D-019, D-060, D-061, D-066)

- **Chaque cité a son empire**, sauvegardé avec elle : routes ouvertes, quotas, marchands, achats et ventes. Rien ne
  passe d'une cité à l'autre (D-061, test `mp_trade_empire_isolation`). Les marchands et navires de l'empire arrivent
  au point d'arrivée de la cité.
- **Prix de Rome** : un seul prix par ressource, le prix d'achat de base de l'original, avec les variations du
  scénario. C'est un état de chaque cité (`empire/trade_prices`).
- **Portorium** : 50 % du prix de Rome, arrondi vers le bas (`trade_price_duty`). En multijoueur, `trade_price_buy`
  rend Rome + portorium et `trade_price_sell` Rome − portorium : marchands aux entrepôts, recettes et dépenses,
  fenêtre des prix. Le classique garde ses deux prix d'origine (taux 0).
- **L'empire vend toujours** : la règle D-048, qui le faisait s'effacer devant un joueur moins cher, est retirée.
- **Hausse des prix quand un joueur arrête le commerce** (T4.3) : pas codée. Les trois propositions de D-066 font
  toutes du portorium un état de chaque cité, sauvegardé et changé par les commandes déjà en place (route, prix).

### 6.2 Autorisations d'exploiter (D-020, D-061, D-062, D-065)

- Les autorisations sont celles de l'original : « notre cité » de l'empire produit telle ressource
  (`empire_city_set_our_production_allowed`). Chaque cité ayant son empire, elles sont sauvegardées avec elle, et
  les clients les reçoivent dans la sauvegarde. Pas de nouvel état.
- Le **plan de la carte** les fixe, nourriture comprise (§8) :
  - terres : fer, marbre, porcs (pas de bois) ;
  - côte : bois, argile, fruits ; la pêche par les quais ;
  - tous : blé et légumes ; olives et vignes réparties selon la carte.
  L'autorisation « viande » ne commande que les porcs : les quais n'en dépendent pas (règle d'origine).
- **Le menu de construction** est celui du joueur local : `building_menu_update` ne fait rien dans la cité d'un
  autre joueur (`mp_session_is_other_players_city`). Une commande d'un autre joueur ne change donc pas ce que voit
  le joueur local.
- **Les commandes de construction vérifient l'autorisation** comme le menu (`mp_permissions_may_build`) : une ferme,
  une carrière, une mine, une glaisière ou un chantier de bois interdit à la cité est refusé, sur une carte
  multijoueur seulement (`game_rules_multiplayer_map`). Les ateliers ne sont pas vérifiés : le menu les règle seul,
  et une route de l'empire qui fournit leur matière les ouvre toujours (règle d'origine, *à valider*, D-061, D-065).
- **Armes** : stratégiques, car les casernes en consomment une par légionnaire. Seuls les joueurs des terres
  extraient le fer ; les autres doivent acheter ou conquérir.
- **Bois** (D-058) : les javeliniers, renforcés en multijoueur, coûtent un chargement de bois, que seuls les
  joueurs côtiers extraient. Les cavaliers restent gratuits. Trois troupes, trois coûts : armes (joueur des
  terres), bois (côtiers), rien (cavaliers, rapides et plus faibles).
- Équilibrage (M10) : on mesure par des parties simulées sans tête le temps nécessaire à chaque joueur pour
  aligner une légion, et on règle prix, quotas, gisements et fonds de départ.

### 6.3 Commerce entre joueurs (D-019, D-043, D-070)

- Il faut qu'une **route construite** relie les deux cités. Elle peut traverser la carte et emprunter les routes
  d'autres joueurs. La route commerciale s'ouvre quand les deux l'ont proposée (`MP_ACTION_PROPOSE_ROUTE`).
- **Prix** : chaque vendeur fixe un prix par ressource et par acheteur (`MP_ACTION_SET_SELL_PRICE`), le prix de Rome
  par défaut, sans portorium. Chaque acheteur dit à qui il achète quoi (`MP_ACTION_SET_BUYS_FROM`). Prix, achats et
  propositions sont dans l'état de chaque cité (`mp/trade`, écrit par `game/extra_state.c`). L'alerte de prix chez
  l'acheteur est un état d'affichage, jamais sauvegardé.
- **Caravanes** : chaque mois, une par ressource achetée, 8 chargements au plus, dans la limite de ce que l'acheteur
  peut payer. Elles ne marchent que sur les routes et reprennent `figure_trade_caravan_action` (code-map/04 §1.4).
  L'acheteur paie à l'arrivée ce qui entre dans ses entrepôts ; le reste repart chez le vendeur.
- **Bornes de stock**, par ressource et par cité, pour l'empire et pour les joueurs (D-070) :
  - « vendre au-dessus de N » : le seuil d'export de l'original (`MP_ACTION_CHANGE_EXPORT_OVER`) ;
  - « acheter jusqu'à M » : `MP_ACTION_CHANGE_BUY_LIMIT`, de 0 (sans) à 400, dans la pièce commune
    `mp_trade_bounds` du `.mpsav` (facultative : une partie plus ancienne se charge sans borne). Entre joueurs, la
    caravane part avec au plus la place laissée sous M, chargements en route compris. Avec l'empire, M remplace la
    limite automatique de l'original (`mp_trade_empire_buy_limit`, lu par `empire/empire.c`).
- **Interception** (M10) : une caravane est un civil, attaquable par les soldats ennemis, et sa cargaison est
  perdue. Si la route est coupée ou murée, plus de caravane.

## 7. Guerre entre joueurs (exigence E4) — principes, détails au jalon M9

- Chaque figure, formation et bâtiment a un propriétaire, déduit de sa tranche. Une fonction d'hostilité
  `is_hostile(a, b)` remplace les tests « par type ». En mode classique, elle reproduit exactement la matrice
  actuelle.
- Les légions d'un joueur peuvent entrer chez un autre. Elles y sont traitées comme des envahisseurs : les tours,
  les soldats et la règle de paix s'appliquent. Elles attaquent soldats, citoyens et bâtiments grâce à un nouvel
  ordre « attaquer ». Les portes et les murs ne laissent passer que leur propriétaire.
- Moral, totaux et sons de combat sont propres à chaque camp. Les arcs de triomphe, qui venaient des batailles
  lointaines, récompenseront les victoires contre un joueur.
- **Fait en première tranche** (T5.5, D-077) : `mp/war.c` tient l'état de chaque paire de joueurs (paix, préavis,
  combats) et les propositions de paix, sauvegardés dans le morceau `mp_war`. L'hostilité est ajoutée aux tests
  existants, jamais à leur place : `figure_combat_attack_figure_at`, les recherches de cible des soldats et des tours,
  les javelots et carreaux, l'ordre « nettoyer » consultent `mp_war_*` seulement quand deux joueurs se battent
  (`mp_war_any_fighting`), si bien que la matrice d'origine reste exacte en classique et en paix. Les recherches
  parcourent d'abord la tranche de la cité, puis celles de ses ennemis. Un bâtiment abattu s'effondre dans le
  contexte de son propriétaire. Une caravane entre joueurs regarde chaque tick les soldats ennemis à une case.

### 7.1 Le pont de César (T4.8, D-068, *à valider* ; règles de jeu : [CESAR.md](CESAR.md) §7.6)

Sur la carte à 4, le pont de César est le seul passage terrestre entre les deux rives : caravanes entre rives,
missionnaire et armées y passent ; le commerce avec l'étranger, les immigrants, les campagnes et les expéditions de
César n'en ont pas besoin. Il est indestructible et appartient à César (D-034, D-047). Proposition retenue en
attendant Alexandre : la **terre de César**.
- **Où** : les cases à 15 au plus de chaque entrée du pont (réglage de départ), marquées à la création de la carte
  (`mp/mapgen`, qui connaît le pont : `mp_mapgen_caesar_bridge`) dans une grille sauvegardée, par exemple celle des
  zones avec César pour propriétaire.
- **Zones** (`mp/territory`) : aucune zone ne s'étend sur ces cases. Une porte, une tour ou un fort y sont donc
  refusés par la règle de zone existante (`mp_territory_allows_building`). La mission, qui ne demande pas de zone
  (`mp_mission_allows_place`), reçoit le même refus.
- **Murs** : ils se construisent partout (D-036), sauf sur la terre de César : un refus de plus, en multijoueur
  seulement. Les routes des joueurs restent permises.
- **Aujourd'hui** (lu dans le code, à vérifier par un test), une porte peut être posée sur une route de César si
  l'endroit est dans la zone du joueur (`building_construction_place_building` accepte une route sous une porte).
  Avec la terre de César, ce n'est plus possible près du pont. Ailleurs sur les routes de César, la question reste
  ouverte (D-068).
- **Hostilité par paire** (M10.1) : les soldats n'attaquent que les figures des joueurs en guerre avec leur
  propriétaire. Les personnages ne se bloquent pas entre eux (règle d'origine) : un joueur neutre passe toujours.
- **Colère** (M10.5) : dans le terme de puissance, une légion dont le centre est sur la terre de César compte comme
  une légion dans le territoire adverse.
- **Paix** (M10.1, M10.5) : au bilan du mois qui suit la paix, les légions restées sur la terre de César rentrent à
  leur fort. C'est la simulation qui l'ordonne, pas une commande d'un joueur : même effet sur toutes les machines.
- **Eau** : un aqueduc ne se pose ni sur l'eau ni sur un pont (règle d'origine), le pont ne porte donc jamais d'eau.
  La carte à 4 refaite (D-062, T4.14) doit placer un réservoir de César sur chaque rive qui a un joueur des terres.
- **Tests prévus** (M10) :
  - porte, mur et mission refusés sur la terre de César ; route permise ; partie seule comprise ;
  - J1 et J3 en guerre : une caravane de J2 vers J4 traverse le pont, celle de J3 vers J1 est interceptée ;
  - une légion sur le pont ajoute sa puissance à la colère ; à la paix, elle rentre à son fort.

## 8. Cartes multijoueur

- Le jeu se joue sur **des cartes préparées** (D-033, D-044, D-047, D-069), recalculées à chaque partie par
  `mp/mapgen` (`mp_mapgen_create_prepared`) : jamais de fichier de carte dans le dépôt (I4). Deux cartes, chacune
  en deux tailles (`PREPARED_LAYOUTS`) :

  | Carte | 1 ou 2 joueurs | 3 ou 4 joueurs |
  |-------|----------------|----------------|
  | Carte 1 | 200 cases, bras de mer d'ouest en est | 260 cases, deux joueurs sur chaque rive |
  | Carte 2 | 220 cases, bras de mer en diagonale | 240 cases, terres au nord, côte au sud |

  La règle `prepared_map` choisit la carte (salon, §4) ; le nombre de joueurs choisit la taille. Chaque carte suit
  un **plan fixe** (`map_layout`) : villes, points d'arrivée sur les bords, routes de César, tracé du bras de mer,
  pont et réservoirs de César. Les chemins les plus longs restent sous 400 pas (`MAX_PATH` 500, D-064). Le reste
  (forêts, clairières, prés, côtes irrégulières) vient d'un bruit déterministe à graine fixe : la même carte
  sur tous les ordinateurs, l'hôte l'envoie de toute façon. Les bois couvrent 14 à 18 % de la carte et restent
  infranchissables ; aucune clairière n'est enfermée par eux (D-052). Aucun étang : la mer est la seule eau
  (D-055).
- Les joueurs **tirent au sort** leur emplacement (D-052) : l'hôte mélange les emplacements des joueurs présents
  avec le nombre tiré par le salon ; la graine 0 garde l'ordre du plan (tests).
- Chaque emplacement n'offre que la nourriture et les matériaux que son joueur peut produire (D-062, D-065) : ce sont
  les autorisations de production de sa cité, sauvegardées avec elle. Les joueurs des terres (fer, marbre, porcs ;
  un seul à 2, deux à 3 et 4) n'ont pas d'eau, seulement l'aqueduc de César, un réservoir de César par joueur sur la
  côte la plus proche : le couper assèche sa ville (D-055). Les autres (bois, argile, fruits, pêche) ont leur côte
  dans leur zone de départ. À 3 joueurs, la place laissée libre est sur la côte. Les commandes de construction
  refusent, comme le menu, une ferme ou une matière que la cité ne peut pas produire (§6.2).
- La première mission est gratuite et construite d'office ; chaque mission de plus coûte 30 chargements de marbre
  (`MP_MISSION_MARBLE_LOADS`, D-067).
- Le modèle (`mp_mapgen_prepared_template`, Lindum d'abord) ne donne que l'empire, l'année et les fonds ; le climat
  est toujours celui du nord.
- La carte passe par un `.mpsav` (`mp/savegame`), le même format qu'une partie en cours : les clients la reçoivent
  avec le message de bienvenue.
- Il reste, pour les tests seulement : un **générateur aléatoire** (`mp_mapgen_create`) et un outil de
  **composition** (`mp/compose`) qui place des cartes ou sauvegardes classiques côte à côte.

## 9. Interface

- Menu principal :
  - « Multijoueur (réseau local) » ;
  - « Partie libre (solo) » ;
  - « Charger » ;
  - « Éditeur de cartes » ;
  - « Options » ;
  - « Quitter ».

  La campagne et les scénarios personnalisés disparaissent du menu.
- Le salon de jeu permet d'héberger ou de rejoindre une partie, de choisir les joueurs et leurs couleurs, la carte
  et les règles, et d'indiquer qu'on est prêt.
- Chaque joueur a sa couleur, visible :
  - sur un calque « propriétaires » (qui possède quoi) ;
  - sur la minicarte ;
  - sur une marque portée par les bâtiments et les soldats des autres joueurs.
- Un tableau des scores résume population, notes et trésor.
- La faveur de César n'est plus montrée ; les lauriers la remplacent (conseiller impérial, 4e pilier de
  l'évaluation, sénat, barre latérale, §5). Les nouveaux textes sont traduits (le français d'abord, via
  `src/translation/`).

## 10. Formats de fichiers

| Fichier | Format |
|---------|--------|
| `.sav` / `.map` classiques | inchangés (0x66), pour les tests et le mode classique |
| `.mpsav` (sauvegarde multijoueur) | en-tête (signature, version), règles, joueurs, monde (grilles au pas de la carte, calendrier, générateur du monde, empire, entités neutres), puis une section par cité (`city_data` avec champs élargis, `city_extra`, générateur complet, tranches) ; compression par parties, avec erreur explicite si une partie est trop grosse |
| `.mpmap` (carte multijoueur) | en-tête, taille, grilles, points d'arrivée avec leurs autorisations, données de scénario « monde » |
| `.mprec` (enregistrement) | sauvegarde de départ + journal `(tick, joueur, séquence, commande)` : rejeu et débogage |

Pièces communes du `.mpsav` (`mp_savegame_visit`), toutes dans la somme de contrôle : `mp_header` (taille de la
carte, nombre de joueurs, règles), `owner_grid`, `mp_endgame`, `mp_caesar` (lauriers, colère, attente des cadeaux,
historique des lauriers ; version 4), `mp_trade_bounds` (bornes « acheter jusqu'à », facultative), `caesar_buildings`,
`territory_grid`, `fog_grid`. Puis, pour chaque cité : son état d'origine (empire, prix de Rome, autorisations
compris) et son état multijoueur (`game/extra_state.c` : prix et achats entre joueurs, routes proposées…). Une pièce
plus récente qui manque à une ancienne partie se charge vide (sans borne, sans attente, sans historique).

## 11. Organisation du code

- `src/mp/` (nouveau) regroupe ce qui est propre au multijoueur :
  - règles (`rules`) ;
  - contexte (`context`) ;
  - commandes (`command*`) ;
  - somme de contrôle (`checksum`) ;
  - sauvegarde multijoueur (`savegame`) ;
  - propriété des infrastructures (`owner`) et autorisations (`permits`) ;
  - lockstep (`lockstep`) ;
  - salon (`lobby`) ;
  - générateur de cartes (`mapgen`).
- `src/platform/net*.c` contient les sockets et `src/platform/automation.c` le pilote de tests.
- Le reste se fait **dans** les modules existants, par petites touches conditionnées au mode, pour rester proche
  de Julius et pouvoir récupérer ses correctifs.
- Le style de code est celui de Julius : C99, préfixes de module, accolade de fonction à la ligne, commentaires en
  anglais.
