# Feuille de route

> **Jalon en cours : M3, moteur multi-cités** (priorité d'Alexandre : statistiques séparées par joueur, E13).
> Ordre retenu : M3.4 → M3.5 → M3.6 → M3.7, puis M3.1 à M3.3 (grande grille), puis M3.8. Le reste de M2 (M2.6, M2.7,
> M2.9 à M2.11) suit, car il ne bloque pas.

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
- [ ] **T1.3** Défilement au bord haut de l'écran bloqué par la barre de menu (plein écran).
  *Fait, à vérifier par Alexandre* (impossible sans vraie fenêtre) : la barre de menu du jeu laisse passer la
  souris ; c'est celle de macOS qui descendait sur le jeu en plein écran « Space ». Le plein écran du jeu n'utilise
  plus de Space sur macOS (`SDL_HINT_VIDEO_MAC_FULLSCREEN_SPACES`) : barre de menu et Dock restent cachés.
- [x] **T1.4** 1920×1080 et 2K proposées dans la liste des résolutions.
  *Fait* : 1280 × 720, 1920 × 1080 et 2560 × 1440 après les trois tailles d'origine (capture : fenêtre de
  1920 × 1080 obtenue).
- [x] **T1.5** Routes de la carte à César : neutres (pas à la couleur d'un joueur), indestructibles (D-034).
  *Fait* : propriétaire `MAP_OWNER_CAESAR` ; le rouge venait du mode « copie de carte », qui attribuait les routes
  de chaque copie à son joueur. Test `mp_caesar_roads` : aucun joueur ne les démolit, pas de teinte, les maisons
  qui les bordent reçoivent des immigrants. Le dessin en blanc reste à faire (MC.1).
- [x] **T1.6** Le salon ne propose plus que les cartes multijoueur (D-033).
  *Fait* : plus de choix « copies / carte générée » ; une partie ou carte multijoueur reprend telle quelle, tout
  autre fichier donne le climat, l'empire et les fonds d'une carte générée. `tools/mp-lobby-test.sh` n'a plus de
  variante.

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
- [ ] **MT.4** Bâtiments hors zone : message puis effondrement après le délai de grâce.
- [ ] **MT.5** Frontières tracées à la couleur du joueur, teinte légère des bâtiments adverses (D-039).

## MB — Brouillard de guerre (D-038)
- [ ] **MB.1** Option du salon, zones découvertes, éclairage de 20 cases, minicarte et scores adverses masqués.

## MA — Apparence (D-039)
- [ ] **MA.1** Variantes de couleur des bâtiments calculées au lancement (toits brique, ardoise…), jamais dans git.

## M8 — Commerce entre joueurs
- [ ] **M8.1** Conception détaillée (D-019) ; routes commerciales par joueur avec les villes de l'empire.
- [ ] **M8.2** Ouverture d'une route entre deux joueurs (accord des deux, chemin routier entre entrepôts).
- [ ] **M8.3** Caravanes entre joueurs, uniquement sur les routes, avec achat et vente selon les réglages, quotas
  et argent.
- [ ] **M8.4** Interception : caravanes attaquables, cargaison perdue, route coupée.
- [ ] **M8.5** Tests : rejeux, cargaisons et argent conservés, interception, scénarios visuels.

## M9 — Guerre entre joueurs
- [ ] **M9.1** Hostilité par propriétaire. En classique, elle reproduit exactement la matrice actuelle.
- [ ] **M9.2** Légions chez l'adversaire, ordre « attaquer », portes et murs qui ne laissent passer que leur
  propriétaire.
- [ ] **M9.3** Moral et totaux par camp, paix, arcs de triomphe.
- [ ] **M9.4** Tests : scénarios de combat rejoués, captures.

## M10 — Finitions
- [ ] Équilibrage par parties simulées sans tête : autorisations, prix et rareté des **armes**, quotas, fonds de
  départ, rythme. Performance, paquets d'installation (app macOS, exécutable Windows),
  documentation pour les joueurs.
