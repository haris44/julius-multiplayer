# Journal de bord

> Une entrée par session, **la plus récente en haut**. Format : fait / appris / prochaine étape / points ouverts.
> C'est la mémoire du projet d'une session à l'autre : rester factuel et concis.

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
