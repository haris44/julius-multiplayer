# Feuille de route

> **Jalon en cours : M9, César juge** (D-050, D-053, plan complet : [CESAR.md](CESAR.md)). César revient comme
> arbitre : lauriers de la cité et de César, campagnes, demandes ; la première cité au score gagne. Jouable en paix
> et seul avant la guerre (M10). Ordre : M9.1 → M9.9.
>
> **Nuit du 2026-10-06** (à partir de 23 h 45) : d'abord les demandes **T4** d'Alexandre, avec ce qu'il ajoutera
> pendant la session de test du soir. Elles passent avant la suite de M9, et T4.1 et T4.2 la redéfinissent en
> partie.

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
  rejeu. *En cours* (D-054) : dépôt `haris44/julius-multiplayer` ; `.github/workflows/multiplayer.yml` lance tous
  les tests sous Linux, Windows (MinGW 64 bits) et macOS, et fabrique l'AppImage, le dossier Windows et le DMG.
  Premier passage
  vert le 2026-10-06 : 173 tests sous Linux, 162 sous Windows (les parties en réseau sur un même ordinateur n'y
  tournent pas : leurs scripts sont en `sh`). Le `.exe` ne demande que SDL2 et SDL2_mixer, fournies, et des
  bibliothèques de Windows 10. Reste : une tâche qui compare les traces d'une même partie multijoueur calculées sur
  Mac, Linux et Windows.
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

## T4 — Demandes d'Alexandre pour la version suivante (2026-10-06 soir)
Notées le 2026-10-06 en fin de journée. Travail de nuit à partir de 23 h 45, avec ce qu'Alexandre ajoutera pendant
la session de test du soir. Les questions marquées **?** se tranchent ce soir ; sinon je tranche provisoirement et
le note « à valider ».

**État au 2026-10-07** (après la nuit et la revue) :

| Tâche | Sujet | État | Décisions |
|-------|-------|------|-----------|
| T4.1 | Lauriers dans l'évaluation de la cité, estime = lauriers | fait, à valider | D-071 |
| T4.2 | Cadeaux à César par commandes | fait | D-067, D-073 |
| T4.3 | L'empire vend toujours ; hausse des prix à l'arrêt du commerce | en partie : l'empire vend toujours ; la hausse n'est pas faite (A, B ou C à choisir) | D-060, D-066 |
| T4.4 | « Facile » par défaut | fait | D-063 |
| T4.5 | Stock minimum et maximum du commerce | fait, à valider | D-070 |
| T4.6 | Statistiques : maquettes | maquettes faites, à valider ; pas de code | MAQUETTES.md |
| T4.7 | Mission de plus : 30 marbres | fait | D-067 |
| T4.8 | Pont de César dans la guerre | plan fait, à valider ; code avec M10 | D-068, D-073 |
| T4.9 | Eau de César pour J2 à J4 | fait | D-064, D-073 |
| T4.10 | Difficulté du salon pour toute la partie | fait | D-063, D-073 |
| T4.11 | Règles du salon synchronisées ; invasions coupées | fait (partie reprise : à valider) | D-063, D-073 |
| T4.12 | Prix de Rome et portorium | fait | D-060, D-061, D-066 |
| T4.13 | Menu de construction du joueur local | fait | D-061, D-064 |
| T4.14 | Deux cartes de plus, choix dans le salon | fait, à valider | D-069 |
| T4.15 | Nourriture du plan, plus de bois dans les terres | fait | D-062, D-065 |
| T4.16 | Le jeu paraît plus restrictif | mesuré : pas de bug ; à valider | D-072 |
| T4.17 | Chemins sur les grandes cartes | fait | D-064, D-073 |

**Questions « à valider » pour Alexandre**, par thème :
- **Commerce**
  - Prix de Rome = prix d'achat de base de l'original (marbre 200) ? (D-060, D-066)
  - Hausse des prix quand un joueur arrête le commerce : A (route fermée, portorium de la victime à 100 % pendant
    12 mois), B (embargo par ressource) ou C (Rome punit le fautif) ? Préférence : A. (D-066, T4.3)
  - Le portorium s'applique aussi en partie seule ? (D-066)
  - Un achat à un autre joueur n'ouvre pas d'atelier ; les commandes ne vérifient pas les ateliers. (D-061, D-065)
  - « Acheter jusqu'à M » remplace la limite automatique de l'empire (plutôt que le plus petit des deux) ? Un pas
    de 4 chargements par clic ? Une seule paire de bornes par ressource, pas par partenaire ? (D-070)
  - La page du commerce multijoueur aussi en partie seule (revoit D-051) ? (D-070)
- **Cartes**
  - « Carte au hasard » par défaut ? Les deux nouvelles cartes (formes, places, tailles) ? Sur la carte 2 pour 2, le
    côtier arrive par le bord sud ? (D-069)
  - Répartition hors nourriture : olives à un joueur des terres, vignes à l'autre à 4 ; olives aux terres et vignes
    à la côte à 2. (D-062)
  - Rapprocher le point d'entrée de chaque joueur, si le début de partie paraît lent (route d'entrée de 48 à 63
    cases à 4) ? (D-072)
- **Salon et règles**
  - Réglages modifiables jusqu'au lancement, plutôt que figés après « Héberger » ? (D-063)
  - La ligne de commande (`tools/play-mp.sh`) passe-t-elle aussi en facile ? (D-063)
  - Une partie reprise garde ses règles : le salon ne peut plus rien changer. Une carte `.mpmap` garde ses
    territoires. (D-073)
- **César**
  - L'estime = les lauriers rapportés au score (lecture 2) ; garder le pilier de la paix ou le remplacer par une note
    de César ; sans score, hauteur rapportée au rang suivant ou aux 1 000 lauriers ? (D-071)
  - Au rang 0, le salaire est nul : pas d'épargne ni de cadeau au début. Une épargne de départ ou un salaire plus
    tôt ? (D-067)
  - Pont de César : la terre de César (option 1 sur 3) ? (D-068)
- **Statistiques** : les sept questions de [MAQUETTES.md](MAQUETTES.md) §10 (option, premières statistiques, ce
  qu'on voit des autres, durée, unités, nom). (T4.6)
- **Gêne ressentie** : si le jeu paraît encore restrictif en facile, où et quand (une capture) ? Le rétrécissement
  de la zone d'une cité sans bras n'est pas mesuré. (D-072)

- [~] **T4.1** César dans l'**évaluation de la cité** (conseiller des notes, les piliers de marbre) : y montrer
  l'évolution, puis les lauriers ; **regrouper les lauriers et l'estime de César** déjà existante, pour un seul
  système plus simple (Alexandre : « à voir ce qui est possible de faire pour que ce soit plus simple »).
  **?** Comment fondre un compteur cumulé (les lauriers, sans plafond) et une jauge de 0 à 100 (l'estime) :
  - l'estime devient le niveau de César envers la cité, et les lauriers s'accumulent chaque mois selon ce niveau ;
  - ou le pilier « Estime » montre les lauriers rapportés au score.
  Les autres piliers (culture, prospérité, paix) sont-ils gardés ?
  *Fait* (lecture proposée, **à valider**, D-071) : l'estime de César, ce sont les lauriers. En multijoueur, le 4e
  pilier devient « Lauriers » : hauteur = lauriers rapportés au score (sans score, au rang suivant), nombre = lauriers.
  Cliqué, il dit le gain du mois dernier, la tendance, la place dans la province et ce qui rapporte des lauriers.
  Culture, prospérité et paix restent ceux d'origine. Plus aucune faveur affichée en multijoueur : drapeau et
  infobulle du sénat, barre latérale montrent aussi les lauriers. Historique des 12 derniers mois par cité dans
  `mp_caesar` (version 4, protocole 18). Test `mp_caesar_history` (gain, tendance, estime, sauvegarde, deux machines,
  ancienne sauvegarde, classique) ; capture `test/automation/caesar-ratings.txt`.
- [x] **T4.2** **Cadeaux à César** : ils comptent dans son estime (avec T4.1). Ils doivent passer par des
  commandes réseau (aujourd'hui, les boutons sont cachés en multijoueur, D-057).
  *Fait* (D-067) : cadeau, salaire et don sont des commandes réseau (`MP_ACTION_SEND_GIFT`, `SET_SALARY`,
  `DONATE`), les trois boutons du conseiller impérial reviennent en multijoueur. Un cadeau rapporte 4, 7 ou 10
  lauriers, un seul compte tous les 12 mois (état sauvegardé, `mp_caesar` version 3) ; le salaire est limité par le
  rang et de nouveau versé. Test `mp_caesar_gifts` : épargne et lauriers de l'expéditeur seul, deux machines au même
  résultat (somme de contrôle), ancienne sauvegarde chargée, classique sans lauriers.
  *Fait (revue, D-073)* : en multijoueur, le cadeau ne touche plus à la faveur d'origine, ni aux mois depuis le
  dernier cadeau, ni à la pénalité des cadeaux répétés ; épargne et lauriers inchangés. Test `mp_caesar_gifts`.
- [ ] **T4.3** **L'empire vend toujours** : on peut toujours acheter à l'étranger, ce qui revoit D-048. En
  revanche, l'empire **augmente fortement ses prix quand un joueur arrête le commerce**.
  **?** Arrêter quel commerce :
  - un vendeur qui ne vend plus une ressource à un joueur (embargo), et l'empire qui fait alors payer plus cher
    cette ressource à la victime ;
  - ou un joueur qui ferme sa route avec un autre ?
  Plus cher de combien, et pendant combien de temps ?
  *Fait (première partie, avec T4.12)* : l'empire vend toujours, même ce qu'un joueur vend moins cher et même quand
  ce joueur n'en a plus (fin de la règle D-048). Test `mp_trade_empire_always_sells`. *Reste* : la hausse des prix
  quand un joueur arrête le commerce. Trois propositions dans D-066, *à valider*.
- [x] **T4.4** Le multijoueur proposé en **facile** par défaut (salon).
  *Fait* : le salon propose « facile » (`mp_lobby_rules_init`, nouveau module `mp/lobby.c`, D-063). Les règles par
  défaut des tests restent en difficile, aucun test n'a changé. Test `mp_lobby_rules`.
- [~] **T4.5** Page du commerce : un **stock minimum et maximum** dans les entrepôts, pour l'import et pour
  l'export. Pour mémoire : l'original a un seuil d'export, et Julius des réglages par entrepôt.
  **?** Les deux bornes valent-elles pour l'empire et pour les joueurs ?
  *Fait* (D-070, **à valider**) : par ressource et par cité, « vendre au-dessus de N » (le seuil d'export de
  l'original) et « acheter jusqu'à M » (0 = sans), pour l'empire et pour les joueurs. M est une commande réseau
  (`MP_ACTION_CHANGE_BUY_LIMIT`) et une pièce de la sauvegarde (`mp_trade_bounds`). Onglet « Stocks » de la page du
  commerce, aussi en partie seule. Test `mp_trade_bounds` : l'acheteur s'arrête à M chez un joueur et chez l'empire,
  le vendeur garde N, deux machines à la même somme de contrôle, bornes sauvegardées, ancienne partie chargée sans
  borne, classique sans borne.
  *Q2-ui-640* : les boutons de 16 px de la page (prix, bornes, « non », statut) n'avaient ni bord droit ni bord bas
  (`button_border_draw` ne dessine que le coin haut-gauche sur un seul bloc) : un cadre dessiné, jaune au survol. La
  bannière de la partie (T4.1) ne dépasse plus la vue (sans la barre latérale) : sans « Multijoueur - joueur N », puis
  sans l'unité de la cible, puis sans la cible. Capture `ui-host-city.png` ajoutée à `tools/mp-ui-test.sh`.
- [~] **T4.6** **Statistiques et décisions** : trouver une manière propre de faire évoluer ces vues. On voudra des
  statistiques et un commerce plus fin, sans surcharger l'écran.
  - Proposer d'abord des maquettes (pages de design à regarder ensemble), avant tout code.
  - **?** Quelles statistiques en premier : évolution des lauriers et des notes, échanges par partenaire, prix ?
  *Fait* (maquettes écrites, **à valider**) : [MAQUETTES.md](MAQUETTES.md). Trois options dessinées à la taille de
  l'écran (640 × 480) avec les pièces du jeu d'origine : onglets dans chaque conseiller, **les Annales** (une page
  de courbes ouverte depuis les conseillers, recommandée), une vue d'ensemble en vignettes. Pour le commerce fin, une
  fiche par ressource (prix par joueur, bornes de T4.5). Statistiques proposées d'abord : lauriers et notes, échanges
  par partenaire, prix. Données à enregistrer chaque mois (environ 450 octets par cité), la même pièce que la
  télémétrie de M9.9. Sept questions pour Alexandre (§10). Pas de code, donc pas de test.
- [x] **T4.7** **Une mission supplémentaire coûte 30 chargements de marbre** (4 aujourd'hui), pour éviter les
  extensions trop sauvages. Pas de changement pour la première, gratuite tant qu'on n'a pas de terre.
  *Fait* : `MP_MISSION_MARBLE_LOADS` passe à 30 (D-067). La première mission reste gratuite. Textes du salon
  (message de construction), README et LISEZMOI mis à jour. Test `mp_missions` : 29 chargements ne suffisent pas,
  30 sont prélevés.
- [~] **T4.8** Plan de la guerre : réfléchir à l'**impact du pont de César**, seul passage terrestre entre les deux
  rives à 4 joueurs, donc un endroit très stratégique. Peut-on le bloquer, le tenir, le couper ? Que fait César
  si un joueur le ferme ? Va dans CESAR.md §7 et DESIGN §7 avant M10.
  *Fait* (plan écrit, **à valider**, D-068) : [CESAR.md](CESAR.md) §7.6 et DESIGN §7.1. Ce qui passe par le pont
  (caravanes entre rives, missionnaire, armées ; ni l'eau, ni l'étranger, ni les campagnes). Trois options ; proposé :
  la **terre de César**, 15 cases autour du pont où rien ne se bâtit, pont ouvert à tous en paix, tenu par les
  soldats en guerre sans gêner les neutres, légions sur sa terre comptées dans la colère. Relevé au passage :
  aujourd'hui une porte semble pouvoir se poser sur une route de César dans sa zone (lu dans le code, à vérifier),
  et un aqueduc ne traverse pas la mer (carte à 4 refaite : un réservoir par rive ayant un joueur des terres).
  Tests à écrire avec M10, listés en DESIGN §7.1.
  *Fait (revue, D-073)* : CESAR §7.6 recalcule les distances au pont pour chaque emplacement de la carte à 4 refaite
  (côte 82 et 85 cases par la route, terres 136 et 139) ; seules les légions de l'**agresseur** sur la terre de César
  comptent dans la colère, comme au §7.2.
- [x] **T4.9** **Bug : l'aqueduc de César ne donne pas d'eau à J2, J3, J4** (essai du soir). *Diagnostic* : l'eau de
  chaque cité se calcule l'une après l'autre (`map_water_supply_update_reservoir_fountain_of_city`, tick 27), mais
  l'état « aqueduc en eau » de la grille n'est remis à zéro qu'au tour de la première cité. Au tour des suivantes,
  `fill_aqueducts_from_offset` saute les cases déjà en eau (`!map_aqueduct_at`) : la propagation depuis le réservoir
  de César s'arrête net et n'atteint jamais leur réservoir. Seul J1 reçoit l'eau.
  *Correctif prévu* : une marque « visitée » propre à chaque passage, en multijoueur seulement (le classique reste
  identique). *Test* : `simtool` avec le joueur des terres en J2, J3 et J4 (graines de placement), réservoir
  alimenté et maisons desservies.
  *Fait* (D-064) : en multijoueur, `fill_aqueducts_from_offset` tient sa propre grille « atteint », remise à zéro au
  tour de chaque cité, au lieu de lire l'eau de la grille des aqueducs ; le classique garde son test d'origine. Test
  `mp_caesar_aqueduct_{2,3,4}_players` (`simtool inlandwater`) : pour chaque joueur, un tirage de placement le met
  à l'intérieur des terres ; son réservoir au bout de l'aqueduc de César a de l'eau, une fontaine à portée aussi, la
  maison voisine est desservie ; tout l'aqueduc de César porte l'eau ; un aqueduc isolé de chaque joueur reste à
  sec ; deux parties identiques donnent la même somme de contrôle. Le test échouait avant le correctif.
  *Fait (revue, D-073)* : le test ajoute, à 2, 3 et 4 joueurs, un réservoir et un aqueduc de chaque autre joueur dans
  sa zone, loin de l'eau : ils restent à sec, l'eau du joueur des terres ne les atteint pas ; à 4, l'autre joueur des
  terres a l'eau dans son réservoir et dans l'aqueduc qui en part. Cas risqué (zones qui se touchent) : le joueur
  suivant raccorde son propre aqueduc, puis un réservoir à lui, au bout de l'aqueduc que le joueur des terres tire
  de son réservoir ; l'eau s'arrête à la limite : l'aqueduc et le réservoir de l'autre restent à sec. Sans le
  contrôle du propriétaire dans `fill_aqueducts_from_offset`, ce test échoue.
- [x] **T4.10** **La difficulté est celle du salon, pour toute la partie** (essai du soir). Celle réglée dans le menu
  principal ne règle pas la partie, et on ne doit plus pouvoir la régler par joueur.
  *État* : la simulation lit bien la difficulté du salon (`game_rules_difficulty`). Mais le menu Options,
  Difficulté, reste ouvert en partie, change seulement le réglage local, sans effet. La ville affiche aussi ce
  réglage local (`window/city.c`), d'où la confusion.
  *À faire* :
  - en multijoueur, le menu Difficulté est désactivé, et l'affichage montre la difficulté du salon ;
  - vérifier ce que voient les clients dans le salon (la difficulté de l'hôte) ;
  - « facile » par défaut (T4.4).
  *Fait* : en multijoueur, Options n'a plus l'entrée Difficulté (`widget/top_menu.c`, cachée, et sans effet si on
  l'atteint) ; l'affichage de la difficulté en ville (`window/city.c`) lit `game_rules_difficulty()`. Les clients
  voient la difficulté de l'hôte dans le salon (T4.11). Classique inchangé. Pas de test automatique pour
  l'interface ; la règle elle-même est testée par `mp_lan_lobby_rules` (la partie se joue avec la difficulté du
  salon, chez l'hôte et chez les clients).
  *Fait (revue, D-073)* : le conseiller religieux suit les dieux de la partie (`game_rules_gods_enabled`) ;
  `tools/check-determinism.sh` refuse toute autre lecture des réglages locaux (difficulté, dieux, correctifs).
- [x] **T4.11** **Les réglages du salon ne sont pas synchronisés ; les insurrections ne se désactivent pas** (essai du
  soir : « même les insurrections IA ne sont pas désactivées même si on les désactive »).
  *Diagnostic* :
  - le salon d'un joueur qui rejoint affiche et laisse cliquer ses propres réglages, sans effet : seuls ceux de
    l'hôte partent avec la partie (message d'accueil), et les autres ne les voient qu'au lancement ;
  - « Invasions IA : non » coupe les invasions et les soulèvements locaux du scénario, mais pas le **soulèvement
    envoyé par Mars** en colère (`scenario_invasion_start_from_mars`, dieux activés), ni les révoltes de gladiateurs
    du scénario.
  *À faire* :
  - le salon des joueurs qui rejoignent montre les réglages de l'hôte, en lecture seule (un message du salon,
    changement de protocole) ;
  - « non » coupe toutes les invasions et tous les soulèvements, ceux du scénario comme celui de Mars, dès le
    début de la partie. Les **révoltes de gladiateurs restent** (Alexandre) ;
  - un test par source d'attaque.
  *Précisions d'Alexandre* :
  - il était l'**hôte**, il a vu « Soulèvement local », en début de partie, invasions désactivées dans le salon ;
  - il pense que le démarrage en difficile y contribue : la difficulté du salon (difficile par défaut) s'applique
    quel que soit le réglage du menu principal (T4.10).
  Alexandre : pas de colère de Mars, c'est bien un problème de synchronisation des options.
  *Cause trouvée* : le salon lit les réglages au clic sur « Héberger » (`button_host` puis `mp_lockstep_set_rules`).
  En attendant les joueurs, les boutons des règles restent actifs et changent l'affichage, mais la partie garde les
  réglages du clic. Invasions coupées après « Héberger » : la partie démarre avec les invasions.
  *Correctif* :
  - les réglages sont relus au clic sur « Lancer la partie » (ou figés et grisés dès « Héberger ») ;
  - les clients voient les réglages de l'hôte ;
  - test : changer chaque réglage après « Héberger », vérifier les règles de la partie chez l'hôte et chez un client.
  *En attendant* (essais du soir) : choisir les réglages **avant** de cliquer sur « Héberger ».
  *Vérification de la boucle d'événements* (Alexandre : « c'est peut-être elle qui injecte des événements
  d'invasion en dehors du moteur classique ») : non. Toutes les armées naissent dans `scenario/invasion.c`
  (`start_invasion`), par quatre entrées : les invasions et soulèvements du scénario (`scenario_invasion_process`,
  tick du mois, coupés par la règle), Mars (`city/gods.c`, pas coupé, voir ci-dessus), César (`city/emperor.c`,
  jamais en multijoueur), la triche. La règle est en place avant le premier tick : l'hôte appelle
  `start_session` avant d'envoyer l'accueil, le client la reçoit dans l'accueil et l'applique en dernier, après la
  sauvegarde reçue. Seul le réglage lu au clic sur « Héberger » est faux. À noter : le scénario est copié par
  cité, chaque cité traite donc le calendrier du modèle pour elle-même ; un soulèvement du modèle frappe chaque
  cité, dans sa propre cité. Le correctif doit aussi mettre à jour `data.rules` de l'hôte, pas seulement
  `host_rules` (`mp_lockstep_set_rules` ne touche que la copie lue par `mp_lockstep_host`).
  *Fait* (D-063) :
  - les réglages restent modifiables après « Héberger » ; chaque changement part aussitôt dans la partie
    (`mp_lockstep_set_rules` met aussi à jour `data.rules`) et chez les joueurs déjà là ; « Lancer la partie »
    relit les réglages du moment (`mp_lobby_start_game`) ;
  - les joueurs qui rejoignent voient les réglages de l'hôte, en lecture seule, tenus à jour (message `MSG_RULES`,
    protocole 13) ;
  - « Invasions IA : non » coupe aussi le soulèvement de Mars (`scenario_invasion_start_from_mars`) ; les révoltes
    de gladiateurs restent ; classique inchangé.
  *Tests* : `mp_attack_source_army`, `mp_attack_source_uprising`, `mp_attack_source_mars` (chaque source attaque en
  classique et avec l'option, aucune sans) ; `mp_lobby_rules` (partie seule : les règles changées après
  « Héberger ») ; `mp_lan_lobby_rules` (3 joueurs : l'hôte change tous les réglages après « Héberger », tous jouent
  avec, les clients les ont vus dans leur salon et ne peuvent pas les changer).
  *Fait (revue, D-073)* : les règles reçues de l'hôte sont vérifiées, hors bornes elles sont ignorées (salon) ou
  refusées (accueil) : test `mp_lobby_bad_rules`. Une partie reprise d'un `.mpsav` garde ses règles enregistrées,
  territoires compris ; le salon les montre grisées (*à valider*) : test `mp_lobby_resume_rules`.
  *Fait (finitions du salon)* : les textes d'état de `lockstep.c` sont des clés de traduction
  (`TR_MP_STATUS_*`, français et anglais, chiffres par `%d` et `%s`), les tests comparent la clé
  (`mp_lockstep_status_key`) ; un joueur qui a rejoint ne voit plus « Héberger » ni « - / + » (idem « - / + » chez
  l'hôte une fois la partie ouverte, qui ne changeaient que l'affichage, *à valider*) ; `MSG_RULES` porte le nombre de joueurs de
  l'hôte (protocole 19), affiché en lecture seule même quand on rejoint par adresse. Tests : `mp_status_translations`,
  `mp_lobby_rules` (textes et nombre de joueurs), `mp_lobby_bad_rules` (nombre hors bornes), `mp_lan_lobby_rules`
  (chaque joueur voit 3 joueurs).
- [x] **T4.12** **Les prix entre joueurs sont mal calculés par rapport à l'empire** (essai du soir).
  *Règle actuelle* (D-043, D-048) :
  - le prix par défaut d'un joueur est le prix d'achat de base de l'empire ;
  - acheter à l'empire coûte ce prix + 50 % ;
  - vendre à l'empire rapporte son prix de vente, plus bas. Marbre : de base 200 ; l'empire le vend 300 et
    l'achète 140 ; entre joueurs, 200 par défaut.
  *Réponse d'Alexandre* (D-060) : plus de prix d'achat et de vente différents. Chaque ressource a un **prix fixe de
  l'empire**, et une **taxe de douane, le portorium**, s'applique à l'entrée et à la sortie de la province. C'est
  elle qui fera monter les prix (T4.3). À traiter avec T4.3.
  *Précision d'Alexandre* (D-060, D-061) : le prix de départ est le prix de Rome, le même pour tout le monde ;
  acheter à l'étranger coûte le prix de Rome **+ 50 %** de douane ; entre joueurs, le prix de Rome par défaut, que le
  vendeur modifie comme il le souhaite.
  *À faire* : un seul prix par ressource (`empire/trade_prices`), le portorium à 50 % à l'achat et à la vente à
  l'empire (confirmé par Alexandre), le prix de Rome par défaut entre joueurs, la page du commerce qui montre
  prix de Rome, douane et prix payé. Tests : prix payés et reçus avec l'empire, prix par défaut entre joueurs.
  *Fait* (D-066) :
  - un seul prix par ressource, le **prix de Rome** (prix d'achat de base de l'original, les variations du jeu
    s'appliquent) ; le portorium vaut 50 % de ce prix (`empire/trade_prices`, multijoueur seulement) ;
  - acheter à l'empire coûte Rome + 50 % (marbre 300), vendre à l'empire rapporte Rome − 50 % (marbre 100). Cela vaut
    pour les marchands et les navires aux entrepôts, les recettes et dépenses du commerce, et la fenêtre « Prix de
    l'empire » ;
  - entre joueurs : prix de Rome par défaut, sans douane ; chaque joueur a son propre commerce avec l'empire (D-061) ;
  - la page du commerce montre, pour l'empire, le prix de Rome, le prix payé ou reçu, et « Empire, portorium 50 % »
    en titre ;
  - tests : `mp_empire_import_price` (1,5 fois à l'achat, 0,5 fois à la vente, toutes les ressources, variation de
    prix, classique inchangé), `mp_trade_prices` (prix de Rome par défaut entre joueurs), `mp_trade_empire_isolation`
    (le joueur 1 ouvre une route de l'empire, importe, achète et vend pendant 60 jours : le commerce du joueur 2
    avec l'empire ne bouge pas d'un octet), `mp_trade_empire_always_sells` (T4.3). Capture : `tools/mp-trade-test.sh`.
  *Fait (revue, D-073)* : `mp_trade_prices` vérifie une vraie livraison entre joueurs, payée au prix du vendeur par
  l'un et à l'autre, sans portorium ; l'aide de `simtool` est à jour (`empiresells`, `tradeisolation`).
- [x] **T4.13** **Bug : le joueur côtier perd l'argile et reçoit le fer et le marbre** (essai du soir : « les
  ressources ne sont plus les bonnes entre le joueur terrestre et le joueur côtier »).
  *Diagnostic* : les permissions de chaque cité sont justes, dans la carte et dans la sauvegarde. Le menu de
  construction est un état de l'interface, unique, recalculé par `building_menu_update` dans la cité courante.
  Quand un joueur ouvre une route de commerce (`MP_ACTION_OPEN_TRADE_ROUTE`, `mp/actions.c`), la commande s'exécute
  dans **sa** cité, chez tous les joueurs : chacun voit alors le menu de ce joueur. Si le joueur des terres ouvre
  une route, le côtier perd l'argile et reçoit le fer et le marbre ; et inversement. La simulation n'est pas
  touchée (le menu ne sert qu'à l'interface), d'où l'absence de désynchronisation.
  *Règle d'Alexandre* (D-061) : le commerce avec l'étranger n'est pas partagé entre joueurs ; le menu de
  construction de chacun ne montre que les matières premières de son emplacement (argile et bois pour les côtiers,
  marbre et fer pour les terriens), quoi que fasse un autre joueur avec l'étranger.
  *Correctif prévu* : le menu ne se recalcule que dans la cité du joueur local (garde dans `building_menu_update`,
  sans effet en classique), et jamais pendant la commande d'un autre joueur. *Tests* : `simtool`, J1 ouvre une
  route pendant que J2 est le joueur local ; le menu de J2 garde l'argile, sans fer ni marbre ; puis l'inverse.
  Vérifier aussi qu'une route ouverte par J1 n'apparaît pas dans le commerce de J2.
  *Fait* (D-064) : `building_menu_update` ne fait rien quand la cité courante n'est pas celle du joueur local
  (`mp_session_is_other_players_city()`, faux en classique) ; la route qu'ouvre le joueur local dans sa propre cité
  recalcule bien son menu. Test `mp_build_menu_owner` (`simtool menuowner`) : à deux, chacun son tour joueur
  local ; l'autre ouvre une route, le menu local garde les mêmes matières et n'est pas recalculé, la route n'est
  pas ouverte chez lui ; sa propre route recalcule son menu. Le test échouait avant le correctif. `mp_permissions_*`
  règle maintenant le joueur local de chaque cité qu'il interroge.
  *En attendant* (essais du soir) : sauvegarder et reprendre depuis le salon recalcule le bon menu.
- [~] **T4.14** **Deux cartes préparées de plus** (Alexandre : « avant de terminer, tu me généreras 2 cartes de
  plus »), à faire en fin de nuit.
  *Choix provisoire, à valider* :
  - d'abord **refaire la carte à 4** selon D-062 : deux joueurs des terres (l'aqueduc de César les atteint tous
    les deux) et deux de la côte ; à 3, l'un des deux emplacements de la côte reste libre ;
  - puis une nouvelle carte pour 2 joueurs et une pour 3 ou 4, avec un relief et une disposition différents ;
  - les mêmes règles que les cartes actuelles : bras de mer, pont et aqueduc de César, un joueur des terres,
    emplacements tirés au sort ;
  - la carte est choisie dans le salon, ou tirée au sort.
  *Tests* : ceux des cartes préparées (`mp_prepared_map*`), appliqués à chaque carte.
  *Fait* (D-069, *à valider* par Alexandre, qui doit les essayer) :
  - **carte 2 pour 2 joueurs** (220 cases) : bras de mer en diagonale, du nord-ouest au sud-est ; les deux joueurs
    au sud-ouest de la mer. Joueur des terres dans le coin sud-ouest (arrivée à l'ouest), son réservoir de César sur
    la côte au nord-est de lui ; joueur de la côte au sud, sur le rivage (arrivée au sud). Le pont de César mène au
    grand nord-est sauvage ;
  - **carte 2 pour 3 ou 4 joueurs** (240 cases) : bras de mer sinueux au sud. Les deux joueurs des terres sur la
    grande rive nord, reliés par une route d'ouest en est (olives à l'ouest, vignes à l'est), chacun avec son
    réservoir de César sur la côte nord ; les deux côtiers sur la rive sud. À 3, le sud-est (côte) reste libre ;
  - mêmes règles que la carte 1 : ressources et nourriture du plan, aucune eau à moins de 45 cases des joueurs des
    terres, pont, routes de César jusqu'à chaque joueur, point de pêche et navires des côtiers, tirage des
    emplacements (graine 0 : ordre du plan) ;
  - **le salon choisit la carte** : « Carte 1 », « Carte 2 » ou « Carte au hasard » (par défaut, *à valider*), à côté
    des invasions. Nouvelle règle `prepared_map` dans les règles de la partie : sauvegardée, envoyée avec l'accueil et
    avec les règles du salon (protocole 16). Au hasard, l'hôte tire la carte avec la graine du salon ; la partie garde
    la carte tirée, les clients la reçoivent avec la sauvegarde. Les cartes actuelles sont la carte 1, celle des
    tests et de la ligne de commande ;
  - *tests* : `simtool --map 2` fait passer chaque test des cartes sur la carte 2 : `mp_prepared_map2_{2,3,4}_players`,
    `mp_prepared_map2_drying_{1,2,4}_players`, `mp_caesar_aqueduct_map2_{2,3,4}_players`, `mp_prepared_map2_placement`,
    `mp_prepared_map2_reachable_{2,4}_players`, `mp_missionary_bridge_map2`, `mp_long_routes_map2_{2,4}_players` et
    `mp_long_routes_2_players` ; `mp_prepared_map_choice` (`simtool mapchoice` : règle du salon, tirage identique pour
    une même graine, cartes différentes, carte gardée par la sauvegarde et par la partie hébergée) ;
    `mp_lan_generated_map2` (3 joueurs en réseau sur la carte 2, port 27451). Les scripts d'automatisation du salon
    choisissent la carte 1.
- [x] **T4.15** **Nourritures inexploitées sur la carte** (Alexandre : « quelles sont les ressources qui restent
  et que nous n'exploitons pas sur cette carte ? porc ? »). *Mesuré* (`simtool preparedmap`, modèle Lindum, qui
  affiche désormais ce que chaque cité peut produire) : chacun n'a que le **blé et les légumes** ; les côtiers ont
  en plus la pêche. Ni **fruits**, ni **porcs** pour personne : la nourriture vient de l'empire du modèle, pas du
  plan de la carte (le modèle de test, Brugle, donne les fruits à la place des légumes). Les matières premières
  sont bien celles du plan.
  *Validé par Alexandre* : le plan de la carte fixe aussi la nourriture. Blé et légumes pour tous ; les **porcs**
  aux joueurs des terres, qui n'ont pas la pêche ; les **fruits** aux côtiers (D-062).
  *Et plus de bois dans les terres* (Alexandre, D-062) : retirer le bois des emplacements des terres, inverser le
  test `inland_timber` de `mp_prepared_map_placement`.
  *Fait* (D-062, D-065) :
  - le plan de chaque carte fixe nourriture et matières ; ce sont les autorisations de production de chaque cité,
    sauvegardées avec elle. Terres : blé, légumes, porcs, fer, marbre (olives à l'un, vignes à l'autre à 4 ; olives
    à 2). Côte : blé, légumes, fruits, pêche, bois, argile (vignes à 2). Plus de bois ni de bois plantés dans les
    terres ;
  - carte à 4 refaite (premier point de T4.14) : deux joueurs des terres à l'ouest, un sur chaque rive, chacun avec
    son réservoir de César sur sa côte ; deux côtiers à l'est. À 3, le sud-est (côte) reste libre ;
  - les porcs et les quais donnent la même viande : l'autorisation « viande » ne commande que les porcs, les quais
    n'en dépendent pas (comme dans l'original). Les commandes de construction refusent, comme le menu, une ferme ou
    une matière interdite à la cité (multijoueur seulement) ;
  - tests : `mp_prepared_map_placement` (12 tirages à 2, 3 et 4 : nourriture et matières de chaque emplacement, deux
    joueurs des terres à 3 et 4, pas de bois dans les terres, autorisations) ; `mp_prepared_map_*_players`
    (autorisations identiques après écriture et lecture de la carte, menu et commande : porcs refusés sur la côte et
    permis dans les terres, fruits l'inverse ; les deux joueurs des terres ont l'eau de César dans leur réservoir).
- [x] **T4.16** **Le jeu paraît plus restrictif qu'en classique** (Alexandre : distance d'effet des bâtiments,
  emplacement des habitations, manque de main-d'œuvre ; « peut-être lié à la difficulté en difficile par
  défaut »).
  *Lu dans le code* : rien en multijoueur ne touche la portée des promeneurs, l'embauche ni la migration ; les
  seuls écarts avec une partie classique sont la **zone** (on ne bâtit qu'à 20 cases des maisons habitées et des
  bâtiments pourvus) et la **difficulté** : en difficile, moitié moins d'argent qu'en facile (100 % contre 200 % des
  fonds de départ) et un moral de base de 50 contre 70, d'où des départs et un manque de bras. La taille de la
  carte n'y change rien : les promeneurs comptent leurs pas, pas la carte.
  *À faire* : ne pas toucher aux règles intérieures (I2) ; « facile » par défaut (T4.4) ; puis **mesurer** : la
  même petite cité bâtie par script sur une carte classique et sur la carte préparée, même difficulté, comparer
  population, employés et couverture après deux ans. Un écart serait un bug à chercher, pas une règle à desserrer.
  *Fait* (D-072) : `simtool restrictiveness` bâtit par commandes la même petite cité (63 bâtiments : maisons, boucle de
  routes, puits, grenier, marché, temple, préfecture, ingénieur, ateliers, **réservoirs et fontaines** alimentés par
  l'aqueduc de César ou la mer) puis, au mois 2, 2 fermes et 7 à 9 ateliers ou hôpitaux (plus d'emplois que de bras).
  Elle la joue deux ans, en facile et en difficile, en classique et en multijoueur, seule et à 4 joueurs, et à 4 avec
  un seul bâtisseur. **Pas de bug** : population, maisons, niveau, couverture (fontaines, puits, nourriture, religion,
  portée du marché, du temple, de la préfecture et des ingénieurs), employés, manque de bras, désirabilité,
  immigrants et moral sont identiques entre classique et multijoueur (seule : 434 habitants à 24 mois, 14 maisons de
  niveau 2,2, 13 servies par les fontaines, 7 emplois vides, moral 96 en facile ; 246 contre 274 habitants à 6 mois
  en difficile). Écarts expliqués : difficulté (12 000 Dn et moral 70 contre 6 000 Dn et 50), zone (un atelier de J3
  à cheval sur la zone d'un voisin), mission de départ (20 employés en classique, aucun en multijoueur : plus de bras
  libres), terrain de chaque emplacement. Test `mp_restrictiveness` : deux fautes volontaires sont vues (portée des
  fontaines du multijoueur supprimée ; 40 % de bras en moins).
  *Deuxième passe* : le même plan est aussi bâti sur du **terrain libre d'une vraie carte classique** (`blank` : carte
  libre faite par le test, dans ctest ; `Lugdunum`, `Londinium`, `Cyrene`, `Valentia`, `Lindum` de `donnees-c3`, lancées
  à la main). Chiffres à 24 mois, en tableau (détail dans TESTING §2 ter) :

  | Cité | Difficulté | Pop. à 6 mois | Pop. à 24 mois | Employés | Emplois vides |
  |------|------------|---------------|----------------|----------|---------------|
  | Classique, carte libre du test et 5 cartes d'origine, une cité seule | facile | 198 à 342 | 246 à 406 | 76 à 172 | 0 à 96 |
  | | difficile | 196 à 328 | 236 à 406 | 76 à 172 | 0 à 95 |
  | Classique = multijoueur, carte préparée, seule | facile / difficile | 274 / 246 | 434 | 189 | 6 à 7 |
  | Multijoueur (= classique), carte préparée, 4 joueurs J1 à J4 | facile | 179 à 236 | 334 à 380 | 142 à 162 | 1 à 44 |
  | | difficile | 173 à 222 | 334 à 380 | 142 à 162 | 1 à 44 |

  **Conclusion : pas de bug.** Les cités multijoueur sont dans l'intervalle des cartes d'origine ou au-dessus, jamais
  en dessous ; le plus grand écart est la place 1 ou 2 à 4 joueurs (334) contre Valentia (406), qui est un effet de
  place (la cité classique de la même place a les mêmes 334). Début de partie : à 6 mois, les cités à 4 joueurs
  (173 à 236) sont plus petites que celles des cartes d'origine (196 à 342), les « classiques » de la même carte aussi :
  piste, non isolée, la route d'entrée plus longue (48 à 63 cases). Test `mp_restrictiveness` : garde aussi que le plan
  tient sur `blank`, que son eau coule et qu'aucune cité multijoueur n'est plus de 15 % sous la plus faible cité
  classique. **Limites** : (1) la série dite « classique » à 4 joueurs passe déjà par le code de la carte
  multijoueur (`game_rules_multiplayer_map()` vrai dès 2 cités) : seules les séries « seule » et « carte
  d'origine » comparent de vraies parties classiques ; (2) une cité et une place par carte d'origine : comparaison par
  intervalle, pas au chiffre près ; (3) le manque de main-d'œuvre par rétrécissement de la zone n'est pas mesuré
  (*à valider* avec Alexandre : où et quand sent-il la gêne ?).
- [x] **T4.17** **Circulation des marchandises sur les grandes cartes** (Alexandre : « la circulation des
  marchands n'est pas altérée pour toi ? »). *Lu dans le code* : charretiers, marchés (40 cases, comme l'original),
  entrepôts et greniers travaillent chacun dans sa cité, sans changement ; chaque cité a bien son propre point
  d'entrée, près d'elle (`set_arrival`), d'où partent ses immigrants. **Une limite nouvelle** : toute recherche de
  chemin s'arrête après 50 000 cases explorées (`GUARD`, `map/routing.c`), jamais atteint sur une carte de
  l'original (26 244 cases). La carte à 4 fait 67 600 cases, dont 9 % d'eau et 17 % de forêt : environ 50 000 cases
  praticables, pile la limite. Les trajets courts n'en souffrent pas (la recherche part du plus proche), mais une
  **caravane entre deux joueurs éloignés** peut ne pas trouver son chemin.
  *À faire* : en multijoueur, la garde suit la taille de la grille (le classique garde 50 000, jamais atteint :
  parité intacte) ; test : un chemin entre les deux joueurs les plus éloignés de la carte à 4, et la caravane qui
  arrive. À faire aussi pour les cartes de T4.14.
  *Fait* (D-064) : relecture du code, la garde est plus étroite que prévu : `GUARD` ne borne que les recherches des
  **bateaux et de la dérive** (`route_queue_boat`, `route_queue_dir8`), jamais celles des marcheurs et des caravanes
  (`route_queue`, sans limite). La carte à 4 n'a que 9 % d'eau (environ 6 000 cases) : elle n'était pas atteinte
  non plus, mais un grand lac l'aurait coupée. Elle suit maintenant la grille en multijoueur (une fois et demie le
  nombre de cases, les cases du bord de carte étant revues par les bateaux ; le classique garde 50 000 : parité
  intacte). Limite des figures (`MAX_PATH` 500, `routing_path.c` et `figure/route.c`) : mesurée, le plus long chemin
  entre deux cités de la carte à 4 fait 253 pas par les routes (J1 et J3, par le pont), 198 sur terre, et la mer de
  bord à bord 259 pas : large marge, donc inchangée (l'élargir changerait le format des sauvegardes, `route_paths`).
  Test `mp_long_routes` (`simtool longroutes`) : chemins entre toutes les paires de cités sous 500 pas, caravane de
  8 chargements de marbre entre les deux joueurs les plus éloignés (arrivée en 90 jours), bateau qui parcourt tout le
  bras de mer, puis une mer grande comme la grille dont l'autre coin est atteint (échouait avant le correctif).
  *À refaire* pour chaque nouvelle carte de T4.14 : `simtool longroutes` (chemin le plus long sous 500 pas).
  *Fait (revue, D-073)* : les ennemis avaient, eux, des limites de cases (5 000 et 25 000 dans `figure/route.c`, 400
  pour l'approche d'une armée) : au-delà, ils passaient « à travers tout ». En multijoueur, elles suivent la taille
  de la carte (`map_routing_noncitizen_max_tiles`, classique inchangé). Test `mp_far_invasion` : une armée qui
  débarque au bord le plus éloigné d'une cité de la carte à 4 trouve son chemin par la terre et l'atteint.
  *Fait* (T4.14) : `simtool longroutes` vérifie aussi moins de 400 pas, sur chaque carte. Plus longs chemins par les
  routes : carte 1, 153 pas à 2 et 298 à 4 ; carte 2, 117 pas à 2 et 241 à 4.

## T5 — Deuxième essai à plusieurs (2026-10-07)
Essai d'Alexandre : 3 joueurs, 3 Mac, réseau local filaire stable, version de la nuit (`95190c7f`, protocole 19).
Les journaux de l'essai ne sont pas sur le Mac de développement : diagnostic par le code et des parties simulées.
- [x] **T5.1** Vérifier que J2, J3 et J4 ne jouent pas en difficile (« voir si les J2/J3/J4 ne sont pas en mode
  difficile »). *Fait* (D-076) : le bug était réel et touchait **toutes** les cités. Charger le modèle de carte remet le
  jeu en mode classique, donc la difficulté du réglage local de l'hôte : le trésor et la faveur de départ (copiés dans
  chaque cité) étaient ceux de ce réglage, pas ceux du salon. Reproduit par `mp_lan_start_easy` (salon facile, hôte
  très difficile : 2 250 Dn au lieu de 6 000, faveur 40 au lieu de 60). Corrigé : la génération (et la composition de
  copies) remet les règles du salon et recalcule fonds et faveur. Le reste (sentiment, loups, armées) se calculait déjà
  par `game_rules_difficulty()`. Tests `mp_lan_start_easy`, `mp_lan_start_hard`, `mp_lan_start_easy_saved_template`,
  `mp_lan_start_easy_cities_map` : 3 joueurs en réseau, mêmes valeurs sur l'hôte et les clients.
  *Journal* : au lancement d'une partie, `julius-log.txt` de chaque ordinateur écrit les règles (difficulté, dieux,
  invasions, carte, score) et le départ de chaque cité (trésor, faveur, rang, salaire, épargne), pour comparer les
  joueurs après un essai. Le journal du lancement précédent est gardé (`julius-log-precedent.txt`).
- [x] **T5.2** Aligner les salaires de Rome pour tous les joueurs. *Fait* (D-076, à valider) : les cités héritaient du
  rang du modèle de carte (rang 5 : 20 Dn par mois au départ) et chacun choisissait son salaire. Maintenant même départ
  pour tous (rang 0, salaire 0, épargne 0) et Rome verse à chacun le salaire de son rang de lauriers, même table, sans
  choix. Tests `mp_salaries` (rangs 0, 1, 3 payés 0, 2, 8 Dn), `mp_lan_start_*` (départ identique malgré un modèle de
  rang 5), `mp_caesar_gifts` mis à jour.
- [x] **T5.3** Désynchronisation au tour 18954, puis impossible de reprendre la partie (« on était peut-être tous les
  3 dans les menus à ce moment-là »).
  *Fait* (D-074) : **cause trouvée** : le conseiller religieux écrivait dans la cité le « dieu le moins content »,
  que la simulation ne recalcule qu'en début de mois ; ouvert sur un seul ordinateur, il changeait l'état de cette
  cité, d'où la désynchronisation. En réseau, il le calcule sans l'écrire. Les autres fenêtres sont passées au crible :
  la commande d'automatisation `windowsweep` ouvre et dessine sans faire avancer la partie tous les conseillers, les
  onglets du commerce et Stocks, les fenêtres de César (salaire, cadeau, don, lettre), la fête, les prix, l'empire,
  les messages, les options, la sauvegarde, les résultats, les menus de construction et les calques, et la fiche de
  chaque type de bâtiment (des joueurs et de César), des figures et du terrain : aucune autre n'écrit dans l'état
  simulé. Tests : `tools/mp-sweep-test.sh` (vrai jeu, 142 fenêtres, avec un dieu en colère : échouait sur le
  conseiller religieux avant le correctif ; aussi sur la carte préparée, `MP_SWEEP_ARGS=--mp-generate`) et
  `tools/mp-menus-test.sh` (trois joueurs ouvrent toutes les fenêtres en même temps, puis chacun seul : 349 tours
  vérifiés, sans désynchronisation). **Reprise** : après la désynchronisation, la partie restait « en cours » et le
  salon refusait d'héberger et de rejoindre ; et il n'y avait pas de sauvegarde à reprendre à plusieurs. Désormais
  « Fichier, Nouvelle partie » et le salon terminent la partie finie, chaque ordinateur sauvegarde la partie en réseau
  au début de chaque mois (`autosave.mpsav`, listée par le salon), et le message de désynchronisation dit de
  reprendre depuis le salon. Test `tools/mp-resume-test.sh` (vrai jeu, deux joueurs depuis le salon : désynchronisation
  provoquée, retour au menu, reprise hébergée et rejointe depuis le salon, sans désynchronisation).
- [x] **T5.4** Impossible de charger une partie sauvegardée.
  *Fait* (D-074) : en multijoueur, « Sauvegarder » écrit un `.mpsav`, que la fenêtre « Charger » ne montrait pas
  (elle ne liste que les `.sav`), et le salon restait bloqué après une partie (T5.3). « Charger » liste maintenant
  aussi les parties multijoueur, « (multijoueur) », et en choisir une ouvre le salon avec cette partie et son nombre
  de joueurs ; pendant une partie en réseau, « Charger » et « Rejouer la mission » sont retirés du menu Fichier ;
  « Sauvegarder » montre les `.mpsav` ; le salon garde la partie choisie quand la liste change et lit les noms par
  leur fin. Le chemin réseau marchait : `mp_lan_save_resume_3_players` (carte 1) et
  `mp_lan_save_resume_map2_2_players` (carte 2) sauvegardent comme le menu Fichier, avec le commerce entre joueurs
  (prix, achats, route, limites de stock), puis l'hôte héberge la sauvegarde comme le salon, dans le même processus,
  les joueurs la rejoignent, l'un remplacé par un nouveau processus : mêmes sommes de contrôle partout. Bout en bout
  dans le vrai jeu : `tools/mp-resume-test.sh` (« Sauvegarder », puis « Charger » depuis le menu principal, qui ouvre
  le salon avec la partie choisie, puis « Héberger »). *À valider* : la sauvegarde de « Sauvegarder » n'est que sur
  l'ordinateur qui l'a faite (c'est lui qui héberge la reprise) ; celle du mois est sur tous.
- [ ] **T5.5** Pouvoir attaquer un autre joueur, même simplement (« nécessaire pour le commerce au démarrage »).
- [ ] **T5.6** Simplifier la fenêtre du commerce ; attention aux textes qui se superposent.
- [x] **T5.7** Impossible de commercer entre joueurs : les caravanes ne circulent pas.
  *Fait* (D-075, provisoire, **à valider**) : pas de panne franche reproduite (parties simulées à 2, 3 et 4 joueurs sur
  les deux cartes, lancées comme depuis le salon : les caravanes partent, arrivent, sont payées), mais trois causes de
  « rien ne circule » trouvées et corrigées : une seule caravane à la fois par ressource alors que les trajets durent
  de 3 à 7 mois sur les grandes cartes (désormais une par mois, ce qui est en route compté comme dépensé et rangé) ;
  un premier entrepôt de l'acheteur sur une route à part bloquait tout (désormais un entrepôt joignable par le
  vendeur) ; rien ne disait pourquoi rien ne part (la colonne « En route » le dit, et `julius-log.txt`), et dans le
  vrai jeu la ligne de la route et son bouton se dessinaient l'un sur l'autre (redessinés quand l'état change). Tests
  `mp_trade_every_player_*`, `mp_trade_warehouse_apart*`, `mp_trade_purchase_state`, `tools/mp-trade3-test.sh`
  (trois vrais jeux depuis le salon, routes et achats par clics). Protocole 20.
- [ ] **T5.8** L'aqueduc de César ne doit pas suivre la route de si près.

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
- [x] **MC.6** Plus d'étangs : la mer est la seule eau, l'aqueduc de César la seule eau du joueur des terres
  (D-055, demande d'Alexandre). *Fait* : plus d'étangs dans les forêts, terre isolée changée en rocher, même part de
  forêt ; protocole 11. Tests `mp_prepared_map_*` (aucune eau hors de la mer ; 800 cases d'étangs avant) et
  `mp_prepared_map_drying_{1,2,4}_players` (seul, à 2 et à 4 : réservoir plein au bout de l'aqueduc ; coupé, de
  l'eau encore 100 jours après, sec à 300 jours avec la portée de ses fontaines ; réparé, de nouveau de l'eau).

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
  route, stock vide, marchands de l'empire eux-mêmes, partie à une cité intacte). *Revu par D-060 (T4.3)* : l'empire
  vend toujours ; `mp_trade_preference` est remplacé par `mp_trade_empire_always_sells`.
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
  *En cours* (D-057) : les cinq notes en version provisoire, calculées chaque mois dans `mp/caesar`. La prospérité
  et la culture sont les notes d'origine, l'habitat et la grandeur suivent le plan. Le commerce est provisoire :
  les exportations des douze derniers mois, en deniers, référence 4 000. Reste : la calibration, `simtool notes`,
  et le commerce en flux nets au prix de référence.
- [ ] **M9.3** Dons en lauriers : salaire et épargne (coûts d'origine), un don compté par an, rangs selon les
  lauriers, salaire limité par le rang. Commandes réseau. *Critères* : ctest (lauriers par taille de don, second
  don de l'année sans lauriers, salaire au-dessus du rang refusé).
- [ ] **M9.4** Fêtes en lauriers (une comptée tous les 6 mois). *Critère* : ctest.
- [x] **M9.5** Lauriers mensuels de la cité, **victoire au score** (la première cité à N lauriers, D-053), rangs par
  dixième du score ; remplace le score provisoire de M4.6. *Critères* : ctest de fin de partie à 1 et 2 joueurs,
  deux cités au score le même mois départagées par les lauriers.
  *Fait* (D-057) : fin `GAME_END_CAESAR`, score dans les règles (`caesar_score`, sauvegardé, protocole 12) ;
  lauriers en dixièmes avec report des restes. Le salon propose 500, 1 000 (par défaut), 1 500, 2 000 ou sans fin.
  Le score par années reste dans le code pour les tests, mais n'est plus proposé. Test `mp_caesar_laurels`
  (Valentia : notes 80/66/44/73/66, environ 81 lauriers par an ; égalité départagée ; règles sauvegardées).
- [ ] **M9.6** Interface : conseiller impérial multijoueur (notes, rang, lauriers par source, dons), bandeau (lauriers
  et jauge), lettres de César en plein écran, salon (mode « Jugement de César », score, options). *Critère* :
  captures sans fenêtre, en partie seule et à deux.
  *En cours* (D-057), fait :
  - conseiller impérial multijoueur (`window/mp_imperial`) : lauriers, rang, notes en barres, lauriers par note,
    classement ;
  - bandeau : les lauriers de chacun et le score ;
  - lettres de César (`window/mp_caesar_letter`) : au début de la partie et à chaque nouveau rang ;
  - salon : le score ;
  - écran de fin : lauriers et héritier.
  Captures : `test/automation/caesar-advisor.txt`, `mp-solo-letter.png`, `mp-lobby-host.png`.
  Reste : les boutons dons, salaire et épargne, quand ils seront des commandes (M9.3), la jauge de colère (M10)
  et les options du salon (demandes, classement public).
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

## MG — Troupes et matières de guerre (D-058)
> MG.1 à MG.4 se font seuls, à tout moment (partie seule, essais contre les envahisseurs de l'IA) ; MG.5 attend la
> guerre entre joueurs (M10.2).
- [ ] **MG.1** Javeliniers renforcés en multijoueur : 136 points de vie et 10 d'attaque ; cavaliers et légionnaires
  inchangés. *Critères* : ctest (valeurs selon le mode) ; parité verte, table d'origine en classique.
- [ ] **MG.2** La caserne reçoit du bois et en consomme un chargement par javelinier : stock séparé des armes (5 au
  plus), sauvegardé dans le `.mpsav` ; bois livré seulement si la cité a une légion de javeliniers. *Critères* :
  ctest (sans bois aucun javelinier, un javelinier par chargement, armes réservées aux légionnaires, cavaliers
  gratuits) ; en classique, javeliniers gratuits (parité) ; partie seule (`tools/mp-solo-test.sh`) où un fort de
  javeliniers se remplit grâce au bois.
- [ ] **MG.3** Bois aux seuls côtiers sur les cartes préparées : retiré au joueur des terres avec ses bois proches,
  donné au joueur du sud-est à 4. *Critères* : `mp_prepared_map_*` (autorisations et matériaux de chaque
  emplacement, terre accessible).
- [ ] **MG.4** Fenêtres : la caserne montre ses stocks d'armes et de bois, le fort ce qu'il lui faut pour recruter ;
  textes en français et en anglais. *Critères* : captures du vrai jeu sans fenêtre.
- [ ] **MG.5** Équilibre des troupes, après M10.2 : batailles simulées légion contre javeliniers (arrêtée en tortue,
  en marche, prise de flanc), cavaliers contre javeliniers et contre caravanes ; chiffres ajustés et notés dans
  DECISIONS (avec M11.3). *Critères* : bornes en ctest.

## M11 — Équilibrage et finitions
- [ ] **M11.1** Bornes d'équilibre en ctest (CESAR §10.3 et §10.5) : frappe ciblée, guerre totale, dons seuls,
  rapport des pertes entre victime et agresseur.
- [ ] **M11.2** Parties d'Alexandre lues avec `simtool laurels`, réglages ajustés et notés dans DECISIONS.
- [ ] **M11.3** Équilibrage par parties simulées sans tête : autorisations, prix et rareté des **armes**, quotas,
  fonds de départ, rythme. Performance, paquets d'installation (app macOS, exécutable Windows), documentation pour
  les joueurs.
