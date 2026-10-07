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
./simtool inlandwater SAVE 4                # l'aqueduc de César arrose le joueur des terres, J1 à J4 (placement tiré au sort)
./simtool longroutes SAVE                   # carte à 4 : chemins les plus longs, caravane entre les joueurs les plus éloignés, mer
./simtool menuowner SAVE                    # le menu de construction du joueur local ignore ce que font les autres
./simtool restrictiveness SAVE blank [CARTE.map ...]  # la même petite cité jouée 2 ans en classique et en multijoueur (seule, à 4), facile et
                                            # difficile, avec réservoirs et fontaines : tableau des chiffres, échoue si une règle multijoueur les change (§2 ter) ;
                                            # blank : carte libre classique faite par le test ; CARTE.map : une carte du jeu, à la main
                                            # (donnees-c3, jamais dans le dépôt) ; RESTRICT_TRACE=1 : population, moral et chômage chaque mois
./simtool tradegame SAVE 3 [TIRAGE]         # partie lancée comme depuis le salon : chacun vend à chacun, une caravane
                                            # par mois et par ressource achetée (T5.7) ; tradewarehouse : un premier
                                            # entrepôt à part ne bloque rien ; tradestatus : pourquoi rien ne part
./simtool tradesave SAVE SORTIE.mpsav       # écrit la partie de tools/mp-trade3-test.sh (3 joueurs, un entrepôt
                                            # chacun, marbre et fer à vendre chez le joueur 2)
./simtool caravans SAVE                     # caravanes entre joueurs ; tradeconservation, traderesume :
                                            # conservation, reprise
./simtool importprice SAVE                  # prix de Rome et portorium (D-060) ; empiresells : l'empire vend
                                            # toujours ; tradeisolation : commerce avec l'empire propre à chacun
./simtool caesarstate SAVE                  # lauriers et colère de César : somme de contrôle, sauvegarde, reprise
./simtool caesarlaurels SAVE                # notes et lauriers mensuels, rangs, lettre, victoire au score
./simtool caesarhistory SAVE                # historique mensuel des lauriers : gain, tendance, estime, sauvegarde
./simtool war SAVE                          # guerre entre joueurs (T5.5, D-077) : déclaration, préavis, paix, sauvegarde ;
                                            # légion chez l'adversaire, bâtiments abattus, combat, caravane prise
```

Pour voir César dans une vraie cité (lettre d'un nouveau rang, conseiller impérial) :
`tools/run-automation.sh test/automation/caesar-advisor.txt 180`. Le pilier des lauriers de l'évaluation de la cité :
`tools/serial.sh tools/run-automation.sh test/automation/caesar-ratings.txt 180`.

## 2 ter. Mesure de la restriction : `simtool restrictiveness` (D-072)

Elle répond à « le multijoueur est-il plus restrictif que le classique ? » (T4.16). Décision et conclusion : D-072.

**Le plan**, bâti par commandes sur le même terrain, puis joué deux ans (19 200 ticks) :
- 63 bâtiments (1 600 à 1 700 Dn) : une boucle de routes, 44 cases de maisons en deux bandes, 5 puits, grenier,
  marché, temple de Cérès, préfecture, bureau d'ingénieur, 4 ateliers ;
- l'eau comme un joueur l'obtient : un réservoir à la source (au bout de l'aqueduc de César dans les terres, contre la
  mer sur la côte), un aqueduc jusqu'à un second réservoir, trois fontaines. C'est la portée des fontaines et des
  réservoirs du multijoueur (`src/map/water_supply.c`) qui est mesurée ;
- au mois 2, quand la zone a grandi : 2 fermes de blé et 7 à 9 ateliers ou hôpitaux, pour avoir plus d'emplois que de
  bras ;
- pour que le hasard ne noie pas la mesure : grenier rempli chaque mois, ni incendie ni effondrement, ni dieux, ni
  armées, ni demandes de César, 100 000 Dn ; la mission de départ retirée des cités classiques (20 employés) ; la
  faveur mise à celle de la difficulté.

**Les séries**, en facile et en difficile :
1. classique, une cité seule sur la carte préparée. La seule vraie partie classique sur cette carte ; son eau vient
   d'un étang à côté du même réservoir (l'aqueduc de César n'existe qu'en multijoueur) ;
2. multijoueur, une cité seule (la partie d'Alexandre) ;
3. classique et multijoueur à 4 joueurs. Attention : à plusieurs cités, la série « classique » passe déjà par le code
   de la carte multijoueur (`game_rules_multiplayer_map()`) ; elle ne diffère que par la zone ;
4. multijoueur à 4 où un seul joueur bâtit ;
5. le même plan sur du terrain libre d'une vraie carte classique, règles et scénario d'origine :
   `simtool restrictiveness SAVE blank [CARTE.map ...]`. `blank` est une carte libre faite par le test (80 × 80, un
   lac), dans ctest. Les cartes du jeu (`Lugdunum`, `Londinium`, `Cyrene`, `Valentia`, `Lindum` de `donnees-c3`) se
   lancent à la main, jamais copiées dans le dépôt (I4). Corinthus, Toletum et Tarraco n'ont pas de bloc dégagé
   accessible près de l'entrée. `RESTRICT_TRACE=1` affiche population, moral et chômage chaque mois.

**Résultats sur la carte préparée** (niveau : 0 petite tente, 1 grande tente, 2 petite cabane ; « manque » : emplois
que personne ne peut occuper ; seule la cité J3 diffère entre classique et multijoueur) :

| Cité | Pop. 6 / 12 / 24 mois | Maisons | Niveau | Fontaine / nourr. | Employés | Manque | Moral | Migration |
|------|----------------------|---------|--------|-------------------|----------|--------|-------|-----------|
| Seule, facile, classique = multijoueur | 274 / 426 / 434 | 14 | 2,2 | 13 / 13 | 189 | 7 | 96 | 100 % |
| Seule, difficile, classique = multijoueur | 246 / 424 / 434 | 14 | 2,2 | 13 / 13 | 190 | 6 | 95 | 100 % |
| À 4, facile, J1 | 236 / 334 / 334 | 20 | 0,9 | 17 / 6 | 142 | 44 | 100 | 100 % |
| À 4, facile, J2 | 207 / 316 / 334 | 20 | 0,9 | 17 / 4 | 145 | 1 | 82 | 100 % |
| À 4, facile, J3 (classique ; MP) | 207 / 353 / 380 | 17 | 1,5 | 16 / 9 | 162 | 44 ; 34 | 78 | 100 % |
| À 4, facile, J4 | 179 / 338 / 372 | 17 | 1,2 | 16 / 6 | 156 | 40 | 78 | 100 % |
| À 4, difficile, J1 à J4 | 222 / 333 / 334 ; 191 / 310 / 334 ; 199 / 347 / 380 ; 173 / 329 / 372 | 17 à 20 | 0,9 à 1,5 | 16–17 / 4–9 | 142 à 162 | 44 ; 1 ; 44 (MP 35) ; 39 | 86 ; 62 ; 64 ; 60 | 100 ; 75 ; 75 ; 50 % |

**Résultats sur les cartes classiques** (règles d'origine, une cité seule ; facile ; difficile) :

| Carte | Pop. 6 / 12 / 24 mois | Niveau | Fontaine | Employés | Manque | Remarque |
|-------|----------------------|--------|----------|----------|--------|----------|
| Carte libre du test (`blank`) | 306 / 306 / 306 ; 306 / 308 / 308 | 0,6 / 0,7 | 18 | 133 / 141 | 83 / 45 | lac, terrain plat |
| Lugdunum | 342 / 281 / 246 ; 328 / 322 / 236 | 1,2 / 1,0 | 14 | 76 | 0 (39 sans emploi) | bloc au bord de la carte : pas de place pour les ateliers, des chômeurs partent |
| Londinium | 286 / 318 / 333 ; 253 / 318 / 333 | 1,0 | 17 | 142 | 44 / 46 | étang |
| Cyrene | 327 / 340 / 340 ; 315 / 382 / 382 | 1,0 / 1,5 | 14 / 11 | 144 / 163 | 52 / 33 | |
| Valentia | 302 / 406 / 406 ; 267 / 399 / 406 | 1,4 | 16 | 172 | 14 | étang |
| Lindum | 198 / 255 / 266 ; 196 / 253 / 266 | 0,3 | 19 | 120 | 96 / 95 | forêt, désirabilité −11,6 : maisons restées en tentes |

**Ce qui explique les écarts** : la difficulté (fonds 6 000 Dn contre 12 000, moral de base 50 contre 70) ; la zone
(une version précédente bâtissait les fermes au jour 0, avant que la zone grandisse : J3 avait 278 habitants à 12 mois
en multijoueur, 304 en classique) ; le
terrain (désirabilité moyenne des maisons −0,8 seule, −4,5 à −7,8 à 4, −3,5 à −8,6 sur les cartes d'origine ; les
arbres ne comptent pas dans la désirabilité, ils obligent seulement à défricher). Aucune maison ne dépasse la petite
cabane : le plan n'a ni école, ni bains, ni seconde nourriture.

**Les gardes** du test `mp_restrictiveness` :
- classique contre multijoueur, même carte : population, maisons, niveau, couverture, employés, manque de bras,
  désirabilité, immigration et moral à une tolérance de 8 à 25 % près selon la mesure ; plan bâti en entier, eau
  dans les deux réservoirs, plus d'emplois que de bras. Vérifiée par deux fautes volontaires, retirées ensuite :
  portée des fontaines et réservoirs multijoueur supprimée (434 habitants contre 358, vu) ; 40 % de bras en moins en
  multijoueur seul (vu) ;
- sur `blank` : le plan tient en entier et son eau coule ; aucune cité multijoueur n'a moins de 85 % de la
  population et des employés, ou 20 points de moins de maisons servies par l'eau, que la plus faible cité classique.
  Pas de faute volontaire pour cette seconde garde.

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
| `mpwaitdesync` | partie en réseau : attend qu'elle s'arrête sur une désynchronisation (T5.3) |
| `windowsweep` | ouvre et dessine, sans faire avancer la partie, chaque conseiller, onglet du commerce, fenêtre de dialogue, menu de construction, calque et fiche de bâtiment, de figure et de terrain ; échoue si l'une change l'état simulé (T5.3) |
| `angrygod` | tests : un dieu en colère dans la cité locale, autre que le « moins content » qu'elle garde (change l'état de ce seul ordinateur : provoque une désynchronisation à plusieurs) |
| `lobbylists NOM` / `lobbyselected NOM` | salon : échoue s'il ne liste pas le fichier NOM / si NOM n'est pas la partie choisie (T5.4) |
| `tradecheck` | la page du commerce a redessiné son cadre pour l'état courant de la route (T5.6) ; échoue sinon |
| `log TEXTE` | écrit un repère dans le journal |
| `quit` | quitte ; c'est aussi automatique en fin de script |

Les réglages (`c3.inf`, `julius.ini`) ne sont **pas** sauvegardés à la sortie d'un run automatisé.

Une partie en réseau se sauvegarde chaque mois dans le dossier des données (`autosave.mpsav`, D-074) : les scripts
`tools/mp-*-test.sh` mettent de côté celle d'Alexandre avant de jouer et la remettent après
(`tools/mp-autosave-guard.sh`). `text` donne le texte directement au jeu : un événement de texte poussé dans SDL fait
planter SDL 2 sur SDL 3 (`sdl2-compat`). Le champ de nom de fichier refuse le point.

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
  le commerce »). La commande `tradecheck` après la proposition de la route échoue si la page ne redessine pas son
  cadre quand l'état de la route change (vérifié : sans le correctif, elle échoue) ; `trade-route-waiting.png` est la
  capture à regarder pour ce défaut (bouton « Proposer la route » sans « Retirer la proposition » dessous, une seule
  ligne d'aide).
- `tools/mp-trade4-test.sh` : partie à quatre (un hôte et trois clients) ; l'hôte ouvre le commerce (cinq onglets : Empire,
  trois joueurs, Stocks), monte un prix à quatre chiffres et prend les captures `trade4-*.png` : aucun texte ne doit
  en recouvrir un autre (T5.6). La langue est celle des données du jeu ; pour l'anglais, forcer temporairement
  `translation_load(LANGUAGE_ENGLISH)` dans `src/game/game.c`, sans commiter.
- `tools/mp-trade3-test.sh` : trois vrais jeux depuis le salon (hôte et deux clients). L'hôte charge, depuis le menu
  principal, la partie écrite par `simtool tradesave` (carte 1 pour 3, un entrepôt par joueur au bout d'une route
  reliée à la grande route, 16 marbres et 16 fers chez le joueur 2), qui ouvre le salon ; il l'héberge. Sur la page
  du commerce, chacun propose la route aux deux autres et achète du marbre au premier, du fer au second, par clics ;
  la page reste ouverte pendant que la partie tourne (un clic droit dans la ville ouvrirait la fiche du terrain).
  Vérifie dans le journal des trois machines, sans désynchronisation : les six achats examinés, des caravanes du
  joueur 2 parties, le marbre livré au joueur 1 et le fer au joueur 3 (lignes « Trade between players: … loads
  delivered »), et les autres achats sans rien à vendre. Captures `trade3-*.png` : achats, marbre en route, livré,
  raisons dans la colonne « En route » (T5.7, T5.6).
- `tools/mp-ui-test.sh` : hôte en 640 × 480 et client en 1024 × 768 (l'écran factice le ramène à 1024 × 736),
  depuis le salon ; captures `build/automation/ui-*.png` des fenêtres revues par T4 (voir le tableau ci-dessous).
- `tools/mp-war-test.sh` : hôte en 640 × 480 et client en 1024 × 768 ; l'hôte déclare une guerre brutale depuis le
  conseiller militaire (bouton « Guerre », deux clics), le client la voit annoncée sur sa cité et propose la paix,
  l'hôte l'accepte ; captures `build/automation/war-*.png` (T5.5).
- `test/automation/display.txt` : les options d'affichage (plus grande fenêtre qui tient sur l'écran).

Taille de l'écran dans un script : `key F7` (640 × 480), `key F8` (800 × 600, la taille par défaut), `key F9`
(1024 × 768). Les fenêtres de 640 × 480 (salon, conseillers) sont centrées : à 1024 × 736, ajouter 192 et 128 à leurs
coordonnées ; la barre latérale reste collée au bord droit (bouton industrie : x = largeur − 32, y = 422).

**Qui couvre quoi** (tous lancés par `tools/serial.sh`, passe du 2026-10-07 : tous verts) :

| Script | Couvre |
|--------|--------|
| `tools/mp-solo-test.sh` | salon à 1 joueur (règles, carte), lettre de César, mission et zone, missionnaire, pont, bandeau des lauriers, conseiller impérial et page du commerce seule (onglet Stocks) en 800 × 600 |
| `tools/mp-lobby-test.sh` | salon hôte et client (partie trouvée, salle d'attente), partie à deux sans désynchronisation |
| `tools/mp-trade-test.sh` | page du commerce (onglet d'un joueur, route proposée, onglet Empire avec prix de Rome et portorium, onglet Stocks avec une limite), achat, alerte de prix |
| `tools/mp-trade4-test.sh` | page du commerce à quatre joueurs (cinq onglets) et prix à quatre chiffres, sans texte superposé |
| `tools/mp-trade3-test.sh` | partie à trois chargée depuis le menu principal (`simtool tradesave`) : routes et achats par clics entre les trois joueurs, caravanes du joueur 2 parties et arrivées chez les joueurs 1 et 3, raison affichée quand rien ne part (T5.7) |
| `tools/mp-real-test.sh` | partie en réseau sans salon (`--mp-host`), constructions et conseillers pendant le jeu |
| `tools/mp-sweep-test.sh` | partie en réseau seule (cité de `brugle-lugdunum.sav`, ou la carte préparée avec `MP_SWEEP_ARGS=--mp-generate`) : `windowsweep`, avant et après `angrygod` |
| `tools/mp-menus-test.sh` | trois joueurs (`--mp-host`, cités de `brugle-lugdunum.sav`) : `windowsweep` des trois en même temps, puis de chacun seul, partie qui continue sans désynchronisation |
| `tools/mp-resume-test.sh` | deux joueurs depuis le salon : « Sauvegarder » (un `.mpsav`), désynchronisation provoquée, « Fichier, Nouvelle partie », « Charger » qui ouvre le salon avec la partie choisie, `autosave.mpsav` listée, reprise hébergée et rejointe, sans désynchronisation |
| `tools/mp-ui-test.sh` | salon du client avec les règles de l'hôte (carte 1, difficulté changée), menus fermes et matières premières d'un joueur des terres et d'un joueur de la côte, menu Options sans Difficulté, conseiller impérial, évaluation (pilier des lauriers), commerce et Stocks, en 640 × 480 et 1024 × 736 |
| `tools/mp-war-test.sh` | conseiller militaire (bouton « Guerre »), fenêtre « Guerre et paix » en paix, confirmation, en guerre, offre de paix reçue, paix signée, annonces sur la cité, en 640 × 480 et 1024 × 736 |
| `caesar-advisor.txt`, `caesar-gifts.txt`, `caesar-ratings.txt` | César dans une grande cité (`rules mp`) : lettre de rang, conseiller impérial, cadeau, don, salaire, pilier des lauriers |
| `smoke.txt`, `run.txt`, `build-road.txt`, `display.txt` | classique : menu, chargement, boucle du jeu, route à la souris, options d'affichage |

Pour relire les textes anglais, forcer temporairement `LANGUAGE_ENGLISH` dans `translation_load`
(`src/translation/translation.c`), sans le committer : les textes du jeu d'origine restent en français, ceux du
multijoueur passent en anglais.

Sur macOS, le vrai jeu écrit aussi `julius-log.txt` dans le dossier des données (pas pendant l'automatisation) :
c'est là qu'on lit ce qui s'est passé chez Alexandre sans ouvrir de fenêtre.

## 3 bis. Parties en réseau

- **Sans tête, dans ctest** : `test/sim/lan_test.sh` lance un hôte et des clients `simtool mpnode` sur la machine,
  qui envoient des commandes scriptées (routes, maisons, impôts). Tests `mp_lan_2_players`, `mp_lan_4_players`
  (sommes de contrôle finales identiques et tous les tours vérifiés) et `mp_lan_desync_detected` (un client modifie
  son état en local : tous détectent la désynchronisation au même tour). `mp_lan_war` (T5.5, port 27550) : deux
  copies d'`inv0.sav`, le joueur 1 déclare une guerre brutale et envoie ses javeliniers chez le joueur 2, puis tous
  deux signent la paix ; mêmes pertes, même état de guerre et même somme de contrôle chez les deux.
- **Vrai jeu** : `tools/mp-real-test.sh` lance deux instances du jeu sans fenêtre, hôte et client, pilotées par
  `test/automation/mp-host.txt` et `mp-client.txt`. Elles construisent à la souris et ouvrent des conseillers
  pendant que le jeu tourne. Constaté : 150 tours vérifiés, sans désynchronisation.
- **À la main, pour Alexandre** : `tools/play-mp.sh [JOUEURS] [SAUVEGARDE]` ouvre une vraie fenêtre par joueur sur le
  Mac. Ne jamais le lancer soi-même.
- En cas de désynchronisation, chaque machine écrit `mp-desync-PORT-pN-turnT.mpsav` dans le dossier des données
  (son nom est dans le journal) : à comparer avec `simtool diffpieces` ou `build/test/compare`. La partie se reprend
  depuis le salon par `autosave.mpsav`, la sauvegarde du début du mois (D-074).
- **Sauvegarde et reprise** (T5.4, ctest) : `mp_lan_save_resume_3_players` et `mp_lan_save_resume_map2_2_players`
  (`lan_test.sh … saveresume`) : les joueurs commercent, sauvegardent comme le menu Fichier, l'hôte héberge sa
  sauvegarde comme le salon dans le même processus, les autres rejoignent, le dernier remplacé par un nouveau
  processus ; mêmes sommes de contrôle et même commerce pour tous.

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
