# Feuille de route

> **Jalon en cours : M9, César juge** (D-050, D-053, plan complet : [CESAR.md](CESAR.md)). César revient comme
> arbitre : lauriers de la cité et de César, campagnes, demandes ; la première cité au score gagne. Jouable en paix
> et seul avant la guerre (M10). Ordre : M9.1 → M9.9.

## M0 — Infrastructure de développement et de test ✅
- [x] **M0.1** Environnement macOS : build, données `../donnees-c3`, branche `multiplayer`, tag `upstream-base`.
- [x] **M0.2** `tools/check.sh` : build et 36 tests de parité avec le jeu original.
- [x] **M0.3** Pilote `--automation` et `tools/run-automation.sh` : jeu sans fenêtre, entrées simulées, captures
  PNG, commandes `run` et `ticks`.
- [x] **M0.4** Cartographie du code de Julius (`doc/mp/code-map/01` à `05`).
- [x] **M0.5** Documents de pilotage (VISION, DESIGN, DECISIONS, ROADMAP, TESTING, JOURNAL, CLAUDE.md) et commande
  `/suite`.

## M1 — Déterminisme et outils de vérification
Prérequis à tout le reste : savoir prouver que deux simulations sont identiques et trouver où elles divergent.
- [x] **M1.1** Outil `simtool` (sans tête, mêmes bouchons qu'`autopilot`) qui suit un script : `load`, `run N`,
  `save`, `checksum`, `trace` (somme de contrôle à chaque tick). *Critère* : deux exécutions donnent la même trace
  (ctest).
- [x] **M1.2** Module `mp/checksum` : FNV-1a 64 bits sur l'état de simulation, sans ce qui dépend de l'interface
  (DESIGN §3.8). *Critères* : stable pour un même état, change après un tick ; ctest.
- [x] **M1.3** Remise à zéro de l'état caché à chaque démarrage ou chargement (D-015). *Critères* : nouveau test
  « idempotence » (même sauvegarde chargée deux fois dans un processus → mêmes traces), rouge avant, vert après ;
  parité verte.
- [x] **M1.4** `game_rules` (D-016) : mode de jeu (classique ou multijoueur), difficulté, dieux, correctifs. En
  classique, les valeurs viennent des réglages locaux. *Critères* : parité verte ; en multijoueur, modifier
  `c3.inf` ne change pas la trace.
- [x] **M1.5** Garde-fou dans `check.sh` : ni flottant, ni `rand`, ni horloge dans les dossiers de simulation.
- [x] **M1.6** Aligner la commande d'automatisation `ticks` sur le comportement de l'autopilot (code-map/01 §2.1).
  Plus la vérification croisée `tools/cross-check.sh` : vrai jeu et `simtool` donnent les mêmes sommes de contrôle.

## M2 — Couche de commandes et séparation interface / simulation
- [x] **M2.1** Infrastructure de commandes : structure, sérialisation petit-boutiste, file, exécuteur, numéro de
  séquence. *Critère* : tests unitaires d'aller-retour.
- [x] **M2.2** Construction en commandes à paramètres absolus : bâtiments, tracés de routes, murs et aqueducs,
  maisons, ponts, forts, portes, temples. Inventaire : code-map/05 §4.
  *Fait* : `MP_COMMAND_BUILD` rejoue « appuyer, glisser, relâcher » ; test `mp_build_equivalence_*` (234 placements
  identiques à l'ancien chemin, y compris 25 ticks après) et `test/automation/build-road.txt` dans le vrai jeu.
- [x] **M2.3** Démolition en commande : la question « détruire le fort / le pont ? » est posée avant l'envoi et la
  réponse voyage dans la commande. Test `mp_clear_equivalence_*` (fenêtre puis réponse = réponse dans la commande).
  L'aperçu de l'effacement qui marque les vrais bâtiments est traité avec M2P.3 / M2.9.
- [x] **M2.4** Réglages de la cité en commandes : impôts, salaires, priorités, fêtes, entrepôts et greniers, centre
  de commerce, ouverture de route, import/export, seuils, stockage, industries à l'arrêt (`mp/actions`). Chaque
  commande refait exactement les appels du bouton d'origine (y compris les « +1 » relatifs). Test
  `mp_action_equivalence_*`.
- [x] **M2.5** Ordres militaires en commandes : déplacer, retour au fort, formation, service (et, pour la ville
  classique partagée, bataille lointaine et demandes impériales).
- [ ] **M2.6** Recalculs déclenchés par l'interface (conseillers) transformés en commandes ; phrases des figures
  calculées localement.
- [ ] **M2.7** Enregistrement et rejeu (`.mprec`). *Critères* : un rejeu donne deux fois la même trace ; une partie
  jouée par script d'automatisation, une fois rejouée sans tête, donne la même somme de contrôle finale.
- [~] **M2.8** `sim_step` : en multijoueur, la cadence de la simulation ne dépend plus de l'interface (pas de pause
  liée aux fenêtres, aux tracés ou au défilement, pas de perte de ticks). Le classique ne change pas.
- [ ] **M2.9** En multijoueur, l'aperçu de construction et le rendu n'écrivent plus dans l'état pendant les ticks
  (D-010).
- [ ] **M2.10** Orientation canonique de la simulation en multijoueur (D-009). *Critère* : même trace quelle que
  soit l'orientation locale.
- [ ] **M2.11** Messages adressés à un joueur, popups locales, pas de victoire « classique » en multijoueur.
  *Critère* : `tools/cross-check.sh` identique aussi sur `brugle-massilia-start` et `brugle-lugdunum`.

## M2P — Premier multijoueur jouable sur Mac (ville partagée)
Priorité d'Alexandre : pouvoir lancer une partie multijoueur sur son Mac le plus tôt possible, pour tester. Prototype
« coopératif » : 2 joueurs (2 fenêtres sur le même Mac, ou 2 Mac en réseau local) construisent **la même cité**
classique. Il valide en vrai la couche de commandes, le lockstep et la détection de désynchronisation, avant le moteur
multi-cités. Prérequis : M2.1 à M2.4 et M2.8.
- [x] **M2P.1** Réseau TCP minimal (`src/platform/net.c`) : `--host PORT` / `--join IP:PORT` en ligne de commande.
- [x] **M2P.2** Lockstep : tours de K ticks, commandes exécutées au tour T+2, somme de contrôle par tour, pause et
  sauvegarde de diagnostic en cas de désynchronisation.
- [x] **M2P.3** Pendant les ticks, l'aperçu de construction (y compris celui de l'effacement) est retiré de la carte
  puis remis ; rotation de la vue
  bloquée en multijoueur (en attendant M2.10).
- [x] **M2P.4** `tools/play-mp.sh` : lance l'hôte et un client (deux fenêtres) sur une sauvegarde, sur le Mac.
- [x] **M2P.5** Test automatisé : deux instances sans fenêtre reliées en local, commandes scriptées, mêmes sommes de
  contrôle. *Critère final* : Alexandre joue une partie à deux fenêtres sur son Mac sans désynchronisation.
  *Fait* : `mp_lan_*` (ctest), `tools/mp-real-test.sh` (vrai jeu), et testé par Alexandre le 2026-10-04 : les
  constructions se synchronisent bien.
- [x] **M2P.6** Une cité par joueur (demande d'Alexandre : statistiques séparées, E13). L'hôte recopie la cité de
  départ pour chaque joueur (D-025) et envoie le `.mpsav` ; chaque machine affiche sa cité.
  *Fait* : `mp_lan_cities_*` (2 et 4 joueurs, désynchronisation détectée) : chaque commande ne change que la cité de
  son joueur, et les sommes de contrôle sont identiques sur toutes les machines. `tools/mp-real-test.sh` : les deux
  instances affichent chacune leur trésorerie. *À tester par Alexandre* avec `tools/play-mp.sh`.

## M3 — Moteur multi-cités
- [x] **M3.1** Types élargis en mémoire (coordonnées 16 bits, offsets 32 bits). Le format classique est réécrit à
  l'identique. *Critère* : parité.
- [x] **M3.2** Grille de taille variable (`map_grid_size()`), tables d'offsets calculées. *Critère* : parité.
- [x] **M3.3** Outil de composition : placer une sauvegarde classique dans un monde plus grand, avec un décalage,
  puis l'extraire. *Critère* : test d'invariance par translation sur les 36 cas.
- [x] **M3.4** Contexte de cité : `city_data` par cité, `city_extra`, générateur aléatoire par cité (D-004, D-007).
  *Critère* : parité.
- [x] **M3.5** Tranches d'identifiants et conversion des boucles (D-005). *Critère* : parité.
- [x] **M3.6** Ordonnancement d'un tick à N cités (DESIGN §3.3). *Critère* : parité. Plan détaillé et liste des 14 tâches
  à adapter : `doc/mp/code-map/06-ordonnancement-multi-cites.md`.
- [x] **M3.7** **Test d'isolement « jumeaux »** : la même sauvegarde placée deux fois (cités 0 et 1) redonne,
  pour chacune, la référence du jeu original, sur les 36 cas. Plus le test d'indice (une seule cité placée en
  position k). C'est la preuve de E7 pour le moteur multi-cités.
  *Fait* : `simtool twins` et `twinfigures`, 16 sauvegardes identiques sur 3 000 ticks (et 12 000 pour 4 d'entre
  elles), dont la jumelle copie exacte personnage par personnage (D-023). Seul écart connu : les sans-abri d'une carte
  sans point de sortie visent tous la case (0, 0) de la carte (inv0, après 4 300 ticks) ; réglé par M4.2.
- [x] **M3.8** Sauvegarde multijoueur `.mpsav` (monde, cités, état caché). *Critère* : test de reprise exacte
  (continuer la partie = sauvegarder puis recharger).
  *Fait* : `mp/savegame` (D-024), `simtool mpresume`. Sur 13 sauvegardes, en jumelles (1 500 + 400 ticks) et en cité
  seule sur grande grille (700 + 300), l'état rechargé a la même somme de contrôle et la partie reprise reste identique
  tick par tick. Un oracle compare en plus toute la mémoire enregistrée de chaque cité (hors messages, qui relèvent de
  l'interface). 16 cas dans ctest.

## M4 — Règles du jeu libre multijoueur
- [x] **M4.1** Neutralisation de César en multijoueur (liste : code-map/04 §3). *Critères* : en multijoueur, une
  sauvegarde avec demande ou invasion de César ne déclenche rien ; parité verte.
  *Fait* (D-026) : `simtool caesarfree` ; sur 12 000 ticks avec une faveur à 0, la partie classique voit une
  demande, la colère, 32 soldats impériaux et le salaire ; la même partie en multijoueur, rien.
- [x] **M4.2** Point d'arrivée par joueur : entrée et sortie, « route de Rome » calculée par cité.
  *Fait* : entrée, sortie et « route de Rome » sont déjà propres à chaque cité (contexte de cité, composition). Le
  test long a révélé des fuites entre cités, corrigées (D-027) : sans-abri envoyés au coin de la grande carte,
  sentinelles et chevaux recalculés pour toutes les cités, ennemis IA visant les légions d'une autre cité. Les
  13 sauvegardes restent des copies exactes sur 12 000 ticks (jumelles et figure par figure).
- [x] **M4.3** Construction partout et propriété (D-018) : grille `owner` des infrastructures, démolition limitée
  à ce qui vous appartient, recherches de cible filtrées par propriétaire, eau par propriétaire.
  *Critère* : test « voisins branchés » (deux cités reliées par une route ; aucune couverture, aucun ouvrier,
  aucune marchandise, aucune eau ni aucun pompier ne passe chez l'autre ; chaque cité non branchée reste identique
  à l'original).
  *Fait* (D-028) : `simtool neighbours` (deux cités reliées par 5 routes, des marcheurs passent chez l'autre) et
  `simtool intruders` (les marcheurs de la jumelle déplacés dans la première cité : sans les filtres, des dizaines
  de milliers d'effets sur ses bâtiments ; avec, aucun, sur 13 sauvegardes). Les recherches de bâtiments ne voyaient
  déjà que la tranche de la cité (M3). Les jumelles restent exactes.
- [x] **M4.4** Démarrage d'une partie multijoueur depuis une carte, de 1 à 4 joueurs, par un chemin déterministe.
  La partie libre solo est jouable (menu provisoire). *Critère* : scénario d'automatisation avec captures.
  *Fait* : le salon (M5.4) démarre depuis une carte `.map` ou une sauvegarde. L'hôte compose une copie par joueur,
  ouvre la terre entre les cités, écrit le `.mpsav` et l'envoie : un seul calcul, donc aucun risque de divergence.
  Tests : `tools/mp-lobby-test.sh` (captures), `simtool openland` (route entre deux cités).
- [x] **M4.5** Entités neutres (indigènes, animaux, menaces IA) et leurs options.
  *Fait* (D-029) : option « invasions IA » du salon. `simtool aiinvasions` : 16 et 10 ennemis avec, aucun sans.
- [x] **M4.6** Fin de partie multijoueur : sans fin, conquête, score.
  *Fait* (D-029) : sans fin ou au score (5, 10, 20 ans), `mp/endgame`, écran de classement. Tests :
  `simtool endscore`, partie réseau du vrai jeu finie au bout d'un an (`--mp-score-years 1`). La conquête viendra
  avec M9.
- [x] **M4.7** Autorisations d'exploiter par point d'arrivée (D-020) : la règle d'origine est évaluée par cité,
  le menu de construction suit. *Critère* : une cité sans autorisation de fer ne peut ni bâtir de mine ni forger
  sans route qui fournit du fer.
  *Fait* : `mp/permissions` (répartition provisoire, D-020) appliquée par l'hôte. `simtool permissions` vérifie,
  cité par cité, le menu de construction : chaque matière pour une seule cité, l'atelier d'armes seulement avec
  du fer à soi ou importable.

## M5 — Réseau local → premier prototype jouable en LAN
- [x] **M5.1** Couche sockets (TCP non bloquant, POSIX et Winsock). *Critère* : tests en boucle locale.
  *Fait avec M2P.1* (`platform/net`), plus l'UDP de découverte (M5.4). Winsock est écrit mais pas encore compilé.
- [x] **M5.2** Protocole lockstep : tours, délai, relais, sommes de contrôle, détection des désynchronisations et
  sauvegardes de diagnostic.
  *Fait avec M2P.2* (`mp/lockstep`), protocole v3 (cités séparées, règles de l'hôte).
- [x] **M5.3** Banc de test multi-processus sans tête (`tools/lan-test.sh`) : de 2 à 4 instances, commandes
  scriptées, traces identiques.
  *Fait avec M2P.5* : `test/sim/lan_test.sh` (ville partagée et cités séparées, 2 à 4 joueurs, désynchronisation).
- [x] **M5.4** Salon minimal : héberger, rejoindre par IP ou par découverte UDP, vérifier les sommes de contrôle des
  données, choisir les règles, se déclarer prêt.
  *Fait* (demande d'Alexandre, avancé avant M4.3) : entrée « Multijoueur » du menu principal (`window/mp_lobby`).
  L'hôte choisit une carte du jeu libre (`.map`) ou une sauvegarde et le nombre de joueurs ; les autres trouvent la
  partie sur le réseau local (annonce UDP, `mp/discovery`) ou tapent l'adresse. Test : `tools/mp-lobby-test.sh`
  (deux instances du vrai jeu, tout à la souris). Reste pour M5.8 : choix des règles, « prêt », vérification des
  données du jeu.
- [x] **M5.5** Pause, vitesse, déconnexion ; sauvegarde coordonnée par l'hôte.
  *Fait* : la pause est décidée par l'hôte, qui cesse d'accorder des tours, donc toutes les machines s'arrêtent au
  même tick (un client la demande ; protocole v3). La vitesse est celle de l'hôte ; un client en retard rattrape.
  Un joueur qui part n'arrête plus la partie : sa cité continue sans ordres. « Enregistrer » écrit un `.mpsav`, et
  le salon reprend une partie depuis un `.mpsav` (nombre de joueurs fixé par le fichier). Tests : `mp_lan_pause`,
  `mp_lan_player_leaves`, `mp_lan_resume_mpsav`.
- [ ] **M5.6** Intégration continue multiplateforme (macOS arm64, Windows x64, Linux) qui compare les traces de
  rejeu. *Nécessite un dépôt GitHub : à demander à Alexandre.*
- [ ] **M5.7** Première vraie partie en LAN avec Alexandre (Mac et PC) et retour d'expérience.
- [x] **M5.8** Salon complet : choix des règles (difficulté, dieux), joueurs « prêts » avant le lancement par l'hôte,
  vérification que tous ont les mêmes données du jeu.
  *Fait* : difficulté, dieux, fin de partie et invasions IA dans le salon ; l'hôte lance la partie quand tout le
  monde est là (« Lancer la partie ») ; un client dont la version du protocole ou les données qui changent la
  simulation (`c3_model.txt`, empires) diffèrent est refusé avec la raison. Tests : `tools/mp-lobby-test.sh`,
  `mp_lan_other_game_data_refused`.

## M6 — Grandes cartes
- [x] **M6.1** Format `.mpmap` : taille, points d'arrivée et leurs autorisations.
  *Fait* (D-030) : une partie de départ au format `.mpsav`, listée par le salon.
- [x] **M6.2** Générateur de cartes aléatoires : relief, eau, forêts, roches, gisements, positions et autorisations
  complémentaires équilibrées (fer et armes rares).
  *Fait* (D-030) : `mp/mapgen`, option « grande carte générée » du salon et `--mp-generate`. Pas encore de relief.
  Tests : chaque joueur s'installe et reçoit des immigrants (2 et 4 joueurs), partie réseau sur carte générée,
  `tools/mp-lobby-test.sh`.
- [ ] **M6.3** Éditeur : grandes tailles, points d'arrivée, autorisations.
  *Reporté* : le générateur couvre le besoin pour l'instant ; à reprendre si Alexandre veut dessiner ses cartes.
- [x] **M6.4** Rendu, minicarte et captures sur grandes cartes ; mesure et optimisation du routage.
  *Fait* : rendu et minicarte vérifiés par captures sur des cartes de 200 et 260 cases. Mesure : 4 grandes cités sur
  la grille de 512 coûtent environ 0,4 ms par tick (22 ms disponibles à vitesse normale), aucune optimisation
  nécessaire.

## M7 — Interface multijoueur
- [x] **M7.1** Menu principal sans campagne ; écrans du salon.
  *Fait* (D-031) : « Multijoueur » en tête du menu ; la campagne reste accessible plus bas (invariant I2, à valider).
- [x] **M7.2** Couleurs des joueurs : frontières, minicarte, marques sur les bâtiments et les soldats.
  *Fait* : `mp/colors` (1 bleu, 2 rouge, 3 vert, 4 jaune) ; bâtiments, infrastructures et personnages des autres
  joueurs teintés dans la vue et sur la minicarte. Pas de frontières (pas de territoire, D-018). Capture :
  commande d'automatisation `gotocity`.
- [x] **M7.3** Tableau des scores, messages par joueur, écran de fin.
  *Fait* : scores de chaque joueur dans le bandeau, dans sa couleur, et « PAUSE » ; messages déjà propres à chaque
  cité ; avertissements des commandes des autres joueurs plus affichés chez soi ; écran de fin (M4.6).
- [x] **M7.4** Interface de César masquée ; traductions françaises des nouveaux textes.
  *Fait* : conseiller impérial inaccessible en multijoueur (demandes, cadeaux, salaire) ; tous les nouveaux textes
  en français et en anglais.
- [ ] **M7.5** Discussion entre joueurs (optionnelle).
  *Non faite* (optionnelle).

## T1 — Retours du premier test d'Alexandre (TEST_1, QUESTIONS_1, 2026-10-05)
- [x] **T1.1** Popups, sons et avertissements de la cité d'un autre joueur plus jamais chez soi (D-032).
  *Fait* : `mp_session_is_other_players_city()` ; test `mp_private_popups_*` (2 popups et 3 sons de la cité jumelle
  sortaient chez le joueur 1 avant le correctif).
- [x] **T1.2** Grande carte : la vue reste bloquée en haut à droite, on ne voit que la moitié et on ne peut plus
  défiler. *Fait* : la carte générée était dans le coin de la grille de 512, alors que les limites de la caméra et la
  minicarte supposent une carte centrée, comme les cartes classiques ; elle est maintenant centrée. Test
  `mp_generated_map_view_*` : la caméra atteint les quatre coins (trois hors d'atteinte avant).
- [x] **T1.3** Défilement au bord haut de l'écran bloqué par la barre de menu (plein écran).
  *Fait, vérifié par Alexandre le 2026-10-06* : la barre de menu du jeu laisse passer la
  souris ; c'est celle de macOS qui descendait sur le jeu en plein écran « Space ». Le plein écran du jeu n'utilise
  plus de Space sur macOS (`SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES`) : barre de menu et Dock restent cachés.
  *Revu par T2.7* : ce plein écran sans Space marchait mal ; retour au Space, barre et Dock masqués autrement.
- [x] **T1.4** 1920×1080 et 2K proposées dans la liste des résolutions.
  *Fait* : 1280 × 720, 1920 × 1080 et 2560 × 1440 après les trois tailles d'origine (capture : fenêtre de
  1920 × 1080 obtenue). *Remplacé par T2.5* : ces tailles dépassaient l'écran d'un MacBook.
- [x] **T1.5** Routes de la carte à César : neutres (pas à la couleur d'un joueur), indestructibles (D-034).
  *Fait* : propriétaire `MAP_OWNER_CAESAR` ; le rouge venait du mode « copie de carte », qui attribuait les routes
  de chaque copie à son joueur. Test `mp_caesar_roads` : aucun joueur ne les démolit, pas de teinte, les maisons
  qui les bordent reçoivent des immigrants. Le dessin en blanc reste à faire (MC.1).
- [x] **T1.6** Le salon ne propose plus que les cartes multijoueur (D-033).
  *Fait* : plus de choix « copies / carte générée » ; une partie ou carte multijoueur reprend telle quelle, tout
  autre fichier donne le climat, l'empire et les fonds d'une carte générée. `tools/mp-lobby-test.sh` n'a plus de
  variante.

## T2 — Retours du deuxième essai d'Alexandre (2026-10-05)
- [x] **T2.1** Seul dans le salon, on construit partout et le missionnaire ne bouge pas : la partie seule applique
  les règles de la carte (D-044). *Fait* : `game_rules_multiplayer_map()` remplace « plus d'une cité » pour les
  zones, le brouillard, l'eau de César et les réservoirs. `tools/mp-solo-test.sh` (vrai jeu, sans fenêtre) : maison
  hors zone refusée, mission construite, zone de 42 × 42 cases, missionnaire déplacé à la souris.
- [x] **T2.2** Au départ, la vue est sur le missionnaire et l'objectif « fonder sa mission » s'affiche jusqu'à la
  première mission ; le brouillard est calculé dès la création de la carte (écran noir avant). Captures
  `mp-solo-start.png`, `mp-solo-mission.png`.
- [x] **T2.3** Plus de sélecteur de carte : « Nouvelle partie (forêt) » ou une partie à reprendre (D-044).
- [x] **T2.4** Carte en forêt, de l'eau pour tous : un lac et sa rivière vers le bord pour chaque joueur, docks et
  navires de son empire, points de pêche, forêts et étangs loin des cités (D-044). Test `mp_prepared_map_*` : au
  moins 8 % d'eau et 20 % de forêt, climat du nord, points de pêche dans l'eau, navires de chaque joueur jusqu'à
  son lac ; brouillard : les mouettes n'éclairent rien.
- [x] **T2.6** La mission est construite d'office au départ (D-045). *Fait* : une mission par joueur au bord de la
  route de César qui traverse l'emplacement de sa cité, sa zone et son brouillard calculés à la création de la
  carte ; tests `mp_territory`, `mp_missions`, `mp_outside_territory` revus (la mission suivante coûte du marbre,
  la ville qui perd sa mission perd sa zone) ; `tools/mp-solo-test.sh` : mission et zone au départ.
- [x] **T2.7** Plein écran : barre de menus et Dock visibles au lancement ; après un passage fenêtre → plein écran,
  plus de défilement vers le bas. *Fait* (D-045, confirmé par Alexandre : « beaucoup mieux ») : plein écran natif de
  macOS (Space), barre de menus et Dock masqués par le délégué de la fenêtre ; l'ancien plein écran sans Space est
  abandonné.
- [x] **T2.8** En plein écran, la souris en haut de l'écran ne fait plus défiler la carte (« la barre de menu de
  Caesar empêche le déplacement »). *Fait, à vérifier par Alexandre* (D-046) : sur macOS, la position de la souris
  est relue auprès du système à chaque image (`SDL_GetGlobalMouseState`) et ramenée au bord de la fenêtre en plein
  écran ; les événements de mouvement ne suffisent pas (bande noire de l'encoche au-dessus de la fenêtre, positions
  périmées de macOS 26+, SDL #15967). *Deuxième correction* (toujours bloqué) : la capture de la souris en plein
  écran (`SDL_SetWindowGrab`) est retirée sur macOS, car SDL 3 confine le curseur à un rectangle calculé depuis
  `contentLayoutRect`, qui dans un Space exclut la barre de titre cachée : le curseur ne pouvait plus atteindre les
  28 points du haut, juste la hauteur de la barre de menu du jeu ; la position est lue sur la fenêtre Cocoa
  (`mouseLocationOutsideOfEventStream`). Journal `julius-log.txt` dans le dossier des données (macOS) avec les
  positions vues près du haut, pour vérifier chez Alexandre.
- [x] **T2.5** Résolutions : la liste propose des tailles fixes plus grandes que l'écran du MacBook (1470 × 956,
  1710 × 1112) et le défilement par les bords ne marche plus. *Fait, vérifié par Alexandre le 2026-10-06* : les tailles fixes
  de T1.4 sont retirées ; après les trois tailles d'origine, la ligne « L par H (écran) » donne la plus grande
  fenêtre qui tient sur l'écran (zone utile moins la barre de titre) ; aucune fenêtre n'est plus grande que l'écran
  (au lancement aussi : une taille enregistrée trop grande est réduite). Script
  `test/automation/display.txt` (écran factice de 1024 × 768 : ligne « 1024 par 736 (écran) », 1024 × 768
  ramené à 1024 × 736). Le plein écran est revu par T2.7.

## T3 — Retours du troisième essai d'Alexandre (2026-10-06)
- [x] **T3.1** Le missionnaire ne traverse pas le pont : un clic dessus ne fait rien. *Fait* : le pont pour navires
  est dessiné en hauteur, au-dessus de ses cases, et le clic tombait sur l'eau voisine ; un ordre vers l'eau ou les
  rochers vise maintenant la case praticable la plus proche (3 cases au plus), et un missionnaire sans chemin le dit.
  Test `mp_missionary_bridge` (rouge avant) ; `tools/mp-solo-test.sh` clique sur le pont dans le vrai jeu.
- [x] **T3.2** « Aller au problème » d'une catastrophe envoie la vue à l'autre bout de la carte. *Fait* : la place
  d'un message (incendie, effondrement…) était tronquée sur 16 bits, au-delà de la 163e ligne des grandes cartes ;
  elle est entière en mémoire et large dans les sauvegardes multijoueur (D-024), et la fenêtre du message ne la
  borne plus à 162 × 162. Test `mp_message_location` (rouge avant). Sauvegardes multijoueur en version 3, protocole
  9.
- [x] **T3.3** Commerce plus lisible, empire et joueurs sur la même page (D-051). *Fait* : le conseiller au commerce
  montre, pour chaque ressource, le stock, l'empire (commerce et prix) et le joueur choisi (mon prix, son prix,
  « J'achète », chargements en route) ; le fournisseur en vert. La fenêtre « Joueurs » séparée disparaît. Test
  `mp_trade_caravans` (chargements en route), captures de `tools/mp-trade-test.sh`.
- [x] **T3.4** « Aller partout » sur les cartes (D-052). *Fait* : bois infranchissables comme dans l'original, mais
  deux fois moins nombreux (14 et 17,5 %) ; les clairières enfermées deviennent du bois (moins de 16 cases) ou
  reçoivent un passage. Test `mp_prepared_map_reachable_2_players` et `_4_players` : 100 % des terres atteignables.
- [x] **T3.5** Le joueur des terres exploite aussi le bois (chantiers de bois), avec des bois près de sa cité.
- [x] **T3.6** Emplacements tirés au sort entre les joueurs (le joueur 1 n'est plus toujours dans les terres). Test
  `mp_prepared_map_placement` : sur 12 parties, chacun des joueurs a eu l'emplacement des terres ; à 3, le sud-est
  reste libre.

## MC — Cartes multijoueur préparées et César (D-033, D-034)
- [ ] **MC.1** Ouvrages de César dessinés en blanc (la teinte actuelle ne fait que foncer les images) ; son aqueduc
  porte l'eau de son réservoir à toutes les cités. Le propriétaire existe déjà (T1.5).
- [x] **MC.2** Carte à 2 joueurs et carte à 4 joueurs (3 joueurs sur la carte à 4) : emplacements avec prés et
  seulement les ressources autorisées, tout relié à la route principale. *Fait* (à relire par Alexandre) :
  `mp_mapgen_create_prepared`, toujours la même carte (200 cases à 2, 260 à 4) ; route de César de chaque point
  d'arrivée au centre ; lac central relié au coin nord-est par une rivière navigable (D-041) ; autorisations fixées par la carte. Test
  `mp_prepared_map_*` (terre accessible, matériaux et autorisations de chaque emplacement, carte identique d'une
  création à l'autre, cités qui grandissent) ; image : `MAPGEN_PICTURE=carte.ppm simtool preparedmap`. Les
  cartes sont recalculées au lancement à partir des données du joueur : aucune `.mpmap` dans git (I4).
- [x] **MC.3** Aqueduc de César et son réservoir indestructible, près des joueurs loin de l'eau.
  *Fait* : tranche de bâtiments de César (ids 8001 à 9999), qu'aucune cité ne fait tourner, sauvegardée dans le
  `.mpsav` (format large) ; réservoir au bord du lac central, aqueduc vers l'ouest et le nord ; chaque cité le
  remplit dans son calcul de l'eau et ses réservoirs branchés dessus reçoivent l'eau. Test `mp_prepared_map_*`
  (après passage par un fichier, comme en partie) : réservoir branché plein, réservoir à l'écart sec ; captures du
  vrai jeu (`goto X Y`).

- [x] **MC.4** Un grand bras de mer traverse la carte (D-047) : à 2 joueurs, les deux sur la même rive ; à 4, deux sur
  chaque rive ; pas d'eau près du joueur de la pierre, sauf l'aqueduc de César. *Fait* : plan fixe par carte (villes,
  points d'arrivée, routes de César, tracé de la mer), bras de mer d'environ 25 cases qui serpente d'un bord à
  l'autre, pont de César pour navires sur la route qui le traverse, réservoir de César sur la côte et aqueduc vers le
  joueur de la pierre, joueurs de la côte avec leur rivage dans leur zone de départ, points de pêche au large.
  Test `mp_prepared_map_*` : mer d'un bord à l'autre et navires sous le pont, pont de César, aucune eau à moins de
  45 cases du joueur de la pierre, au moins 30 cases de mer dans la zone de départ des autres, navires de chaque
  joueur côtier jusqu'à sa côte. `simtool terrain` : une lettre par case pour relire la carte.
- [x] **MC.5** Les prés fertiles n'apparaissaient qu'après un coup de pelle (retour d'Alexandre). *Fait* : le
  générateur dessinait les prés avant l'herbe, qui les recouvrait ; même ordre que le chargement d'une carte. Test
  `mp_prepared_map_*` : aucun pré dessiné autrement (5 785 l'étaient avant le correctif).

## ME — Eau (D-035)
- [x] **ME.1** Réservoirs à niveau en multijoueur : vidage en environ 5 minutes, remplissage en environ 1 minute
  (en ticks), alimentation tant qu'il reste de l'eau. *Fait* : niveau de 0 à 270 jours d'eau par réservoir (état
  « extra » de chaque cité), +5 par jour alimenté, −1 par jour coupé, eau tant qu'il en reste ; un réservoir neuf
  part vide ; la fenêtre d'un réservoir affiche sa réserve. Test `mp_reservoir_level` (rempli en 54 jours, encore
  de l'eau 100 jours après la coupure, sec après 270, de nouveau de l'eau une fois rebranché) ; parité intacte.

## MT — Territoires, missions et missionnaire (D-036, D-037)
- [x] **MT.1** Zone constructible : 20 cases autour des bâtiments installés et des missions, premier arrivé,
  routes, murs et aqueducs partout. *Fait* : `mp/territory` (grille sauvegardée dans le `.mpsav`, mise à jour
  chaque jour au tick 13, cité par cité, par différences de lignes) ; règle de partie `territories` (active sur les
  cartes préparées, protocole v4) ; refus et aperçu rouge hors zone, avertissement « Hors de votre territoire ».
  Test `mp_territory` : rien sans zone, mission → 20 cases, maisons habitées → 20 cases plus loin, l'autre joueur
  n'y bâtit que des routes. *Provisoire* : la mission se construit n'importe où tant que MT.3 n'existe pas.
- [x] **MT.2** Mission multijoueur : plus d'indigènes, première gratuite puis marbre, forme des missionnaires.
  *Fait* : gratuite tant que la cité ne possède aucune case (première mission, ou après avoir tout perdu), puis 4
  chargements de marbre et aucun denier ; plus de missionnaire en tournée ; bouton « Former un missionnaire :
  300 Dn » dans sa fenêtre, un missionnaire vivant par mission.
- [x] **MT.3** Missionnaire déplaçable comme une légion, mortel ; mission à 20 cases au plus de lui.
  *Fait* : figure « missionnaire » avec deux états multijoueur (attend, marche comme un soldat) ; un au départ par
  joueur ; clic dessus puis clic sur la destination (commande `MP_ACTION_MISSIONARY_MOVE`), clic droit pour
  annuler ; jamais de mission dans la zone d'un autre. Test `mp_missions` ; captures du vrai jeu.
- [x] **MT.4** Bâtiments hors zone : message puis effondrement après le délai de grâce.
  *Fait* : compteur de jours par bâtiment (état « extra » de chaque cité) ; avertissement le premier jour et un mois
  avant ; effondrement après 48 jours (3 mois) ; une mission rebâtie à temps les sauve. Test
  `mp_outside_territory`.
- [x] **MT.5** Frontières tracées à la couleur du joueur, teinte légère des bâtiments adverses (D-039).
  *Fait* : bord de chaque zone tracé au sol, en escalier isométrique, dans les quatre orientations ; teintes des
  joueurs éclaircies ; bandeau multijoueur (joueur, scores, pause) déplacé en bas de la vue, qui cachait les
  avertissements. Commande d'automatisation `build TYPE X1 Y1 X2 Y2`. Captures du vrai jeu.

## MB — Brouillard de guerre (D-038)
- [x] **MB.1** Option du salon, zones découvertes, éclairage de 20 cases, minicarte et scores adverses masqués.
  *Fait* : `mp/fog` (bits « découvert » et « éclairé » par joueur, calculés chaque jour au tick 14 par la simulation,
  sauvegardés) ; règle `fog_of_war` (oui par défaut, protocole v5) ; vue, surcouches et minicarte en noir hors du
  découvert ; personnages des autres cachés hors de l'éclairé ; pas de fiche de bâtiment sur le non-découvert ;
  scores adverses cachés. Test `mp_fog_of_war` ; captures du vrai jeu.

## MA — Apparence (D-039)
- [ ] **MA.1** Variantes de couleur des bâtiments calculées au lancement (toits brique, ardoise…), jamais dans git.

## M8 — Commerce entre joueurs (D-019, D-043)
- [x] **M8.1** Conception détaillée : D-019, D-042 (voies vers l'extérieur), D-043 (prix).
- [x] **M8.2** Achats à l'empire majorés de 50 % en multijoueur. *Fait* : `trade_price_buy` majoré en multijoueur
  (paiement et affichage), `trade_price_buy_base` pour le prix du jeu. Test `mp_empire_import_price` ; parité
  intacte.
- [x] **M8.3** Prix de vente par ressource et par joueur acheteur (commande), message à l'acheteur quand le prix
  d'une ressource qu'il achète change. *Fait* : `mp/trade` (prix demandés et achats, état « extra » de chaque
  cité), commandes `MP_ACTION_SET_SELL_PRICE` et `MP_ACTION_SET_BUYS_FROM`, avertissement « Le joueur 2 vend
  désormais : marbre 180 Dn (au lieu de 150) » sur l'ordinateur de l'acheteur seulement. Test `mp_trade_prices`.
- [x] **M8.4** Ouverture d'une route entre deux joueurs (accord des deux, chemin routier entre leurs cités).
  *Fait* : commande `MP_ACTION_PROPOSE_ROUTE`, route ouverte quand les deux l'ont proposée, message à l'autre
  joueur ; le chemin est vérifié à chaque départ (entrepôts des deux sur le même réseau routier).
- [x] **M8.5** Caravanes entre joueurs, uniquement sur les routes : le vendeur expédie ce qu'il exporte, l'acheteur
  paie le prix du vendeur à la livraison. *Fait* : une caravane par mois et par route (tick 15), 8 chargements au
  plus, dans la limite de ce que l'acheteur peut payer ; livraison dans ses entrepôts, le reste revient ; route
  coupée : la cargaison revient. Test `mp_trade_caravans` (8 marbres livrés en 42 jours, 1 200 Dn payés et reçus).
  *Reste fait par M8.9.*
- [x] **M8.6** Fenêtre du commerce entre joueurs : prix, routes, achats.
  *Fait* : bouton « Joueurs » du conseiller au commerce (multijoueur seulement) ; un onglet par joueur, état de la
  route et bouton pour la proposer ou la retirer, pour chaque ressource mon prix (−/+ par 10), son prix, « J'achète
  oui/non ». Essayée en réseau dans le vrai jeu (captures), sans désynchronisation.
- [ ] **M8.7** Interception : caravanes attaquables, cargaison perdue, route coupée. *Déplacée en M10.3*, avec la
  guerre.
- [x] **M8.8** Tests : rejeux, cargaisons et argent conservés, scénarios visuels. *Fait* : `mp_trade_conservation`
  (chaque jour, chargements des deux cités et des caravanes, et argent des deux joueurs, constants : acheteur à court
  d'argent servi de ce qu'il peut payer, entrepôt presque plein : 2 chargements payés et 6 rendus, puis le reste en
  deux voyages) ; `mp_trade_resume` (sauvegarde pendant que deux caravanes roulent, partie reprise identique tick par
  tick sur 60 jours, mêmes livraisons) ; captures de la fenêtre en réseau (`trade-window.png`). Les interceptions
  viendront avec M8.7.
- [x] **M8.9** Une caravane par ressource achetée et par mois (D-048) ; l'empire ne vend pas ce qu'un joueur vend
  moins cher. *Fait* : plusieurs caravanes sur une route, une par ressource à la fois, le budget de l'acheteur
  partagé entre elles ; `empire_can_import_resource_from_city` refuse en multijoueur une ressource achetée à un
  joueur moins cher par une route ouverte, même sans stock (Alexandre : « assécher le stock d'un adversaire fait
  partie du jeu »). Tests `mp_trade_caravans` (marbre et fer en route ensemble), `mp_trade_preference` (prix, achat,
  route, stock vide, marchands de l'empire eux-mêmes, partie à une cité intacte).
- [x] **M8.10** Livraisons annoncées à l'acheteur (« Le joueur 2 vous a livré : 8 Marbre pour 1200 Dn », ou
  « n'a pu vous livrer (entrepôts pleins) »).
- [x] **M8.11** Fenêtre « Joueurs » : colonne Empire (prix majoré, « - » si l'empire ne vend pas la ressource), le
  moins cher en vert ; textes resserrés (capture en réseau).
- [x] **M8.12** Alerte plein écran chez l'acheteur quand un vendeur change le prix d'une ressource qu'il lui achète
  (D-049, demande d'Alexandre). *Fait* : `window/mp_price_alert`, une ligne par vendeur et ressource ; test
  `mp_trade_prices` (ligne créée, mise à jour, effacée au retour à l'ancien prix, fermée) ; `tools/mp-trade-test.sh` :
  le client achète le marbre, l'hôte monte deux fois son prix, captures `price-alert.png` et
  `price-alert-trade.png`.

## M9 — César juge (D-050, [CESAR.md](CESAR.md) §4 à §6)
César revient comme arbitre de la partie. Jouable en paix et **seul** : livré à Alexandre avant la guerre.
- [x] **M9.1** Module `mp/caesar` : par cité les lauriers (en dixièmes) et leur détail par source ; la jauge commune
  de colère ; sauvegarde, somme de contrôle ; table de réglages `mp/caesar_rules.h`. *Critères* : ctest
  sauvegarde et reprise identiques tick par tick ; parité verte (classique intact).
  *Fait* : pièce `mp_caesar` du `.mpsav`, remise à zéro par toute partie classique ; protocole 10. Test
  `mp_caesar_state` : lauriers et colère dans la somme de contrôle, jauge bornée de 0 à 100, partie reprise
  identique tick par tick, rien en classique ; il échoue dès la tick 0 si le chargement oublie la pièce.
- [ ] **M9.2** Les cinq notes (prospérité, commerce, habitat, culture-éducation, grandeur ; CESAR §5) et
  `simtool notes SAVE`. Références calibrées sur les cités de `test/data` (CESAR §10.2), valeurs notées dans
  DECISIONS. *Critères* : ctest des notes sur des sauvegardes connues ; flux nets et prix de référence pour le
  commerce (ping-pong entre deux joueurs sans effet).
- [ ] **M9.3** Dons en lauriers : salaire et épargne (coûts d'origine), un don compté par an, rangs selon les
  lauriers, salaire limité par le rang. Commandes réseau. *Critères* : ctest (lauriers par taille de don, second
  don de l'année sans lauriers, salaire au-dessus du rang refusé).
- [ ] **M9.4** Fêtes en lauriers (une comptée tous les 6 mois). *Critère* : ctest.
- [ ] **M9.5** Lauriers mensuels de la cité, **victoire au score** (la première cité à N lauriers, D-053), rangs par
  dixième du score ; remplace le score provisoire de M4.6. *Critères* : ctest de fin de partie à 1 et 2 joueurs,
  deux cités au score le même mois départagées par les lauriers.
- [ ] **M9.6** Interface : conseiller impérial multijoueur (notes, rang, lauriers par source, dons), bandeau (lauriers
  et jauge), lettres de César en plein écran, salon (mode « Jugement de César », score, options). *Critère* :
  captures sans fenêtre, en partie seule et à deux.
- [ ] **M9.7** Campagnes de César : batailles lointaines partagées, troupes de tous les joueurs additionnées,
  récompenses selon la part, refus pénalisé. *Critères* : ctest (envoi, voyage, résolution, récompenses), partie
  reprise identique pendant une campagne.
- [ ] **M9.8** Demandes de César pour toute la province (CESAR §6.4, D-053) : envois de chacun en plusieurs fois,
  cagnotte partagée selon les envois, échéance. *Critères* : ctest (partage, échéance manquée, partie seule).
- [ ] **M9.9** Télémétrie mensuelle dans la sauvegarde et `simtool laurels PARTIE.mpsav` ; LISEZMOI ; DMG pour
  Alexandre.

## M10 — La guerre sous l'œil de César ([CESAR.md](CESAR.md) §7, DESIGN §7)
- [ ] **M10.1** Hostilité par propriétaire et état de guerre par paire de joueurs : déclaration (commande, annonce ;
  guerre honorable avec 3 mois de préavis, ou brutale et immédiate, D-053), paix proposée des deux côtés. En classique, la matrice actuelle exactement.
- [ ] **M10.2** Légions chez l'adversaire, ordre « attaquer », portes et murs qui ne laissent passer que leur
  propriétaire.
- [ ] **M10.3** Interception des caravanes (ancien M8.7) : cargaison prise, route coupée.
- [ ] **M10.4** Motifs de guerre (riposte, mandat de César, sans motif) et prix de la guerre brutale, triomphes,
  lauriers gagnés ou perdus.
- [ ] **M10.5** Jauge de colère : durée, puissance, dégâts (sans les caravanes d'une ressource demandée par César),
  ×1,5 en guerre brutale, décrue, belligérance de chacun ; avertissement et ultimatum (paix imposée, « ennemi de
  Rome »).
- [ ] **M10.6** Expédition punitive : invasion de César dans chaque cité, disgrâce de 12 mois, pertes du fautif ;
  seconde expédition fatale (option, désactivée par défaut).
- [ ] **M10.7** Armée trop puissante : légions au-delà de la tolérance de César.
- [ ] **M10.8** Moral et totaux par camp, arcs de triomphe ; combats rejoués et duels scriptés (CESAR §10.3),
  captures.

## M11 — Équilibrage et finitions
- [ ] **M11.1** Bornes d'équilibre en ctest (CESAR §10.3 et §10.5) : frappe ciblée, guerre totale, dons seuls,
  rapport des pertes entre victime et agresseur.
- [ ] **M11.2** Parties d'Alexandre lues avec `simtool laurels`, réglages ajustés et notés dans DECISIONS.
- [ ] **M11.3** Équilibrage par parties simulées sans tête : autorisations, prix et rareté des **armes**, quotas,
  fonds de départ, rythme. Performance, paquets d'installation (app macOS, exécutable Windows), documentation pour
  les joueurs.
