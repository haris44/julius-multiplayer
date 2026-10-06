# Journal de bord

> Une entrée par session, **la plus récente en haut**. Format : fait / appris / prochaine étape / points ouverts.
> C'est la mémoire du projet d'une session à l'autre : rester factuel et concis.

---

## 2026-10-06 — Dépôt GitHub, versions Linux et Windows (M5.6, D-054)

**Fait**
- Code poussé sur `github.com/haris44/julius-multiplayer` (fork public de Julius, branche `multiplayer`, remote
  `github`). Avant l'envoi : aucune donnée du jeu ni secret, et l'adresse des 88 commits d'Alexandre remplacée par
  son adresse privée GitHub (son choix).
- `.github/workflows/multiplayer.yml` : tous les tests puis une AppImage sous Linux, tous les tests puis un dossier
  `.exe` + DLL sous Windows. Les compilations de Julius sont désactivées sur le fork.
- Premier essai Windows rouge : `ws2_32` manquait à l'édition de liens (jeu, autopilot, simtool). Corrigé ; les deux
  sont verts.
- LISEZMOI pour Mac, Linux et Windows (programme non signé, pare-feu, FUSE, Wayland).

**Appris**
- Les journaux d'une tâche se lisent par l'API avec le jeton de `gh` ; `gh run view --log-failed` attend la fin de
  toute la compilation.
- L'AppImage (Ubuntu 24.04, SDL 2.30) embarque libdecor et les bibliothèques Wayland : elle peut s'afficher en
  Wayland comme en X11. Elle utilise le runtime statique (pas besoin de libfuse2).
- GCC sous Windows signale environ 40 avertissements de format que clang ne voit pas : à nettoyer un jour.
- `mp_lan_pause` échouait parfois sous charge : le client levait la pause « une seconde » après l'avoir demandée,
  mesurée avec `time()` (secondes entières), donc parfois presque aussitôt, avant que les autres la voient.
  Il la lève maintenant deux secondes après l'avoir vue lui-même ; ce que le test vérifie ne change pas.
  Vérifié : 3 passages de 18 tests réseau en parallèle (-j16).

**Prochaine étape** : qu'Alexandre essaie l'AppImage sur Fedora 44 ; puis M9.2.

**Points ouverts**
- M5.6 : comparer les traces d'une même partie sur Mac, Linux et Windows.

---

## 2026-10-06 — Réponses d'Alexandre sur César (D-053), module `mp/caesar` (M9.1)

**Fait**
- Les sept points de CESAR.md §13 posés à Alexandre et reportés dans le plan (D-053) :
  - **victoire au score** : la première cité à 1 000 lauriers (au choix 500 à 2 000) gagne ;
  - guerre **honorable** (3 mois de préavis) ou **brutale** (immédiate, −15 lauriers et colère ×1,5) ;
  - **demandes de César à toute la province**, une cagnotte partagée selon les envois, frappes permises sur la
    ressource ;
  - le reste confirmé tel quel.
  T1.3 et T2.5 cochés (vérifiés par Alexandre).
- M9.1 : `mp/caesar` (lauriers par cité et par source, en dixièmes ; jauge de colère et part de chacun) et
  `mp/caesar_rules.h` (tous les réglages du plan). Pièce `mp_caesar` du `.mpsav`, protocole 10. Test
  `mp_caesar_state`.

**Appris**
- Une vérification tick par tick sur deux mois coûte environ 65 s (somme de contrôle complète à chaque tick) ;
  quatre jours suffisent quand l'état testé n'évolue pas seul (8 s).

**Prochaine étape** : M9.2, les cinq notes et `simtool notes SAVE`, calibrées sur les cités de `test/data`.

**Points ouverts**
- Interprétation à confirmer par Alexandre : pendant une demande de César, intercepter les caravanes de la
  ressource ne fâche pas César, mais l'interception demande toujours une guerre déclarée.

---

## 2026-10-06 — On va partout, le bois pour le joueur des terres, emplacements tirés au sort (T3.4 à T3.6, D-052)

**Fait**
- Moins de forêt (14 et 17,5 % au lieu de 34 et 37 %), toujours infranchissable ; `open_shut_in_clearings` comble
  les petites clairières enfermées et ouvre un passage vers les autres (marche 0-1 : terre libre 0, arbre 1).
- Bois ajouté à l'emplacement des terres (`MAX_SLOT_RESOURCES` 4).
- `mp_mapgen_create_prepared(fichier, joueurs, graine)` : l'hôte tire les emplacements avec le nombre du salon ;
  `slot_of(joueur)` passe par `slot_of_player`. Les fonctions qui recevaient un « slot » reçoivent un joueur.

**Appris**
- Alexandre veut garder les règles de terrain de l'original : une demande « aller partout » se règle dans la carte,
  pas dans le calcul des chemins. Premier essai (missionnaire à travers bois) annulé.
- Les cases d'eau marquées « pont » sur la carte des tests viennent de la sauvegarde modèle (`brugle-massilia`),
  pas de la génération : avec Lindum, seul le pont de César apparaît.
- Recréer une carte préparée dans un même processus demande `player_context_set_num_players(1)` avant.

**Prochaine étape** : les réponses d'Alexandre sur CESAR.md §13, puis M9.1.

---

## 2026-10-06 — Troisième essai d'Alexandre : pont, catastrophes, page du commerce (T3)

**Fait**
- T3.1 : un clic sur le pont de César tombait sur l'eau voisine (le pont pour navires est dessiné en hauteur) et le
  missionnaire s'arrêtait sans rien dire. Son ordre vise maintenant la case praticable la plus proche ; sans chemin,
  un avertissement. `mpinfo` donne aussi la destination.
- T3.2 : « aller au problème » partait au coin de la carte. La place d'un message était tronquée en `short` (messages
  à délai : incendies, effondrements) et sauvegardée sur 16 bits ; la fenêtre du message la bornait à 162 × 162.
  Sauvegardes multijoueur version 3, protocole 9.
- T3.3 et D-051 : une seule page de commerce, l'empire et les joueurs (demande d'Alexandre en cours de travail) ;
  `window/mp_trade` devient cette page du conseiller, `mp_trade_cheapest_seller` et `mp_trade_loads_on_the_way`
  pour l'affichage.
- Question d'Alexandre sur les raccourcis : ceux de Julius (configurables dans les options), voir le résumé.

**Appris**
- Les tests de l'outil sans tête n'ont pas les textes du jeu : tout ce qui lit le type d'un message
  (`lang_get_message`) y est muet. Tester la donnée elle-même.
- Une reproduction dans le vrai jeu (`goto`, clic au centre, `mpinfo`) a trouvé la cause du pont en deux essais,
  quand le test de simulation passait.

**Prochaine étape** : les réponses d'Alexandre sur CESAR.md §13, puis M9.1.

---

## 2026-10-06 — Plan « César juge » (D-050, demande d'Alexandre)

**Demande** : César revient et donne la victoire à la cité la plus méritante (prospérité, commerce, habitat,
troupes prêtées, fêtes, culture et éducation, guerres justes, dons) ; sa colère frappe toute la province quand les
joueurs sont trop belliqueux. Plan complet, centré sur l'équilibrage.

**Fait**
- `doc/mp/CESAR.md` : lauriers mensuels = valeur de la cité (5 notes) × faveur (×0,5 à ×1,5) ; faveur qui revient
  vers 50 (l'argent seul plafonne vers 75) ; campagnes de César partagées ; guerre déclarée avec motif, jauge de
  colère commune (durée et puissance au carré), avertissement, ultimatum, expédition punitive ; exploits et
  garde-fous ; méthode d'équilibrage (réglages en table, calibrage sur `test/data`, duels scriptés, télémétrie,
  critères chiffrés) ; 8 points à valider.
- Fiche `code-map/07` (sous-agent) : formules d'origine de la faveur, des notes, des dons, des fêtes, des batailles
  lointaines et de la colère de César, avec `fichier:ligne`.
- ROADMAP : M9 César juge, M10 la guerre sous l'œil de César (avec l'ancien M8.7), M11 équilibrage ; en-tête
  « jalon en cours » enfin à jour. VISION : E8 remplacée, H2, H3 et H5 revues. D-050.

**Appris**
- En partie libre, l'original remet la faveur à 50 chaque mois : `mp/caesar` doit tenir la faveur lui-même.
- DESIGN §5.1 disait le tribut et le prêt de secours désactivés en multijoueur ; D-026 et le code les gardent.
  Corrigé. La dette fait encore baisser la faveur en multijoueur.

**Révision (même jour)** : Alexandre trouve le multiplicateur de faveur trop compliqué et préfère « juste les
points ». Les lauriers s'additionnent : lauriers de la cité (notes, au plus 120 par an) et lauriers de César (fête
+2/+4/+6 une par semestre, don +4/+7/+10 un par an, campagnes, triomphes ; guerre sans motif −15). La faveur
d'origine reste figée et cachée. CESAR.md, D-050, ROADMAP et VISION mis à jour.

**Prochaine étape** : réponses d'Alexandre aux 7 points restants de CESAR.md §13, puis M9.1.

---

## 2026-10-06 — Alerte plein écran au changement de prix (M8.12, demande d'Alexandre)

**Demande** : quand un joueur change ses prix, une alerte plein écran chez son client. Avant, Alexandre a vérifié
que le prix de l'empire n'est pas indexé sur celui des joueurs (il ne l'est pas : table d'origine × 1,5 ; seul le
prix par défaut d'un joueur, tant qu'il n'y a pas touché, suit le prix de base de l'empire).

**Fait**
- `window/mp_price_alert` (D-049) : fenêtre 640 × 480 sur la ville grisée, vendeur, ressource, ancien et nouveau
  prix, « Voir le commerce » (onglet du vendeur, `window_mp_trade_show_partner`) et « OK ». Elle attend la vue de la
  ville hors construction, comme les popups de l'original.
- `mp/trade` : la file d'alertes remplace l'avertissement ; une ligne par vendeur et ressource, effacée si le prix
  revient à l'ancien. État d'affichage local, non sauvegardé.
- Tests : `mp_trade_prices` étendu (vérifié rouge en neutralisant l'effacement) ; `tools/mp-trade-test.sh` a son
  propre script client (`mp-trade-client.txt`) : achat du marbre, deux « + » de l'hôte, une seule ligne 200 → 220.

**Appris** : après une inversion de ligne pour voir un test échouer, la recompilation peut rater le retour (horodatage
à la seconde) : supprimer le `.o` (piège déjà noté dans `CLAUDE.md`).

**Prochaine étape** : M9 (guerre entre joueurs) avec M8.7. Points ouverts inchangés : vitesse des caravanes (D-048),
densité de la forêt (D-044).

---

## 2026-10-06 — Nettoyage avant la suite (demande d'Alexandre)

**Demande** : nettoyer ce qui n'est pas sûr, pas propre ou trop itéré, pour le prochain développement.

**Fait**
- `mp/mapgen` : type `map_layout` unique (plus de `struct`/`typedef` à deux noms), côtes de la mer dans l'état du
  module, commentaires à jour sur les cartes préparées.
- Plus aucun texte français en dur dans le code ajouté ces derniers jours : objectif du bandeau, carte modèle
  absente, taille « écran » des options d'affichage, nom d'une nouvelle partie annoncée sur le réseau
  (`MP_DISCOVERY_NEW_GAME`, traduit par le salon). Les statuts de `mp/lockstep` restent en français en dur, comme
  avant.
- `simtool` : dernier avertissement de compilation corrigé, `conserved_days` ne compte plus deux fois un échec,
  option `raw` de `terrain` au lieu d'une variable d'environnement.
- `tools/mp-trade-test.sh` pour la fenêtre du commerce en réseau ; `TESTING.md` (commandes multijoueur de
  `simtool`, `mpinfo`, scripts prêts, `julius-log.txt`), `DESIGN.md` §5.2 et §8 (cartes préparées, plan fixe, bras
  de mer), `CLAUDE.md` (commandes, pièges appris, « Alexandre essaie seul »).
- Compilation sans avertissement, 167 tests, trois scripts du vrai jeu verts.

**Prochaine étape** : M9 (guerre entre joueurs) avec M8.7 (interception des caravanes). Points ouverts : vitesse des
caravanes (D-048), densité de la forêt (D-044).

---

## 2026-10-05 — Session 2 (suite) : commerce, caravanes par ressource, l'empire en repli (M8.8 à M8.11)

**Retour d'Alexandre** : la carte et le jeu en solo sont validés (« c'est parfait »), on passe au commerce. Pendant
le travail : pas de repli sur l'empire quand un joueur n'a plus de stock, assécher un rival fait partie du jeu.

**Fait** (D-048, protocole 8)
- Une caravane par ressource achetée et par mois (avant, une seule par route) ; budget de l'acheteur partagé.
- L'empire ne vend pas ce qu'une cité achète moins cher à un joueur par une route ouverte, même sans stock
  (`empire_can_import_resource_from_city`, multijoueur seulement ; à une cité, rien ne change).
- Livraisons annoncées à l'acheteur ; colonne Empire dans la fenêtre « Joueurs », le moins cher en vert.
- Tests : `mp_trade_caravans` étendu, `mp_trade_preference`, `mp_trade_conservation`, `mp_trade_resume` (167 en
  tout) ; capture de la fenêtre en réseau.

**Appris**
- Une caravane met environ 15 ticks par case : 155 cases de route, environ 46 jours. Les tests attendent l'arrivée
  au lieu d'une durée fixe.
- Un entrepôt tout juste construit n'est « en service » qu'au tick suivant la mise à jour des états : ses stocks ne
  comptent pas avant.
- Remplir un entrepôt d'une autre ressource laisse la place restante dans l'emplacement déjà entamé (4 chargements
  par emplacement) : livraison partielle, pas nulle.

**Prochaine étape** : M9 (guerre) avec M8.7 (interception des caravanes). Points ouverts : vitesse des caravanes
(D-048, à valider).

---

## 2026-10-05 — Session 2 (suite) : bras de mer, joueur de la pierre sans eau, prés visibles (MC.4, MC.5)

**Demande d'Alexandre** : le défilement « fonctionne nickel » (T2.8 validé). Un grand bras de mer qui traverse la
carte, deux joueurs sur la même rive à 2, deux par rive à 4 ; pas d'eau hors aqueduc pour le joueur de la pierre.
En cours de travail : « les plaines fertiles n'apparaissent toujours pas, sauf quand je passe un coup de pelle ».

**Fait** (D-047)
- Générateur des cartes préparées réécrit autour d'un plan fixe par carte : villes, points d'arrivée, routes de
  César, tracé de la mer ; bras de mer d'un bord à l'autre ; pont de César pour navires (`map_bridge_add` sur un
  chenal droit de trois cases) ; réservoir de César sur la côte, aqueduc vers le joueur de la pierre ; plus de lacs
  de joueurs ni de lac central.
- Prés : le générateur dessinait les prés puis l'herbe, qui les recouvrait (le chargement d'une carte fait
  l'inverse) ; ordre corrigé, contrôle ajouté au test (5 785 prés cachés avant, 0 après).
- `simtool terrain` (une lettre par case, `TERRAIN_RAW=1` pour les valeurs brutes) ; ligne de diagnostic de la
  souris retirée. Images : `../CARTES_3/` (cartes, pont, prés).

**Appris**
- Un pont se bâtit depuis une case d'eau dont un seul voisin est de la terre et se termine sur une case pareille :
  sur une côte irrégulière, il faut creuser un chenal droit.
- Les quais de pierre sur le rivage près d'un bâtiment sont du jeu d'origine (rive fortifiée), pas un défaut.
- `git stash` pour « vérifier sans le correctif » retire aussi le test : inverser seulement la ligne fautive.

**Prochaine étape** : retour d'Alexandre sur la carte ; puis le commerce (choix du moins cher) et M9 (le pont de
César sera un enjeu de la guerre).

---

## 2026-10-05 — Session 2 (suite) : défilement vers le haut, deuxième correction (T2.8)

**Retour d'Alexandre** : la carte est bien, mais la souris en haut de l'écran ne fait toujours pas défiler.

**Fait** (D-046, complément)
- Relecture de SDL 3 : en plein écran le jeu capturait la souris (`SDL_SetWindowGrab`) ; SDL 3 confine alors le
  curseur à `mouseConfinementRect`, calculé depuis `contentLayoutRect`, qui dans un Space exclut la barre de titre
  cachée (28 points) : le curseur ne pouvait plus atteindre le haut de l'écran, juste la hauteur de la barre de menu
  du jeu, d'où le ressenti d'Alexandre. Cela explique aussi pourquoi la première correction (position lue auprès du
  système) n'a rien changé : le curseur était vraiment bloqué sous la barre. Capture retirée sur macOS.
- Position lue sur la fenêtre Cocoa (`mouseLocationOutsideOfEventStream`), indépendante des événements et de la
  position que SDL croit connaître ; `SDL_GetGlobalMouseState` en secours.
- macOS : `julius-log.txt` écrit dans le dossier des données (jamais pendant l'automatisation) ; près du haut de
  l'écran, une ligne par seconde avec la position vue par le jeu et par le système, à lire chez Alexandre
  (`../donnees-c3_test2/julius-log.txt`).

**Appris**
- Sur macOS, `SDL_Log` d'une application lancée depuis le Finder ne va nulle part de lisible : un fichier est
  nécessaire pour diagnostiquer chez Alexandre sans ouvrir de fenêtre (I6).
- Contrepartie de la capture retirée : avec deux écrans, un curseur parti sur l'autre écran fait défiler la carte
  tant qu'il y reste.

**Prochaine étape** : retour d'Alexandre ; si le blocage persiste, lire `julius-log.txt` ; puis le commerce (choix
du moins cher) et M9.

---

## 2026-10-05 — Session 2 (suite) : défilement vers le haut en plein écran (T2.8)

**Retour d'Alexandre** : plein écran « beaucoup mieux », mais la souris en haut de l'écran ne fait plus défiler
(« la barre de menu de Caesar empêche le déplacement »). Travail confié à Fable.

**Fait** (D-046)
- Diagnostic par lecture de SDL 3.4.18 (Cocoa) et des tickets SDL : sur un écran à encoche, le plein écran natif
  place la fenêtre sous la bande noire de l'encoche ; le curseur y sort de la fenêtre et plus aucun événement de
  mouvement n'arrive, la position connue du jeu reste figée sur la barre de menu. De plus, macOS 26+ (Alexandre est
  en macOS 27) livre des positions périmées en haut de l'écran (SDL #15967).
- `platform_screen_get_system_mouse_position` : position du curseur demandée au système, convertie en coordonnées
  du jeu ; `sync_mouse_with_system` (macOS) avant chaque image : en plein écran la position est ramenée au bord de
  la fenêtre, en fenêtre elle est corrigée tant que le curseur est dessus. Pas pendant l'automatisation.

**Appris**
- La barre de menu du jeu n'y était pour rien : survolée, elle laisse passer le défilement (`handle_mouse_menu`
  ne rend 1 qu'au clic). Le bord de défilement fait 5 pixels : seul un curseur réellement en haut déclenche.
- Mode compatibilité de l'encoche conservé (par défaut) : sans lui, la barre de menu du jeu passerait sous l'encoche.

**Prochaine étape** : retour d'Alexandre (impossible à vérifier sans vraie fenêtre) ; puis le commerce (choix du
moins cher) et M9.

---

## 2026-10-05 — Session 2 (suite) : mission d'office, plein écran natif (T2.6, T2.7)

**Retours d'Alexandre** : la mission doit être construite au démarrage ; en plein écran, barre de menus et Dock
restent visibles, sauf après un passage fenêtre → plein écran, où l'on ne peut plus défiler vers le bas.

**Fait** (commits 3d5d2d76, cfcfcda0 ; protocole 7)
- D-045 : mission de chaque joueur bâtie à la création de la carte, au bord de la route de César, avec sa zone et
  son brouillard ; tests de zones et de missions revus ; `tools/mp-solo-test.sh` adapté.
- Plein écran : lecture du code de SDL 3.4 (Cocoa). Sans Space, la fenêtre ne passe au-dessus de la barre et du
  Dock que si elle a le focus quand la souris est capturée (pas au lancement), et le bas sortait de l'écran
  (encoche probable). Retour au Space natif ; le délégué de fenêtre de SDL renvoie « barre et Dock masqués »
  (comme SDL 2) ; `SDL_VIDEO_MAC_FULLSCREEN_MENU_VISIBILITY` = 0.

**Appris**
- Le Homebrew du Mac fournit sdl2-compat sur SDL 3 : les comportements macOS sont ceux de SDL 3 (sources lues dans
  le bloc-notes, pas dans le dépôt).
- Un entrepôt sans accès à la route ne compte pas dans les stocks de la cité.

**Prochaine étape** : retour d'Alexandre sur le plein écran (impossible à vérifier sans vraie fenêtre).

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
