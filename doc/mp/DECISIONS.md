# Journal des décisions

> Une entrée par décision structurante : on ajoute, on ne réécrit pas. Pour changer d'avis, on crée une nouvelle
> entrée qui remplace l'ancienne et on met à jour son statut. Statuts : **adoptée**, **à valider**
> (Claude a tranché, Alexandre peut revenir dessus), **remplacée par D-xxx**.

---

### D-001 — Partir de Julius plutôt que décompiler Caesar III
- 2026-10-04 · **adoptée** (proposée par Alexandre)
- Julius est déjà le résultat de la décompilation. C'est une réimplémentation fidèle, sous licence AGPL, avec des
  tests de parité contre le jeu original. Tout refaire coûterait des années pour revenir au même point, avec un
  risque juridique.
- Conséquence : le fork reste sous AGPL-3.0. Les données du jeu ne sont jamais dans le dépôt.

### D-002 — Le mode classique reste intact : on neutralise, on ne supprime pas
- 2026-10-04 · **adoptée**
- Les 36 tests de parité couvrent aussi César, les demandes, les invasions et les batailles lointaines. Ils sont la
  preuve que le gameplay intérieur reste celui de l'original (E7).
- Décision : tout ce qui est retiré du jeu (César, campagne) est **désactivé par le mode de jeu**. Toute nouveauté
  est inactive en mode classique. Un test de parité rouge est une régression, jamais un test à adapter.

### D-003 — Réseau en lockstep déterministe
- 2026-10-04 · **adoptée**
- L'état d'une partie est énorme (grilles, milliers d'entités) et le jeu se rapproche d'un RTS : le lockstep est
  la méthode d'AoE2. Le code de simulation est déjà sans flottant ni horloge.
- Alternative rejetée : un serveur qui fait autorité et réplique l'état. Trop de bande passante, et tout le code
  de rendu serait à revoir.

### D-004 — Contexte de cité par pointeur (option A)
- 2026-10-04 · **adoptée**
- `city_data` devient une macro sur la cité courante. Coût estimé : 120 à 150 sites à modifier, contre 2 500 à
  3 500 lignes pour passer un identifiant partout. Avec un seul joueur, le chemin d'exécution est identique par
  construction. Détail dans code-map/02 §5.
- Risque : oublier de basculer le contexte. Parade : `push`/`pop` avec contrôle d'équilibre, et le test
  d'isolement, qui détecte toute fuite.

### D-005 — Tranches d'identifiants par joueur
- 2026-10-04 · **adoptée**
- Bâtiments, figures, formations, chemins et stockages sont découpés en tranches de la taille d'origine, une par
  joueur plus une pour le monde.
- Avantages :
  - chaque cité garde les limites et l'ordre d'allocation d'origine (E5 et E7) ;
  - avec un seul joueur, on retrouve exactement l'original ;
  - les ids restent globaux, et le propriétaire se déduit de l'id.
- Alternative rejetée : un tableau commun avec un compteur par joueur. L'ordre de traitement et la réutilisation
  des emplacements mêleraient les cités, ce qui rend impossible le test d'isolement exact.

### D-006 — Territoires fixes séparés par une bande neutre
- 2026-10-04 · **remplacée par D-018** (Alexandre veut construire partout, avec branchements)
- Chaque joueur ne construit que sur son territoire. Par défaut, ce sont des cellules de Voronoï autour des points
  d'arrivée.
- Raisons :
  - routes, eau, main-d'œuvre et services de deux cités ne peuvent pas se connecter. On n'a donc pas à filtrer par
    propriétaire des dizaines de recherches de cible et le code de déplacement ;
  - un adversaire ne peut pas bloquer votre « route de Rome » ;
  - c'est lisible pour les joueurs.
- Différence avec AoE2, où l'on construit partout. Si Alexandre préfère construire partout, il faudra une grille de
  propriété des routes et environ 60 filtres supplémentaires (code-map/03 §6.2).

### D-007 — Un générateur aléatoire par cité, plus un pour le monde
- 2026-10-04 · **adoptée**
- Une cité ne doit dépendre ni des tirages ni de l'activité de ses voisines. C'est nécessaire au test d'isolement
  et plus juste : sinon, à un tick donné, toutes les cités tirent les mêmes épidémies et les mêmes colères divines.

### D-008 — Grille de taille variable, fixée au chargement
- 2026-10-04 · **adoptée**
- 162 pour les sauvegardes classiques (aucune conversion), plus pour les cartes multijoueur. Les types sont
  élargis en mémoire et le format classique est réécrit à l'identique.
- Alternative rejetée : `GRID_SIZE` agrandi à la compilation avec conversion des sauvegardes. Il faudrait traduire
  chaque offset stocké, et la performance des cartes classiques en pâtirait.

### D-009 — Orientation canonique pour la simulation en multijoueur
- 2026-10-04 · **adoptée**
- Aujourd'hui, la rotation de la vue modifie la simulation : passabilité des murs, sentinelles, chevaux, poses.
  Chaque joueur ayant sa propre vue, la simulation multijoueur utilise toujours le nord. La rotation devient
  purement visuelle. Le mode classique garde le comportement actuel, car des tests sont en orientation 4.

### D-010 — En multijoueur : pas d'annulation, pas de pause liée à l'interface
- 2026-10-04 · **adoptée**
- L'annulation restaure des grilles entières et effacerait les constructions des voisins.
- La simulation ne peut pas s'arrêter parce qu'un seul joueur ouvre un conseiller. Le temps ne s'arrête que par
  une pause commune, comme dans AoE2.
- Conséquence visible : on ne peut plus construire pendant que le jeu est « figé » derrière une fenêtre.

### D-011 — Sauvegarde multijoueur séparée (`.mpsav`)
- 2026-10-04 · **adoptée**
- Le format classique (0x66) est figé : grilles de 162, champs de 8 et 16 bits, deux enregistrements de cité
  seulement. Le multijoueur a son propre format versionné, qui contient aussi tout l'état caché.

### D-012 — Commerce entre joueurs par caravanes physiques
- 2026-10-04 · **adoptée**, précisée par D-019 (validée par Alexandre)
- Les caravanes traversent la carte entre les entrepôts des deux cités et peuvent être interceptées. C'est fidèle
  à l'esprit des caravanes de Caesar III et à AoE2.
- Alternative possible : un échange instantané, où chaque joueur est vu comme une ville commerçante virtuelle.
  Plus simple, mais sans lien avec la guerre. Le détail sera fixé au jalon M8.

### D-013 — Réseau en étoile, TCP, sockets natives
- 2026-10-04 · **adoptée**
- Au plus 4 joueurs, sur un réseau local : l'hôte relaie tout. Une petite couche POSIX/Winsock évite toute
  nouvelle dépendance.

### D-014 — Partie libre solo = partie multijoueur à un joueur
- 2026-10-04 · **adoptée**
- Un seul chemin de code. Utile pour s'entraîner, et surtout pour tester sans réseau.

### D-015 — Remise à zéro de l'état caché à chaque démarrage ou chargement
- 2026-10-04 · **adoptée**. Confirmée en M1.3 : 17 sauvegardes sur 17 sont identiques au rechargement sur
  6 000 ticks, et la parité reste verte. Il fallait remettre à zéro **avant** la lecture du fichier : certains
  modules (les compteurs de bâtiments) ne sauvegardent qu'une partie de leur état.
- Sept sauvegardes sur neuf divergent quand on les recharge dans le même processus, à cause de variables non
  sauvegardées. Les remettre à l'état d'un processus neuf rend les chargements reproductibles, sans casser la
  parité (code-map/01 §3.6).

### D-016 — Les réglages qui influencent la simulation passent par `game_rules`
- 2026-10-04 · **adoptée**
- Difficulté, dieux et correctifs de gameplay sont aujourd'hui lus dans `c3.inf` et `julius.ini`, propres à chaque
  machine. En multijoueur, ils viennent des règles de la partie, synchronisées et sauvegardées.

### D-017 — Tribut, salaire, épargne et prêt de secours supprimés en multijoueur
- 2026-10-04 · **à valider** (hypothèse H2)
- Ce sont des relations avec César et Rome. Les supprimer rend l'économie un peu plus facile ; on pourra
  compenser en réglant les fonds de départ. Les salaires de Rome, qui servent de référence économique, sont
  conservés.

### D-018 — Construction partout, branchements possibles, économie propre à chaque cité
- 2026-10-04 · **adoptée** (décision d'Alexandre) ; « construire partout » **remplacé par D-036** ; la règle « chacun
  ne sert que sa cité » est confirmée par Alexandre le 2026-10-05 (Q2 à Q4 : marchandes et immigrés circulent,
  la désirabilité passe)
- On construit n'importe où sur une case libre, comme dans AoE2. Les routes, aqueducs et murs de deux joueurs
  peuvent se toucher et se brancher.
- Propriété :
  - les bâtiments, figures et formations appartiennent au joueur de leur tranche d'ids (D-005) ;
  - une nouvelle grille `owner` donne le propriétaire des infrastructures posées sur le terrain (routes, murs,
    aqueducs, jardins, places, ponts) ;
  - on ne démolit que ce qui vous appartient. Détruire chez l'autre relève de la guerre.
- Les personnages marchent sur **toutes** les routes, mais n'agissent que sur les bâtiments **de leur
  propriétaire** : couverture des services, recrutement, marchés, charrettes, migrants, pompiers. Les seules
  exceptions sont les mécanismes multijoueur (caravanes de commerce, soldats).
- Conséquences :
  - environ 60 recherches de cible à filtrer par propriétaire (code-map/03 §6.2, code-map/02 §4.2), en plus du
    réseau routier ;
  - l'eau se calcule par réseau **et** par propriétaire ;
  - la désirabilité traverse les cités : c'est un effet de voisinage voulu ;
  - la « route de Rome » est calculée pour chaque cité depuis son point d'arrivée. La démolition automatique ne
    touche plus que les murs, aqueducs et bâtiments du joueur concerné. Être emmuré par un voisin devient un acte
    de guerre.
- Effet assumé : branchée, une cité n'est plus strictement identique à l'original, car ses personnages peuvent
  partir errer chez le voisin. **Non branchée, elle l'est** : c'est ce que vérifie le test d'isolement « jumeaux ».

### D-019 — Commerce entre joueurs sur des routes construites, interceptable
- 2026-10-04 · **adoptée** (décision d'Alexandre) ; détails à fixer au jalon M8
- Deux joueurs ne commercent que si une **route construite** relie leurs cités. Cette route peut traverser toute
  la carte et emprunter les routes d'autres joueurs.
- Ouvrir une route commerciale entre deux joueurs se fait par une commande. Elle exige l'accord des deux joueurs et
  un chemin routier entre leurs entrepôts.
- Les caravanes partent d'une cité, suivent **uniquement les routes**, achètent et vendent dans les entrepôts de
  l'autre selon ses réglages d'import et d'export, puis reviennent. Elles réutilisent la logique des caravanes
  d'origine.
- **Interception** : les caravanes sont des civils, attaquables par les soldats ennemis, et leur cargaison est
  perdue. Couper ou contrôler la route est un levier militaire.
- Argent : l'exportateur encaisse le prix de vente, l'importateur paie le prix d'achat (prix communs au monde),
  avec des quotas par route.

### D-020 — Autorisations d'exploiter différentes selon le point d'arrivée
- 2026-10-04 · **adoptée** (décision d'Alexandre) ; l'équilibrage est à faire (M10)
- Dans le jeu de base, une industrie n'est constructible que si la ville « à nous » de la carte de l'empire produit
  la matière première, ou si une route ouverte la fournit (`empire_can_produce_resource`, code-map/04 §1.1).
- En multijoueur, chaque **point d'arrivée** porte son propre jeu d'autorisations, défini dans la carte ou par le
  générateur. La règle d'origine est évaluée **dans le contexte de chaque cité** : on peut aussi construire
  l'atelier d'une matière qu'on ne produit pas si une route commerciale ouverte la fournit (empire ou joueur).
- Les autorisations sont **complémentaires** entre joueurs, pour forcer le commerce. Le terrain doit contenir les
  gisements correspondants : fer, argile, bois, marbre, terres fertiles.
- **Armes** : ressource stratégique, car les casernes en consomment pour former les soldats. Seuls certains points
  d'arrivée ont le droit d'extraire le fer et de forger. Les autres doivent les acheter (à un joueur ou à
  l'empire) ou les prendre. Les villes de l'empire n'en vendront qu'avec parcimonie (réglage).
- Points d'équilibrage à mesurer en M10 : nombre de joueurs avec fer ou armes, prix des armes, quotas, fonds de
  départ. On évaluera avec des parties simulées sans tête.
- *Répartition provisoire (M4.7, à valider)*, en attendant que les cartes la définissent (M6) : l'agriculture reste
  permise à tous ; le fer, donc la forge d'armes, va au seul joueur 1 ; argile, bois, olives, vignes et marbre
  sont distribués à tour de rôle à partir du joueur 2. Une cité garde le droit de forger si une route peut lui
  fournir du fer (règle d'origine). Les bâtiments déjà présents dans une sauvegarde continuent de fonctionner.


### D-021 — Contexte de cité par échange de zones mémoire enregistrées (précise D-004)
- 2026-10-04 · **adoptée**
- Chaque module qui détient de l'état « par cité » (`city_data`, couvertures culturelles, compteurs de bâtiments,
  greniers, sentinelles, totaux de légions, victoire, messages, générateur aléatoire, listes de travail…) enregistre
  sa zone de mémoire (`player_context_register`). Changer de cité courante recopie les zones de la cité quittée dans
  son emplacement et charge celles de la nouvelle.
- Pourquoi plutôt qu'une macro sur un pointeur : un seul mécanisme pour `city_data` et pour la douzaine de `static`
  dispersés, aucune des 2 111 lignes `city_data.` ni du code des modules à modifier, et rien ne change avec un seul
  joueur (aucun échange).
- Coût : environ 60 Ko recopiés par changement. Grâce aux tranches d'ids (D-005), on change de cité quelques fois
  par tick et par joueur, pas à chaque entité. À mesurer en M3.6.
- Risque : oublier d'enregistrer un état. Parade : les tests d'indice et « jumeaux » (M3.7) détectent toute fuite.

### D-022 — Coordonnées 16 bits qui « bouclent » à 256 sur les cartes classiques
- 2026-10-04 · **adoptée**
- Les coordonnées des bâtiments et des personnages passent de 8 à 16 bits, et leurs offsets de grille de 16 à
  32 bits, pour les grandes cartes. Mais Caesar III calcule parfois des coordonnées négatives près du bord (explosions
  du séisme, positions de formation) qui « bouclent » à 255 sur 8 bits. Le test de parité `sav_earthquake2` l'a
  révélé.
- Toute affectation d'une coordonnée de bâtiment ou de personnage passe par `GRID_COORD()`. Le masque dépend de la
  taille de la **carte**, car les coordonnées comptent à partir de son premier coin : 8 bits pour une carte de moins
  de 256 cases (comportement d'origine), 16 bits au-delà. Le test de translation sur une grille de 400 l'a montré.
- Conséquence assumée : sur une grande carte multijoueur, ce bogue d'origine (coordonnées négatives qui bouclent
  à 255) n'existe plus. Il n'a d'effet que sur des personnages hors carte (explosions du séisme près du bord).
- Attention : ne jamais appliquer ce masque à d'autres structures (points de carte, tuiles) où -1 signifie
  « invalide ». Un premier essai trop large a cassé 22 tests de parité.

### D-023 — Ce que le test « jumeaux » impose au moteur multi-cités
- 2026-10-04 · **adoptée** (M3.7)
- Le test place une ville et sa copie sur une même grande carte, chacune entourée de 24 cases de forêt et d'eau, et
  vérifie deux choses : la ville d'origine évolue exactement comme si elle était seule, et la copie obtient
  exactement la même évolution. Il a révélé et fait corriger :
  - des **numéros utilisés comme des nombres** : direction des troupeaux, attentes et parité des animaux et des
    ennemis (`id & 0x1f`…). On passe désormais par le numéro **local** (`FIGURE_LOCAL_ID`, `FORMATION_LOCAL_ID`,
    `BUILDING_LOCAL_ID`), égal au numéro d'origine en partie classique ;
  - des champs 8 bits trop étroits : `storage_id`, `formation_position_*` (positions des soldats), numéros de réseau
    routier, qui passent à 16 bits et « bouclent » à 256 sur les cartes classiques, comme l'original ;
  - des boucles devenues globales à tort : création des débris flottants et colère/bénédiction de Mars (cité
    courante seulement) ;
  - la figure « nulle » n° 0, enregistrement poubelle commun à toutes les cités : exclue de la somme de contrôle,
    comme le bâtiment n° 0.
- La copie est placée **en diagonale** (même décalage en x et en y). Un bogue d'origine conservé
  (`building_granary_for_getting` passe x à la place de y) rend sinon les distances différentes. Conséquence pour
  le jeu : ce calcul dépend légèrement de la position sur la carte. C'était déjà le cas dans Caesar III, et ce n'est
  pas une fuite entre cités.
- Effet de voisinage voulu (DESIGN §3.4) : à moins de 24 cases, la désirabilité et l'errance des troupeaux traversent
  d'une cité à l'autre.

### D-024 — Sauvegarde et somme de contrôle multijoueur : le format classique, élargi
- 2026-10-04 · **adoptée** (M3.8)
- Une partie multijoueur s'enregistre avec les fonctions de sauvegarde **existantes**, dans un mode « large » où la
  quinzaine de champs étroits passent à 16 ou 32 bits : coordonnées, offsets de grille, numéros de stockage et de réseau.
  On y trouve les grilles au pas de la carte, toutes les tranches d'entités, puis l'état de chaque cité, joueur par
  joueur.
- La somme de contrôle se calcule sur cette même sérialisation. Elle couvre donc toutes les cités, alors que la
  version classique ne voyait que le joueur 1.
- Rejeté : la copie brute de la mémoire. Plus simple, mais les octets de bourrage des structures et les
  différences possibles d'agencement entre compilateurs (Mac clang, Windows MSVC) la rendraient non portable.
  L'écriture explicite petit-boutiste l'est.
- L'état caché qu'un chargement classique recalcule (stocks des greniers hors « réception », curseurs…) est écrit
  explicitement dans l'état « extra » de chaque cité, car le recalcul ne redonne pas la valeur en cours. La copie
  brute de la mémoire de chaque cité sert seulement d'**oracle de test** (`mpresume`), jamais de format.

### D-025 — Partie réseau provisoire : une copie de la cité de départ par joueur
- 2026-10-04 · **remplacée par D-033** (le salon ne propose plus de copie de carte classique ; l'outil reste pour les
  tests)
- Par défaut, une partie réseau donne à chaque joueur **sa propre cité** : l'hôte charge la sauvegarde classique,
  la recopie une fois par joueur sur une grille de 512 (carré 2 × 2, 24 cases de roche entre les cités, joueurs 1 et 2
  en diagonale), écrit le `.mpsav`, le recharge et l'envoie aux clients. Chaque machine affiche sa cité : trésorerie,
  population, conseillers et notes sont ceux du joueur local (E13). `--mp-shared-city` garde l'ancien mode M2P.
- Les cités ne se touchent pas (roche) : pas encore de commerce ni de guerre entre joueurs.
  *Mis à jour le 2026-10-04 (M4.4)* : en partie réseau, la roche entre les cités devient de la terre
  constructible (`mp_compose_open_land_between_cities`). Les joueurs peuvent y bâtir et relier leurs cités par des
  routes (D-018, D-028). Les tests d'isolement gardent la roche.
- Les joueurs 3 et 4 ne sont pas en diagonale : un bogue d'origine (x passé pour y dans le choix d'un grenier,
  D-023) y donne des choix de grenier légèrement différents de la cité d'origine. C'est le comportement de l'original
  sur une carte décalée, sans effet sur le déterminisme.

### D-026 — Ce que devient César en multijoueur
- 2026-10-04 · **adoptée** (M4.1) · *à valider* par Alexandre pour le tribut et les salaires de Rome
- **Supprimés** en multijoueur, par le mode (le classique ne change pas) : demandes de biens et de troupes,
  colère et invasions de César (y compris celles prévues par le scénario), batailles lointaines, changement
  d'empereur, évolution de la faveur (figée à sa valeur), salaire du gouverneur.
- **Gardés**, parce que l'économie interne en dépend (gameplay intérieur identique, E7) : tribut annuel, prêt de
  secours en cas de dette, variations aléatoires des salaires de Rome (référence du sentiment et de la prospérité ;
  le message « Rome augmente les salaires » reste donc).
- Pas encore traité : l'interface (conseiller impérial, cadeaux, faveur dans la barre latérale), à masquer en M7.

### D-027 — Chaque cité a son propre « coin (0, 0) » et ses propres cibles
- 2026-10-04 · **adoptée** (M4.2) · à revoir en M9 (guerre entre joueurs)
- L'original envoie vers la case (0, 0) les figures dont le bâtiment a disparu : un bâtiment supprimé est remis à
  zéro, donc placé en (0, 0). Sur une carte composée, ce point doit être le coin de la cité, pas celui de la grande
  carte. Le « bâtiment nul » de chaque tranche (`building_get(0)`) porte ce coin, et un bâtiment supprimé y est
  replacé. En partie classique, c'est toujours (0, 0).
- Les recherches de cible des combats (soldats, ennemis IA, loups, tirs, préfets contre émeutiers) et les
  recalculs de sentinelles et de chevaux ne voient que les figures de la cité courante. Certaines n'ont pas de
  limite de distance (« la légion libre la plus proche, sinon la première ») : une armée IA traversait la carte vers
  une autre cité. M9 ouvrira volontairement les combats aux figures des autres joueurs.

### D-028 — Propriété : ce qui est revendiqué, et ce qui reste partagé (précise D-018)
- 2026-10-04 · **adoptée** (M4.3)
- Une case d'infrastructure (route, mur, aqueduc, jardin) est revendiquée par le joueur dont la **commande de
  construction** la crée. Elle est libérée quand l'infrastructure disparaît. Les mises à jour générales de la carte,
  qui tournent pendant le tour d'une cité, ne revendiquent rien : sinon une cité s'appropriait les routes d'une autre.
- Une cité n'agit que sur ses propres bâtiments : couverture des services et des marchés, fusion des maisons,
  propagation du feu, émeutiers, séisme (qui s'arrête aussi aux infrastructures des autres), démolition, « route de
  Rome » qui supprime un mur ou un aqueduc bloquant.
- Eau : les aqueducs d'une cité ne portent que son eau, vers ses réservoirs. Chaque cité a sa propre grille des zones
  desservies (sauvegardée dans l'état « extra »). Les bits du terrain restent l'union de toutes les cités, pour les
  sauvegardes et l'affichage.
- Restent partagés, comme prévu : la circulation sur toutes les routes et la désirabilité.

### D-029 — Règles de partie : invasions IA et fin au score (provisoire)
- 2026-10-04 · **adoptée, provisoire** (M4.5, M4.6) · *à valider* par Alexandre (formule du score, durées)
- L'hôte choisit les règles dans le salon ; elles voyagent avec le message d'accueil (protocole v3) et dans le
  `.mpsav`. Les machines ne lisent jamais leurs réglages locaux pour simuler (D-016).
- **Invasions IA** (oui par défaut) : armées ennemies et soulèvements prévus par la carte. Non : aucun. César
  n'existe pas en multijoueur (D-026). Indigènes et animaux font partie de la carte et restent propres à la cité où
  ils se trouvent.
- **Fin de partie** : aucune (par défaut), ou au score après 5, 10 ou 20 ans de partie. Le score provisoire d'une cité
  est culture + prospérité + paix + population / 100. À la fin, la simulation s'arrête au même tick sur toutes les
  machines et un écran donne le classement ; en cas d'égalité, le joueur de plus petit numéro l'emporte. La
  conquête viendra avec la guerre (M9).

### D-030 — Cartes multijoueur : générées, et le format `.mpmap`
- 2026-10-04 · **adoptée, provisoire** (M6) · *à valider* par Alexandre (taille, disposition, ressources)
- Le salon propose deux sortes de cartes : une copie de la carte choisie par joueur (D-025), ou une **grande carte
  générée** (`mp/mapgen`) : 200 cases de côté à 2 joueurs, 260 au-delà. Lacs, forêts, rochers et prés sortent d'un
  bruit déterministe (même graine, même carte). La carte choisie ne donne que le climat, l'empire, l'année et les
  fonds.
- Un point d'arrivée par joueur, au milieu d'un bord (gauche, droite, haut, bas). Autour de chaque cité, de la
  terre dégagée, une forêt, des rochers, un étang et des prés : chacun peut tout bâtir, et c'est l'autorisation
  (D-020) qui décide ce qu'il exploite. Les invasions IA arrivent par les bords.
- Le format **`.mpmap`** est une partie de départ au format `.mpsav` : taille, terrain, points d'arrivée et
  autorisations y sont. Le salon les liste ; le nombre de joueurs est celui de la carte. Pas d'éditeur pour
  l'instant : le générateur et `simtool mapgen` (variable `MAPGEN_OUTPUT`) les produisent.

### D-031 — Menu principal : le multijoueur d'abord, la campagne gardée
- 2026-10-04 · **adoptée, provisoire** · *à valider* par Alexandre
- Le menu principal commence par « Multijoueur », puis le jeu libre (Mode Bâtisseur), le chargement, et seulement
  ensuite « Nouvelle carrière ». La campagne n'est pas retirée : l'invariant I2 la neutralise par le mode, et le
  jeu classique sert de référence aux tests de parité. La retirer du menu reste possible si Alexandre le souhaite.

### D-032 — Popups, sons et avertissements : seulement chez le joueur de la cité
- 2026-10-05 · **adoptée** (retours de TEST_1, T1.1)
- Chaque machine calcule toutes les cités. Les messages restaient bien propres à chaque cité, mais la fenêtre et le
  son d'un message, et les effets sonores (incendie, effondrement, séisme), sortaient aussi chez les autres joueurs :
  « Mars est en colère » pouvait concerner le voisin. `mp_session_is_other_players_city()` les coupe pendant le tour
  d'une autre cité. Les messages ne sont pas dans la somme de contrôle : aucun risque de désynchronisation.

### D-033 — Seulement des cartes multijoueur préparées (remplace l'option « copie de carte » de D-025 et précise D-030)
- 2026-10-05 · **adoptée** (Alexandre, QUESTIONS_1 Q13 à Q17)
- Le salon ne propose plus que des cartes multijoueur : pour l'instant **une carte à 2 joueurs et une à 4**,
  produites par Claude et relues par Alexandre. À 3 joueurs, on joue sur la carte à 4 avec un emplacement vide.
- Chaque emplacement de départ offre **des prés cultivables** et **seulement les ressources que son joueur a le droit
  d'exploiter** (D-020), pour forcer le commerce. Toutes les zones constructibles sont reliées à la route principale.
- Le joueur qui reçoit le marbre est celui qui n'a ni eau ni commerce fluvial : c'est sa contrepartie.
- La génération aléatoire (D-030) reste un outil pour produire ces cartes ; elle reviendra dans le salon plus tard.

### D-034 — César, propriétaire neutre des routes et aqueducs de la carte (précise D-026)
- 2026-10-05 · **adoptée** (Alexandre, Q11, Q12, Q35 à Q40)
- Les routes et l'aqueduc posés par la carte appartiennent à **César**, un propriétaire neutre qui n'agit pas : il
  ne joue pas, ne demande rien (E8 tient toujours). Ses ouvrages sont dessinés en blanc, jamais à la couleur d'un
  joueur.
- Ses routes sont **indestructibles**. Son aqueduc l'est aussi jusqu'à la guerre (M9), puis les soldats des joueurs
  et les envahisseurs pourront le casser ; on peut construire des routes dessous.
- Un seul réseau d'aqueduc, alimenté dès le départ par un **réservoir indestructible**, passe près des joueurs
  éloignés de l'eau, et seulement d'eux. Le casser pour priver un voisin d'eau est une arme voulue.

### D-035 — Réservoirs qui se vident et se remplissent (multijoueur seulement)
- 2026-10-05 · **adoptée** (Alexandre, Q41 à Q43) ; durées **à régler** en jeu
- En multijoueur, **tous** les réservoirs ont un niveau. Coupé de sa source, un réservoir continue d'alimenter
  fontaines et bains jusqu'à être vide, en environ 5 minutes à vitesse normale ; il se remplit en environ 1 minute.
  Les durées sont comptées en temps de jeu (ticks), jamais en temps réel (I3). Le mode classique ne change pas.

### D-036 — Territoires : la zone constructible suit la ville vivante (remplace « construire partout » de D-018)
- 2026-10-05 · **adoptée** (Alexandre, Q18 à Q25 et précision du 2026-10-05) ; rayon et délais **à régler**
- But : empêcher qu'on aille construire une tour à côté d'un adversaire (« rush »), **sans jamais brider** la
  croissance d'une ville ou d'un nouveau quartier.
- On construit à **20 cases** au plus de l'un de ses bâtiments « installés » (maison habitée, bâtiment avec des
  employés) ou d'une mission. Routes, murs et aqueducs ne donnent pas de zone, statues, jardins et bâtiments vides
  non plus : une chaîne de bâtiments bon marché ne fait pas avancer. Les blocs de 8 bâtiments envisagés dans
  TEST_1 sont abandonnés : ils permettaient de s'étendre sans fin.
- Routes, aqueducs (et murs) se construisent partout, pour relier les cités.
- Une case revendiquée appartient au premier joueur qui l'a obtenue : on ne construit jamais dans la zone d'un
  autre. Sa ville étant entourée de 20 cases à lui, aucune tour adverse ne peut s'en approcher.
- Si une mission est détruite et que des bâtiments sortent de toute zone, un message prévient et ils s'effondrent
  après un délai de grâce (environ 3 mois de jeu) s'ils ne sont pas de nouveau couverts.
- La zone est tracée par une ligne de la couleur du joueur (remplace la teinte forte des bâtiments, voir D-039).

### D-037 — La mission et le missionnaire
- 2026-10-05 · **adoptée** (Alexandre, Q22 à Q28, Q45)
- En multijoueur, la mission ne sert plus aux indigènes (il n'y en a plus) : elle **prend possession d'une zone** et
  **forme des missionnaires**. La première mission est gratuite, les suivantes coûtent du **marbre** (achetable aussi
  à l'empire). Elle n'a pas d'autre rôle et peut être détruite.
- Chaque joueur commence avec un **missionnaire**, déplacé comme une légion, qui suit les règles de terrain des
  soldats. On construit une mission à 20 cases au plus d'un missionnaire. Il peut mourir ; une mission en forme un
  nouveau, cher.
- Ce n'est pas une option : c'est la nouvelle façon de s'installer.

### D-038 — Brouillard de guerre (option du salon)
- 2026-10-05 · **adoptée** (Alexandre, Q29 à Q32, Q45)
- Désactivable dans le salon. Tout ce qui appartient au joueur (bâtiments, personnages, routes, missionnaire)
  éclaire 20 cases ; une zone découverte reste visible pour le terrain, sans ce qui bouge. La minicarte respecte le
  brouillard et les scores adverses sont cachés.
- Purement local à l'affichage : chaque machine calcule tout, un tricheur pourrait voir à travers, accepté entre
  amis.

### D-039 — Apparence des joueurs : teinte légère, variantes générées au lancement
- 2026-10-05 · **adoptée** (Alexandre, Q33 à Q35)
- Les bâtiments adverses gardent une teinte **légère** ; à terme, des variantes de couleur (toits de brique,
  d'ardoise…). César en blanc.
- Toute image modifiée est **calculée au lancement** à partir des données du joueur : une image dérivée de Caesar III
  ne va jamais dans git (I4). Des dessins entièrement nouveaux d'Alexandre pourront s'y ajouter.

### D-040 — Répartition des matériaux sur les cartes préparées (précise D-033 et remplace D-020 pour elles)
- 2026-10-05 · **remplacée par D-041**
- L'argile demande de l'eau à côté, le marbre et le fer des rochers, le bois des arbres, toutes les fermes des
  prés. Le joueur du marbre n'a ni eau ni argile.
- Carte à 2 (ouest, est) : J1 fer, argile, vignes, avec un lac ; J2 marbre, bois, olives, sans eau.
- Carte à 4 (ouest, est, nord, sud) : J1 fer et olives, sans eau ; J2 marbre et vignes, sans eau ; J3 argile et
  bois, avec un lac ; J4 bois et olives, avec un lac. À 3 joueurs, le sud reste libre.
- Blé, légumes, fruits et porcs pour tous (prés). Loin de toutes les cités, des bois ; nulle part ailleurs de
  rocher ni d'eau, sauf le lac central réservé à César.

### D-041 — Matériaux des cartes préparées, rivière du lac central (remplace D-040)
- 2026-10-05 · **adoptée** (Alexandre) ; répartition du reste : équitable, provisoire
- Le joueur des rochers a **le fer et le marbre**, sans eau ; un autre a **le bois et l'argile** (l'argile demande
  de l'eau), avec un lac ; olives et vignes sont partagées.
  - Carte à 2 (ouest, est) : J1 fer, marbre, olives ; J2 bois, argile, vignes, lac.
  - Carte à 4 (ouest, est, nord, sud) : J1 fer, marbre ; J2 bois, argile, lac ; J3 olives, bois ; J4 vignes,
    argile, lac. À 3 joueurs, le sud reste libre. J1 et J3 auront l'aqueduc de César.
- Le lac central est relié au coin nord-est de la carte par une **rivière** (entrée et sortie des navires) : les
  navires de l'empire remontent jusqu'aux docks bâtis sur ses rives. Ni le lac ni la rivière ne coupent une route
  principale.

### D-042 — Voies commerciales vers l'extérieur sur les cartes préparées (précise D-041)
- 2026-10-05 · **adoptée** (Alexandre : « il faut des voies maritimes et des voies terrestres vers l'extérieur »)
- Voie terrestre : le point d'arrivée de chaque joueur, au bord de la carte, relié par la route de César ; les
  caravanes de l'empire y entrent.
- Voie maritime : le lac de chaque joueur qui en a un rejoint le bord le plus proche par une rivière, et les navires
  de son empire entrent par là ; les autres joueurs ont la rivière du lac central (point d'entrée des navires propre
  à chaque cité). Le joueur des rochers n'a pas de lac à lui : c'est la contrepartie voulue.
- La carte modèle choisie dans le salon doit avoir un empire qui commerce par terre **et** par mer ; sinon la partie
  est refusée avec un message (Corinthus, Hierosolyma, Lugdunum, Mediolanum et Toletum ne conviennent pas).

### D-043 — Prix du commerce : un prix par joueur acheteur, l'empire plus cher (précise D-019)
- 2026-10-05 · **adoptée** (Alexandre, questions du 2026-10-05)
- **Empire** : ses prix restent ceux du jeu (et leurs variations), mais en multijoueur, **acheter à l'empire coûte
  50 % de plus** (transport, taxes). Vendre à l'empire rapporte son prix habituel. Le commerce entre joueurs est donc
  normalement plus avantageux.
- **Entre joueurs** : chaque vendeur fixe, **pour chaque ressource et chaque joueur acheteur**, son prix de vente
  (par défaut le prix d'achat de l'empire sans majoration). Il peut vendre le marbre 150 à J2 et 250 à J3.
- Quand un vendeur change le prix d'une ressource que lui achète un joueur, celui-ci reçoit un **message** (« J2
  vend désormais le marbre 180 au lieu de 150 ») ; ses achats continuent au nouveau prix, il peut les arrêter.
- Le reste de D-019 tient : il faut une route entre les deux cités et l'accord des deux, les caravanes ne suivent que
  les routes, elles pourront être interceptées (M9).
- Les réglages « importer / exporter » de l'empire ne s'appliquent pas entre joueurs (le jeu les refuse quand
  l'empire n'achète ou ne vend pas la ressource) : l'acheteur dit à qui il achète quoi ; le vendeur vend au-delà de
  son seuil d'exportation, sauf ce qu'il met en réserve. Chaque mois, une caravane d'au plus 8 chargements par
  route, dans la limite de ce que l'acheteur peut payer ; il paie à la livraison, ce qui ne trouve pas de place
  repart chez le vendeur.

### D-044 — Une seule carte, en forêt, de l'eau pour tous ; partie seule sur la carte (précise D-033, D-041, D-042)
- 2026-10-05 · **adoptée** (Alexandre, deuxième essai : « je voudrais n'avoir qu'une carte pour 1 ou 2 joueurs ou 3
  à 4, dans la forêt, pas dans le désert » ; « il n'y a pas assez d'eau » ; « pas de route commerciale sur l'eau »)
- **Plus de choix de carte** dans le salon : « Nouvelle partie (forêt) » joue la carte préparée pour 2 (1 ou 2
  joueurs) ou pour 4 (3 ou 4 joueurs) ; la liste ne propose sinon que les parties multijoueur à reprendre. L'empire,
  l'année et les fonds viennent de la première carte trouvée dans les données parmi Lindum, Londinium, Valentia,
  Tarraco, Caesarea, Cyrene et Carthago (empire qui commerce par terre et par mer, Bretagne d'abord).
- **Climat du nord** (forêts) quelle que soit la carte modèle. Loin des cités (au-delà de 30 cases), forêts avec
  clairières et étangs (*plus d'étangs depuis D-055*) ; près d'elles, toujours rien qu'un joueur puisse exploiter et
  pas un autre.
- **Chaque joueur a son lac** (rayon 12), du côté de son point d'arrivée et loin du lac central, relié au bord par
  une rivière de 7 cases (les ponts la franchissent) : docks, navires de son empire, pêche (un point de pêche par lac
  et au lac central). Le joueur des rochers a donc aussi de l'eau (remplace « sans eau » de D-041/D-042) ; il garde
  l'aqueduc de César, comme J3 sur la carte à 4.
- Environ 8 % d'eau et 35 à 40 % de forêt (14 à 18 % depuis D-052). Une terre que l'eau et les rochers isoleraient
  de la route devient un étang (*un rocher depuis D-055*).
- **Partie seule** (1 joueur dans le salon) : elle se joue sur la carte pour 2 avec les règles de la carte (zones,
  missionnaire, eau de César, réservoirs à niveau, brouillard). `game_rules_multiplayer_map()` : plusieurs cités, ou
  une carte préparée jouée seul.
- **Départ** : la vue est sur le missionnaire, et un objectif reste affiché tant que le joueur n'a pas de mission
  (« construisez votre première mission, menu Éducation, gratuite, à moins de 20 cases du missionnaire »). Le
  brouillard est calculé dès la création de la carte. *Remplacé par D-045 : la mission est construite d'office.*

### D-045 — La mission de départ est construite d'office ; plein écran dans un Space (précise D-037, D-044)
- 2026-10-05 · **adoptée** (Alexandre, troisième essai : « je veux que la mission soit construite au démarrage, le
  joueur n'a pas le choix »)
- Chaque joueur commence avec **sa mission déjà bâtie**, au bord de la route de César qui traverse l'emplacement de
  sa cité (accès à la route), et la zone de 20 cases qu'elle donne. La vue de départ est sur elle. Son missionnaire
  sert à fonder les missions suivantes, qui coûtent du marbre puisqu'il possède déjà une zone. L'objectif « fondez
  votre mission » ne s'affiche plus que si un joueur perd toute sa zone (sa mission suivante est alors gratuite).
- **Plein écran sur macOS** : retour au plein écran natif de macOS (un Space), qui place la fenêtre correctement
  sur un écran à encoche. Le plein écran « sans Space » de T1.3 ne passait au-dessus de la barre de menus et du Dock
  que si la fenêtre avait le focus quand le jeu capturait la souris (pas au lancement) et laissait le bas de l'image
  hors de l'écran. Dans le Space, le délégué de la fenêtre de SDL 3 demande désormais à macOS de masquer
  entièrement barre de menus et Dock (ce que faisait SDL 2) ; l'indication `SDL_VIDEO_MAC_FULLSCREEN_MENU_VISIBILITY`
  vaut 0.

### D-046 — Position de la souris relue auprès du système sur macOS (précise D-045)
- 2026-10-05 · **adoptée** (Alexandre, troisième essai : en plein écran, « la barre de menu de Caesar empêche le
  déplacement » vers le haut)
- Le défilement par les bords repose sur la position de la souris reçue par événements. Sur macOS ils ne suffisent
  pas : (1) sur un écran à encoche, le plein écran natif place la fenêtre sous la bande que macOS garde noire ; le
  curseur y entre, sort de la fenêtre et plus aucun événement n'arrive, la dernière position connue (souvent sur la
  barre de menu du jeu) reste figée ; (2) macOS 26 et suivants livrent des positions périmées près du haut de
  l'écran (SDL #15967, contourné pour les événements dans SDL 3.4.12+ mais pas pour l'absence d'événements).
- Sur macOS, à chaque image, la position du curseur est demandée au système (`SDL_GetGlobalMouseState` moins la
  position de la fenêtre, convertie en coordonnées du jeu). En plein écran elle est ramenée dans la fenêtre : un
  curseur poussé au-dessus ou en dehors fait défiler comme s'il touchait le bord. En fenêtre, elle corrige la
  position tant que le curseur est sur la fenêtre. Un écart d'un pixel est ignoré (double-clic). Jamais pendant
  l'automatisation (pilote factice), ni en mode relatif (déplacement de la carte au clic droit), ni au toucher.
- Le mode compatibilité de l'encoche (`NSPrefersDisplaySafeAreaCompatibilityMode`, défaut) est conservé : la barre
  de menu du jeu resterait sinon en partie sous l'encoche.
- **Complément (même jour, le blocage persistait)** : en plein écran, le jeu capturait la souris (`SDL_SetWindowGrab`,
  pour la garder sur l'écran du jeu quand il y en a plusieurs). SDL 3 réalise cette capture par
  `mouseConfinementRect`, calculé depuis `contentLayoutRect` de la fenêtre ; dans un Space ce rectangle exclut la
  barre de titre cachée (28 points), si bien que le curseur ne pouvait plus monter dans les 28 points du haut de
  l'écran, exactement la hauteur de la barre de menu du jeu. Sur macOS la capture est retirée ; la position du
  curseur est lue directement sur la fenêtre Cocoa (`mouseLocationOutsideOfEventStream`, indépendante des
  événements et de la position de fenêtre que SDL croit connaître), avec `SDL_GetGlobalMouseState` en secours.
  Contrepartie : avec plusieurs écrans, un curseur parti sur l'autre écran fait défiler la carte tant qu'il y reste
  (comportement de Julius avant 2020). Sur macOS, le jeu écrit aussi `julius-log.txt` dans le dossier des données
  (pas pendant l'automatisation) ; près du haut de l'écran il y note chaque seconde la position vue par le jeu et
  par le système, pour vérifier la correction chez Alexandre.

### D-047 — Un grand bras de mer ; le joueur de la pierre sans eau (remplace l'eau de D-044)
- 2026-10-05 · **adoptée** (Alexandre : « un grand bras de mer qui traverse la carte ; en carte 2 joueurs, les 2
  joueurs doivent être sur la même rive, en 4 joueurs 2 de chaque rive. Ne mets pas d'eau hors aqueduc à celui qui a
  la pierre »)
- Chaque carte suit un **plan fixe** : villes, points d'arrivée (bords ouest et est), routes de César le long de la
  rangée de chaque ville, tracé de la mer. Le bras de mer (environ 25 cases, côtes irrégulières) va du bord ouest au
  bord est ; il n'atteint ni une ville ni une route, sauf celle qui le franchit sur le **pont de César** (pont pour
  navires, indestructible).
  - Carte pour 2 (200 cases) : mer au nord ; J1 (pierre) au sud-ouest, J2 (bois, argile, vignes) sur la côte au
    sud-est ; le pont mène à la rive nord, sauvage.
  - Carte pour 4 (260 cases) : mer au milieu ; ~~rive nord J1 (pierre, ouest) et J2 (bois, argile, côte) ; rive sud J3
    (olives, bois) et J4 (vignes, argile), tous deux sur la côte~~. Depuis D-062 : à l'ouest, un joueur des terres sur
    chaque rive (J1 au nord, J3 au sud), loin de la mer ; à l'est, un joueur sur chaque côte (J2 au nord, J4 au sud).
    À 3 joueurs, le sud-est (côte) reste libre.
- **Joueur de la pierre** : aucune eau à moins de 45 cases de sa ville, ni étang ; seul l'aqueduc de César, depuis
  un réservoir sur la côte, lui apporte l'eau. À 4, chacun des deux joueurs des terres a son réservoir de César, sur
  la côte de sa rive (D-062). Les autres joueurs n'ont plus de lac à eux ni d'aqueduc de César :
  leur rivage est dans leur zone de départ, les navires de leur empire arrivent par le bord de mer le plus proche, un
  point de pêche est au large de chacun.
- Les quais de pierre dessinés sur le rivage près du réservoir de César sont ceux du jeu d'origine (« rive
  fortifiée » près d'un bâtiment).

### D-048 — Plusieurs caravanes par route ; l'empire ne vend pas ce qu'un joueur vend moins cher (précise D-043)
- 2026-10-05 · **adoptée** (suite du plan du commerce ; Alexandre, pendant le travail : « tu ne dois pas passer sur
  les imports de l'empire si le joueur n'a plus de stock, c'est aussi dans le gameplay de pouvoir assécher le stock
  d'un adversaire »)
- Chaque mois, le vendeur envoie **une caravane par ressource** que l'acheteur lui achète (8 chargements au plus,
  une à la fois par ressource), dans la limite de ce que l'acheteur peut payer, partagée entre elles. Avant, une
  seule caravane par route : l'acheteur de marbre et de fer ne recevait que le marbre.
- **L'empire est la source de repli la plus chère** : en multijoueur, ses marchands ne vendent pas à une cité une
  ressource qu'elle achète à un joueur moins cher (prix majoré de l'empire comparé) par une route ouverte. **Même si
  ce joueur n'en a plus** : assécher le stock d'un rival est un levier de jeu. Pour revenir à l'empire, l'acheteur
  cesse d'acheter à ce joueur, ou la route se ferme, ou le vendeur monte son prix au niveau de l'empire.
  *Remplacé par D-060 et D-066 : l'empire vend toujours.*
- Chaque livraison est annoncée à l'acheteur ; la fenêtre « Joueurs » montre le prix de l'empire et le moins cher
  en vert.
- Le trajet d'une caravane suit la vitesse du jeu (environ 15 ticks par case) : d'un bout à l'autre de la carte
  pour 2, environ 45 jours. *À valider* : la distance compte-t-elle assez, ou faut-il des caravanes plus rapides ?

### D-049 — Alerte plein écran quand un vendeur change son prix (précise D-043)
- 2026-10-06 · **adoptée** (Alexandre : « je veux que lorsque un joueur change ses prix, une alerte plein écran
  s'affiche de changement de prix pour le joueur client »)
- Le petit avertissement en haut de la ville est remplacé par une fenêtre de la taille de l'écran d'origine
  (640 × 480), ville grisée derrière : vendeur, ressource, ancien et nouveau prix (rouge en hausse, vert en baisse),
  boutons « Voir le commerce » (conseiller au commerce sur l'onglet du vendeur, D-051) et « OK ».
- Seul l'acheteur est alerté, et seulement pour une ressource qu'il achète à ce vendeur (comme avant D-049).
- Une ligne par vendeur et ressource : plusieurs clics sur « + » ne donnent qu'une ligne, de l'ancien prix connu au
  dernier ; un prix revenu à l'ancien efface la ligne. La fenêtre ouverte se met à jour en direct.
- Comme les messages en popup de l'original, l'alerte attend que le joueur soit sur la vue de la ville, hors d'un
  tracé de construction. La partie ne s'arrête pas (la simulation tourne en réseau quelle que soit la fenêtre).
- La file des alertes est un état d'affichage de l'ordinateur de l'acheteur, ni sauvegardé ni lu par la simulation
  (I3 intact).

### D-050 — César revient comme arbitre de la partie (remplace E8, revoit D-026)
- 2026-10-06 · **adoptée** sur le principe (Alexandre : « César va ré-apparaître, c'est lui qui donnera la victoire à
  la cité la plus prospère […] César pourra être en colère contre des joueurs qui sont trop belliqueux entre eux, et
  envoyer son armée pour attaquer tous les joueurs de la carte »). Détails validés par D-053.
- La victoire par défaut devient le **jugement de César** : la première cité à un score de lauriers l'emporte
  (D-053). Ce mode remplace le score provisoire de M4.6. Les lauriers **s'additionnent** :
  - les lauriers de la cité, chaque mois, d'après cinq notes : prospérité, commerce, habitat, culture-éducation,
    grandeur ;
  - les lauriers de César, d'après les actions envers lui : dons depuis l'épargne (salaire limité par le rang),
    fêtes, troupes prêtées aux campagnes (batailles lointaines partagées), demandes, guerres justes, et en négatif
    les agressions.
- Révisée le jour même : un premier jet multipliait les lauriers par la faveur (×0,5 à ×1,5). Alexandre : « je
  trouve le multiplicateur de faveur trop compliqué, je préfèrerais juste les points ». La faveur d'origine reste
  donc figée et cachée (D-026), et chaque action vaut un nombre fixe de lauriers, plafonné par période.
- La **guerre** entre joueurs se déclare, avec un motif déterminé par le jeu : riposte, mandat de César ou sans
  motif. Une **jauge de colère commune** monte avec la durée et la puissance des guerres, et ses seuils mènent à
  l'avertissement, à l'ultimatum, puis à l'expédition punitive dans toutes les cités ; le fautif paie le plus.
- Restent neutralisés (D-026) : changement d'empereur, renvoi de la campagne. Restent actifs comme avant (D-026) :
  tribut annuel, prêt de secours, salaires de Rome.
- Jalons : M9 (César juge, en paix), M10 (la guerre sous l'œil de César), M11 (équilibrage). Les réglages chiffrés
  sont des valeurs de départ, mesurées et corrigées selon CESAR.md §10.

### D-051 — Une seule page de commerce : l'empire et les joueurs (précise D-043)
- 2026-10-06 · **adoptée** (Alexandre : « la vue commerciale n'est pas assez claire : un onglet joueur pas assez
  visible, les menus d'importation ne permettent pas de comprendre assez vite à qui on importe, et à quel prix » ;
  puis : « tu ne peux pas grouper sur la même page le commerce international et le commerce local, plutôt qu'ouvrir
  un onglet séparé »)
- Dès qu'il y a plusieurs joueurs, le conseiller au commerce affiche une seule page (`window/mp_trade`). Une ligne
  par ressource montre :
  - le stock ;
  - l'**empire** : son commerce (importe, exporte ou aucun, en un clic) et le prix qui s'applique (payé à
    l'import, reçu à l'export) ;
  - le **joueur choisi** par un onglet : mon prix (− et +), son prix, « J'achète », les chargements en route.
- Le fournisseur réel, l'empire ou ce joueur, est en vert. Un clic sur le nom de la ressource ouvre ses réglages
  d'origine (seuil d'export, mise en sommeil, stockage). La carte et les prix de l'empire restent en bas.
- Partie seule ou classique : page d'origine, inchangée. La fenêtre « Joueurs » séparée (M8.6) est supprimée.

### D-052 — On va partout ; le joueur des terres a le bois ; emplacements tirés au sort (précise D-044, D-047)
- 2026-10-06 · **adoptée** (Alexandre : « sur les maps, fais en sorte que nous puissions aller partout » ; « il ne
  faut pas que les forêts soient traversables, juste qu'il y ait moins de forêt » ; « donne aussi la compétence […]
  à celui qui commence dans les terres » ; « je parle des chantiers de bois » ; « fais un random entre les joueurs
  pour leur placement, le J1 ne pop pas forcément dans les terres ») ; le bois du joueur des terres **remplacé
  par D-058**
- **Moins de forêt** : 14 % de la carte pour 2, 17,5 % de celle pour 4, au lieu de 34 et 37 % (seuil du bruit des
  bois, `WOODS_THRESHOLD`). Les bois restent **infranchissables**, comme dans l'original. Un premier essai qui
  laissait le missionnaire traverser les bois a été abandonné à la demande d'Alexandre.
- **Aucune clairière enfermée** (`open_shut_in_clearings`) :
  - une clairière que les bois coupent de la route et qui fait moins de 16 cases devient du bois ;
  - les autres reçoivent un passage, ouvert par les arbres les moins nombreux.
  Toutes les terres où l'on marche sont atteignables depuis la route (test `mp_prepared_map_reachable_*` : 100 %).
- Le **joueur des terres** (celui de la pierre et de l'aqueduc de César) exploite aussi le **bois**, avec des bois
  près de sa cité. **Remplacé par D-058** : le bois va aux seuls côtiers.
- **Emplacements tirés au sort** entre les joueurs présents, par l'hôte, avec le nombre que le salon tire à chaque
  partie. La carte part ensuite chez les clients, comme avant. À 3 joueurs, le sud-est reste libre ; seul, on reste
  dans les terres, pour que quelqu'un ait le marbre des missions. Les tests gardent l'ordre du plan (graine 0).

### D-053 — Réponses d'Alexandre au plan de César (précise D-050)
- 2026-10-06 · **adoptée** (réponses d'Alexandre aux sept points de CESAR.md §13)
- Confirmés tels quels : lauriers **cumulés** mois après mois ; valeurs des lauriers du plan (réglages de départ,
  ajustés en M11) ; **classement public** des lauriers.
- **Victoire au score** (« je préfèrerais une victoire au score, celui à tant de lauriers ») : la première cité qui
  atteint le score du salon (500, 1 000, 1 500 ou 2 000 lauriers ; 1 000 par défaut) gagne, et la partie s'arrête.
  Plus de durée fixe ni de consulat anticipé. Les rangs de l'original vont par dixième du score ; le dernier,
  César, est la victoire.
- **Deux formes de guerre** (« soit tu fais une guerre brutale et c'est immédiat, par contre tu ne fais pas plaisir
  à César, soit tu fais une guerre propre, et tu as un préavis de 3 mois ») :
  - **guerre honorable** : les combats commencent 3 mois après la déclaration ;
  - **guerre brutale** : tout de suite, mais −15 lauriers de plus et une colère comptée ×1,5.
  La forme s'ajoute au motif (riposte, mandat, sans motif), que le jeu détermine toujours.
- **Demandes de César pour toute la province** (« chacun participe, et la faveur est accordée en fonction de ce que
  chacun donne. À ce moment-là, tout est permis, et les frappes commerciales sur ces produits ne sont pas
  pénalisées par César ») :
  - une cagnotte de 10 lauriers par joueur, partagée selon les chargements envoyés ;
  - pendant la demande, intercepter les caravanes de la ressource demandée n'ajoute rien à la colère.
    L'interception demande toujours une guerre déclarée : c'est une interprétation, **à confirmer** par Alexandre.
- **Seconde expédition punitive fatale** : option du salon, désactivée par défaut. Sans elle, la seconde expédition
  frappe comme la première.

### D-054 — Dépôt GitHub public et versions Linux et Windows (M5.6)
- 2026-10-06 · **adoptée** (Alexandre : « est-ce que ça serait compliqué de faire une version Linux Wayland
  Fedora 44 ? », puis « j'ai fait un fork de Julius pour être plus lisible ») ; la branche `multiplayer` est
  devenue `master` (D-059)
- Dépôt : `github.com/haris44/julius-multiplayer`, un fork **public** de Julius. Le code part dans la branche
  `multiplayer` (remote `github`) ; `origin` reste le Julius d'origine, pour en récupérer les correctifs.
- Aucune donnée du jeu dans le dépôt, ni dans son historique (vérifié avant le premier envoi). Les sauvegardes de
  `test/data` viennent du dépôt public de Julius.
- Auteur des commits : l'adresse privée GitHub d'Alexandre (`…@users.noreply.github.com`), pas son adresse
  professionnelle. Les 88 commits déjà faits ont été réécrits avant l'envoi (Alexandre l'a choisi) : leurs anciens
  numéros, cités dans le JOURNAL, ne pointent plus vers rien.
- Compilation automatique : `.github/workflows/multiplayer.yml`, à chaque envoi sur `multiplayer`. Tous les tests
  sous Linux et sous Windows (MinGW 64 bits), puis une AppImage Linux et un dossier Windows, avec le LISEZMOI et
  le code source. Depuis le même jour, aussi sous macOS (puce Apple) : tous les tests, puis le DMG de
  `tools/package-mac.sh`, qui refuse toute image contenant un fichier du jeu original. Les fichiers de compilation de Julius (`main.yml`, `codeql.yml`) restent dans le dépôt, mais sont
  désactivés sur le fork, pour garder les fusions avec Julius simples.
- Linux : l'AppImage tourne sur toute distribution récente, sous Wayland comme sous X11. Le pare-feu de Fedora
  doit laisser passer les ports 27400 (TCP, la partie) et 27401 (UDP, le salon) : c'est dans le LISEZMOI.

### D-055 — Plus d'étangs : l'aqueduc de César, seule eau du joueur des terres (précise D-034, D-044, D-047)
- 2026-10-06 · **adoptée** (Alexandre : « retire les points d'eau, il faut que le mécanisme d'assèchement via
  l'aqueduc fonctionne »)
- **Plus aucun étang** sur les cartes préparées : la seule eau est le bras de mer. Les forêts lointaines en avaient
  (800 cases sur la carte pour 2, aucune à moins de 60 cases du joueur des terres) : en y fondant une mission, ce
  joueur pouvait y bâtir un réservoir et se passer de César, ce qui rendait vaine la coupure de son aqueduc (D-034).
- Une terre que la mer et les rochers isoleraient de la route devient **rocher**, plus jamais un étang (le cas ne se
  présente pas avec la graine des cartes).
- Les bois prennent la place des étangs : seuil du bruit des bois 168 au lieu de 167, pour garder 14 et 17,5 % de
  forêt (D-052).
- Le mécanisme est vérifié sur les vraies cartes, seul, à 2 et à 4 : un réservoir au bout de l'aqueduc de César se
  remplit ; l'aqueduc coupé, il sert encore environ 270 jours (D-035), puis s'assèche avec la portée de ses
  fontaines ; réparé, il se remplit de nouveau. Aujourd'hui personne ne peut casser l'aqueduc de César : ce sera
  l'affaire des soldats en guerre (M10).
- **À valider** : le joueur des terres peut encore aller chercher l'eau de la mer, en fondant une mission sur la
  côte (4 marbres), en y bâtissant un réservoir et en tirant son propre aqueduc (environ 80 cases). C'est loin et
  cher, et cet aqueduc-là se coupe aussi en guerre : je le laisse. Si Alexandre veut fermer cette porte : interdire
  le bord de mer aux réservoirs du joueur des terres.

### D-056 — Défilement par les bords sous Linux : la position du curseur demandée au serveur X (précise D-046)
- 2026-10-06 · **adoptée**, **à vérifier** chez Alexandre (« sur la version Linux (et Linux uniquement), le
  déplacement sur les bords d'écran est au pixel près »)
- Cause non reproduite : pas d'écran Linux ici, et la compilation automatique n'en a pas. SDL 2 choisit X11 avant
  Wayland, donc sous Fedora l'AppImage passe par XWayland. Pistes : le curseur sort de la fenêtre avant d'atteindre
  la bande de 5 pixels où la carte défile (plus aucun événement ne dit alors où il est), ou il s'arrête sur des
  bandes noires autour de l'image (écran d'une autre forme que le jeu), hors de l'image.
- Même remède que sur macOS (D-046), **en plein écran seulement** : à chaque image, la position du curseur est
  demandée au serveur X et ramenée au bord de l'image ; un curseur hors de la fenêtre ou sur une bande noire fait
  défiler comme s'il touchait le bord. Jamais en fenêtre, pendant l'automatisation, ni en mode relatif (glisser au
  clic droit). En Wayland natif (`SDL_VIDEODRIVER=wayland`), le système ne donne pas cette position : rien ne change.
- Partout, une position au-dessus d'une bande noire est ramenée au bord de l'image.
- Diagnostic : sous Linux aussi, le jeu écrit `julius-log.txt` dans le dossier de Caesar III (taille de la fenêtre,
  en pixels, échelle ; près d'un bord, au plus une fois par seconde, la position vue par le jeu et par le système).
  Si le défaut persiste, ce fichier dira pourquoi.
- Contrepartie, comme sur macOS : si le curseur peut quitter l'écran du jeu (plusieurs écrans), la carte défile tant
  qu'il est sur l'autre écran.

### D-057 — César visible d'abord, sans ses mécaniques complexes (M9.2 provisoire, M9.5, M9.6)
- 2026-10-06 · **adoptée** (Alexandre : « lance l'implémentation de la partie visuelle de César, sans les
  mécaniques complexes »)
- Fait avant le reste de M9 :
  - les notes et les lauriers mensuels ;
  - les rangs et la victoire au score ;
  - l'interface : bandeau, conseiller impérial, lettres de César, salon, écran de fin.
  Restent pour plus tard : campagnes, colère, guerres, demandes, dons et fêtes en lauriers, et la note de
  commerce en flux nets.
- **Notes provisoires** : la prospérité et la culture d'origine ; l'habitat et la grandeur comme dans le plan. Le
  commerce prend les exportations (vers l'empire et les joueurs) des douze derniers mois, en deniers, avec 50 à
  4 000 Dn. Ce sont des prix négociés, donc manipulables entre complices : la version au prix de référence et en
  flux nets viendra avec la calibration (M9.2).
- **Conseiller impérial rouvert en multijoueur**, mais en lecture seule : dons, salaire et épargne changent la cité
  hors des commandes réseau. Leurs boutons restent cachés en multijoueur jusqu'à M9.3.
- **Lauriers publics dans le bandeau**, même avec le brouillard de guerre (D-053, classement public). Le score
  provisoire par années reste caché par le brouillard, comme avant (D-038).
- **Lettres de César** : la lettre d'accueil s'affiche quand une partie commence sans lauriers (pas à la reprise
  d'une sauvegarde) ; une lettre à chaque nouveau rang. Elles sont un état d'affichage de l'ordinateur du joueur :
  ni sauvegardées, ni lues par la simulation.
- Rangs : ceux de l'original (textes du jeu), un par dixième du score ; sans score (partie sans fin), sur la base
  de 1 000 lauriers.

### D-058 — Trois troupes, trois coûts : bois des côtiers pour les javeliniers (précise D-020, revoit D-052)
- 2026-10-06 · **adoptée** (Alexandre : « cavaliers avec rien : unité rapide et 1,5x moins forte que le légionnaire,
  javeliniers 1.1x moins fort que le légionnaire (+ à distance), et légionnaire (le + fort nécessitant des armes),
  avec l'extraction de bois uniquement pour les côtiers » ; puis « oui écris le ») ; chiffres **à régler** (MG.5,
  M11)
- **Constat** : dans l'original, seuls les légionnaires coûtent une ressource, un chargement d'armes par recrue
  (`building/barracks.c`). Javeliniers et cavaliers sont gratuits mais faibles. Les légions dominent surtout par
  leurs bonus de formation, qui n'existent que pour elles.
- **Mesure de la force** : points de vie × attaque, qui décide d'un duel au corps à corps (les défenses de base des
  soldats sont à 0 ; un coup retire l'attaque moins la défense, `figure/combat.c`).
- **Cavaliers** : gratuits, les plus rapides (vitesse 3, contre 2 aux javeliniers et 1 aux légionnaires).
  **Inchangés** : 120 × 8 contre 150 × 10, soit 1,56 fois moins forts que le légionnaire, ce que demande Alexandre.
- **Javeliniers** : un chargement de **bois** par recrue, livré à la caserne comme les armes, dans un stock séparé
  (5 au plus), seulement si la cité a une légion de javeliniers. En multijoueur seulement, **136 points de vie et
  10 d'attaque** au lieu de 80 et 4, soit 1,1 fois moins forts ; tir (portée 10, 4 dégâts) et vitesse inchangés.
- **Légionnaires** : inchangés, un chargement d'armes par recrue. Leurs **bonus de formation restent** : +4 en
  attaque arrêtés en formation ; en défense +7 en tortue, +4 en double ligne, −4 pris de flanc ; un javelot ne fait
  que 1 dégât à une tortue arrêtée. Rôles voulus : la légion tient le terrain mais marche lentement ; les
  javeliniers gagnent contre une légion en marche ou prise de flanc ; les cavaliers font les raids et chassent les
  caravanes (M10.3).
- **Bois aux seuls côtiers** (cartes préparées, `mp/mapgen.c`) :
  - à 2 joueurs, le joueur des terres garde fer, marbre et olives ; il perd le bois et les bois plantés près de sa
    cité pour lui (D-052). Il achète du bois ou des meubles pour ses maisons ;
  - à 4 joueurs, les trois côtiers ont le bois, y compris celui du sud-est (vignes, argile) : une cité à légions
    face à trois cités à javeliniers. Le fer reste au seul joueur des terres.
- En classique, rien ne change : javeliniers gratuits, table des figures d'origine (parité).
- **Risque** à mesurer après M10.2 : des javeliniers presque aussi forts, deux fois plus rapides et qui tirent
  pourraient devenir la meilleure troupe en terrain découvert (batailles simulées : légion arrêtée, en marche, prise
  de flanc).
- **Pistes écartées** :
  - le blé comme fourrage des cavaliers : les fermes demandent le même pré que l'élevage de porcs, sans asymétrie
    entre joueurs ;
  - les porcs changés en chevaux : la viande est une nourriture des maisons et partage sa place avec le poisson des
    quais ; aucune image de chevaux pour l'élevage, l'icône, la charrette ou l'entrepôt ;
  - de nouvelles marchandises (lances, chevaux) : Julius ne charge aucune image nouvelle (il faudrait le chargeur
    d'Augustus) et le moteur est limité à 16 ressources (`RESOURCE_MAX`, format des sauvegardes).

### D-059 — `master` est le projet multijoueur (précise D-054)
- 2026-10-06 · **adoptée** (Alexandre : « On va mettre sur master, on s'en fou du projet de base le but est le
  développement du multi uniquement »)
- La branche `multiplayer` devient `master`, dans le dépôt local comme sur GitHub, où `master` est la branche
  affichée par défaut : la page du dépôt présente le projet (README). `github/multiplayer` est supprimée.
- La compilation automatique (`multiplayer.yml` : Linux, Windows, macOS) se déclenche sur `master` et sur les
  étiquettes `mp-*`. On envoie par `git push github master` ; les worktrees partent de `master`.
- Julius reste la source des correctifs : remote `origin`, `origin/master` et tag `upstream-base`
  (`git fetch origin && git log upstream-base..origin/master`). L'ancienne branche locale `master`, simple copie
  de `origin/master` sans commit propre, est supprimée.

### D-060 — Prix fixes de l'empire et portorium (revoit D-043 et D-048) — à préciser
- 2026-10-06 · **adoptée sur le principe** (Alexandre : « plutôt que de faire un prix de vente et d'achat
  différent, pars sur des prix fixes empire avec une taxe douanière à l'entrée et à la sortie : c'est ça qui fera
  l'augmentation de prix » ; avant : « le but est de pouvoir toujours acheter à l'étranger, par contre l'étranger
  augmente fortement ses prix si un joueur stoppe le commerce »)
- Un seul **prix de l'empire** par ressource, au lieu du prix d'achat et du prix de vente de l'original.
- Le **portorium**, la douane romaine des ports et des frontières de province (en Gaule, le « quarantième », 2,5 %),
  s'applique à ce qui entre dans la province (achat à l'empire) et à ce qui en sort (vente à l'empire) :
  - on paie le prix plus le portorium ;
  - on reçoit le prix moins le portorium.
- **L'empire vend toujours** (fin de la règle D-048 qui le faisait s'effacer devant un joueur moins cher).
- **La hausse des prix quand un joueur arrête le commerce** passe par le portorium, qui augmente.
- **Prix de départ** (Alexandre : « le prix de départ est le prix du jeu pour tout le monde, l'international a les
  frais de douane, et ensuite les joueurs le font évoluer comme ils le souhaitent ») :
  - le prix fixe de l'empire est le prix du jeu, et c'est aussi le prix de départ de chaque joueur ;
  - je prends le prix d'achat de base de l'original, déjà le prix par défaut des joueurs (D-043) : marbre 200.
    *À valider* ;
  - seul le commerce avec l'empire paie le portorium. Entre joueurs, pas de douane : le commerce ne quitte pas la
    province ;
  - chaque joueur fait ensuite évoluer ses prix comme il le souhaite, joueur par joueur (D-043).
- **Taux de départ : 50 %** (Alexandre : « un joueur local peut commercer avec l'étranger au prix de Rome + taxes
  douanières (50 %) » ; confirmé : « 50 % de droits de douane oui »). Marbre : prix de Rome 200 ; acheté à
  l'empire 300 ; vendu à l'empire 100 (l'original l'achetait 140).
- **À préciser** (je tranche provisoirement s'il le faut, en le notant) : qui voit le portorium augmenter, de
  combien et combien de temps, et ce qu'est « arrêter le commerce » (T4.3).
- Classique : inchangé.

### D-061 — Le commerce avec l'étranger n'est pas partagé ; le menu de construction suit l'emplacement (précise D-020, D-043, D-060)
- 2026-10-06 · **adoptée** (Alexandre, après le bug T4.13 : « on ne partage pas le commerce qu'on fait avec
  l'étranger entre joueurs locaux. Un joueur local peut commercer avec l'étranger au prix de Rome + taxes douanières
  (50 %). Il pourra aussi commercer avec un autre joueur local, avec le prix de Rome par défaut, modifiable par le
  vendeur. En revanche, sur les ressources disponibles dans les menus de construction de chacun des joueurs :
  argile et bois pour les côtiers, marbre et fer pour les terriens »)
- **Chaque joueur a son propre commerce avec l'étranger** : ses routes ouvertes, ses achats et ses ventes. Rien
  n'en passe à un autre joueur, ni les routes, ni les prix, ni les matières qu'une route rend disponibles. Il paie
  le prix de Rome plus le portorium (D-060).
- **Entre joueurs** : le prix de Rome par défaut, que le vendeur modifie comme il veut, joueur par joueur (D-043).
- **Le menu de construction d'un joueur ne montre que les matières premières de son emplacement** : argile et bois
  pour les joueurs de la côte, marbre et fer pour le joueur des terres. Il ne doit jamais montrer celles d'un autre
  joueur, quoi que fasse celui-ci avec l'étranger. Le menu est recalculé dans la seule cité du joueur local.
- *À valider* :
  - ~~les cartes actuelles donnent aussi au joueur des terres le bois~~ tranché : plus de bois dans les terres
    (D-062) ;
  - l'original permet l'atelier d'une matière qu'une route de l'empire fournit (D-020) ; cette règle reste, dans la
    cité de chaque joueur. Un achat à un autre joueur n'ouvre pas d'atelier tant que ce n'est pas tranché.
- Classique : inchangé.

### D-062 — Deux joueurs des terres et deux de la côte à 4 ; plus de bois dans les terres (revoit D-047, T3.5)
- 2026-10-06 · **adoptée** (Alexandre : « ne mets plus le bois au joueur des terres. Par ailleurs, il y a deux
  joueurs dans les terres, et deux joueurs sur la côte quand tu es 4 »)
- **Le joueur des terres n'a plus le bois** : ses matières sont le fer et le marbre (D-061). Fin de T3.5 (« je
  parle des chantiers de bois »), qui lui donnait les chantiers.
- **Carte à 4 : deux joueurs des terres et deux de la côte**, au lieu d'un seul dans les terres (D-047). Les deux
  joueurs des terres vivent de l'aqueduc de César, qui doit atteindre les deux ; à 3 joueurs, l'emplacement laissé
  libre est l'un des deux de la côte, pour qu'il y ait toujours un joueur des terres et un de la côte.
- *Répartition* (la nourriture validée par Alexandre, T4.15 ; le reste à valider) :
  - terres : fer et marbre pour les deux ; les olives à l'un, les vignes à l'autre ; les porcs ;
  - côte : bois et argile pour les deux ; la pêche ; les fruits ;
  - blé et légumes pour tous.
  À 2 joueurs : terres = fer, marbre, olives, porcs ; côte = bois, argile, vignes, pêche, fruits.
- *Fait* (T4.15, D-065) : cartes refaites selon ce plan ; tests `mp_prepared_map_placement` et
  `mp_prepared_map_*_players`.
- Classique : inchangé.

### D-063 — Les réglages du salon sont ceux de l'hôte, jusqu'au lancement ; « facile » par défaut (T4.4, T4.10, T4.11)
- 2026-10-06 · **adoptée provisoirement** (essai du soir d'Alexandre)
- **L'hôte règle la partie jusqu'au clic sur « Lancer la partie »** : après « Héberger », les réglages restent
  modifiables (plutôt que figés et grisés). Chaque changement part aussitôt dans la partie et chez les joueurs déjà
  là ; le lancement relit les réglages du moment.
- **Les joueurs qui rejoignent voient les réglages de l'hôte**, tenus à jour, sans pouvoir les changer. Un message
  du salon de l'hôte vers les clients (protocole 13).
- **Le salon propose « facile »** (T4.4). Les règles par défaut du multijoueur, celles des tests et de la ligne de
  commande (`--mp-host`), restent en difficile, pour ne pas changer les tests.
- **En partie, la difficulté est celle du salon pour tous** : le menu Options n'a plus l'entrée Difficulté en
  multijoueur, et la ville affiche la difficulté de la partie.
- **« Invasions IA : non » coupe toutes les attaques de l'IA** : armées et soulèvements locaux du scénario, et le
  soulèvement envoyé par Mars en colère. Mars en colère, sans soulèvement, donne le même message que dans les
  premières missions (`MESSAGE_WRATH_OF_MARS_NO_MILITARY`). Les **révoltes de gladiateurs restent** (Alexandre).
- *À valider* :
  - réglages modifiables jusqu'au lancement plutôt que figés après « Héberger » ;
  - la ligne de commande (`tools/play-mp.sh`) reste en difficile : la passer aussi en facile ?
- Classique : inchangé.

### D-064 — Eau de César pour tous, menu du joueur local, garde des recherches de chemin (T4.9, T4.13, T4.17)
- 2026-10-06 · **adoptée** (corrections de bugs du premier essai, sans changement de règle de jeu)
- **L'eau d'une cité ne dépend plus de l'eau déjà posée par les cités précédentes.** Les cités calculent l'eau l'une
  après l'autre sur la même grille d'aqueducs, remise à zéro au seul tour de la première (tick 27). En
  multijoueur, la propagation de l'eau (`map/water_supply.c`) tient sa propre grille « atteint », remise à zéro au
  début du tour de chaque cité ; elle n'est jamais sauvegardée (vide entre deux tours). Les aqueducs d'un autre
  joueur ne portent toujours pas l'eau de celui-ci, ceux de César oui (D-034). Classique : test d'origine inchangé.
- **Le menu de construction est celui du joueur local seulement.** `building_menu_update` ne fait rien quand la
  cité courante est celle d'un autre joueur. Une commande d'un autre joueur (route de commerce, arc de triomphe...)
  ne change donc plus ce que voit le joueur local (D-061). Classique : jamais « cité d'un autre joueur ».
- **Recherches de chemin sur les grandes cartes (T4.17).** `GUARD` (`map/routing.c`) limite les recherches des
  bateaux et de la dérive. En multijoueur, la garde est d'une fois et demie la taille de la grille, au moins 50 000 :
  le classique (26 244 cases) garde 50 000. Les citoyens, les caravanes et les animaux cherchent sans limite de
  cases. *Corrigé* (revue, D-073) : les ennemis en ont une, choisie pour les cartes de 162 cases. Leur recherche par
  la terre s'arrête après 5 000 puis 25 000 cases (`figure/route.c`), l'approche d'une armée après 400
  (`figure/formation_enemy.c`) ; au-delà, ils passaient « à travers tout », forts compris. En multijoueur, ces
  limites suivent le nombre de cases de la carte (× 2,6 sur la carte à 4, jamais moins que l'original,
  `map_routing_noncitizen_max_tiles`). La recherche de 600 cases autour d'un point d'arrivée d'une armée reste telle
  quelle : elle ne dépend pas de la taille de la carte. Classique : inchangé. La limite de
  500 pas d'un chemin de figure (`MAX_PATH`) est conservée : le plus long chemin de la carte à 4 fait 253 pas, et
  l'élargir changerait le format des sauvegardes (*à revoir* si une nouvelle carte de T4.14 dépasse 400 pas).

### D-065 — La nourriture du plan passe par les autorisations de la cité ; les commandes les vérifient (T4.15, précise D-020, D-062)
- 2026-10-07 · **adoptée** (pour appliquer la nourriture validée par Alexandre, T4.15)
- **Pas de nouvel état sauvegardé.** La nourriture suit la même autorisation que les matières premières : « notre
  cité » de l'empire produit telle ressource (`empire_city_set_our_production_allowed`). Chaque cité a son empire,
  sauvegardé avec elle : les clients, qui ne reçoivent que la sauvegarde, ont les mêmes autorisations (vérifié après
  écriture et lecture de la carte).
- **Porcs et quais donnent tous deux de la viande.** Dans l'original, les quais ne dépendent pas de l'autorisation
  « viande » : la viande est offerte dès que les quais sont permis (`city/resource.c`). L'autorisation « viande »
  ne commande donc que l'élevage de porcs. Les joueurs des terres l'ont ; ceux de la côte ne l'ont pas, mais
  pêchent. Les cartes préparées permettent toujours les quais et les fermes, quel que soit le modèle.
- **Les commandes de construction vérifient l'autorisation**, comme le menu (`mp_permissions_may_build`) : une ferme,
  une carrière, une mine, une glaisière ou un chantier de bois d'une ressource que la cité ne peut pas produire est
  refusé. Seulement sur une carte multijoueur (`game_rules_multiplayer_map`) : rien ne change en classique.
- *À valider* : les ateliers ne sont pas vérifiés par les commandes (le menu les règle encore seul, D-020, D-061).
- Effet de l'original à connaître : quand les quais sont permis, la viande est dessinée en poisson (icônes, charrettes,
  entrepôts), aussi celle des porcs des joueurs des terres.

### D-066 — Prix de Rome et portorium en place ; la hausse des prix à l'arrêt du commerce reste à choisir (précise D-060, D-061, T4.3, T4.12)
- 2026-10-07 · **appliquée** pour les prix ; **à valider** pour la hausse (travail de nuit, sans Alexandre)
- **Prix de Rome** : le prix d'achat de base de l'original (marbre 200), qui suit toujours les variations de prix du
  scénario. Le prix de vente de l'original ne sert plus en multijoueur.
- **Portorium** : 50 % du prix de Rome, arrondi vers le bas. Acheter à l'empire coûte Rome + portorium, vendre lui
  rapporte Rome − portorium. Marbre : 300 et 100. Vin (215) : 322 et 108. Cela vaut partout où l'empire paie ou est
  payé : marchands et navires aux entrepôts, recettes et dépenses du commerce, fenêtre « Prix de l'empire ».
- **L'empire vend toujours** : la règle D-048 qui le faisait s'effacer devant un joueur moins cher est retirée. La
  page du commerce montre toujours en vert la source la moins chère, à titre d'information.
- **Entre joueurs** : prix de Rome par défaut, sans portorium, puis le prix fixé par le vendeur (D-043).
- **Chaque joueur a son commerce avec l'empire** (D-061) : routes, quantités, prix et réglages sont dans l'état de
  sa cité. Le test `mp_trade_empire_isolation` le vérifie octet par octet.
- **Page du commerce** : titre « Empire, portorium 50 % », colonnes « Rome » et « Payé/reçu » ; la partie du joueur
  choisi est décalée à droite, la page tient toujours en 640 × 480.
- **Partie seule** (un joueur sur la carte) : c'est une partie multijoueur, la règle s'y applique aussi. *À valider*.
- **Hausse des prix quand un joueur arrête le commerce** (T4.3), non codée. Trois propositions, *à valider* :
  - **A. La route fermée** : quand un joueur ferme sa route avec un autre, le portorium de l'autre monte à 100 % sur
    les ressources qu'il lui achetait, pendant 12 mois, puis redescend de 10 points par mois. Simple et lisible ;
    seule la victime paie plus cher.
  - **B. L'embargo par ressource** : quand un vendeur cesse de livrer une ressource achetée (route fermée, prix
    monté de plus de 50 %, ou stock vide tout un mois), le portorium de cette ressource monte de 25 points par mois
    pour l'acheteur (au plus 150 %), et redescend de 10 points par mois après la reprise. Plus fin, plus de règles à
    expliquer.
  - **C. Rome punit le fautif** : celui qui coupe le commerce voit son propre portorium monter de 50 points sur
    toutes les ressources pendant 12 mois. L'embargo devient coûteux au lieu d'être une arme.
  - Ma préférence : **A**, la plus simple à comprendre et à afficher (une ligne « portorium 100 % » sur la page
    du commerce). Dans les trois cas, le portorium devient un état de chaque cité (sauvegardé, dans la somme de
    contrôle), changé par les commandes déjà en place (route, prix) : le déterminisme est gardé.
- Classique : inchangé (prix d'achat et de vente de l'original, pas de portorium).

### D-067 — Une mission coûte 30 chargements de marbre ; les cadeaux à César passent par des commandes (T4.7, T4.2)
- 2026-10-07 · **adoptée** (Alexandre, essai du soir : T4.7 et T4.2) · précise D-037 et D-057
- **Mission supplémentaire : 30 chargements de marbre** (4 auparavant), pour éviter les extensions trop sauvages. La
  première mission reste gratuite tant que le joueur n'a pas de terre.
- **Cadeaux, salaire et dons à César : trois commandes réseau** (`MP_ACTION_SEND_GIFT`, `MP_ACTION_SET_SALARY`,
  `MP_ACTION_DONATE`), appliquées dans la cité de l'expéditeur. Les boutons du conseiller impérial reviennent en
  multijoueur (ils étaient cachés, D-057). Dans les fenêtres, le choix en cours (taille du cadeau, montant du don)
  est un état de la fenêtre et n'écrit rien dans la simulation, pour ne pas désynchroniser les machines. Classique :
  les mêmes appels qu'avant.
- **Cadeau** : les trois tailles de l'original, payées sur l'épargne personnelle (épargne / 8 + 20, / 4 + 50,
  / 2 + 100). Il rapporte 4, 7 ou 10 lauriers de cadeaux (`MP_CAESAR_GIFT_LAURELS`, CESAR §6).
  **Un seul cadeau compte tous les 12 mois** (CESAR §6.1) : un cadeau envoyé pendant l'attente est payé et ne
  rapporte rien, la fenêtre dit dans combien de mois le prochain comptera. L'attente est un compteur de mois par
  joueur dans la pièce `mp_caesar` (`STATE_VERSION` 3 ; les anciennes parties se chargent, sans attente).
  La faveur d'origine ne sert plus (D-026).
- **Salaire : il est de nouveau versé en multijoueur** (ce que D-026 supprimait), sinon l'épargne reste vide et les
  cadeaux sont impossibles. Chaque mois, il est prélevé sur le trésor et versé à l'épargne. Le salaire ne peut pas
  dépasser le **rang** (CESAR §6.1) : la commande refuse un rang trop haut, et le salaire retombe au rang au mois
  suivant si le joueur perd des lauriers. Les rangs de l'original donnent 0, 2, 5, 8, 12, 20, 30, 40, 60, 80, 100 Dn.
- **Don à la cité** : de l'épargne vers le trésor, plafonné à l'épargne, sans lauriers.
- *À valider* : le rang 0 au départ donne un salaire de 0, donc **pas d'épargne avant le rang 1** (100 lauriers pour
  un score de 1 000) ; la partie commence donc sans cadeaux possibles, sauf épargne de départ de la carte. Faut-il
  une épargne de départ ou un salaire plus tôt ?

### D-068 — Le pont de César dans la guerre : terre de César, ouvert en paix, tenu en guerre (T4.8)
- 2026-10-06 · **à valider** (proposition de Claude, demande d'Alexandre : « réfléchir à l'impact du pont de César,
  seul passage terrestre entre les deux rives à 4 joueurs […] Peut-on le bloquer, le tenir, le couper ? Que fait
  César si un joueur le ferme ? »). Détail et options : [CESAR.md](CESAR.md) §7.6 ; technique : DESIGN §7.1.
- **Constat** : à 4 joueurs, caravanes entre rives, missionnaire et armées n'ont que ce pont. Le commerce avec
  l'étranger, les immigrants, les campagnes et les expéditions de César n'en ont pas besoin. Il ne porte pas d'eau :
  un aqueduc ne se pose ni sur l'eau ni sur un pont.
- **Aujourd'hui** : indestructible (D-047). Mais une porte peut se poser sur une route de César, si l'endroit est
  dans la zone du joueur (lu dans le code, à vérifier). Quand les portes ne laisseront passer que leur propriétaire
  (M10.2), une porte à l'entrée du pont le fermerait à tous.
- **Proposé** (option 1 sur 3) :
  - **terre de César** : 15 cases autour de chaque entrée du pont, où personne n'a de zone ni ne bâtit (ni
    mission, ni porte, ni mur, ni tour, ni fort) ; seules les routes y sont permises ;
  - le pont reste **indestructible** et **ouvert à tous en paix** ;
  - **en guerre**, on le tient avec des soldats, qui n'attaquent que l'ennemi : les joueurs neutres passent ;
  - les légions de l'agresseur sur la terre de César comptent dans la colère comme dans le territoire adverse (CESAR
    §7.2), pas celles du défenseur ; à la paix, toutes rentrent d'office à leur fort.
- **Écartés pour l'instant** : le blocus permis puis puni (option 2 : plus de règles, frappe les neutres) ; le pont
  destructible et rebâti par César (option 3 : coupe toute la province, reconstruction à écrire).
- **Lien avec T4.14** : un aqueduc ne traverse pas la mer. Si les deux joueurs des terres (D-062) sont sur deux
  rives, il faut un réservoir de César sur chacune. *Fait* : la carte refaite (T4.15) met un joueur des terres sur
  chaque rive, chacun avec son réservoir ; distances au pont dans CESAR §7.6.
- Classique : inchangé (rien de tout cela n'existe hors des cartes multijoueur).

### D-069 — Deux cartes préparées de plus ; le salon choisit la carte, au hasard par défaut (T4.14, précise D-047, D-062)
- 2026-10-07 · **adoptée provisoirement**, **à valider** par Alexandre (« tu me généreras 2 cartes de plus »)
- **Carte 2 pour 2 joueurs** (220 cases) : le bras de mer traverse la carte en diagonale, du nord-ouest au sud-est.
  Les deux joueurs vivent au sud-ouest de la mer (même rive, D-047). Le joueur des terres est dans le coin sud-ouest,
  à plus de 45 cases de l'eau ; son réservoir de César est sur la côte, au nord-est de lui. Le joueur de la côte est
  sur le rivage, au sud ; il arrive par le bord sud (les autres arrivent par l'ouest ou l'est). Le pont de César mène
  au nord-est sauvage, la moitié de la carte, avec un peu moins de bois (16,5 % de la carte).
- **Carte 2 pour 3 ou 4 joueurs** (240 cases) : un bras de mer sinueux au sud. Les deux joueurs des terres sont sur la
  grande rive nord, reliés par une route de César de l'ouest à l'est ; les deux côtiers sur la rive sud. Il y a donc
  toujours deux joueurs sur chaque rive (D-047), mais les deux joueurs des terres sont sur la même rive, avec chacun son
  réservoir de César sur la côte nord (D-068 : un aqueduc ne traverse pas la mer). À 3, le côtier du sud-est manque.
- **Mêmes règles que la carte 1** (D-055, D-062, D-065) : matières et nourriture du plan, pas d'étang, pont de César,
  routes de César jusqu'à chaque joueur, point de pêche et navires de l'empire pour les côtiers, emplacements tirés au
  sort (graine 0 : ordre du plan). Les chemins les plus longs restent sous 400 pas (D-064) : 117 pas à 2, 241 à 4.
- **Le salon choisit la carte** : « Carte 1 », « Carte 2 » ou « Carte au hasard ». C'est une règle de la partie
  (`prepared_map`), comme la difficulté : sauvegardée, envoyée avec le message d'accueil et avec les règles du salon
  (T4.11, protocole 16). Les parties sauvegardées avant se lisent comme jouées sur la carte 1.
- **Au hasard** : l'hôte tire la carte avec la graine du salon (celle qui tire les emplacements, par un autre calcul).
  La même graine donne toujours la même carte. Les règles de la partie gardent la carte tirée, et les clients
  reçoivent la carte toute faite dans la sauvegarde : tous jouent sur la même.
- **Par défaut** : « Carte au hasard » dans le salon. Les règles par défaut des tests et de la ligne de commande
  (`--mp-host`, `tools/play-mp.sh`) gardent la carte 1, comme la difficulté (D-063).
- *À valider* :
  - le choix par défaut, au hasard ;
  - les deux nouvelles cartes elles-mêmes (formes, places des joueurs, taille) ;
  - sur la carte 2 pour 2, le joueur de la côte arrive par le bord sud, et non par l'ouest ou l'est.
- Classique : inchangé (rien de tout cela hors des cartes multijoueur).

### D-070 — Stock minimum et maximum du commerce, pour l'empire et pour les joueurs (T4.5, précise D-043, D-051, D-060)
- 2026-10-07 · **à valider** (lecture provisoire de T4.5 et de la question 5 de MAQUETTES.md, travail sans Alexandre)
- **Deux bornes par ressource et par cité**, valables pour l'empire **et** pour les autres joueurs. Le stock est
  celui des entrepôts, compté comme dans l'original (`city_resource_count`).
  - **« Vendre au-dessus de N »** : la cité garde N chargements ; elle ne vend rien, ni à l'empire ni à un joueur,
    tant que son stock est à N ou moins. C'est le **seuil d'export de l'original**, repris tel quel (déjà appliqué
    aux joueurs par D-043) : de 0 à 100, même commande qu'avant (`MP_ACTION_CHANGE_EXPORT_OVER`).
  - **« Acheter jusqu'à M »** : la cité n'achète plus rien, ni à l'empire ni à un joueur, dès que son stock atteint
    M. 0 (« sans ») : pas de borne. Nouvelle commande `MP_ACTION_CHANGE_BUY_LIMIT` (ressource, écart), de 0 à 400.
- **Entre joueurs** : la caravane part avec au plus ce que la borne laisse de place, chargements déjà en route
  compris (ceux de tous les vendeurs). À l'arrivée, l'acheteur ne prend que la place restante sous M (son stock a pu
  monter pendant le trajet) ; le reste repart chez le vendeur, comme quand l'entrepôt est plein.
- **Avec l'empire** : quand M est réglé, **M remplace la limite automatique de l'original** (10 chargements pour le
  marbre, selon la population pour la nourriture, selon les ateliers pour les matières premières). Avec « sans »,
  la règle de l'original reste. *À valider* : l'autre lecture, où M ne ferait que s'ajouter à la limite d'origine
  (le plus petit des deux), empêcherait d'acheter 40 marbres à l'empire.
- **L'état** : M est une nouvelle pièce de la sauvegarde multijoueur, `mp_trade_bounds` (toutes les cités, version
  1), comptée dans la somme de contrôle. Une partie d'avant n'a pas cette pièce : elle se charge sans borne. Le format
  `.mpsav` ne change pas de version (pièce facultative). `PROTOCOL_VERSION` 16.
- **La page du commerce** : un onglet **« Stocks »** à droite des onglets des joueurs. Il remplace la partie du
  joueur choisi par les deux bornes, avec − et + (4 chargements par clic, une place d'entrepôt), sur les 16 lignes ;
  « sans » quand M vaut 0. Deux lignes d'explication en bas. Tout tient en 640 × 480. À 4 joueurs, les quatre
  onglets sont plus étroits, en petite police, pour ne pas couvrir le titre. Captures :
  `tools/mp-trade-test.sh` (`trade-stocks.png`), `tools/mp-solo-test.sh` (`mp-solo-trade.png`). *À valider* : le
  pas de 4.
- **Partie seule** (un joueur sur la carte) : la page du commerce multijoueur s'affiche aussi, avec le seul onglet
  « Stocks », pour qu'Alexandre puisse régler les bornes seul. Cela revoit D-051 (« partie seule : page
  d'origine »). *À valider*.
- **Pas de paire de bornes par partenaire** (autre lecture de la question 5) : une seule paire par ressource,
  plus simple à lire. La fiche de ressource de MAQUETTES §7 pourra les reprendre plus tard.
- Classique : inchangé (la borne M vaut toujours 0, la commande ne fait rien, la page d'origine reste).

### D-071 — L'estime de César, ce sont les lauriers : le quatrième pilier les montre (T4.1)
- 2026-10-07 · **à valider** (proposition de Claude, demande d'Alexandre : « regrouper les lauriers et l'estime de
  César déjà existante, pour un seul système plus simple ») · précise D-026, D-057, D-067
- **Constat** : deux compteurs. La faveur d'origine (0 à 100) est figée et cachée en multijoueur (D-026, D-067), mais
  elle restait visible dans l'évaluation de la cité (4e pilier), sur les drapeaux et l'infobulle du sénat, et dans la
  barre latérale. Les lauriers, eux, s'additionnent sans plafond et donnent la victoire.
- **Deux façons de les fondre** (ROADMAP T4.1) :
  1. l'estime devient un niveau de César (0 à 100), qui bouge avec les actions, et les lauriers s'accumulent chaque
     mois selon ce niveau. Deux compteurs liés : plus dur à lire, et tout l'équilibrage de CESAR §4 est à refaire ;
  2. **proposé** : un seul système, les lauriers. L'« estime » montrée au joueur n'est qu'une **lecture** des
     lauriers : lauriers / score du salon × 100. Sans score (partie sans fin), rapportés au rang suivant. Aucune règle
     ne change.
- **À l'écran, en multijoueur** (le classique ne change pas) :
  - évaluation de la cité : le 4e pilier s'appelle « Lauriers ». Sa hauteur est l'estime, son nombre les lauriers,
    et « X requis » le score (ou le rang suivant). Cliqué, il dit les lauriers gagnés le mois dernier (au dixième),
    la tendance (en hausse, stable, en baisse), la place de la cité dans la province, et ce qui rapporte des
    lauriers ;
  - son infobulle d'origine parle de faveur : elle disparaît en multijoueur (les infobulles des conseillers ne se
    traduisent pas) ;
  - sénat (drapeau de la faveur, infobulle) et barre latérale (« Lauriers », avec le score à atteindre) : les
    lauriers aussi ;
  - rien d'autre ne montrait la faveur : le conseiller impérial a déjà sa page des lauriers, le conseiller en chef et
    la barre du haut ne la montrent pas, la fenêtre de fin de mission ne s'ouvre jamais en multijoueur.
- **Les trois autres piliers restent ceux d'origine** : culture et prospérité sont aussi deux des cinq notes de
  César. La paix ne rapporte pas de lauriers ; elle reste, car elle dit toujours l'état de la cité.
- **Historique** : chaque cité note ses lauriers à la fin de chacun de ses mois, les 12 derniers sont gardés
  (`mp_caesar`, `STATE_VERSION` 4, protocole 16 ; les anciennes parties se chargent, sans historique). Gain du
  mois : l'écart entre les deux derniers bilans, cadeaux compris. Tendance : les 3 derniers mois contre les 3
  d'avant, « stable » à un laurier par mois ou un dixième près. Cet historique pourra nourrir les Annales (T4.6).
  - la lecture 2 (l'estime = les lauriers rapportés au score) plutôt que la 1 ;
  - garder le pilier de la paix, ou le remplacer par une note de César (commerce ou habitat) ;
  - sans score, la hauteur rapportée au rang suivant retombe à chaque nouveau rang. Autre choix : la rapporter aux
    1 000 lauriers de base.

### D-073 — Une partie reprise garde ses règles ; corrections de la revue (T4.2, T4.9, T4.10, T4.11, T4.12, T4.17)
- 2026-10-07 · **adoptée provisoirement** (corrections de la revue, sans Alexandre)
- **Une partie reprise d'une sauvegarde multijoueur (`.mpsav`) garde les règles enregistrées** : difficulté, dieux,
  invasions, brouillard, territoires, fin de partie et score. Avant, le salon les remplaçait par les siennes, et les
  territoires tombaient à 0 : les joueurs bâtissaient partout. Après « Héberger », le salon affiche les règles de la
  sauvegarde, grisées, et les changements n'ont plus d'effet ; les joueurs qui rejoignent les voient aussi. *À
  valider* : le salon ne peut plus rien changer d'une partie reprise. Avant « Héberger », il montre encore ses propres
  réglages. Une sauvegarde plus ancienne (avant le score de César ou avant les territoires) garde aussi ses règles :
  celles qui manquent valent 0, comme au chargement. Tests `mp_lobby_resume_rules`,
  `mp_lobby_resume_rules_before_score` et `mp_lobby_resume_rules_before_territories`.
- **Une carte multijoueur (`.mpmap`) garde ses territoires** ; les autres règles viennent du salon. *À valider*.
- **Les règles reçues du réseau sont vérifiées** (salon et message d'accueil) : difficulté de 0 à 4, oui/non, fin de
  partie, années et score dans leurs bornes. Un message hors bornes est ignoré dans le salon ; un accueil hors bornes
  est refusé (« Message de l'hôte invalide »), comme un accueil dont le numéro du joueur ou le nombre de joueurs
  (2 à 4) sort de ses bornes. Test `mp_lobby_bad_rules` (faux hôte, port 27490).
- **Le conseiller religieux** suit les dieux de la partie, et non le réglage local. `tools/check-determinism.sh`
  refuse désormais toute autre lecture des réglages locaux que remplacent les règles (difficulté, dieux, correctifs).
- **Cadeau à César en multijoueur** : il est payé sur l'épargne et rapporte ses lauriers (D-067), mais ne touche
  plus à la faveur d'origine, ni au compteur des mois depuis le dernier cadeau, ni à la pénalité des cadeaux
  répétés (D-026). Test `mp_caesar_gifts`.
- **Ennemis sur les grandes cartes** : limites des recherches de chemin à l'échelle de la carte (D-064 corrigée).
  Test `mp_far_invasion` : une armée qui débarque au bord le plus éloigné d'une cité (carte à 4) trouve son chemin
  par la terre, sans passer « à travers tout », et atteint la cité.
- **Pont de César** (CESAR §7.6, précise D-068) : distances au pont recalculées pour chaque emplacement de la carte à
  4 refaite ; comme au §7.2, seules les légions de l'agresseur sur la terre de César comptent dans la colère.
- Classique : inchangé.
