# Journal de bord

> Une entrée par session, **la plus récente en haut**. Format : fait / appris / prochaine étape / points ouverts.
> C'est la mémoire du projet d'une session à l'autre : rester factuel et concis.

---

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
- D-006 : des territoires fixes par joueur plutôt que la construction libre partout.
- D-012 : commerce entre joueurs par caravanes physiques.
- D-017 : suppression du tribut, du salaire et du prêt de secours.
- H12 : pas de pause quand on ouvre une fenêtre en multijoueur, pas d'annulation.
- M5.6 : l'intégration continue multiplateforme demandera de publier le fork sur GitHub.
