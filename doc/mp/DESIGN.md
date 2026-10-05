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
  ou par adresse IP. On vérifie les versions et les sommes de contrôle des données. Tous démarrent par le même
  chemin déterministe (remise à zéro de l'état caché, chargement de la carte, application des règles).
- Vitesse et pause sont des commandes de l'hôte. En cas de déconnexion, le jeu se met en pause et l'hôte peut
  continuer sans le joueur parti. La cité de ce joueur reste figée ou est retirée : à décider plus tard.
- Implémentation : une petite couche `src/platform/net*.c` (sockets POSIX et Winsock), sans nouvelle dépendance.

## 5. Jeu libre multijoueur sans César

### 5.1 Interventions de César neutralisées (liste complète : code-map/04 §3)

Désactivées quand `game_rules.mode == MP` :
- faveur ;
- demandes impériales ;
- colère et invasions de César ;
- batailles lointaines et service de l'empire ;
- changement d'empereur ;
- cadeaux, salaire, épargne et dons ;
- rangs et promotions ;
- tribut annuel ;
- prêt de secours ;
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
- règle du trésor à −5 000 (plus de construction possible).

Impact assumé (H2) : sans tribut ni salaire, l'économie des cités est un peu plus facile. La prospérité, qui tient
compte du tribut impayé, considère simplement le tribut comme payé.

### 5.2 Arrivée des joueurs

Chaque joueur a un **point d'arrivée** sur la carte : c'est l'endroit où il « atterrit ». Ce point sert d'entrée et
de sortie de sa cité :
- les immigrants et les caravanes des villes de l'empire y arrivent ;
- la règle de la « route de Rome » s'y rapporte.

Il porte aussi les **autorisations d'exploiter** du joueur (§6.2). L'arrivée des navires est propre à chaque cité
(point d'entrée de la rivière par cité) : sur les cartes préparées, le bout du bras de mer le plus proche.

### 5.3 Entités neutres

Indigènes, animaux et envahisseurs IA (option « menaces neutres ») appartiennent à la tranche « monde ».
- La colère des indigènes et ses cibles visent le joueur fautif.
- Les invasions IA visent un joueur désigné par la règle, par exemple à tour de rôle.

### 5.4 Fin de partie

Les conditions de fin se choisissent dans le salon :
- **sans fin** : la partie s'arrête par décision commune ;
- **conquête** : est éliminé le joueur qui a perdu son sénat ou toute sa population ;
- **score à durée limitée** : le score combine population, culture, prospérité et paix.

## 6. Commerce et autorisations (exigences E3 et E12) — principes, détails au jalon M8

### 6.1 Routes commerciales (D-019)

- Le commerce avec les villes de l'empire est conservé, avec un état de route **par joueur** : ouverture,
  quotas, marchands. Les prix sont communs au monde. Les marchands de l'empire arrivent au point d'arrivée de la
  cité.
- Commerce entre joueurs : il faut qu'une **route construite** relie les deux cités. Elle peut traverser la carte
  et emprunter les routes d'autres joueurs. La route commerciale s'ouvre par une commande acceptée des deux côtés.
- Les **caravanes** partent de la cité exportatrice, ne marchent que **sur les routes**, achètent et vendent dans
  les entrepôts de l'autre cité selon ses réglages d'import et d'export, puis reviennent. Elles réutilisent la
  logique de `figure_trade_caravan_action` (code-map/04 §1.4).
- **Interception** : une caravane est un civil, attaquable par les soldats ennemis, et sa cargaison est perdue. Si
  la route est coupée ou murée, plus de caravane.
- Argent : l'exportateur encaisse le prix de vente, l'importateur paie le prix d'achat. Les quotas s'appliquent par
  route et par an.

### 6.2 Autorisations d'exploiter (D-020)

- Chaque point d'arrivée porte une liste d'**autorisations** : matières premières que le joueur a le droit
  d'extraire ou de cultiver (blé, légumes, fruits, olives, vigne, viande, argile, bois, fer, marbre).
- La règle d'origine (`empire_can_produce_resource`, menu de construction) est évaluée dans le contexte de la
  cité. Un atelier est donc aussi constructible si une route commerciale ouverte, avec l'empire ou un joueur,
  fournit sa matière première.
- Autorisations **complémentaires** entre joueurs, avec des gisements présents sur le terrain de chacun.
- **Armes** : stratégiques, car les casernes en consomment pour chaque soldat. Seule une partie des joueurs peut
  extraire le fer et forger, les autres doivent acheter ou conquérir. L'empire en vend peu.
- Équilibrage (M10) : on mesure par des parties simulées sans tête le temps nécessaire à chaque joueur pour
  aligner une légion, et on règle prix, quotas, gisements et fonds de départ.

## 7. Guerre entre joueurs (exigence E4) — principes, détails au jalon M9

- Chaque figure, formation et bâtiment a un propriétaire, déduit de sa tranche. Une fonction d'hostilité
  `is_hostile(a, b)` remplace les tests « par type ». En mode classique, elle reproduit exactement la matrice
  actuelle.
- Les légions d'un joueur peuvent entrer chez un autre. Elles y sont traitées comme des envahisseurs : les tours,
  les soldats et la règle de paix s'appliquent. Elles attaquent soldats, citoyens et bâtiments grâce à un nouvel
  ordre « attaquer ». Les portes et les murs ne laissent passer que leur propriétaire.
- Moral, totaux et sons de combat sont propres à chaque camp. Les arcs de triomphe, qui venaient des batailles
  lointaines, récompenseront les victoires contre un joueur.

## 8. Cartes multijoueur

- Le jeu se joue sur **deux cartes préparées** (D-033, D-044, D-047), recalculées à chaque partie par `mp/mapgen`
  (`mp_mapgen_create_prepared`) : jamais de fichier de carte dans le dépôt (I4). Une pour 1 ou 2 joueurs (200
  cases), une pour 3 ou 4 (260 cases). Chacune suit un **plan fixe** (`map_layout`) : villes, points d'arrivée sur
  les bords ouest et est, routes de César, tracé du bras de mer qui traverse la carte, réservoir de César. Le reste
  (forêts, clairières, étangs, prés, côtes irrégulières) vient d'un bruit déterministe à graine fixe : la même carte
  sur tous les ordinateurs, l'hôte l'envoie de toute façon.
- Chaque emplacement n'offre que les matériaux que son joueur peut exploiter ; le joueur de la pierre n'a pas d'eau,
  seulement l'aqueduc de César ; les autres ont leur côte dans leur zone de départ.
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
- Les écrans et indicateurs de César sont masqués. Les nouveaux textes sont traduits (le français d'abord, via
  `src/translation/`).

## 10. Formats de fichiers

| Fichier | Format |
|---------|--------|
| `.sav` / `.map` classiques | inchangés (0x66), pour les tests et le mode classique |
| `.mpsav` (sauvegarde multijoueur) | en-tête (signature, version), règles, joueurs, monde (grilles au pas de la carte, calendrier, générateur du monde, empire, entités neutres), puis une section par cité (`city_data` avec champs élargis, `city_extra`, générateur complet, tranches) ; compression par parties, avec erreur explicite si une partie est trop grosse |
| `.mpmap` (carte multijoueur) | en-tête, taille, grilles, points d'arrivée avec leurs autorisations, données de scénario « monde » |
| `.mprec` (enregistrement) | sauvegarde de départ + journal `(tick, joueur, séquence, commande)` : rejeu et débogage |

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
