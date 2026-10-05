# Journal de bord

> Une entrée par session, **la plus récente en haut**. Format : fait / appris / prochaine étape / points ouverts.
> C'est la mémoire du projet d'une session à l'autre : rester factuel et concis.

---

## 2026-10-05 — Session 2 (suite) : retours du deuxième essai (T2)

**Retours d'Alexandre** : le défilement à la souris dépend de la résolution choisie et ne marche pas (MacBook en
1470 × 956 ou 1710 × 1112) ; le missionnaire ne bouge pas ; la vue devrait être sur lui et sa mission ; on
construit partout ; pas assez d'eau ; un sélecteur de carte alors qu'il veut une carte pour 1-2 et une pour 3-4
joueurs, en forêt ; pas de route commerciale sur l'eau.

**Fait** (commits ff51d664, 1b5079ef, 164 tests ctest)
- Sa sauvegarde (`simtool inspect`) : partie **seule** (1 joueur), modèle désert. Zones, brouillard et missionnaire
  exigeaient au moins 2 cités : `game_rules_multiplayer_map()` les applique aussi à une carte préparée jouée seul.
- Départ : vue sur le missionnaire, objectif « construisez votre première mission » jusqu'à la première mission,
  brouillard calculé dès la création de la carte.
- D-044 : plus de choix de carte (« Nouvelle partie (forêt) » ou une partie à reprendre), climat du nord, un lac
  et sa rivière pour chaque joueur, points de pêche, forêts et étangs loin des cités ; environ 8 % d'eau.
- T2.5 : tailles fixes retirées, ligne « (écran) » = plus grande fenêtre qui tient sur l'écran, fenêtres jamais
  plus grandes que l'écran, barre de menus et Dock masqués en plein écran sur macOS.
- Outils : `simtool inspect` (contenu d'un `.mpsav`), commande d'automatisation `mpinfo`,
  `tools/mp-solo-test.sh`, `test/automation/display.txt`. Images des cartes : `../CARTES_2/`.

**Appris**
- Les mouettes des points de pêche appartiennent à la première cité : elles éclairaient le brouillard chez les
  autres joueurs.
- Le `.mpsav` de départ est écrit avant que les règles de la partie soient posées : ses règles ne disent rien de la
  partie jouée.
- Les ponts franchissent jusqu'à 40 cases d'eau : des rivières de 7 cases restent franchissables.

**Prochaine étape** : retour d'Alexandre sur T2.5 et T1.3 (défilement en plein écran, impossible à vérifier sans
vraie fenêtre) et sur la carte ; puis préférence de l'acheteur pour le moins cher, MA.1, M9.

**Points ouverts** : ceux des entrées précédentes ; densité de la forêt (35 à 40 %, à défricher pour s'étendre
au-delà de 30 cases).

---

## 2026-10-05 — Session 2 (suite) : voies vers l'extérieur et commerce entre joueurs

**Demande d'Alexandre** : voies maritimes et terrestres vers l'extérieur ; prix fixés par le vendeur pour chaque
joueur acheteur, empire plus cher, message quand un prix change (réponses : D-043).

**Fait** (commits dcad747d à 1af2d285, 167 tests ctest)
- D-042 : rivière du lac de chaque joueur qui en a un jusqu'au bord (entrée de ses navires), rivière centrale pour
  les autres ; carte modèle refusée si son empire ne commerce pas par terre et par mer.
- M8.2 : achats à l'empire +50 % en multijoueur.
- M8.3 : prix par ressource et par acheteur, achats, message à l'acheteur.
- M8.4, M8.5 : routes ouvertes par les deux joueurs, caravane mensuelle par la route (8 chargements), paiement à
  la livraison.
- M8.6 : fenêtre « Commerce entre joueurs » (bouton « Joueurs » du conseiller au commerce).

**Appris**
- Les réglages « importer / exporter » du jeu dépendent de l'empire : le commerce entre joueurs a ses propres
  réglages.
- Le missionnaire de départ occupe une case : un test qui bâtit au centre d'une cité doit l'éviter.

**Prochaine étape** : préférence de l'acheteur pour le moins cher (joueurs avant l'empire), MA.1, puis M9 (guerre,
interception des caravanes).

**Points ouverts** : voir l'entrée précédente ; s'y ajoutent le pas de prix (10) et la taille des caravanes (8).

---

## 2026-10-05 — Session 2 (suite) : eau de César, territoires, missionnaire, brouillard de guerre

**Demande d'Alexandre** : lac central relié à l'extérieur et navigable ; le joueur des rochers a fer et marbre,
un autre bois et argile (D-041) ; « continue l'implémentation ».

**Fait** (commits f07256dc à fe492a7a, 164 tests ctest)
- MC.2 révisé : rivière qui serpente du lac central au coin nord-est, entrée des navires.
- MC.3 : tranche de bâtiments de César (ids 8001+, aucune cité ne la fait tourner) ; réservoir au bord du lac,
  aqueduc vers les joueurs sans eau ; chaque cité le remplit dans son calcul de l'eau.
- ME.1 : réservoirs à niveau (270 jours d'eau, remplis en 54), réserve affichée dans leur fenêtre.
- MT.1 à MT.5 : territoires (zone de 20 cases autour des bâtiments installés et des missions, premier arrivé),
  missions (gratuite sans terre, puis 4 marbres), missionnaire déplacé au clic, formé pour 300 Dn, bâtiments hors
  zone effondrés après 3 mois, frontières aux couleurs des joueurs, teintes plus légères.
- MB.1 : brouillard de guerre (option du salon), vue, minicarte et scores.
- Automatisation : `goto X Y`, `build TYPE X1 Y1 X2 Y2`. Bandeau multijoueur déplacé en bas de la vue.

**Appris**
- Tout ce qui s'écrit hors des pièces « larges » du `.mpsav` (ici la tranche de César) doit forcer le format
  large : sinon positions et ids sont tronqués à 16 bits, sans erreur. Le test doit passer par le fichier, comme une
  vraie partie.
- Les invasions prévues par la carte modèle détruisent des bâtiments dans les tests longs : les couper.
- `check.sh | tail && git commit` commite même si les tests échouent (corrigé dans CLAUDE.md).

**Prochaine étape** : MA.1 (variantes de couleur), dessin en blanc des ouvrages de César (MC.1), puis M8 (commerce).

**Points ouverts, à valider par Alexandre** : T1.3 (défilement en plein écran), relecture des cartes, coûts et
durées (4 marbres par mission, 300 Dn le missionnaire, 3 mois de grâce, 5 min / 1 min pour les réservoirs).

---

## 2026-10-05 — Session 2 : retours du premier test (TEST_1), cartes préparées

**Demande d'Alexandre** : TEST_1.md (bugs et évolutions), puis réponses à QUESTIONS_1.md. Décisions D-032 à D-040,
E11 remplacée (zone constructible qui suit la ville vivante), nouveaux jalons T1, MC, ME, MT, MB, MA avant M8.

**Fait** (commits e491670d à d752a320, 157 tests ctest)
- T1.1 : popups, fanfares et effets sonores de la cité d'un autre joueur ne sortent plus chez soi (2 popups et 3
  sons de la cité jumelle passaient avant).
- T1.2 : la carte générée était dans le coin de la grille de 512 ; caméra et minicarte supposent une carte centrée.
- T1.3 (à vérifier par Alexandre) : plein écran sans « Space » sur macOS, la barre de menu ne descend plus.
- T1.4 : 1280×720, 1920×1080, 2560×1440 dans les options d'affichage.
- T1.5, T1.6 : propriétaire César (`MAP_OWNER_CAESAR`) ; le salon ne propose plus de copies de carte classique.
- MC.2 : cartes préparées à 2 et à 4 (3 joueurs sur celle à 4), images dans `../CARTES_1/`.

**Appris**
- Un propriétaire qui n'est aucun joueur suffit à rendre une route indémolissable et non teintée : toute la logique
  « autre propriétaire » existait déjà.
- `git stash` puis recompilation : les `.o` des fichiers restaurés peuvent ne pas être refaits (noté dans CLAUDE.md).

**Prochaine étape** : MC.3 et ME.1 (aqueduc et réservoir de César, réservoirs à niveau), puis MT (territoires).

**Points ouverts, à valider par Alexandre** : T1.3 (défilement en plein écran), relecture des cartes et de la
répartition D-040 ; teinte blanche de César (MC.1) demande un mode de dessin qui éclaircit.

---

## 2026-10-04 — Session 1 (suite) : M5 à M7, arrivée à M8

**Fait** (commits d78e1d07 à 002b99e3, 146 tests ctest)
- M5.5 : pause décidée par l'hôte (toutes les machines s'arrêtent au même tick), vitesse de l'hôte, un joueur qui
  part n'arrête plus la partie, sauvegarde `.mpsav` en cours de partie et reprise depuis le salon.
- M5.8 : salon complet (difficulté, dieux, fin, invasions), lancement par l'hôte, refus d'un client dont la version
  ou les données du jeu diffèrent.
- M6 : grandes cartes générées (200 ou 260 cases, un point d'arrivée par joueur au bord, ressources autour de chaque
  cité), format `.mpmap`, option du salon ; 0,4 ms par tick pour 4 grandes cités. L'éditeur (M6.3) est reporté.
- M7 : « Multijoueur » en tête du menu, couleurs des joueurs (vue et minicarte), scores dans le bandeau, conseiller
  impérial masqué, avertissements des autres joueurs plus affichés chez soi.

**Appris**
- Répéter le test réel à deux instances a révélé trois désynchronisations dues à l'interface : sauvegarde
  d'annulation des aqueducs dans la somme de contrôle, propriété des cases modifiée par l'aperçu « dégager le
  terrain », écriture dans le bâtiment nul pour le son des jardins. Règle : la propriété ne change que pendant un
  tick ou une commande. `MP_TRACE_TURNS=début-fin` trace la somme de chaque pièce pour comparer deux journaux.
- Trois options de ligne de commande ajoutées n'étaient pas initialisées : le jeu démarrait parfois sur une carte
  générée. Toujours initialiser un nouveau champ d'arguments.

**Prochaine étape** : M8, commerce entre joueurs (D-019) : conception détaillée d'abord.

**Points ouverts, à valider par Alexandre** : D-020 (autorisations), D-025, D-026 (tribut, salaires de Rome),
D-029 (score), D-030 (cartes générées), D-031 (menu). M5.6 (intégration continue) demande un dépôt GitHub ; M5.7
est la première partie avec lui ; M7.5 (discussion) reste optionnelle.

---

## 2026-10-04 — Session 1 (suite) : jalon M4 complet, salon multijoueur

**Demande d'Alexandre** : « continuer jusqu'à M8 ». Ordre suivi : M4.1, M4.2, salon (M5.4, avancé à sa demande),
puis M4.3 à M4.7.

**Fait** (commits 5530a854 à 33ae6110, 140 tests ctest environ)
- M4.1 : plus de César en multijoueur (demandes, colère, invasions, batailles lointaines, salaire), D-026.
- M4.2 : fuites entre cités trouvées par les jumelles longues et corrigées : coin (0, 0) propre à chaque cité,
  recherches de cible des combats limitées à la cité (D-027). 13 sauvegardes exactes sur 12 000 ticks.
- M5.4 : entrée « Multijoueur » du menu, salon (cartes `.map` du jeu libre ou sauvegardes, nombre de joueurs,
  découverte des parties par UDP, adresse à la main), testé de bout en bout à la souris par `tools/mp-lobby-test.sh`.
- M4.3 : propriété (D-028) : les infrastructures sont revendiquées par la commande qui les construit ; une cité n'agit
  que sur ses propres bâtiments ; l'eau suit les aqueducs et les zones desservies de chaque cité. Tests « intrus »
  (sans filtres, des dizaines de milliers d'effets chez le voisin ; avec, aucun) et « voisins branchés ».
- M4.4 : la partie réseau ouvre la terre entre les cités : les joueurs peuvent s'y relier par des routes.
- M4.5 et M4.6 : règles de l'hôte transmises aux clients (protocole v3) ; invasions IA en option ; fin au score
  après 5, 10 ou 20 ans, écran de classement (D-029).
- M4.7 : autorisations d'exploiter réparties entre les cités ; le fer et les armes chez un seul joueur (D-020).

**Appris**
- Deux fois le même piège : l'état « extra » mesuré dans un tampon trop petit. La mesure signale maintenant un
  débordement dans le journal.
- Revendiquer une case « au passage » dans `map_terrain_add` était faux : des mises à jour générales de la carte
  tournent pendant le tour d'une autre cité. Seules les commandes revendiquent.
- Un commit (9a2dde6e) a été fait alors que `check.sh` échouait (contrôle de déterminisme) ; corrigé juste après.
  Toujours relire la dernière ligne de `check.sh` avant de commiter.

**Prochaine étape** : M5.5 (pause, vitesse, déconnexion, sauvegarde coordonnée), M5.8 (salon complet), puis M6
(cartes) et M7 (interface), avant M8 (commerce par routes).

**Points ouverts, à valider par Alexandre** : formule du score et durées (D-029), répartition des autorisations
(D-020), tribut et salaires de Rome gardés (D-026). M5.6 demande un dépôt GitHub ; M5.7 se joue avec lui.

---

## 2026-10-04 — Session 1 (suite) : une cité par joueur en réseau (M2P.6)

**Fait** : une partie réseau donne maintenant à chaque joueur sa propre cité (D-025), ce qui sépare les statistiques
(E13). La composition sait ajouter jusqu'à 3 copies (`mp_compose_separate_cities`), en renumérotant les réseaux
routiers de toutes les cités. L'hôte envoie un `.mpsav` (protocole v2, jusqu'à 32 Mo ; 9 Mo pour 4 cités). Tests :
reprise exacte à 3 et 4 cités, parties réseau sans tête à 2 et 4 cités séparées, désynchronisation détectée, vrai jeu
à deux instances. 120 tests ctest.

**Appris**
- Deux tests ctest qui écrivent le même fichier échouent seulement en parallèle : chaque fichier de test porte
  maintenant tout ce qui distingue le cas (sauvegarde, nombre de cités, port).
- L'hôte qui détecte une désynchronisation doit prévenir les clients **avant** d'écrire la sauvegarde de
  diagnostic, sinon ils voient seulement une connexion perdue.

**Prochaine étape** : faire tester Alexandre (`tools/play-mp.sh`), puis M4.1 (neutraliser César : les messages
« Rome augmente les salaires » arrivent encore) et M4.2 (point d'arrivée par joueur).

**Points ouverts** : la vue de départ est centrée sur la cité locale, mais les cités étant des copies, les
captures des deux joueurs se ressemblent ; la minicarte et l'interface ne distinguent pas encore les cités des
autres joueurs (M7).

---

## 2026-10-04 — Session 1 (suite) : sauvegarde multijoueur (M3.8)

**Fait** : M3.8. Une partie à plusieurs cités sur grande grille s'enregistre dans un `.mpsav` (format classique élargi,
D-024) et se recharge exactement : `simtool mpresume` vérifie, sur 13 sauvegardes, que l'état rechargé a la même somme
de contrôle et que la partie reprise reste identique tick par tick à la partie continuée. 115 tests ctest.

**Appris** (quatre pièges trouvés par le test de reprise)
- `city_view_init` remplissait une table écran ↔ case dimensionnée pour 162 cases : débordement sur une grille de
  512, qui écrasait les compteurs de bâtiments. La table est maintenant à la taille maximale.
- La taille de l'état « extra » (qui contient la grille des forces militaires) était mesurée une seule fois dans un
  tampon de 64 Ko : fausse dès que la grille grandit. Elle est mesurée à chaque appel.
- Le filtre des grilles partagées retenait tout nom contenant « grid », y compris `city_entry_exit_grid_offset`,
  propre à chaque cité : la deuxième cité rechargeait les points d'entrée de la première.
- Le cache des greniers (`non_getting_granaries`), recalculé tous les 50 ticks, n'était pas sauvegardé : une
  reprise entre deux recalculs divergeait. Il entre dans l'état « extra ».

**Prochaine étape** : partie réseau avec une cité par joueur (cités jumelles ou composées, l'hôte envoie le
`.mpsav`), pour que chaque joueur voie ses propres statistiques (E13).

**Points ouverts** : la somme de contrôle ignore les messages (interface) ; la zone `messages` contient aussi la file
des popups, à séparer par joueur avec l'interface (M7).

---

## 2026-10-04 — Session 1 (suite) : décisions d'Alexandre, commandes, premier multijoueur sur Mac

**Décisions d'Alexandre** : construction partout avec branchements (D-018, remplace les territoires), commerce sur
routes construites et interceptable (D-019), autorisations d'exploiter par point d'arrivée avec armes rares (D-020).
Priorité : pouvoir lancer le multijoueur sur son Mac au plus vite, d'où le nouveau jalon **M2P** (ville partagée).

**Fait** (commits 03e873f6 à aujourd'hui ; 66 tests ctest)
- M2.1 à M2.5 : toutes les actions du joueur passent par des commandes : construction, démolition (la réponse
  fort/pont voyage dans la commande), réglages de la cité, ordres militaires. Chaque commande refait exactement les
  appels de l'interface d'origine. Des tests d'équivalence le prouvent : 234 constructions, 204 confirmations et
  19 réglages identiques à l'ancien chemin direct.
- M2P :
  - réseau TCP (`platform/net`) ;
  - lockstep (`mp/lockstep`) : tours de 4 ticks, exécution au tour +2, somme de contrôle comparée à chaque tour,
    sauvegarde de diagnostic en cas de désynchronisation ;
  - cadence indépendante des fenêtres ;
  - aperçu de construction retiré pendant les ticks ;
  - rotation, annulation, triches et victoire classique désactivées en réseau ;
  - bandeau d'état ;
  - options `--mp-host`, `--mp-join` et `--mp-players`.
- Tests réseau : 2 et 4 joueurs sans tête, désynchronisation provoquée et détectée, vrai jeu à deux instances
  (150 tours vérifiés pendant que les joueurs construisent à la souris et ouvrent des conseillers).

**Appris**
- Les dates avant J.-C. donnent un compteur de ticks négatif : ne jamais utiliser -1 comme valeur « non initialisé »
  pour un tick. `game_time_absolute_tick()` est toujours positif.
- Le texte d'évolution des maisons (panneau d'info) et la note sélectionnée sont écrits par l'interface dans des
  données sauvegardées. Ils sont exclus de la somme de contrôle.

**Prochaine étape** : retour d'Alexandre sur `tools/play-mp.sh`. Ensuite :
- M2.6 : recalculs des conseillers, pour l'instant simplement sautés en réseau ;
- M2.7 : enregistrement et rejeu ;
- M2.9 à M2.11 ;
- puis M3, le moteur multi-cités.

**Limites connues du prototype**
- Les cadeaux, dons et salaire de César appellent encore directement la simulation. Ils seront neutralisés en M4.1 ;
  ne pas les utiliser en réseau d'ici là.
- Chaque joueur règle sa propre vitesse : la partie avance à la vitesse du plus lent, et la pause d'un joueur
  bloque tout le monde.
- Pas de sauvegarde ni de reprise d'une partie en réseau (M3.8). Pas de salon : on lance en ligne de commande.

## 2026-10-04 — Session 1 : mise en place (M0) puis déterminisme (M1)

**Fait**
- Analyse des fichiers d'origine. L'installeur PC (Inno Setup, repack Abandonware France) contient le jeu complet avec
  la traduction FrDeluxe. Le CD Mac (HFS) est inutile : le repack PC contient déjà toutes les musiques et tous les
  sons.
- Données assemblées dans `../donnees-c3` (535 Mo, sans `sgs/` ni le doublon `Soundfx/`). Julius détecte le français.
- Julius compilé sur macOS arm64 (dépendances Homebrew : cmake, sdl2, sdl2_mixer). Les 36 tests de parité passent.
  Branche `multiplayer` créée, tag `upstream-base` posé sur 34d1ecd5.
- `tools/check.sh` : compilation, tests de parité, garde-fou sur l'espace disque.
- Pilote d'automatisation `--automation` (commits 6566d085 et 30ca8060) : le vrai jeu tourne sans fenêtre (SDL
  `dummy`), avec entrées simulées, horloge virtuelle, `load`, `run N`, `ticks N` et captures PNG. Vérifié : menu
  principal en français, ville de Massilia, caméra stable.
- Cartographie complète du code par 5 sous-agents, dans `doc/mp/code-map/01` à `05`.
- Rédaction de VISION, DESIGN, DECISIONS (D-001 à D-017), ROADMAP (M0 à M10), TESTING, CLAUDE.md et de la commande
  `/suite`.

**Appris** (détails dans code-map)
- Le format de sauvegarde de Caesar III réserve **deux** enregistrements de cité (`other_player`), et les entités
  ont un champ `faction_id` jamais lu par la logique. Un multijoueur était probablement prévu à l'origine.
- La simulation est entièrement entière (pas de flottant ni d'horloge), avec **un seul générateur aléatoire** qui
  avance une fois par tick. Mais il existe de l'état caché non sauvegardé : 7 sauvegardes sur 9 divergent quand
  on les recharge dans le même processus. Le remettre à zéro corrige tout sans casser la parité.
- L'interface écrit dans la simulation : aperçus de construction, conseillers, rotation de la vue, animations. La
  simulation dépend aussi de réglages locaux (difficulté, dieux) qui ne sont pas dans la sauvegarde.
- Limites : 2 000 bâtiments, 1 000 figures, 50 formations (6 légions), grille de 162. Les coordonnées tiennent
  sur 8 bits et les offsets sur 16 bits, ce qui bloque au-delà de 181×181.
- Il n'existe pas de vrai mode « jeu libre », seulement l'option d'éditeur *open play*, avec laquelle César reste
  actif.

**Jalon M1 terminé** (commits 1811e6b6 à 531b2e2b ; 56 tests ctest en 3 s)
- `mp/checksum` : somme de contrôle de tout l'état de simulation, calculée sur la sérialisation de la sauvegarde
  (donc identique sur toutes les plateformes). Les parties écrites par l'interface en sont exclues.
- `simtool` : `checksum`, `trace`, `run`, `pieces`, `idempotence`, `diffpieces`, plus les options `--mp`,
  `--difficulty` et `--gods`.
- `game/extra_state` : l'état caché est remis à zéro **avant** chaque chargement ou démarrage de partie. Les
  17 sauvegardes de test sont maintenant identiques quand on les recharge dans le même processus. Avant :
  7 sur 17 divergeaient.
- `game/rules` : en multijoueur, la difficulté, les dieux et les correctifs viennent des règles de la partie, plus
  de `c3.inf`. Testé dans les deux sens.
- `tools/check-determinism.sh` : ni flottant, ni horloge, ni `rand()` dans la simulation.
- Automatisation : commandes `checksum`, `pieces`, `pause`, `rules`. `tools/cross-check.sh` vérifie que le vrai
  jeu et `simtool` donnent les mêmes sommes de contrôle : c'est le cas sur 3 000 ticks.

**Appris pendant M1**
- Il existait un autre état caché, non repéré par la cartographie : les compteurs de bâtiments ne sont sauvegardés
  que pour certains types. Les forums et le sénat repartaient avec les valeurs de la partie précédente, ce qui
  faussait la distribution du trésor. D'où la remise à zéro *avant* la lecture du fichier.
- Pour retrouver un champ divergent :
  1. `simtool idempotence` donne le tick ;
  2. `simtool diffpieces` donne la partie de la sauvegarde ;
  3. `compare` donne l'enregistrement et l'octet ;
  4. on calcule ensuite la position des champs en déroulant `save_main_data`.
- Le bouchon d'interface des tests « continue de gouverner » automatiquement à la victoire, alors que le vrai jeu
  attend le joueur. C'est la seule différence observée entre les bouchons et le vrai jeu (à traiter en M2.11).

**Prochaine étape** : M2.1, l'infrastructure de la couche de commandes. Ensuite la conversion des actions du joueur,
famille par famille (inventaire : code-map/05 §4).

**Points ouverts pour Alexandre** (décisions « à valider »)
- ~~D-006 territoires~~ : tranché par Alexandre → construction partout avec branchements (D-018). Reste à valider
  H11 : branchées, les cités ne se rendent pas service (services, main-d'œuvre, eau, pompiers).
- ~~D-012~~ : tranché → commerce sur routes construites, interceptable (D-019) ; autorisations d'exploiter par point
  d'arrivée, armes rares (D-020).
- D-017 : suppression du tribut, du salaire et du prêt de secours.
- H12 : pas de pause quand on ouvre une fenêtre en multijoueur, pas d'annulation.
- M5.6 : l'intégration continue multiplateforme demandera de publier le fork sur GitHub.
