# Tester le projet (sans humain)

> Comment Claude vérifie lui-même que tout marche. Toute nouvelle fonctionnalité arrive avec au
> moins un test de cette page ; une tâche de la roadmap n'est « faite » que si ses tests passent.

## 1. Vérification standard : `tools/check.sh`

```sh
tools/check.sh          # build + tous les tests ctest ; doit être vert avant CHAQUE commit
```

- Compile le jeu (`build/julius.app`) et les outils de test.
- Lance `ctest`, qui exécute en particulier les **tests de parité** (§2).
- Refuse de tourner si le disque a moins de 300 Mo libres.

## 2. Tests de parité avec le Caesar III original (existants, hérités de Julius)

`test/CMakeLists.txt` → exécutable `autopilot` (sans graphismes, sans données du jeu, grâce aux
stubs de `test/stub/`). Chaque test charge une sauvegarde de `test/data/`, simule N ticks et compare
le résultat **octet par octet** (via `test/sav/sav_compare.c`) à une sauvegarde produite par le jeu
original au même moment.

Ces 36 scénarios couvrent invasions, légions de César, batailles lointaines, séismes, colères des
dieux, demandes impériales, grandes cités, indigènes, commerce… C'est **la preuve que le gameplay
intérieur reste celui de l'original** (exigence E7). Règles :

- Ils tournent en **mode classique** (un joueur, César actif, format de sauvegarde d'origine). Tout
  code multijoueur doit être inactif dans ce mode.
- Un test rouge = régression à corriger, **jamais** un test à modifier ou à désactiver.
- Durée : ~1,3 s. On les lance tout le temps.

## 2 bis. Simulation sans tête : `simtool` (sommes de contrôle)

`build/test/simtool` contient la même simulation que l'autopilot, sans graphismes, et sait calculer la **somme de
contrôle** de l'état (`src/mp/checksum.c`, DESIGN §3.8). On le lance depuis `build/test`, où sont copiées les
sauvegardes de `test/data` :

```sh
cd build/test
./simtool checksum tower.sav                 # somme de contrôle de la sauvegarde chargée
./simtool trace tower.sav 2000 50            # "tick somme" tous les 50 ticks
./simtool pieces tower.sav 100               # somme par partie de la sauvegarde après 100 ticks
./simtool idempotence tower.sav 1500 25      # charge et joue deux fois : échoue si les traces diffèrent
./simtool diffpieces tower.sav 1500 10       # joue 1500 ticks, recharge, joue 10 ticks, liste les parties
                                             # qui diffèrent et écrit diffpieces-{first,second}.sav
./compare diffpieces-first.sav diffpieces-second.sav   # puis le détail champ par champ
```

Méthode pour trouver un état caché : `idempotence` donne le premier tick divergent, `diffpieces` la partie de la
sauvegarde concernée, et `compare` l'enregistrement et l'octet. On remonte ensuite au code qui écrit ce champ.

`run SAVE TICKS SORTIE` écrit l'état après N ticks dans une sauvegarde, à comparer avec `compare`.
`--mp`, `--difficulty N` et `--gods 0|1`, placés avant la commande, choisissent les règles et les réglages locaux.

**Vérification croisée avec le vrai jeu** : `tools/cross-check.sh tower.sav 3000 100` fait tourner la même sauvegarde
dans le vrai jeu (vraies données) et dans `simtool` (bouchons), puis compare les sommes de contrôle tous les 100
ticks. Constaté en M1.6 : résultats identiques sur 3 000 ticks pour `tower` et `valentia57`. Seul écart connu : quand
une cité atteint la victoire classique, le bouchon d'interface « continue de gouverner » tout seul, alors que le
vrai jeu attend la réponse du joueur (`massilia`, `lugdunum`). Cet écart disparaîtra avec M2.11.

Tests ctest associés :
- `sim_trace_deterministic` : deux processus produisent la même trace ;
- `sim_idempotence_*` : 17 sauvegardes rechargées dans le même processus se comportent pareil.

Toute la suite de tests tourne en environ 3 s.

### Commandes multijoueur de `simtool`

`./simtool` sans argument liste tout. Les tests `mp_*` de ctest les appellent ; à la main, les plus utiles :

```
./simtool preparedmap SAVE 2 4000           # la carte préparée pour 2 (ou 4) : terre, mer, pont, matériaux, cités
MAPGEN_PICTURE=carte.ppm ./simtool preparedmap SAVE 2 10   # ... et son image (sips -s format png pour la lire)
./simtool terrain SAVE 2 60 40 30 20 [raw]  # une lettre par case d'une zone de la carte (W eau, R route, B pont...)
./simtool inspect PARTIE.mpsav              # joueurs, règles, climat, commerce, missionnaires d'une sauvegarde
./simtool caravans SAVE                     # caravanes entre joueurs ; tradepreference, tradeconservation,
                                            # traderesume : l'empire en repli, conservation, reprise
./simtool caesarstate SAVE                  # lauriers et colère de César : somme de contrôle, sauvegarde, reprise
./simtool caesarlaurels SAVE                # notes et lauriers mensuels, rangs, lettre, victoire au score
```

Pour voir César dans une vraie cité (lettre d'un nouveau rang, conseiller impérial) :
`tools/run-automation.sh test/automation/caesar-advisor.txt 180`.

## 3. Pilotage du vrai jeu : `--automation` (captures d'écran)

Le jeu tourne **sans fenêtre** (pilotes SDL `dummy`) en suivant un script, avec une horloge virtuelle
(16 ms par frame), donc de façon reproductible. Claude regarde les PNG produits avec l'outil `Read`.

```sh
tools/run-automation.sh test/automation/smoke.txt [TIMEOUT_S]
# → build/automation/*.png ; code de sortie 0 si OK, 3 si une commande a échoué, 142 si timeout
```

Données du jeu : `C3_DATA_DIR` (par défaut `../donnees-c3`). Les chemins du script sont relatifs au
répertoire courant (lancer depuis la racine du dépôt).

### Syntaxe des scripts (une commande par ligne, `#` = commentaire)

| Commande | Effet |
|----------|-------|
| `wait N` | laisse passer N frames |
| `mouse X Y` | déplace la souris (coordonnées écran, fenêtre 800×600) |
| `click X Y` / `rclick X Y` | clic gauche / droit (appui puis relâchement sur 2 frames) |
| `drag X1 Y1 X2 Y2` | glisser (ex. tracer une route) |
| `key NOM` | touche SDL, avec modificateurs : `key Escape`, `key ctrl+S`, `key F1` |
| `text TEXTE` | saisie de texte |
| `load FICHIER.sav` | charge une sauvegarde et affiche la ville |
| `save FICHIER.sav` | écrit une sauvegarde |
| `ticks N` | avance la simulation de N ticks d'un coup, en appelant directement `game_tick_run` (ignore les pauses dues à l'interface) |
| `run N` | laisse la boucle normale du jeu avancer de N ticks, pauses d'interface comprises ; échoue si le jeu reste bloqué |
| `speed N` | vitesse de jeu (10 à 500) |
| `screenshot F.png` | capture de l'écran courant |
| `cityshot F.png` | capture de toute la ville (fichier lourd : ~10 Mo pour 162×162) |
| `checksum` | écrit la somme de contrôle de l'état dans le journal |
| `pieces` | écrit la somme de contrôle de chaque partie de la sauvegarde |
| `pause` / `unpause` | met en pause la boucle de jeu : seul `ticks` fait alors avancer la simulation, au tick près |
| `rules mp` / `rules classic` | règles multijoueur par défaut, ou réglages locaux |
| `gotocity P` | place la vue sur la cité du joueur P (cités recopiées) |
| `goto X Y` | place la vue sur la case (X, Y) de la carte |
| `build TYPE X1 Y1 X2 Y2` | le joueur local construit (numéro de `building_type`), par une commande comme à la souris |
| `mpinfo` | écrit dans le journal la position et la destination du missionnaire, le nombre de bâtiments et la taille de la zone du joueur local |
| `mpplayers N` | partie en réseau, hôte : attend que N joueurs (hôte compris) soient connectés |
| `mpwait N` | partie en réseau : attend qu'elle ait démarré et tourné N ticks ; échoue en cas de désynchronisation ou de déconnexion |
| `mpcheck` | partie en réseau : écrit l'état et le nombre de tours vérifiés ; échoue si la partie ne tourne plus |
| `log TEXTE` | écrit un repère dans le journal |
| `quit` | quitte ; c'est aussi automatique en fin de script |

Les réglages (`c3.inf`, `julius.ini`) ne sont **pas** sauvegardés à la sortie d'un run automatisé.

Bon à savoir :
- La souris démarre au centre de l'écran. En (0,0), la vue défilerait toute seule par les bords, ce qui gèle aussi la simulation.
- Un événement injecté n'est traité qu'à la frame suivante. Un clic s'étale donc sur 2 à 3 frames.
- Deux clics au même endroit à moins de 300 ms, soit environ 18 frames virtuelles, comptent comme un double-clic. Mettre un `wait 20` entre deux clics.
- La simulation n'avance que dans la vue de la ville : elle est figée pendant un glisser de construction ou quand une fenêtre est ouverte.
- Pour voir si quelque chose bouge, comparer deux captures plutôt que de les regarder une par une. Par exemple, une caméra stable donne environ 2 % de pixels différents, dus aux animations.

### Garde-fous

- Ne **jamais** lancer `julius` sans `SDL_VIDEODRIVER=dummy` : une vraie fenêtre s'ouvrirait chez
  Alexandre. `--help` ne quitte pas sur macOS : il lance le jeu.
- Pas d'enregistrement d'images frame par frame (`SDL_VIDEO_DUMMY_SAVE_FRAMES`) : ~2 Mo par frame,
  et le disque est presque plein.
- Les PNG vont dans `build/automation/` (ignoré par git). Faire le ménage après usage.

### Scripts prêts

- `tools/mp-solo-test.sh` : partie seule depuis le salon sur la carte préparée (mission et zone de départ, maison
  hors zone refusée, missionnaire déplacé à la souris, clic sur le pont de César).
- `tools/mp-lobby-test.sh` : hôte et client depuis le salon, 400 ticks sans désynchronisation.
- `tools/mp-trade-test.sh` : idem, puis l'hôte ouvre le conseiller au commerce, page commune à l'empire et aux
  joueurs (`build/automation/trade-window.png`) ; le client achète le marbre (`trade-client-buys.png`), l'hôte en
  monte le prix et le client voit l'alerte plein écran (`price-alert.png`, puis `price-alert-trade.png` après « Voir
  le commerce »).
- `test/automation/display.txt` : les options d'affichage (plus grande fenêtre qui tient sur l'écran).

Sur macOS, le vrai jeu écrit aussi `julius-log.txt` dans le dossier des données (pas pendant l'automatisation) :
c'est là qu'on lit ce qui s'est passé chez Alexandre sans ouvrir de fenêtre.

## 3 bis. Parties en réseau

- **Sans tête, dans ctest** : `test/sim/lan_test.sh` lance un hôte et des clients `simtool mpnode` sur la machine,
  qui envoient des commandes scriptées (routes, maisons, impôts). Tests `mp_lan_2_players`, `mp_lan_4_players`
  (sommes de contrôle finales identiques et tous les tours vérifiés) et `mp_lan_desync_detected` (un client modifie
  son état en local : tous détectent la désynchronisation au même tour).
- **Vrai jeu** : `tools/mp-real-test.sh` lance deux instances du jeu sans fenêtre, hôte et client, pilotées par
  `test/automation/mp-host.txt` et `mp-client.txt`. Elles construisent à la souris et ouvrent des conseillers
  pendant que le jeu tourne. Constaté : 150 tours vérifiés, sans désynchronisation.
- **À la main, pour Alexandre** : `tools/play-mp.sh [JOUEURS] [SAUVEGARDE]` ouvre une vraie fenêtre par joueur sur le
  Mac. Ne jamais le lancer soi-même.
- En cas de désynchronisation, chaque machine écrit `mp-desync-pN-turnT.sav` dans le dossier des données : à comparer
  avec `build/test/compare`.

## 4. Tests à construire (voir ROADMAP)

| Test | Ce qu'il prouve | Jalon |
|------|-----------------|-------|
| **Trace de sommes de contrôle** (`simtool`) : hachage stable de tout l'état de simulation, à chaque tick | base de la détection des désynchronisations ; trouve le premier tick fautif | M1.1–M1.2 |
| **Idempotence** : une même sauvegarde chargée deux fois dans un processus donne la même trace | plus d'état caché qui fuit d'une partie à l'autre | M1.3 |
| **Rejeu** : sauvegarde + journal de commandes → même trace à chaque exécution | déterminisme de la couche de commandes | M2.7 |
| **Équivalence interface / rejeu** : une partie jouée par script d'automatisation, puis rejouée sans tête, donne la même somme de contrôle | les commandes capturent fidèlement les actions du joueur | M2.7 |
| **Invariance par translation** : une sauvegarde classique recopiée dans une grille plus grande, avec un décalage, évolue exactement pareil (comparaison après extraction) | grandes cartes sans changement de gameplay | M3.3 |
| **Isolement « jumeaux »** : la même sauvegarde classique placée deux fois sur une même carte (cités 0 et 1) redonne, pour chaque cité, la référence du jeu original | E7 pour le moteur multi-cités : les cités ne s'influencent pas | M3.7 |
| **Reprise exacte** : continuer une partie = la sauvegarder (`.mpsav`) puis la recharger | état caché complet dans la sauvegarde multijoueur | M3.8 |
| **Lockstep multi-processus** : 2 à 4 instances sans tête reliées en local, commandes scriptées, traces identiques | réseau et synchronisation | M5.3 |
| **Multiplateforme** : mêmes rejeux sur macOS arm64, Windows x64 et Linux (CI), mêmes traces | parties mixtes Mac / PC | M5.6 |
| **Scénarios visuels** (`test/automation/*.txt`) : captures aux étapes clés, relues par Claude | rendu, interface, propriété des bâtiments | chaque jalon UI |
