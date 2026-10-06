# Caesar III multijoueur (fork de Julius) — manuel de travail de Claude

Fork de Julius (réimplémentation fidèle de Caesar III, C99 + SDL2) que l'on transforme en **jeu libre multijoueur
sur réseau local, de 2 à 4 joueurs**, à la manière d'AoE2 : une grande carte partagée, une cité par joueur, du
commerce et de la guerre entre joueurs, sans César ni campagne. Le gameplay **intérieur** d'une cité reste
**exactement** celui de l'original. Alexandre m'a confié le développement en autonomie : je travaille de façon
structurée et je prouve chaque avancée par des tests.

## Au début de chaque session (obligatoire)
1. Lire `doc/mp/ROADMAP.md` (jalon en cours, première tâche non cochée) et l'entrée la plus récente de
   `doc/mp/JOURNAL.md`.
2. `git status`, `git log --oneline -5`, `df -h .` (le disque est presque plein), puis `tools/check.sh`, qui doit être
   vert avant de commencer.
3. Avant de toucher un module, relire sa fiche `doc/mp/code-map/` et la section concernée de `doc/mp/DESIGN.md`.

La commande `/suite` enchaîne tout cela, puis les tâches du jalon.

## Invariants (non négociables)
- **I1 Parité** : `tools/check.sh` est vert à **chaque** commit. Les tests de parité comparent la simulation au Caesar
  III original. Un test rouge est une régression à corriger, jamais un test à modifier.
- **I2 Mode classique intact** : toute nouveauté multijoueur est inactive en mode classique. César et la campagne sont
  **neutralisés par le mode**, jamais supprimés (D-002).
- **I3 Déterminisme** : dans la simulation, pas de flottant, d'horloge, de `rand()` ni d'état caché non sauvegardé.
  Toute action d'un joueur qui modifie la simulation passe par une commande (à partir de M2).
- **I4 Données du jeu** : jamais dans git ni publiées (`../donnees-c3`, Caesar III est toujours vendu).
- **I5 Disque** : vérifier `df -h .` avant toute écriture de plus de 100 Mo ; jamais d'images enregistrées frame par
  frame ; nettoyer `build/automation/`.
- **I6 Pas de fenêtre chez Alexandre** : ne lancer le jeu que par `tools/run-automation.sh` (pilotes SDL `dummy`).

## Commandes
| Besoin | Commande |
|--------|----------|
| Compiler et lancer tous les tests | `tools/check.sh` |
| Un test de parité précis | `cd build && ctest -R sav_caesar1 --output-on-failure` |
| Comparer deux sauvegardes | `build/test/compare attendu.sav obtenu.sav` |
| Lancer le vrai jeu sans fenêtre, piloté par un script | `tools/run-automation.sh test/automation/smoke.txt` (captures dans `build/automation/`, à regarder avec `Read`) |
| Partie en réseau du vrai jeu, sans fenêtre (hôte + client scriptés) | `tools/mp-real-test.sh` |
| Depuis le salon : partie seule, partie à deux, page du commerce et alerte de prix (sans fenêtre) | `tools/mp-solo-test.sh`, `tools/mp-lobby-test.sh`, `tools/mp-trade-test.sh` |
| Lire une sauvegarde multijoueur ou une zone de la carte préparée | `build/test/simtool inspect PARTIE.mpsav`, `build/test/simtool terrain SAVE 2 X Y W H` |
| Partie en réseau dans de vraies fenêtres, **pour Alexandre uniquement** | `tools/play-mp.sh [JOUEURS] [SAUVEGARDE]` |
| Vrai jeu contre simulation de test (mêmes sommes de contrôle ?) | `tools/cross-check.sh SAVE TICKS PAS` |
| Reconfigurer après un ajout de fichier dans CMakeLists | `cmake -S . -B build` |
| Envoyer sur GitHub (compile et teste sous Linux, Windows et macOS, D-054) | `git push github multiplayer` |
| Suivre la compilation, récupérer l'AppImage, le dossier Windows et le DMG | `gh run list -R haris44/julius-multiplayer`, `gh run download ID -R haris44/julius-multiplayer` |

Syntaxe des scripts d'automatisation et pièges : `doc/mp/TESTING.md` §3.

## Où trouver quoi
- `doc/mp/VISION.md` : exigences (E1 à E10) et hypothèses (H*). Ne changent que sur décision d'Alexandre.
- `doc/mp/DESIGN.md` : architecture (lockstep, contexte de cité, tranches d'ids, propriété, autorisations, grille variable,
  formats).
- `doc/mp/DECISIONS.md` : décisions numérotées (D-xxx) avec leur statut. Toute décision structurante y entre.
- `doc/mp/ROADMAP.md` : jalons M0 à M10 et tâches cochables avec critères de réussite.
- `doc/mp/TESTING.md` : comment tester (parité, automatisation, tests prévus).
- `doc/mp/JOURNAL.md` : une entrée par session, la plus récente en haut.
- `doc/mp/code-map/01..05` : cartographie détaillée du code de Julius (avec `fichier:ligne`). Ordre des tâches d'un
  tick, état caché, état de la cité, grilles, limites, César, inventaire des actions du joueur.

## Méthode de travail
- **Une tâche de la roadmap à la fois**, dans l'ordre, sauf décision notée dans le journal.
- **Le test d'abord** : chaque tâche commence par le test (ctest, rejeu, script d'automatisation) qui prouvera
  qu'elle est faite.
- **Petits commits** : message en anglais, préfixé par l'ID de tâche (`M1.2: add state checksum`), terminé par la
  ligne `Co-Authored-By` demandée. Cocher la tâche dans ROADMAP.md quand ses critères sont vérifiés.
- **Une découverte qui change la conception** va dans DECISIONS.md, puis dans DESIGN.md. Ne jamais improviser une
  architecture différente sans l'écrire.
- **Fin de session** : une entrée en tête de JOURNAL.md (fait, appris, prochaine étape, points ouverts), puis
  commit.
- **Explorations larges** : les confier à des sous-agents qui écrivent dans `doc/mp/code-map/`, pour garder mon
  contexte propre.
- **Décisions qui reviennent à Alexandre** (gameplay, hypothèses H*) : je tranche provisoirement, je note « à
  valider » dans DECISIONS.md et je le lui signale dans mon résumé. Je ne bloque pas le travail pour autant.

## Conventions de code
- Style Julius : C99, 4 espaces, accolade de fonction à la ligne et accolade de bloc sur la même ligne, fonctions
  préfixées par leur module (`city_finance_*`), `static` pour tout ce qui est privé. Commentaires en anglais.
- Nouveau code propre au multijoueur dans `src/mp/` ; ailleurs, des touches minimales conditionnées au mode, pour
  pouvoir récupérer les correctifs de Julius (`git fetch origin && git log upstream-base..origin/master`).
- Tout nouveau fichier `.c` doit être ajouté à `CMakeLists.txt` et, s'il fait partie de la simulation, aussi aux
  sources d'`autopilot` dans `test/CMakeLists.txt`.
- Vocabulaire : `player_id` / `owner` pour les joueurs. « city » et `city_id` désignent déjà les villes de l'empire.

## Pièges connus
- La simulation est figée hors de la vue de la ville, pendant un tracé de construction ou un défilement (classique).
- L'interface **écrit** dans l'état de simulation : aperçus, conseillers, rotation de la vue, animations
  (code-map/01 §3.5).
- L'état caché non sauvegardé (générateur aléatoire, direction du feu, compteurs d'images…) rend deux chargements
  successifs différents dans un même processus (code-map/01 §3.6).
- Les tests de parité dépendent de `build/test/c3.inf` (difficulté « difficile », dieux activés). Ne pas les lancer
  avec d'autres réglages.
- `julius --help` ne quitte pas sur macOS : il lance le jeu. Le pilote SDL `offscreen` ne marche pas (pas
  d'OpenGL), seul `dummy` fonctionne.
- `tee fichier | head` tronque le fichier : ne pas l'utiliser pour capturer une sortie complète.
- Après un `git stash` / `git stash pop` (par exemple pour vérifier qu'un test échoue sans le correctif), la
  compilation peut ne pas reprendre les fichiers restaurés : supprimer leurs `.o` (`find build -name "x.c.o"
  -delete`) avant de relancer les tests.
- Ne jamais enchaîner `tools/check.sh | tail -1 && git commit` : `tail` réussit même si les tests échouent. Écrire
  `tools/check.sh > build/check.log 2>&1 && tail -1 build/check.log && git commit ...`.
- `ctest` lance les tests en parallèle : un test qui écrit un fichier doit lui donner un nom propre à ses arguments.
- Un bâtiment construit par commande n'est « en service » qu'au tick suivant : ses stocks et sa zone ne comptent pas
  avant (faire avancer un tick dans les tests avant de compter).
- Pour vérifier qu'un test échoue sans le correctif, inverser la seule ligne fautive : `git stash` retire aussi le
  test.
- Sur macOS, le vrai jeu écrit `julius-log.txt` dans le dossier des données : lire ce fichier plutôt que de
  deviner ce qui s'est passé chez Alexandre (I6).
- Alexandre essaie chaque version **seul** (1 joueur dans le salon) : les règles de la carte doivent marcher à une
  cité (`game_rules_multiplayer_map()`), pas seulement à plusieurs.
- Les grandes cartes dépassent 16 bits : une place (`grid_offset`) ne tient jamais dans un `short`, et se sauvegarde
  par `save_write_offset` (large en multijoueur, D-024). Les bornes « 162 × 162 » ou 26244 de l'original sont à
  remplacer par `map_grid_is_valid_offset`.

## Communication
Avec Alexandre : en français, des résumés courts et concrets (fait, testé comment, suite). Les documents de
`doc/mp/` sont en français ; le code, les commentaires et les messages de commit sont en anglais.
