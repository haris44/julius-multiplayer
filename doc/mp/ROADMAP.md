# Feuille de route

> **Jalon en cours : M2, couche de commandes et séparation interface / simulation.**
> Légende : `[x]` fait · `[~]` en cours · `[ ]` à faire. Une tâche n'est cochée que si ses critères sont vérifiés
> par des tests automatisés (voir [TESTING.md](TESTING.md)). Chaque commit est préfixé par l'ID de sa tâche.
> On suit l'ordre, sauf décision contraire consignée dans le JOURNAL.

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
- [ ] **M2.1** Infrastructure de commandes : structure, sérialisation petit-boutiste, file, exécuteur, numéro de
  séquence. *Critère* : tests unitaires d'aller-retour.
- [ ] **M2.2** Construction en commandes à paramètres absolus : bâtiments, tracés de routes, murs et aqueducs,
  maisons, ponts, forts, portes, temples. Inventaire : code-map/05 §4.
- [ ] **M2.3** Démolition en commande ; l'aperçu de l'effacement ne marque plus les vrais bâtiments.
- [ ] **M2.4** Réglages de la cité en commandes : impôts, salaires, priorités, fêtes, entrepôts et greniers, commerce
  (routes, importation et exportation, seuils), industries à l'arrêt.
- [ ] **M2.5** Ordres militaires en commandes : déplacer, retour au fort, formation, service.
- [ ] **M2.6** Recalculs déclenchés par l'interface (conseillers) transformés en commandes ; phrases des figures
  calculées localement.
- [ ] **M2.7** Enregistrement et rejeu (`.mprec`). *Critères* : un rejeu donne deux fois la même trace ; une partie
  jouée par script d'automatisation, une fois rejouée sans tête, donne la même somme de contrôle finale.
- [ ] **M2.8** `sim_step` : en multijoueur, la cadence de la simulation ne dépend plus de l'interface (pas de pause
  liée aux fenêtres, aux tracés ou au défilement, pas de perte de ticks). Le classique ne change pas.
- [ ] **M2.9** En multijoueur, l'aperçu de construction et le rendu n'écrivent plus dans l'état pendant les ticks
  (D-010).
- [ ] **M2.10** Orientation canonique de la simulation en multijoueur (D-009). *Critère* : même trace quelle que
  soit l'orientation locale.
- [ ] **M2.11** Messages adressés à un joueur, popups locales, pas de victoire « classique » en multijoueur.
  *Critère* : `tools/cross-check.sh` identique aussi sur `brugle-massilia-start` et `brugle-lugdunum`.

## M3 — Moteur multi-cités
- [ ] **M3.1** Types élargis en mémoire (coordonnées 16 bits, offsets 32 bits). Le format classique est réécrit à
  l'identique. *Critère* : parité.
- [ ] **M3.2** Grille de taille variable (`map_grid_size()`), tables d'offsets calculées. *Critère* : parité.
- [ ] **M3.3** Outil de composition : placer une sauvegarde classique dans un monde plus grand, avec un décalage,
  puis l'extraire. *Critère* : test d'invariance par translation sur les 36 cas.
- [ ] **M3.4** Contexte de cité : `city_data` par cité, `city_extra`, générateur aléatoire par cité (D-004, D-007).
  *Critère* : parité.
- [ ] **M3.5** Tranches d'identifiants et conversion des boucles (D-005). *Critère* : parité.
- [ ] **M3.6** Ordonnancement d'un tick à N cités (DESIGN §3.3). *Critère* : parité.
- [ ] **M3.7** **Test d'isolement « jumeaux »** : la même sauvegarde placée deux fois (cités 0 et 1) redonne,
  pour chacune, la référence du jeu original, sur les 36 cas. Plus le test d'indice (une seule cité placée en
  position k). C'est la preuve de E7 pour le moteur multi-cités.
- [ ] **M3.8** Sauvegarde multijoueur `.mpsav` (monde, cités, état caché). *Critère* : test de reprise exacte
  (continuer la partie = sauvegarder puis recharger).

## M4 — Règles du jeu libre multijoueur
- [ ] **M4.1** Neutralisation de César en multijoueur (liste : code-map/04 §3). *Critères* : en multijoueur, une
  sauvegarde avec demande ou invasion de César ne déclenche rien ; parité verte.
- [ ] **M4.2** Point d'arrivée par joueur : entrée et sortie, « route de Rome » calculée par cité.
- [ ] **M4.3** Territoires : grille, construction limitée au territoire, bande neutre, eau filtrée (D-006).
- [ ] **M4.4** Démarrage d'une partie multijoueur depuis une carte, de 1 à 4 joueurs, par un chemin déterministe.
  La partie libre solo est jouable (menu provisoire). *Critère* : scénario d'automatisation avec captures.
- [ ] **M4.5** Entités neutres (indigènes, animaux, menaces IA) et leurs options.
- [ ] **M4.6** Fin de partie multijoueur : sans fin, conquête, score.

## M5 — Réseau local → premier prototype jouable en LAN
- [ ] **M5.1** Couche sockets (TCP non bloquant, POSIX et Winsock). *Critère* : tests en boucle locale.
- [ ] **M5.2** Protocole lockstep : tours, délai, relais, sommes de contrôle, détection des désynchronisations et
  sauvegardes de diagnostic.
- [ ] **M5.3** Banc de test multi-processus sans tête (`tools/lan-test.sh`) : de 2 à 4 instances, commandes
  scriptées, traces identiques.
- [ ] **M5.4** Salon minimal : héberger, rejoindre par IP ou par découverte UDP, vérifier les sommes de contrôle des
  données, choisir les règles, se déclarer prêt.
- [ ] **M5.5** Pause, vitesse, déconnexion ; sauvegarde coordonnée par l'hôte.
- [ ] **M5.6** Intégration continue multiplateforme (macOS arm64, Windows x64, Linux) qui compare les traces de
  rejeu. *Nécessite un dépôt GitHub : à demander à Alexandre.*
- [ ] **M5.7** Première vraie partie en LAN avec Alexandre (Mac et PC) et retour d'expérience.

## M6 — Grandes cartes
- [ ] **M6.1** Format `.mpmap` : taille, points d'arrivée, territoires.
- [ ] **M6.2** Générateur de cartes aléatoires : relief, eau, forêts, roches, gisements, positions équilibrées.
- [ ] **M6.3** Éditeur : grandes tailles, points d'arrivée, territoires.
- [ ] **M6.4** Rendu, minicarte et captures sur grandes cartes ; mesure et optimisation du routage.

## M7 — Interface multijoueur
- [ ] **M7.1** Menu principal sans campagne ; écrans du salon.
- [ ] **M7.2** Couleurs des joueurs : frontières, minicarte, marques sur les bâtiments et les soldats.
- [ ] **M7.3** Tableau des scores, messages par joueur, écran de fin.
- [ ] **M7.4** Interface de César masquée ; traductions françaises des nouveaux textes.
- [ ] **M7.5** Discussion entre joueurs (optionnelle).

## M8 — Commerce entre joueurs
- [ ] **M8.1** Conception détaillée (D-012) ; routes commerciales par joueur avec les villes de l'empire.
- [ ] **M8.2** Routes entre joueurs et caravanes physiques.
- [ ] **M8.3** Tests : rejeux, isolement hors commerce, scénarios visuels.

## M9 — Guerre entre joueurs
- [ ] **M9.1** Hostilité par propriétaire. En classique, elle reproduit exactement la matrice actuelle.
- [ ] **M9.2** Légions chez l'adversaire, ordre « attaquer », portes et murs qui ne laissent passer que leur
  propriétaire.
- [ ] **M9.3** Moral et totaux par camp, paix, arcs de triomphe.
- [ ] **M9.4** Tests : scénarios de combat rejoués, captures.

## M10 — Finitions
- [ ] Équilibrage (fonds de départ, rythme), performance, paquets d'installation (app macOS, exécutable Windows),
  documentation pour les joueurs.
