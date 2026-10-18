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
  *Mesuré* (MG.5, D-088) : le risque ne se vérifie pas. Une légion postée bat les javeliniers qui la chargent, de
  face comme de flanc ; c'est l'inverse qui manquait : ils ne battaient pas une légion en marche. Leur tir passe de
  100 à 70 ticks en multijoueur.
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
  `.mpsav` ne change pas de version (pièce facultative). `PROTOCOL_VERSION` 17.
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
  (`mp_caesar`, `STATE_VERSION` 4, protocole 18 ; les anciennes parties se chargent, sans historique). Gain du
  mois : l'écart entre les deux derniers bilans, cadeaux compris. Tendance : les 3 derniers mois contre les 3
  d'avant, « stable » à un laurier par mois ou un dixième près. Cet historique pourra nourrir les Annales (T4.6).
- *À valider* :
  - la lecture 2 (l'estime = les lauriers rapportés au score) plutôt que la 1 ;
  - garder le pilier de la paix, ou le remplacer par une note de César (commerce ou habitat) ;
  - sans score, la hauteur rapportée au rang suivant retombe à chaque nouveau rang. Autre choix : la rapporter aux
    1 000 lauriers de base.

### D-072 — Mesure : le jeu multijoueur n'est pas plus restrictif qu'en classique, hors zone et difficulté (T4.16)
- 2026-10-07 · **à valider** (Alexandre : « le jeu paraît plus restrictif : distance d'effet des bâtiments, emplacement
  des habitations, manque de main-d'œuvre »). Aucune règle n'a changé : c'est une mesure, gardée par un test.
- **Question** : à plan égal, une cité multijoueur grandit-elle moins qu'une cité classique ?
- **Méthode** : `simtool restrictiveness` (ctest `mp_restrictiveness`) bâtit par commandes la même petite cité
  (63 bâtiments, réservoirs et fontaines, puis au mois 2 plus d'emplois que de bras) et la joue deux ans, en facile
  et en difficile : en classique et en multijoueur, seule et à 4 sur la carte préparée, et sur du terrain libre de
  vraies cartes classiques. Séries, tableaux complets et gardes : [TESTING.md](TESTING.md) §2 ter.
- **Résultats** (une ligne par groupe de cités ; population, employés et emplois vides) :

  | Cité | Difficulté | Pop. à 6 mois | Pop. à 24 mois | Employés | Emplois vides |
  |------|------------|---------------|----------------|----------|---------------|
  | Classique, carte libre du test et 5 cartes d'origine, une cité seule | facile | 198 à 342 | 246 à 406 | 76 à 172 | 0 à 96 |
  | | difficile | 196 à 328 | 236 à 406 | 76 à 172 | 0 à 95 |
  | Classique = multijoueur, carte préparée, seule | facile / difficile | 274 / 246 | 434 | 189 | 6 à 7 |
  | Multijoueur (= classique), carte préparée, 4 joueurs J1 à J4 | facile | 179 à 236 | 334 à 380 | 142 à 162 | 1 à 44 |
  | | difficile | 173 à 222 | 334 à 380 | 142 à 162 | 1 à 44 |

- **Conclusion : pas de bug dans une cité multijoueur.**
  - Sur la même carte, classique et multijoueur donnent les mêmes chiffres : population, maisons et leur niveau,
    couverture (fontaines, puits, nourriture, religion, marché, temple, préfecture, ingénieurs), employés, chômeurs,
    désirabilité, immigrants, moral. Seule exception : un atelier de J3 à cheval sur la zone d'un voisin n'est pas
    bâti en multijoueur (34 emplois vides, contre 44 en classique) ; la population ne change pas.
  - Les cités multijoueur (334 à 434 habitants à 24 mois) sont dans l'intervalle des cartes d'origine (246 à 406) ou
    au-dessus. Le plus grand écart, 334 contre 406 à Valentia, vient de la place : la cité classique de la même place
    a aussi 334.
  - Ce que sent Alexandre vient d'ailleurs : la **difficulté** (difficile par défaut avant D-063 : 6 000 Dn contre
    12 000, moral de base 50 contre 70, migration réduite), la **zone** (on ne bâtit qu'à 20 cases, D-036) et le
    **terrain** de forêt de chaque emplacement (désirabilité des maisons de −4,5 à −7,8 à 4 joueurs, −0,8 seule).
    La mission de départ ne prend aucun employé en multijoueur, contre 20 en classique : plus de bras, pas moins.
  - Le **manque de main-d'œuvre** existe dès que les emplois dépassent les bras, en classique comme en multijoueur
    (14 à 96 emplois vides sur les cartes d'origine).
- **Garde** : `mp_restrictiveness` échoue si une règle multijoueur change ces chiffres au-delà d'une petite
  tolérance, ou si une cité multijoueur passe à plus de 15 % sous la plus faible cité classique. La première garde a
  été vérifiée par deux fautes volontaires.
- *Points ouverts, à valider* :
  - **début de partie** : à 6 mois, les cités de la carte à 4 (173 à 236 habitants) sont plus petites que sur les
    cartes d'origine (196 à 342), les « classiques » de la même carte aussi. Piste non isolée : la route du point
    d'entrée au plan, plus longue (48 à 63 cases). Rapprocher le point d'entrée serait une décision de carte ;
  - **non mesuré** : une zone qui rétrécit dans une cité sans bras (un bâtiment hors zone s'effondre après 3 mois,
    D-036). Si Alexandre sent encore une gêne en facile, lui demander où et quand (une capture) ;
  - **limites** : la série « classique » à 4 passe déjà par le code de la carte multijoueur ; les cartes d'origine
    n'ont qu'une cité et une place chacune, la comparaison est donc par intervalle.
- Classique : inchangé.

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

### D-074 — Aucune fenêtre n'écrit dans la partie ; sauvegarde du mois et reprise par le salon (T5.3, T5.4)
- 2026-10-07 · **adoptée provisoirement** (sans Alexandre)
- **Cause trouvée de la désynchronisation** : le conseiller religieux recalculait, à chaque dessin, le « dieu le moins
  content » et l'**écrivait dans la cité** (`city_gods_calculate_least_happy`). La simulation ne le recalcule qu'au
  début du mois, et pas tous les mois : ouvert sur un seul ordinateur, le conseiller changeait l'état de cette cité,
  et la somme de contrôle du tour suivant différait. C'était le seul calcul des conseillers oublié par M2.6. En réseau,
  le conseiller le calcule pour lui seul (`city_gods_least_happy_now`, sans rien écrire) ; classique inchangé.
- **Garde-fou** : la commande d'automatisation `windowsweep` ouvre et dessine, sans faire avancer la partie, chaque
  conseiller, chaque onglet du commerce (dont Stocks), chaque fenêtre de dialogue (salaire, cadeau, don, fête, prix,
  carte de l'empire, messages, options, sauvegarde, lettre de César, résultats), chaque menu de construction et de
  calque, et la fiche de chaque type de bâtiment, de figure et de terrain, et vérifie que l'état simulé n'a pas changé.
  `tools/mp-sweep-test.sh` (seul, avec un dieu en colère : échouait sans le correctif) et `tools/mp-menus-test.sh`
  (trois joueurs dans les menus en même temps, puis chacun seul, sans désynchronisation).
- **Pourquoi la partie ne reprenait pas** : après la désynchronisation, la partie restait « en cours » pour le jeu.
  Le salon, rouvert, refusait alors d'héberger comme de rejoindre (il se croyait déjà dans une partie), et il n'y avait
  rien à reprendre : à plusieurs, aucune sauvegarde automatique, et les états écrits à la désynchronisation
  (`mp-desync-…`) sont cachés du salon. Désormais :
  - « Fichier, Nouvelle partie » quitte la partie en réseau, et le salon, à son ouverture, termine une partie finie
    (désynchronisée, hôte perdu) ou abandonnée : on peut héberger ou rejoindre aussitôt ;
  - **chaque ordinateur sauvegarde la partie en réseau à son lancement et au début de chaque mois**
    (`autosave.mpsav`, écrit à côté puis renommé), sans dépendre de l'option de sauvegarde automatique ; le salon la
    liste ; le message de désynchronisation dit de reprendre la partie depuis le salon, seulement si cette partie a
    écrit ce fichier (sinon il donne l'état écrit à la désynchronisation : jamais un `autosave.mpsav` d'une autre
    partie). La sauvegarde du mois s'écrit avant que l'aperçu de construction du joueur (route, aqueduc, mur,
    maisons en cours de tracé) ne revienne sur la carte : une route en cours de tracé au changement de mois n'est pas
    dans le fichier (test : `mp_lan_save_resume_map2_2_players` trace une route pendant un changement de mois et
    relit la sauvegarde ; échouait avant). *Constaté en écrivant ce test, non corrigé* : quand, après les tours d'une
    image, un bâtiment de la partie attend dans l'état « annulable » (maison retirée faute de route, fréquent à
    trois), l'aperçu du tracé ne peut pas revenir et le tracé en cours du joueur est perdu ; il faut recliquer. *À valider* : tous les mois, toujours (le fichier
    pèse quelques Mo), et le même nom pour toutes les parties (la dernière jouée).
- **« Impossible de charger une partie sauvegardée »** : en multijoueur, « Sauvegarder » écrit un `.mpsav`, mais la
  fenêtre « Charger » ne listait que les `.sav` (et « Sauvegarder » montrait les `.sav`, pas les parties
  multijoueur). Et « Charger » pendant une partie en réseau aurait chargé une partie sur un seul ordinateur.
  Désormais :
  - « Charger » (menu principal ou partie finie) liste aussi les parties multijoueur, marquées « (multijoueur) » ; en
    choisir une ouvre le salon avec cette partie choisie, et le nombre de joueurs de la sauvegarde : il reste à
    cliquer sur « Héberger » ;
  - pendant une partie en réseau, « Charger » et « Rejouer la mission » disparaissent du menu Fichier (le raccourci
    explique qu'il faut reprendre la partie depuis le salon) ; « Sauvegarder » et « Supprimer » montrent les `.mpsav` ;
  - le salon liste les parties par la fin de leur nom (un nom avec un point, renommé à la main, était ignoré), garde
    la partie choisie quand la liste change, et en reprend le nombre de joueurs.
  *Mesuré* : le chemin réseau lui-même marchait (`mp_lan_save_resume_3_players`, `mp_lan_save_resume_map2_2_players` :
  sauvegarde par le chemin du menu Fichier, partie hébergée à nouveau comme par le salon dans le même processus, un
  joueur remplacé par un nouveau processus, commerce entre joueurs dans la sauvegarde, carte 1 à 3 joueurs et carte 2
  à 2) ; le blocage était dans l'interface. Test de bout en bout `tools/mp-resume-test.sh` (vrai jeu, deux joueurs).
- **Limite** (*à valider*) : une sauvegarde faite par « Sauvegarder » n'existe que sur l'ordinateur qui l'a faite ;
  c'est lui qui doit héberger la reprise. La sauvegarde du mois, elle, est sur tous les ordinateurs.
- Protocole et formats inchangés. Classique : inchangé (le conseiller religieux, « Charger » et le salon y gardent
  leur comportement, sauf la liste de « Charger », qui montre aussi les parties multijoueur).

### D-075 — Une caravane par mois et par ressource, vers un entrepôt joignable ; la page dit pourquoi rien ne part (T5.7, revoit D-048)
- 2026-10-07 · **adoptée provisoirement** (essai à trois d'Alexandre : « impossible de commercer entre chacun : les
  caravanes ne circulent pas » ; sans ses journaux, diagnostic par des parties simulées et le vrai jeu à trois)
- **Ce que l'essai n'a pas permis de voir** : dans une partie simulée à 2, 3 et 4 joueurs sur les deux cartes, partie
  lancée comme depuis le salon (carte écrite puis relue, facile), routes proposées des deux côtés et achats cochés par
  les commandes de la page, entrepôts au bout de routes propres à chaque joueur : les caravanes partent, suivent les
  routes et le pont, arrivent et sont payées. Pas de panne franche. Mais trois défauts, chacun capable de donner
  « rien ne circule » :
  1. **Une seule caravane à la fois par ressource** (D-048), alors que la page annonce « une caravane par mois ». Sur
     les grandes cartes, un trajet dure de 3 à 7 mois (carte 1 à 3 joueurs : premières livraisons aux mois 5 à 7) :
     8 chargements tous les 5 mois environ, rien entre deux.
  2. **Seul le premier entrepôt de l'acheteur comptait** : s'il est sur une route à part (quartier pas encore relié à
     la grande route, port), aucune caravane ne part jamais, d'aucun vendeur, même vers ses autres entrepôts reliés.
  3. **Rien ne disait pourquoi rien ne part** (vendeur sans stock au-dessus de son seuil, ou nourriture restée dans les
     greniers ; acheteur sans entrepôt, sans place, sans argent, à sa limite). Et dans le vrai jeu, la ligne « Route
     commerciale » et le bouton « Proposer / Retirer » se dessinaient l'un sur l'autre dès que l'état de la route
     changeait page ouverte : on ne pouvait pas lire si la route était ouverte (vu à trois dans le vrai jeu).
- **Une caravane chaque mois pour chaque ressource achetée**, même quand celles des mois précédents sont encore en
  route : la règle que la page affiche. Ce qui est en route compte comme déjà dépensé (l'acheteur paie à l'arrivée :
  pas de dette) et comme déjà rangé.
- **Pas de départ sans place** : la caravane ne part qu'avec ce que les entrepôts de l'acheteur peuvent encore
  recevoir, chargements en route déduits. Plus d'aller-retour à vide chaque mois vers des entrepôts pleins.
- **L'entrepôt d'arrivée** est un entrepôt de l'acheteur qui prend la ressource et qui est sur un réseau routier où
  le vendeur a un entrepôt, et non plus son premier entrepôt.
- **La page du commerce dit pourquoi aucune caravane n'est partie** ce mois-ci, dans la colonne « En route » de
  chaque ressource achetée : « sans stock », « sans entrepôt », « sans chemin », « plein », « sans argent »,
  « limite ». C'est un état d'affichage, ni sauvegardé ni lu par la simulation (connu de nouveau un mois après un
  chargement). Chaque changement est aussi écrit dans `julius-log.txt` (« Trade between players: player 2 to player
  1, resource 12: the seller has none to sell »), pour le prochain essai.
- **La ligne de la route** est redessinée dès que son état change, page ouverte (proposée, ouverte, fermée, ici ou par
  l'autre joueur). Le reste des superpositions de la page relève de T5.6.
- `PROTOCOL_VERSION` 20 : la simulation change (départs), deux versions ne doivent pas jouer ensemble. Sauvegardes
  inchangées.
- Tests : `mp_trade_every_player_*` (2, 3, 4 joueurs, deux cartes : chaque joueur achète à chacun, une livraison
  chaque mois une fois les premières arrivées), `mp_trade_warehouse_apart*`, `mp_trade_purchase_state` ;
  `tools/mp-trade3-test.sh` (trois vrais jeux depuis le salon, routes et achats par clics, les six achats examinés
  au même mois sur les trois machines, page lisible ; depuis l'intégration de T5.5 à T5.8, partie écrite par
  `simtool tradesave` où J2 a de quoi vendre : ses caravanes partent et livrent J1 et J3 dans le vrai jeu, ligne
  « … loads delivered » du journal de chaque machine). Vérifiés rouges avec l'ancien `mp/trade.c`.
  - une caravane par mois même si la précédente roule encore (plutôt qu'une à la fois, D-048) ;
  - les trajets de 3 à 7 mois sur les grandes cartes : des caravanes plus rapides (question déjà posée par D-048) ?
  - la nourriture rangée dans les greniers ne se vend pas (comme à l'empire dans l'original) : la vendre aussi aux
    joueurs ?
- Classique : inchangé (pas de commerce entre joueurs).

### D-076 — Les cités démarrent à la difficulté du salon ; Rome verse à chacun le salaire de son rang (T5.1, T5.2, précise D-063, D-067)
- 2026-10-07 · **adoptée provisoirement** ; le salaire du rang est **remplacé par D-089** (le gouverneur choisit) (retour du deuxième essai à 3 joueurs : « voir si J2/J3/J4 ne sont pas en
  difficile », « aligner les salaires de Rome pour tous les joueurs »)
- **Cause (T5.1)** : charger un modèle de carte (`.map`) ou une partie sauvegardée remet le jeu en mode classique
  (`game_rules_set_classic`), donc la difficulté du **réglage local de l'hôte** (« difficile » par défaut). La
  génération de la carte, qui se fait à ce moment-là, calculait avec ce réglage les **fonds de départ** (le trésor,
  `difficulty_adjust_money`) et la **faveur de départ** ; les cités des joueurs 2 à 4 sont des copies de la première,
  donc **toutes** les cités partaient à la difficulté de l'hôte, quelle que soit celle du salon. Tout le reste
  (sentiment de base, loups, taille des armées, prêt de secours) se calcule en cours de partie par
  `game_rules_difficulty()` et suit bien les règles de la partie, sur chaque machine. Les clients reçoivent la sauvegarde
  de l'hôte : ils ont ses valeurs, et la difficulté affichée en ville est celle des règles (D-063).
- **Correction** : `prepare_template` (génération) garde les règles actives avant le chargement et les remet après,
  puis recalcule les fonds et la faveur de la première cité ; l'hôte règle les règles du salon (`game_rules_set_multiplayer`)
  avant de générer la carte. Même chose dans la composition de copies d'une carte du jeu libre (`host_compose_cities`).
  Une partie reprise d'une sauvegarde de partie (`.sav`) garde son trésor et sa faveur : seules les nouvelles cités
  prennent la difficulté du salon.
- **Salaire (T5.2)** : une cité héritait du **rang du modèle de carte** (par exemple 5, soit 20 Dn par mois jusqu'au
  premier changement de mois) ou de la campagne, et chacun pouvait choisir un salaire différent sous le plafond de son
  rang. Règle provisoire : **chaque joueur démarre avec le même rang 0, le même salaire 0 et la même épargne 0, puis
  Rome lui verse chaque mois le salaire de son rang de lauriers, la même table pour tous** (0, 2, 5, 8, 12, 20, 30, 40,
  60, 80, 100 Dn). Il n'y a plus de salaire à choisir : le bouton du conseiller impérial n'ouvre plus la fenêtre, et la
  commande `MP_ACTION_SET_SALARY` (protocole inchangé) ramène simplement le salaire à celui du rang. Le rang de
  l'original (`player_rank`) suit le rang de lauriers, pour que la faveur de l'original ne retire rien.
- **Tests** : `mp_lan_start_easy`, `mp_lan_start_hard`, `mp_lan_start_easy_saved_template`,
  `mp_lan_start_easy_cities_map` (3 joueurs en réseau, difficulté du salon à l'opposé du réglage de l'hôte, modèle de
  carte de rang 5 et de 3 000 Dn : trésor, faveur, rang, salaire, épargne, sentiment, ennemis et loups de chaque cité,
  identiques sur l'hôte et les clients, ports 27540 à 27543) et `mp_salaries` (3 joueurs de rangs 0, 1 et 3 payés 0, 2
  et 8 Dn, un rang gagné ou perdu change le salaire le mois suivant).
- *À valider* :
  - plus de choix du salaire en multijoueur (Rome verse toujours celui du rang) : faut-il laisser choisir un salaire
    plus bas ?
  - au rang 0 le salaire est nul, donc **pas d'épargne** ni de cadeau avant le rang 1 (déjà noté en D-067) : épargne de
    départ ?
  - une partie reprise d'une ancienne `.mpsav` garde les rangs et salaires qu'elle avait.
- Classique : inchangé.

### D-077 — Une première guerre entre joueurs : déclarer, se battre, faire la paix (T5.5, tranche de M10.1 à M10.3)
- 2026-10-07 · **adoptée provisoirement**, *à valider* (Alexandre, deuxième essai à trois : « pouvoir attaquer un
  autre joueur (même simple) — nécessaire pour le commerce au démarrage »)
- **État de guerre par paire de joueurs** (`mp/war.c`) : dans la somme de contrôle et dans le `.mpsav` (morceau
  `mp_war` ; une sauvegarde plus ancienne se charge en paix). Il ne change que par deux commandes, lancées depuis le
  conseiller militaire (bouton « Guerre » en haut à droite, fenêtre « Guerre et paix ») :
  - **guerre brutale** : on se bat tout de suite ; **guerre honorable** : 3 mois de préavis (48 jours) pendant
    lesquels personne ne se bat (D-053). Pendant le préavis, l'un ou l'autre peut déclarer une guerre brutale : le
    défenseur frappe le premier, et la brutalité devient la sienne ;
  - déclarer demande deux clics (« Confirmer ? ») ;
  - **paix** : signée quand les deux joueurs l'ont proposée ; une proposition se retire. À la signature, les combats
    en cours entre eux s'arrêtent et les légions des deux qui sont hors de leur fort y rentrent, sauf si leur joueur
    est encore en guerre avec un troisième ;
  - **annonces à tous** (déclaration, début des combats, proposition de paix, paix signée) : un avertissement en haut
    de la cité, gardé jusqu'à ce que le joueur revienne sur la vue de la cité (il ne se perd pas dans un conseiller).
- **Combats**, seulement entre deux joueurs qui se battent (sinon rien ne change, et rien en classique) :
  - les légions allaient déjà partout : rien ne les retenait chez elles. Elles s'y battent désormais ;
  - un soldat attaque les soldats, les gardes armés (préfets, sentinelles) et les habitants de l'ennemi, comme un
    envahisseur, mais pas ses caravanes (voir plus bas) ; un préfet ou une sentinelle n'attaque que ses soldats ;
    javelots, balistes et tours tirent sur ses soldats ; l'ordre « nettoyer » cherche ses soldats à 20 cases ;
  - un soldat posté avec sa légion abat ce qui le touche de l'ennemi, bâtiments, murs et portes, comme les
    envahisseurs (mêmes seuils que l'original : 10, porte 150, mur 200). Les bâtiments de César sont intacts.
    L'effondrement est compté dans la cité de la victime (sa note de paix, ses sentinelles).
- **Caravanes** (lien avec le commerce demandé par Alexandre) : aucune caravane ne part entre deux joueurs en guerre,
  préavis compris. **Une caravane déjà en route vers l'ennemi au moment de la déclaration fait demi-tour** (préavis
  compris) : elle rentre à l'entrepôt d'où elle est partie (ou à un autre de son joueur), sans livrer ni être payée,
  et ses chargements y retrouvent leur place comme une livraison (choix de l'intégration de T5.5 et T5.7 : avec une
  caravane par mois, D-075, plusieurs sont en route à la fois, et sans cela elles livraient et étaient payées en
  pleine guerre). Une caravane d'un joueur qui passe à une case d'un soldat de son ennemi est prise, à l'aller comme
  au retour : les entrepôts du preneur gardent ce qu'ils peuvent, le reste est perdu, et le vendeur n'est pas payé.
  Les caravanes d'un joueur neutre passent. On peut ainsi couper le commerce d'un rival en tenant une route avec une
  légion.
- **Mesuré** (test `mp_war`, carte préparée pour 2) : la légion de J1 (16 légionnaires) met 1 750 ticks à rejoindre la
  cité de J2 ; en guerre, le premier bâtiment tombe 20 ticks après la déclaration ; quand la légion de J2 sort, 14
  attaquants meurent contre 3 défenseurs (la légion postée est prise de flanc). L'équilibre des troupes reste à faire
  (MG.5).
- **Tests ajoutés à l'intégration avec T5.7** (`mp_war`) : paix signée en pleine mêlée entre les deux légions :
  tous les combats entre eux s'arrêtent à la signature et personne ne meurt ensuite (échoue sans l'arrêt des combats à
  la paix, qui n'était pas couvert) ; la caravane suivante passe la légion de J1 en paix, la guerre est déclarée trois
  cases avant l'entrepôt : elle fait demi-tour et les soldats la prennent sur le chemin du retour (8 chargements,
  vendeur non payé) ; guerre honorable déclarée pendant un trajet : demi-tour, les 8 chargements reviennent chez J2
  pendant le préavis, rien de livré ni de payé. Les deux derniers échouent sans le demi-tour (livrée et payée 1 200 Dn).
  Cause de l'échec de `mp_war` après la fusion avec T5.7 : le test attendait la caravane suivante au départ, mais avec
  une caravane par mois elle était déjà partie et avait dépassé la légion quand la guerre était déclarée.
- **Pas dans cette tranche** (M10.2 à M10.8, D-068) : motif de la guerre et lauriers (−15 sans motif, −15 de plus pour
  une guerre brutale), colère de César et ses suites (avertissement, ultimatum, expédition punitive), triomphes, délai
  de 12 mois avant de redéclarer, portes et murs qui ne laissent passer que leur propriétaire, terre de César autour
  du pont, moral et totaux par camp, ordre « attaquer » qui marche sur une cible. Le conseiller militaire ne dit pas
  encore « en guerre contre le joueur 2 » sur sa page.
- **À valider** :
  - les soldats tuent aussi les habitants de l'ennemi, comme les envahisseurs ;
  - à la paix, toutes les légions hors de leur fort rentrent ;
  - une caravane prise plutôt que détruite, à une case d'un soldat ; plus de caravane entre ennemis, préavis compris ;
  - une caravane en route à la déclaration fait demi-tour et rentre avec ses chargements (plutôt que d'être arrêtée
    sur place, ses biens perdus ou rendus d'un coup) ; sur le chemin du retour, les soldats de l'ennemi peuvent encore
    la prendre ;
  - les annonces en bandeau plutôt qu'une fenêtre plein écran ;
  - la paix signée par les deux, sans que César puisse encore l'imposer.
- Réseau : protocole 22 (20 et 21 sont ceux de T5.7 et T5.8 ; commandes `MP_ACTION_DECLARE_WAR` et
  `MP_ACTION_PROPOSE_PEACE`). Classique : inchangé.

### D-078 — La page du commerce en trois sortes d'onglets ; l'aqueduc de César à huit rangées de la route (T5.6, T5.8, précise D-051, D-060, D-070)
- 2026-10-07 · **adoptée provisoirement** (second essai d'Alexandre, travail sans lui)
- **Les textes qui se superposaient** (essai à trois joueurs, 640 × 480, français et anglais), relevés sur des captures
  à deux et à quatre joueurs :
  - les en-têtes de colonnes se touchaient (« Payé/reçu », « Mon prix », « Son prix », « J'achète », « En route ») ;
  - « Vendre / au-dessus de » sur deux lignes montait sur la ligne « Empire, portorium 50 % » ;
  - à quatre joueurs, les trois onglets des joueurs, réduits en petite police, touchaient le titre ;
  - la ligne « Route commerciale : fermée » touchait le titre, et **changeait sans que le cadre soit redessiné** :
    quand une commande (la sienne, ou celle de l'autre joueur) changeait l'état de la route, le texte de l'ancien état
    restait sous le nouveau (« Retirer la proposition » sur « Proposer la route »). Le tableau, lui, se redessine à
    chaque image (panneau sombre) : seuls les textes posés sur le cadre en bois étaient touchés.
- **Trois sortes d'onglets**, sous le titre, tous de la même largeur (jusqu'à cinq à quatre joueurs) : *Empire*, un
  onglet par autre joueur, *Stocks*. Chacun a peu de colonnes, les nombres sont alignés à droite sous leur titre.
  - **Empire** : stock, commerce (importe, exporte, aucun : le bouton d'origine), prix de Rome, portorium (en plus à
    l'achat, en moins à la vente), prix payé ou reçu. Seul onglet à régler le commerce avec l'empire.
  - **Un joueur** : stock, mon prix (− et +, quatre chiffres), son prix, le prix de l'empire à titre de comparaison
    (le vert désigne toujours le moins cher), « J'achète », « En route ». Quand rien n'est en route pour un achat sur
    une route ouverte, la colonne « En route » dit pourquoi aucune caravane n'est partie ce mois-ci (D-075 : « sans
    stock », « plein »…), en petits caractères alignés à droite.
  - **Stocks** : stock, « Vendre au-dessus de », « Acheter jusqu'à » (D-070, inchangés).
  - Partie seule : les onglets Empire et Stocks (avant, Stocks seul). *À valider*.
- **Une seule ligne d'aide** sous le tableau, propre à l'onglet (portorium de l'empire, règle des caravanes, règle des
  bornes). Sur l'onglet d'un joueur, tant que la route n'est pas ouverte, elle dit où en est la route et ce qu'il
  faut faire. Les deux lignes d'explication d'avant et le préfixe « Route commerciale : » disparaissent.
- **La route** : un seul bouton, dans le coin du titre, dont le texte dit ce que fait un clic : « Proposer la route »,
  « Retirer la proposition », « Accepter la route » (l'autre l'a proposée), « Fermer la route » (ouverte). Mêmes
  commandes qu'avant.
- **Le redessin** : la page fait redessiner le cadre dès que l'état de la route change (un seul mécanisme, qui
  remplace celui de D-075 ; vérifié par la commande d'automatisation `tradecheck`).
- Tests : `tools/mp-trade4-test.sh` (quatre instances du vrai jeu, cinq onglets, prix à quatre chiffres), `mp-trade-test.sh`
  (route proposée, onglet Empire, onglet Stocks), `mp-ui-test.sh` (640 × 480 et 1024 × 736), `mp-solo-test.sh` (seul).
  Contrôle par des captures regardées une à une, en français et en anglais (la langue forcée un instant, sans commit).
- **L'aqueduc de César** longeait la route principale de la cité, **à quatre cases**, sur toute sa dernière portion :
  en entrant dans la zone du joueur, il ne laissait que trois rangées pour bâtir entre la route et lui. Il court
  maintenant **huit rangées** au nord de la route (`AQUEDUCT_ROAD_GAP`), sur les quatre cartes préparées (cartes 1 et 2,
  à 2 et à 4 joueurs). Même point d'arrivée en colonne, même réservoir ; la colonne qui traverse la route d'un joueur
  de la rive nord la traverse comme avant (l'aqueduc passe sur la route).
  - Le test `simtool preparedmap` (donc les huit tests `mp_prepared_map*`) échoue si un tronçon d'aqueduc de César
    court à moins de six cases d'une route, dans le sens de la route. Il échouait avant (4 cases, 19 à 58 cases
    d'aqueduc trop proches selon la carte) et passe maintenant (8 cases).
  - **Protocole 21** (le 20 est celui des caravanes de T5.7, D-075) : la carte préparée est générée chez chaque pair,
    l'aqueduc déplacé change le terrain ; des versions d'avant et d'après se verraient refusées au salon au lieu de se
    désynchroniser à la première somme de contrôle.
  - *À valider* : huit rangées (le texte de la demande parlait de trois à cinq cases, déjà le cas avec quatre) ; un
    aqueduc plus loin laisse la place entre la route et lui, mais coupe la moitié nord de la zone du joueur.
- Classique : inchangé.

### D-079 — Chaque cité a son propre hasard ; Rome paie les mêmes salaires à tous (T5.9, revoit D-076)
- 2026-10-13 · **adoptée** (Alexandre : les joueurs qui ne sont pas l'hôte ont des émeutes, des salaires de Rome et
  des dieux plus durs ; « aligner les salaires de Rome pour tous les joueurs », T5.2)
- **Cause** : les cités des joueurs 2 à 4 sont des copies de la première, générateur aléatoire compris, avec la
  graine fixe du jeu. Les joueurs 2 à 4 partageaient la même suite de hasard, le joueur 1 une autre, et chaque
  partie rejouait le même schéma : hausse des salaires de Rome au mois 16 chez les joueurs 2 à 4 seulement, d'où un
  moral plus bas et des émeutes ; même suite pour le choix du dieu en colère et pour les événements de la carte. La
  difficulté n'y est pour rien : elle est celle du salon sur chaque ordinateur.
- **Chaque cité a sa propre graine**, tirée de la graine du salon et du numéro du joueur, à la création de la carte
  préparée. Elle voyage dans la sauvegarde ; chaque ordinateur a donc le même hasard pour chaque cité. La graine 0
  (tests) garde le hasard du modèle.
- **Rome est une** : ses salaires changent pour toutes les cités à la fois. Le tirage se fait au tour de la
  première cité, aussi souvent que dans une partie d'une cité, et le nouveau salaire (même montant) va à toutes,
  avec le message habituel (`mp/rome.c`).
- Les autres événements (eau contaminée, mine, carrière, commerce perturbé) restent propres à chaque cité, chacune
  avec son hasard.
- *À valider* : D-076 (salaire du gouverneur fixé par le rang) venait d'une mauvaise lecture de T5.2 ; la garder ou
  revenir au salaire choisi sous le plafond du rang (D-067).
- Classique : inchangé.


### D-080 — Se rétracter d'une déclaration de guerre pendant le préavis (M10.1, précise D-053, D-077)
- 2026-10-13 · **adoptée** (Alexandre : « lorsque l'on annonce une guerre à un ennemi, on peut se rétracter : cela
  permet de lui laisser le temps de céder à la pression commerciale ») ; détails *à valider*
- Pendant le préavis d'une guerre honorable (3 mois), **celui qui l'a déclarée** peut la retirer : bouton « Se
  rétracter » à la place de « Honorable » dans la fenêtre « Guerre et paix », deux clics comme pour déclarer
  (commande `MP_ACTION_WITHDRAW_WAR`). La paix revient tout de suite, comme si elle était signée : propositions
  effacées, légions des deux hors de leur fort rentrées (sauf guerre avec un tiers), annonce à tous (« Joueur 1 retire
  sa déclaration de guerre au joueur 2 »).
- **La pression** (*revu par D-089 : le commerce continue pendant le préavis*) : dès la déclaration, plus aucune
  caravane ne part entre eux, et celles en route font demi-tour (D-077). Le défenseur a 3 mois pour céder (prix, ventes, route ouverte). S'il cède, le déclarant se rétracte et le
  commerce reprend avec la caravane suivante. Mesuré (`mp_war`) : les 8 chargements rentrés chez le vendeur sont
  livrés 3 400 ticks (un peu plus de 4 mois de jeu) après le retrait, avec une caravane par mois (D-075).
- **Pas de retrait** d'une guerre brutale, ni une fois les combats commencés, ni par le défenseur (il peut proposer la
  paix). Si le défenseur frappe le premier par une guerre brutale, la guerre devient la sienne et personne ne la
  retire. Dans ces cas, seule la paix signée par les deux y met fin.
- **Avec César** (M10.4, proposé, *à valider*) :
  - une menace retirée ne coûte ni lauriers ni colère ;
  - le prix d'une guerre sans motif (−15 lauriers) se paie quand les combats commencent : à la fin du préavis pour une
    guerre honorable, tout de suite pour une guerre brutale ;
  - contre l'abus des menaces, la déclaration retirée donne au défenseur un motif de riposte pendant 12 mois, et
    redéclarer la guerre au même joueur dans les 12 mois est « sans motif », avec une colère double, comme après une
    paix (CESAR §7.1).
- Tests :
  - `mp_war` : retrait par le seul déclarant pendant le préavis, dans la somme de contrôle et annoncé ; caravane qui
    fait demi-tour à la déclaration, puis commerce repris ; légion envoyée à la frontière rentrée ; pas de retrait d'une
    guerre brutale, après le début des combats, ni après la riposte du défenseur ; rien en classique. Quatre
    vérifications échouent sans le retrait ;
  - `mp_lan_war` : une menace retirée, puis la guerre brutale, en réseau ;
  - `tools/mp-war-test.sh` : vrai jeu, bouton et confirmation chez l'hôte, annonces chez l'autre joueur.
- Réseau : protocole 24.

### D-081 — Portes et murs en guerre ; les soldats coupent l'aqueduc de César qui abreuve l'ennemi (M10.2, précise D-034, D-055, D-077)
- 2026-10-13 · **adoptée provisoirement**, *à valider* (tranche de M10.2 demandée par la session de coordination, sans
  Alexandre)
- **Constat** (lu dans le code, montré par le test) : les soldats marchent sur la grille des habitants, où une porte
  est une route. La légion de J1 en guerre traversait donc les portes de J2 sans s'arrêter. Elle ne passait pas sur
  ses murs, bloqués pour les habitants comme dans l'original.
- **Portes** : une porte est fermée aux soldats des joueurs qui **se battent** contre son propriétaire
  (`mp_war_is_fighting`). Pour eux, c'est un mur à abattre, comme pour les envahisseurs de l'original :
  - comme eux, le soldat cherche d'abord un chemin qui contourne les portes ennemies ; s'il n'y en a pas, il passe par
    elles. Arrivé devant, il l'attaque (dégâts de l'original : un point tous les 4 ticks par soldat, elle tombe
    au-delà de 150, avec les images de l'attaque), puis il passe. Il ne gèle ni ne tourne en rond : l'ordre de
    déplacement n'accepte que des endroits joignables portes ouvertes (règle d'origine), et la seconde recherche les
    traverse ;
  - tous les autres passent comme avant : le propriétaire, les joueurs neutres (soldats, caravanes, missionnaire), et
    chacun en paix ou pendant le préavis d'une guerre honorable. Seuls les soldats sont concernés ;
  - une porte n'est fermée qu'une fois en service (au plus un jour après sa construction, comme ses autres effets) ;
  - technique (`mp/siege.c`) : les grilles de routage sont communes à tous. Pour un soldat d'un joueur qui se bat, une
    recherche de plus, qui saute les cases que lui ferme une fonction (`map_routing_citizen_can_travel_over_land_except`),
    passe avant celle d'origine ; à chaque pas, `advance_route_tile` arrête le soldat devant une porte ennemie. Tant que
    personne ne se bat (`mp_war_any_fighting()`), et toujours en classique, la recherche d'origine est faite telle
    quelle.
- **Murs** : rien à changer. Aucun soldat ne se tient jamais sur un mur (vérifié à chaque tick dans les tests). Une
  cité entièrement murée, sans porte, ne peut pas être visée par un ordre de déplacement (règle d'origine).
- **Aqueducs** : un soldat posté avec sa légion (comme pour les bâtiments, D-077) abat la case d'aqueduc qui le
  touche, au seuil de l'original pour un aqueduc et tout bâtiment (10) :
  - un aqueduc **de César**, s'il abreuve un joueur qu'il combat : en suivant les aqueducs qui portent l'eau de ce
    joueur (ceux de César, les siens, ceux de personne : la règle de `map/water_supply.c`), on atteint un réservoir à
    lui. C'est la règle déterministe la plus simple qui ne frappe que l'ennemi : à 4 joueurs, chaque joueur des
    terres a sa ligne, et un soldat ne coupe ni l'eau d'un neutre ni celle de sa propre cité. Une fois la ligne
    coupée, les cases en amont n'abreuvent plus l'ennemi : les soldats ne cassent que ce qui l'en prive ;
  - un aqueduc **de l'ennemi** lui-même : sans cela, l'aqueduc de César réparé par lui (devenu le sien), ou un
    aqueduc tiré par lui depuis un réservoir de César, serait intouchable ;
  - jamais une case d'aqueduc posée sur une route (la route partirait avec elle ; les envahisseurs de l'original
    passent dessus). Les routes, les réservoirs et le pont de César restent indestructibles.
- **Réparer** : la case abattue devient des décombres sans propriétaire. N'importe quel joueur peut les dégager et y
  poser un aqueduc (sans zone, D-036), qui devient le sien : l'eau revient. César ne répare pas.
- **Mesuré** (légions de 16 légionnaires) :
  - `mp_war_gate` (carte pour 2 ; J2 entoure le milieu de sa cité d'un mur, deux portes sur la route principale) : en
    paix comme pendant un préavis, la légion de J1 entre par la porte en 1 741 ticks. En guerre, le premier soldat
    s'arrête devant la porte à 1 531 ticks, elle tombe 60 ticks plus tard, et les 16 soldats sont à l'étendard dans
    l'enceinte à 1 771 ticks. Pour ressortir au nord-est, ils font le tour par la brèche (676 ticks) au lieu d'abattre
    la porte est (sans la première recherche : 328 ticks, dont 88 arrêtés devant elle) ;
  - `mp_war_aqueduct` (carte pour 2, J1 dans les terres) : la légion de J2 postée le long de l'aqueduc de César laisse
    intacte la ligne qui n'abreuve personne (J1 sans réservoir), mais abat un aqueduc de J1 posé à côté d'un soldat.
    J1 joint un réservoir à la ligne : la première case tombe 46 ticks plus tard. Avec un réservoir déjà rempli :
    rien en paix ni pendant le préavis ; la première case tombe 42 ticks après le début des combats, 6 en 200 ticks
    (celles qui touchent les soldats, en aval de la coupure) ; le réservoir passe de 266 à 256 en 10 jours. La paix
    signée, J1 dégage les décombres et pose son aqueduc : de 250 à 270 (plein) en 10 jours ;
  - `mp_war_gate_neutral` (carte 1 à 3 joueurs : J3 en guerre contre J2, J1 neutre) : la légion de J1 entre dans
    l'enceinte de J2 sans s'arrêter ; sa caravane passe par la porte de J2 sur la route de son entrepôt (30 ticks sur
    la porte) et livre ses 8 chargements ;
  - joué deux fois depuis la même sauvegarde : même somme de contrôle (portes, aqueduc).
- **Point ouvert, à trancher avec D-068** (*résolu par D-085*) : une porte posée sur une route de César fait de ces cases une route du
  joueur (`map_building_tiles_add` libère la case, la porte y remet une route à lui). Abattue par l'ennemi ou supprimée
  par son propriétaire, elle emporte la route : la route principale est coupée, et les caravanes ne marchent que sur
  les routes. C'était déjà vrai avant (D-077, suppression par le joueur). Pistes : rendre ces cases à César sous la
  porte et y remettre sa route quand elle disparaît (construction, suppression, effondrement) ; ou interdire les
  portes sur ses routes (on ne fermerait plus une enceinte en travers de la route principale).
- **À valider** :
  - portes fermées aux seuls soldats des ennemis de leur propriétaire (la feuille de route disait « qui ne laissent
    passer que leur propriétaire ») ; un soldat contourne une porte quand il le peut ;
  - les soldats abattent aussi les aqueducs de l'ennemi, pas seulement ceux de César ;
  - « abreuve l'ennemi » : relié par les aqueducs à un réservoir de l'ennemi ;
  - réparer, c'est dégager les décombres et poser son propre aqueduc ; César ne répare pas.
- Pas dans cette tranche : l'ordre « attaquer » qui marche sur une cible (reste de M10.2), la terre de César autour du
  pont (D-068).
- Classique : inchangé.

### D-082 — La terre de César autour du pont : rien ne s'y bâtit (M10.2, applique l'option 1 de D-068)
- 2026-10-13 · **adoptée provisoirement**, **à valider** par Alexandre (15 cases, et « rien ne s'y bâtit »)
- **Où** : les cases à 15 au plus (distance de Tchebychev, `MP_TERRITORY_CAESAR_LAND_RADIUS`, un seul réglage) de
  chaque entrée du pont, c'est-à-dire des deux cases de rive contre lui. Elles sont marquées à la création de la
  carte préparée (`mp/mapgen`, `mp_territory_mark_caesar_land`), sur les cartes 1 et 2, à 1, 2, 3 ou 4 joueurs. À
  1 ou 2 joueurs, le pont mène à la rive nord sauvage et sa terre existe aussi.
- **Où c'est gardé** : dans la grille des zones (`territory_grid`, valeur 255, qu'aucun joueur n'a). Aucun
  changement de format : une partie sauvegardée la garde, une sauvegarde d'avant se charge sans terre de César
  (rien à migrer, la zone s'étend alors comme avant). `mp_territory_owner` rend -1 sur ces cases ; la revendication
  d'une zone ne prend que les cases libres, donc aucune zone ne les recouvre.
- **Effets** (cités des joueurs, mode multijoueur seulement) :
  - porte, tour, fort et tout bâtiment qui demande une zone : refusés par la règle de zone existante, au menu comme
    par la commande (même fonction, `building_construction_place_building`) ;
  - mission (qui ne demande pas de zone) : `mp_mission_allows_place` refuse ces cases ;
  - murs : refusés (tracé et commande), le tracé contourne la terre ou s'arrête ; `callback_calc_distance_build_wall`
    ne la traverse pas ;
  - routes : permises.
- **Pour la suite** : `mp_territory_is_caesar_land(grid_offset)` dit si une case est à César (0 hors des cartes
  multijoueur). La colère des légions qui s'y trouvent (M10.5) et leur retour au fort à la paix (M10.1, M10.5) s'en
  serviront ; pas faits ici.
- **Porte sur une route de César ailleurs** (question laissée ouverte par DESIGN §7.1, vérifiée par
  `mp_caesar_land_*`) : près du pont, impossible. Au-delà de la terre de César, une porte **peut** être posée sur
  une route de César si l'endroit est dans la zone du joueur ; non modifié. À trancher avec M10.2 (portes qui ne
  laissent passer que leur propriétaire).
- **Tests** : `simtool caesarland` (`mp_caesar_land_{1,2,4}_players` et `mp_caesar_land_map2_*`) : étendue exacte
  de la terre, zone d'une mission qui ne la prend pas, préfecture, porte, tour, mission et mur refusés dedans et
  permis juste dehors, route permise, partie sauvegardée et relue. Chaque contrôle échoue quand on inverse sa ligne
  (marquage, mur, mission, relecture).
- Pas de nouvelle version de protocole dans cette tâche (la carte préparée change : à signaler à la fusion).
- Classique : inchangé (la grille reste vide ; `mp_territory_is_caesar_land` rend 0 hors multijoueur).

### D-083 — Javeliniers renforcés et bois à la caserne (MG.1 à MG.4, applique D-058)
- 2026-10-14 · **adoptée provisoirement** (D-058 fixait le plan ; les chiffres restent à régler en MG.5)
- **MG.1** : en multijoueur, `FIGURE_FORT_JAVELIN` passe à **136 points de vie et 10 d'attaque** (80 et 4 en
  classique). Portée, tir (4 dégâts, délai 100), défenses, légionnaires (150 × 10) et cavaliers (120 × 8) inchangés,
  ainsi que les javeliniers ennemis. Une seule ligne de table de plus dans `figure/properties.c`, choisie par
  `game_rules_is_multiplayer()` ; test `mp_troops_javelin`.
- **MG.2** : la caserne a un **stock de bois séparé** des armes (`building/barracks.c`, 5 chargements au plus).
  - Livraison : les mêmes chariots que les armes (`building_warehouse_determine_worker_task`, priorité 1 du
    porteur de l'entrepôt, `building_get_barracks_for_weapon`), seulement si la cité a un fort de javeliniers, que
    la caserne est en service sur le même réseau de routes et que son stock est sous la limite (l'entrepôt envoie
    jusqu'à 4, comme pour les armes). Déterministe : aucune nouvelle donnée hors du stock.
  - Recrutement : sans bois, aucune légion de javeliniers n'est servie ; un javelinier par chargement ; les armes
    restent aux légionnaires (qui passent avant) ; les cavaliers restent gratuits.
  - Sauvegarde : un entier **à la fin de la pièce `extra_state` de chaque cité** (aussi dans la somme de contrôle) ;
    une partie plus ancienne (pièce plus courte) se charge avec 0 bois, sans changer la version du `.mpsav`.
    Le stock est une région du contexte de cité.
  - Classique : javeliniers gratuits, aucun bois livré (parité verte).
  - Mesuré (`mp_barracks_timber`, carte préparée à 2) : 8 chargements dans l'entrepôt donnent 8 javeliniers, jamais
    plus de 4 chargements en attente à la caserne ; sans fort de javeliniers, rien ne part.
- **MG.3** : **déjà fait par D-062** : `mp_prepared_map_{2,3,4}_players` et `mp_prepared_map2_*` vérifient par
  emplacement (`slot_follows_plan`) que le bois n'est permis qu'aux côtiers, dans les terres jamais. Rien changé.
- **MG.4** : la fiche de la caserne montre les armes et, en multijoueur, le bois (icône, « Bois des javeliniers : N ») ;
  la fiche du fort et celle de la légion disent ce qu'une recrue coûte et le stock de la caserne (« Une charge de
  bois par recrue. En caserne : N », en rouge « La caserne attend du bois pour recruter des javeliniers. » à sec ;
  « Recrues : gratuites » pour les cavaliers). Textes français et anglais, clés ajoutées à la fin du bloc `TR_MP_*`.
  Captures : `tools/serial.sh tools/mg-barracks-test.sh` (`build/automation/mg-*.png`).
- *À valider* : plafond de 5 chargements ; texte rouge d'attente ; le stock de bois est celui d'une seule caserne
  (une par cité, comme les armes) ; le bois d'un entrepôt sert d'abord à la caserne, avant les ateliers de meubles.
- Classique : inchangé.

### D-084 — César juge les guerres : motif, prix, triomphes, colère, armée trop puissante (M10.4, M10.5, M10.7, précise D-053, D-080)
- 2026-10-14 · **adoptée provisoirement**, *à valider* (tâches M10.4, M10.5 et M10.7 ; Claude a tranché les points
  que CESAR §7 laissait ouverts)
- **Module** `mp/wrath.c`, branché sur `mp/war.c` par des appels d'une ligne (déclaration, début des combats, retrait,
  paix, bâtiment abattu, caravane prise) et sur le combat (`figure/combat.c`, `figuretype/missile.c` : combat engagé,
  figure tuée), une fois pour le monde après les cités (`game/tick.c`). Son état (motif, forme et agresseur de chaque
  paire, prix payé, mois de combats, légions du mois, dernier acte hostile et dernière fin de guerre par paire
  orientée, dégâts du mois, ennemi de Rome jusqu'à, triomphes, pertes des légions, étape de la jauge, ultimatum,
  expédition due) est dans le morceau `mp_wrath` du `.mpsav` (version 1) et donc dans la somme de contrôle. Une
  partie plus ancienne se charge : ses guerres en cours sont sans motif et déjà payées, rien d'autre. La jauge et les
  parts restent dans `mp_caesar`. Rien en classique ; en paix, seules la baisse de la jauge et l'armée (M10.7)
  tournent.
- **Motif**, déterminé à la déclaration (aussi quand le défenseur frappe le premier pendant le préavis : la guerre
  devient la sienne, avec son motif), dans cet ordre :
  1. **sans motif**, toujours, si la dernière guerre que le déclarant a déclarée à la même cible a fini (paix signée,
     imposée, ou déclaration retirée) il y a moins de 12 mois ; sa colère compte double ;
  2. **mandat de César** si la cible est ennemie de Rome ;
  3. **riposte** si, dans les 12 derniers mois (16 jours de 50 ticks par mois), la cible a déclaré la guerre au joueur
     (menace retirée comprise), engagé un combat avec une de ses figures, abattu un de ses bâtiments ou pris une de
     ses caravanes ;
  4. **sans motif** sinon.
- **Ennemi de Rome** pour 12 mois : le déclarant d'une guerre sans motif, dès que ses combats commencent (une menace
  retirée n'en fait pas un) ; le principal responsable d'un ultimatum. Pas d'annonce : la fenêtre de guerre (« Mandat
  de César »), le conseiller impérial et la lettre d'ultimatum le disent.
- **Prix**, en lauriers de César (source « guerres »), payé par le déclarant **quand les combats commencent** (D-080) :
  à la fin du préavis, ou tout de suite pour une guerre brutale. Sans motif −15, brutale −15 de plus (une riposte
  brutale coûte 15, une guerre brutale sans motif 30). Une menace retirée ne coûte rien. Le −40 d'une guerre sans
  motif contre un joueur dont les légions sont en campagne pour César attend les campagnes (M9.7) : **pas fait**.
- **Triomphe** : +20 lauriers au joueur dont une figure tue le dernier soldat d'une légion d'un ennemi avec qui il se
  bat, si cette légion a perdu au moins 4 soldats sous les coups de ses ennemis depuis sa dernière destruction, et
  sauf s'il est le déclarant d'une guerre sans motif contre ce joueur. Annoncé à tous.
- **Colère**, une fois pour le monde au début de chaque mois, après les cités, dans cet ordre :
  1. chaque paire de joueurs, par numéros croissants, qui s'est battue pendant le mois (un relevé à la fin de chacun
     des 16 jours) : le n-ième mois de combats ajoute n points, la puissance 2 × L² (L : le plus grand nombre de
     légions de l'agresseur, le déclarant, dont l'étendard était dans le territoire du défenseur à la fin d'un jour
     du mois ; ou sur la terre de César autour du pont : crochet `stands_on_caesar_land`, à brancher à la fusion sur
     `mp_territory_is_caesar_land`, D-082), et les
     dégâts de l'agresseur : 1 par bâtiment ou mur abattu, 2 par caravane prise, 1 par 10 habitants tués. Le tout
     × motif (sans motif 1, riposte ½, mandat ¼) × 1,5 pour une guerre brutale × 2 pour une guerre redéclarée. Les
     dégâts du défenseur comptent à part, comme une riposte (× ½), dans sa propre part ;
  2. le délai d'un ultimatum (voir plus bas) ;
  3. les seuils, sur la jauge **avant** sa baisse : sinon, plafonnée à 100, elle ne pourrait jamais l'atteindre ;
  4. la baisse : −3 si personne ne s'est battu du mois, −1 sinon ; chaque part (belligérance) baisse en proportion,
     arrondie vers le bas.
  Tout est entier, en dixièmes de point : l'ajout mensuel de chaque joueur pour une guerre est arrondi au dixième le
  plus proche, une moitié vers le haut (un premier mois sous mandat : 0,25 → 0,3).
- **Habitants tués** : les figures non militaires de la cité (passants, préfets, sentinelles) tuées par l'ennemi ; ni
  les soldats, ni la population des maisons abattues (l'exemple de CESAR §7.2 compte 8 bâtiments pour 8 points).
- **Étapes** : franchir 40 → lettre de César à tous, qui nomme les joueurs ayant une part de la colère, une seule tant
  que la jauge ne repasse pas sous 40. Franchir 70 → ultimatum, un seul à la fois : lettre, paix dans 3 mois, le
  principal responsable (plus grande part, plus petit numéro à égalité) ennemi de Rome. Au délai, César impose la paix
  à toutes les guerres déclarées, préavis compris, comme une paix signée (combats arrêtés, légions rentrées, le
  déclarant lié 12 mois), avec une annonce et une lettre. Atteindre 100 → l'expédition punitive est **due**
  (`mp_wrath_expedition_due`, sauvegardée) ; rien n'est annoncé ni envoyé : c'est M10.6.
- **Armée trop puissante** (M10.7) : chaque mois, dans le tour de chaque cité, César tolère max(2, population / 5 000)
  légions ; chaque légion de plus coûte 2 lauriers (source « armée »), en paix comme en guerre, seul aussi. Une
  légion compte dès qu'elle a un soldat : un fort vide ne compte pas.
- **Interface** :
  - fenêtre « Guerre et paix » : sous les boutons de chaque joueur, ce que vaut la déclaration (celle qui attend son
    second clic ; honorable sinon, brutale pendant un préavis) : « Sans motif : -15 lauriers au début des combats »,
    « Sans motif : -30 lauriers tout de suite ; colère double (guerre récente) », « Riposte : juste ; brutale : -15
    lauriers tout de suite », « Mandat de César : juste » ; pendant le préavis et la guerre, son motif et son mois ;
  - conseiller militaire : « En guerre contre le joueur 2 » ou « Préavis : combats contre le joueur 2 dans 30 jours »
    à la place de la ligne des ennemis ;
  - bandeau : « Colère 42 » et une jauge, verte, jaune dès 40, rouge dès 70, toujours visible en multijoueur ;
  - lettres de César : avertissement, ultimatum, paix imposée ;
  - conseiller impérial : la colère, la part de chacun, les ennemis de Rome, le délai de l'ultimatum, les lauriers des
    guerres et de l'armée, les légions et celles que César tolère.
- **Mesuré** (carte préparée pour 2) :
  - CESAR §7.2, frappe ciblée sans motif (1 légion, 3 mois de combats dont 2 avec l'étendard chez l'adversaire,
    8 bâtiments et 3 caravanes) : 1, 4 puis 19 points, **24** en tout ; la jauge à 21 après ses baisses du mois, à 0
    en 7 mois de paix. La même en guerre brutale : 1,5, 6 puis 28,5 points, **36**. Redéclarée moins de 12 mois après
    sa paix, en guerre brutale : 3 points le premier mois ;
  - le 4e mois d'une guerre brutale sans motif ajoute 6 points, le premier d'une guerre sous mandat 0,3 (à trois) ;
  - jauge à 10 points, en paix : 7 un mois plus tard (parts 4,2 et 2,8), 0 au bout de 4 mois ;
  - triomphes : 16 soldats du défenseur détruisent les 8 du déclarant d'une guerre sans motif en 550 ticks (14
    survivants) : +20 lauriers au défenseur ; 16 soldats détruisent une légion de 4 en 420 ticks : rien pour le
    déclarant d'une guerre sans motif, +20 dans une riposte (−15 + 20 = +5 lauriers de guerre) ;
  - armée : seul, 3 légions sans habitants : −2 lauriers par mois, 4 légions : −4.
- **Tests** : `mp_war_motives` (motifs, prix au début des combats, menace retirée gratuite, 12 mois de la riposte au
  tick près, paix qui lie le déclarant, actes hostiles, triomphes, mandat à trois, rien en classique),
  `mp_caesar_wrath` (exemples de §7.2, baisse, double colère, avertissement, ultimatum, paix imposée, ennemi de Rome,
  partie reprise au milieu d'un ultimatum et rejouée deux fois : l'état de César et des guerres identique à chaque
  tick, toute la somme de contrôle à 168 ticks dont chaque tick autour des débuts de mois, expédition due, partie
  plus ancienne, rien en classique), `mp_army_tolerance` (seuils, seul, à deux, rien en classique). Chaque
  vérification nouvelle échoue quand on inverse la ligne qui la fait passer (22 inversions). Captures :
  `tools/mp-wrath-test.sh` (commandes de test `addwrath` et `caesarletter`, partie seule), `tools/mp-war-test.sh`
  (fenêtre de guerre et conseiller militaire).
- **À valider** :
  - la paix ne lie que le déclarant de la guerre finie : sa victime garde son motif de riposte (CESAR §7.1 disait
    « redéclarer la guerre au même joueur » sans dire qui) ;
  - le mandat passe avant la riposte quand les deux s'appliquent (colère au quart plutôt qu'à moitié) ;
  - ennemi de Rome au début des combats d'une guerre sans motif, pas à sa déclaration ;
  - triomphe seulement pour une légion qui a perdu au moins 4 soldats sous les coups de ses ennemis ;
  - les dégâts du défenseur comptent comme une riposte, dans sa part ;
  - habitants tués : les figures de la cité, pas la population des maisons abattues ;
  - les relevés de chaque jour (étendards, guerre en cours) plutôt qu'un seul en fin de mois ;
  - l'ennemi de Rome n'est pas annoncé ;
  - une légion compte pour César dès son premier soldat.
- Réseau : nouveau morceau de sauvegarde, nouvel état de la simulation : protocole à monter à la fusion.
- **À la fusion avec D-081 et D-082** : les légions de l'agresseur sur la terre de César (autour du pont) comptent dans
  la puissance comme dans le territoire adverse (`mp_territory_is_caesar_land`) ; une porte, un mur ou un aqueduc
  abattu par les soldats (`mp/siege.c`) compte comme un bâtiment détruit (`mp_war_gate` le vérifie, échoue sans).
- Réseau : protocole 29.


### D-085 — La route de César sous un bâtiment lui revient quand le bâtiment disparaît (M10.2, résout le point ouvert de D-081, précise D-034)
- 2026-10-14 · **adoptée provisoirement**, *à valider* par Alexandre
- **Constat** : une porte posée sur la route de César (permise dans la zone du joueur, hors de sa terre, D-082) libérait
  les cases (`map_building_tiles_add` retire la route, donc la propriété) et la porte y remettait une route du joueur.
  Supprimée, abattue, brûlée ou effondrée, elle laissait de l'herbe ou des décombres : la route principale était coupée
  pour tous, et les caravanes ne marchent que sur les routes.
- **Quels bâtiments** : les seuls qui se posent sur une route (masque `~TERRAIN_ROAD` de `building_construction_place_building`) :
  la **porte** (2×2) et l'**arc de triomphe** (3×3). La place n'est pas un bâtiment (un drapeau sur la route, qui reste).
  Tour, entrepôt et autres exigent un terrain libre de route.
- **Règle** : César garde sa revendication sur la case sous le bâtiment, dans la grille des propriétaires déjà
  sauvegardée : aucun nouveau champ, aucun changement de format, une partie d'avant se charge (ses portes ont alors des
  cases du joueur, comme avant). « Revendiquée par César et sous un bâtiment » veut dire « sa route est dessous ».
  - pose (`map_building_tiles_add`) : les cases qui étaient une route de César, ou qui le sont sous le bâtiment (rotation de
    la vue, qui repose les portes), gardent César ; `update_owner` (`map/terrain.c`) ne libère pas une case de César sous un
    bâtiment quand la route en est retirée (les côtés de l'arc) ;
  - disparition (`map_building_tiles_remove`, `map_building_tiles_set_rubble` : suppression, abattage, effondrement,
    annulation) : chaque case que César revendique sous le bâtiment redevient sa route, sans décombres, puis les images des
    routes et les grilles de routage sont refaites (`map_tiles_update_area_roads`, `map_routing_update_land`), comme à la pose
    d'une route ; blanche et indestructible, comme le reste de sa route ;
  - feu (`destroy_on_fire`) : pas de ruine en flammes sur sa route (comme sur l'eau) : elle revient tout de suite ;
  - effacer (`construction_clear`) : son propriétaire peut toujours supprimer son bâtiment, même si la case est à César
    (le test « appartient à un autre joueur » ne regarde plus la case quand il y a un bâtiment à soi) ;
  - tant que la porte tient, tout est comme D-081 (fermée aux seuls soldats des ennemis de son propriétaire).
- **Mesuré** (`mp_caesar_road_under_gate`, carte pour 2, J2 s'entoure de murs et de deux portes sur la route principale,
  un arc de triomphe aussi) : sous les portes, César garde ses 2 cases chacune (les 2 autres sont au joueur et ne
  reviennent pas), 3 sous l'arc ; sauvegardée puis relue, la partie garde ces revendications ; supprimées par J2, les
  routes de César sont là sans décombres, personne ne peut les effacer, et la caravane de J2 sort de sa cité close et
  livre ses 8 charges en 61 jours ; abattue par la légion de J1 en guerre (1 591 ticks), la porte ouest laisse la route ;
  brûlées et effondrées, portes et arc rendent la route ; même somme de contrôle en rejouant.
- **À valider** : une ruine en flammes ne se pose pas sur la route de César (elle revient d'un coup) ; l'arc de triomphe
  est traité comme la porte.
- Pas de nouvelle version de protocole (la grille des propriétaires a le même format ; la somme de contrôle d'une partie en
  cours ne change que pour les portes posées sur la route de César).
- Classique : inchangé (aucune case n'y est à César).
- **Arc de triomphe par commande** (relevé par la revue de D-085) : la commande de construction refusait déjà ce que
  le menu refuse pour les matières et la nourriture (D-065), mais pas l'arc de triomphe, que le menu n'offre qu'à une
  cité qui l'a gagné. Elle le refuse désormais aussi, en multijoueur (`mp/session.c`) ; `mp_caesar_road_under_gate`
  le vérifie (échoue sans le refus).

### D-086 — L'expédition punitive de César : ses légions dans chaque cité, la disgrâce, les pertes des fautifs, la seconde expédition (M10.6, précise D-053, D-084)
- 2026-10-14 · **adoptée provisoirement**, *à valider* (tâche M10.6 ; Claude a tranché les points que CESAR §7.4 laissait
  ouverts)
- **Module** `mp/expedition.c`, appelé une fois pour le monde après la colère du mois et avant la fin de partie
  (`game/tick.c`). Son état (expéditions envoyées, mois de disgrâce ; par cité : état de la dernière expédition, mois
  restants, soldats débarqués, lauriers perdus) est le morceau `mp_expedition` du `.mpsav` (version 1), donc dans la
  somme de contrôle. Une partie plus ancienne se charge sans expédition ni disgrâce ; une expédition qu'elle notait due
  (D-084) part le jour suivant. Rien en classique.
- **Quand** : au bilan du mois où la jauge atteint 100 (lue avant sa baisse, D-084), le même tick, après la baisse du
  mois : les parts sont celles que la baisse a laissées.
- **Les légions** : les légionnaires de César de l'original (`FIGURE_ENEMY_CAESAR_LEGIONARY`, attaque 13, défense 2),
  en colonne, débarquent au point d'arrivée de chaque cité (celui des immigrants), dans le contexte de la cité, et
  visent ses plus beaux bâtiments : c'est le code de l'invasion de César d'origine (`start_invasion` de
  `scenario/invasion.c`, coupé en deux pour sauter l'ajustement de la difficulté :
  `scenario_invasion_start_caesar_expedition`), avec son message « Une légion nous attaque » et son bouton vers les
  légions. Nombre, avec p la part d'un joueur et P celle du principal responsable (plus grande part, plus petit
  numéro à égalité), en entiers arrondis vers le bas :
  - cité innocente (part nulle) : **32** ;
  - principal responsable : **96 + 48 × P / (somme des parts)** : 144 s'il porte toute la colère, 120 à parts égales
    entre deux, 108 entre quatre ;
  - autre responsable : **32 + (soldats du principal − 32) × p / P** : autant que le principal à part égale ;
  - la **difficulté ne change pas** ces nombres (l'original réduit ou grossit toute invasion, celle de César comprise,
    de 40 à 120 %) ;
  - au plus 150 (borne de l'original, jamais atteinte), et jamais plus que les figures que la cité peut encore créer
    (1 000 par cité ; sinon les soldats de trop écriraient dans la figure 0, commune à toutes) ; formations de
    l'original : une jusqu'à 16 soldats, deux jusqu'à 32, trois au-delà ;
  - **« Invasions IA : non » ne l'arrête pas** : ce n'est pas une invasion de l'IA, son chemin ne lit pas cette règle.
- **Combat** : comme les envahisseurs de l'original, dans leur cité : ils ne cherchent leurs cibles que parmi ses
  figures et ses bâtiments (D-027), et leurs dégâts comptent pour elle (paix, bâtiments détruits). Les règles de
  retraite par la faveur ne s'appliquent pas (`process_caesar_invasion` reste neutralisé). Comme ceux de l'IA, un
  soldat de César qui marche sur la case d'une figure d'une autre cité l'attaque (le combat au contact ne regarde pas la
  cité, déjà vrai pour les invasions de l'IA) : à surveiller, même si chaque point d'arrivée est devant sa cité.
- **Vaincue** : quand plus aucun soldat de l'expédition n'est debout dans la cité avant le terme (tués, ou enfuis parce
  que leur moral s'effondre, ce que l'original compte aussi comme une victoire), relevé chaque jour : la cité gagne
  **20 lauriers** (source « expédition », `MP_LAURELS_PUNISHMENT`), son joueur reçoit une lettre, tous une annonce
  (« Joueur 1 a vaincu les légions de César »).
- **Repartie** : au bout de **12 mois** (comptés aux bilans de mois), les légions repartent vers leur point d'arrivée,
  comme celles de l'original qui se retirent (`formation_caesar_retreat`, redit chaque jour pour les soldats qui se
  battaient), et disparaissent au bord de la carte : rien à gagner ; une lettre au joueur de la cité, une annonce à
  tous.
- **Pertes** (source « expédition ») : le principal responsable perd **25 %** de ses lauriers, un autre responsable
  **25 % × p / P** des siens, arrondi vers le bas ; rien si ses lauriers sont nuls ou négatifs. Les innocents ne
  perdent que la disgrâce et les dégâts.
- **Paix** : toutes les guerres déclarées, préavis compris, prennent fin comme une paix signée (D-084 : combats arrêtés,
  légions rentrées, déclarant lié 12 mois), avec l'annonce « paix imposée par César » ; l'ultimatum en cours prend fin
  avec elles, sans lettre de paix imposée (celle de l'expédition le dit). À ce moment, une guerre peut être en préavis
  ou en combat : ultimatum émis le même mois ou avant et pas encore échu, ou guerre déclarée après une paix imposée.
  Si le délai d'un ultimatum échoit le même mois, sa paix est déjà imposée (avant les seuils).
- **Jauge** : ramenée à **30** ; chaque part suit en proportion (× 30 / jauge, arrondie vers le bas, comme la baisse
  mensuelle). Les étapes repartent de là : nouvel avertissement à 40, nouvel ultimatum à 70, nouvelle expédition à 100.
- **Disgrâce** : pendant les **12 bilans de mois** suivants, les lauriers des cinq notes de chaque cité sont divisés
  par deux (poids × note / 2, les restes gardés d'un mois à l'autre) ; le coût de l'armée trop puissante, les cadeaux
  et le reste ne changent pas. Le conseiller impérial affiche les mois restants, et les lauriers de l'expédition.
- **Seconde expédition** : option du salon « Seconde expédition de César : elle frappe comme la première » (par
  défaut, D-053) ou « Rome reprend la province ». C'est une règle de la partie comme les autres
  (`rome_takes_province`, à la fin des règles : sauvegardée, envoyée avec l'accueil et les messages du salon, montrée à
  tous, l'hôte seul la change ; 0 dans les parties plus anciennes). Sans elle, chaque expédition frappe comme la
  première (mêmes nombres, mêmes pertes, nouvelle disgrâce de 12 mois). Avec elle, quand une expédition est due et
  qu'une autre est déjà partie, aucune ne part : la partie s'arrête sans vainqueur
  (`mp_endgame_rome_takes_province`, gardé dans le morceau `mp_endgame`), l'écran de fin titre « Rome reprend la
  province » et donne les lauriers de chacun, sans héritier. La première frappe normalement.
- **Lettres** (français et anglais) : au débarquement, à tous (soldats dans la cité du joueur, principal responsable
  et ses 25 %, lauriers perdus par le joueur, 12 mois de disgrâce, 20 lauriers pour qui détruit les légions) ; à la
  cité qui les détruit ; à la cité qu'elles quittent.
- **Seul** : sans adversaire, ni guerre ni colère (il n'y a pas de paire de joueurs) : pas d'expédition. La commande de
  test `addwrath` peut en forcer une (captures).
- **Salon** : la règle est sur deux lignes à droite, sous « Rejoindre » : la colonne des règles n'a plus de place, et
  les scripts d'automatisation cliquent les règles là où elles sont.
- **Mesuré** :
  - carte pour 2, difficile (`mp_caesar_expedition`) : le joueur 2 mène une guerre brutale sans motif et porte toute la
    colère : **144** soldats chez lui (3 formations de 48), **32** chez le joueur 1 (2 de 16) ; il perd 12,5 de ses 50
    lauriers, le joueur 1 rien ; paix, jauge à 30. Les **4 légions** du joueur 1 (64 légionnaires), envoyées sur
    elles, détruisent ses 32 soldats de César en **1 300 ticks** (un mois et demi) et en perdent 32 : +20 lauriers. Les
    144 du joueur 2 (sa légion de 16 reste au fort) sont tous debout au bout de 12 mois, puis repartent : partis
    **350 ticks** plus tard. Seconde expédition, option désactivée : 32 et 144 soldats de plus, 9,3 lauriers perdus sur
    37,5 ; option activée : fin sans vainqueur ;
  - carte pour 3, facile, parts de 60 et 40 % (58,2 et 38,8 après la baisse du mois) : **124** et **93** soldats chez
    les responsables, **20** chez l'innocent, qui ne pouvait plus créer que 20 figures (32 sinon) ; pertes de 25 et
    16,6 lauriers sur 100 ; personne ne les combat : toutes repartent au bout de 12 mois, parties 300 ticks plus tard ;
  - Valentia seule, jauge forcée : 144 soldats ; lauriers des notes 35,8 sur les 12 mois de disgrâce (35,7 attendus
    pour la moitié des notes de chaque mois), puis 14,2 en 3 mois (14,0 attendus en entier).
- **Tests** : `mp_caesar_expedition` (ce qui précède, avec « Invasions IA : non » ; partie reprise pendant
  l'expédition et relue deux fois : identique tick après tick sur 1 700 ticks ; partie plus ancienne ; seul ;
  classique), `mp_caesar_wrath` (à 100, l'expédition part), `mp_lobby_rules` et `mp_lan_lobby_rules` (règle par défaut,
  changée après « Héberger », vue par les clients), `mp_lobby_bad_rules` (une valeur hors de oui/non est refusée).
  Chaque vérification nouvelle échoue quand on inverse la ligne qui la fait passer (33 inversions) ; seules ne s'inversent pas celles qui relèvent de l'existant (caractéristiques des
  légionnaires de l'original, jauge immobile seul et en classique). Captures : `tools/mp-expedition-test.sh` (salon,
  message et lettre, légions dans la cité, conseiller impérial, lettres, écran de fin).
- **À valider** :
  - la difficulté ne change pas le nombre de soldats (à « facile », l'original en enverrait 60 %) ;
  - les autres responsables : soldats et pertes selon leur part rapportée à celle du principal ;
  - des légions enfuies par leur moral comptent comme vaincues (20 lauriers) ;
  - la disgrâce ne touche que les lauriers des notes ;
  - les lettres « vaincue » et « repartie » ne vont qu'au joueur de la cité, une annonce aux autres ;
  - la place de la règle dans le salon (à droite, sous « Rejoindre »).
- Réseau : une règle de plus dans les messages du salon et de l'accueil, un morceau de plus dans l'état : protocole à
  monter à la fusion.
- Classique : inchangé (le découpage de `start_invasion` ne change rien : tests de parité verts).

### D-087 — L'ordre « attaquer » d'une légion en guerre (M10.2, précise D-077, D-081)
- 2026-10-14 · **adoptée provisoirement**, *à valider* (reste de M10.2, demandé par la session de coordination, sans
  Alexandre)
- **Interface** : bouton « Attaquer » dans la fenêtre de la légion (clic droit sur ses soldats), à côté de « Retourner
  au fort » (seul au milieu quand elle est au fort). Il n'est offert qu'en multijoueur, quand le joueur se bat contre
  un autre (combats, pas pendant un préavis), à une légion qui a des soldats et peut bouger. Il arme le choix de la
  cible comme l'ordre de déplacement : la vue militaire dit « Cliquez sur la cible : un bâtiment ou une case de
  l'ennemi » ; un clic sur un bâtiment d'un ennemi, ou sur une case de sa zone (ou une route à lui), à 10 cases au plus
  d'un de ses bâtiments, donne l'ordre ; ailleurs elle dit « Aucun bâtiment d'un ennemi en guerre à portée de cette
  case » et attend un autre clic ; un clic droit annule. Pendant l'attaque, le bouton dit « Attaque en cours » (un clic
  choisit une nouvelle cible).
- **Commande** `MP_ACTION_LEGION_ATTACK` (35 : légion, x, y). Refusée, sans rien changer : légion d'un autre joueur,
  sans soldat, maudite par Mars, en campagne lointaine ou démoralisée ; case qui n'est pas à un joueur qu'il combat
  (paix, préavis, joueur neutre, sa propre cité, César, personne) ; aucun bâtiment de ce joueur à 10 cases de la case,
  ou aucun à côté duquel la légion peut se tenir.
- **Conduite de la légion** (`mp/assault.c`, chaque tick, une fois pour le monde après les cités) :
  - elle va à côté du bâtiment choisi, ou du plus proche de la case choisie : l'étendard (où se tient son premier
    soldat) sur une case sans bâtiment qui le touche, joignable d'où elle est, la plus proche d'elle ;
  - ses soldats postés abattent ce qui de l'ennemi les touche, comme avant (D-077, D-081 : bâtiments, murs, portes,
    aqueducs), y compris juste au-delà du rayon ;
  - quand son bâtiment est tombé et que plus rien de l'ennemi ne touche ses soldats postés, elle passe au suivant : **le
    bâtiment de cet ennemi le plus proche de son étendard parmi ceux à 10 cases au plus de la cible choisie** (le plus
    grand des deux écarts, `MP_ASSAULT_RADIUS`), de toute sorte, le plus petit numéro à égalité. Les envahisseurs de
    l'original (`figure/formation_enemy.c`) choisissent dans toute la cité par une liste de priorités (greniers, sénat,
    maisons riches…) puis par la distance ; ici le joueur a choisi le quartier, la légion le rase de proche en proche ;
  - elle attend tant qu'un soldat marche vers l'étendard, nettoie (déploiement « nettoyer ») ou se bat ; un bâtiment
    près duquel aucun soldat n'arrive à se tenir est laissé au bout de 2 jours (100 ticks) pour le suivant ;
  - en chemin, elle se bat comme toute légion en guerre ; portes et murs : D-081.
- **Fin de l'ordre** : plus aucun bâtiment de l'ennemi dans le rayon (la légion reste où elle est) ; paix avec lui,
  signée, imposée ou par retrait (à la paix elle rentre comme les autres, D-077 ; si son joueur combat encore un tiers,
  elle reste dehors mais n'attaque plus) ; un autre ordre du joueur (déplacement, retour au fort) ; plus de soldat
  dehors (morts, fuite, renvoyée au fort), campagne lointaine, malédiction.
- **Sauvegarde** : morceau `mp_assault` du `.mpsav` (version 1 : légion, ennemi, cible, bâtiment visé, attente), donc
  dans la somme de contrôle ; une partie plus ancienne se charge sans ordre. Classique : inchangé.
- **Mesuré** (`mp_war_attack_order`, carte préparée pour 2, légion de 16 légionnaires partie de son fort ; J2 a 7
  bâtiments à 10 cases d'une statue — 3 statues, une grande statue, un puits, 2 préfectures — et 4 au-delà de 16) :
  - la statue choisie tombe 1 784 ticks après l'ordre (la marche) ; la légion va ensuite à 5 autres bâtiments, un 7e
    tombe en passant ; l'ordre finit au tick 2 350 (566 ticks pour 7 bâtiments) : plus rien dans le rayon, rien au-delà
    de 16 cases, la légion reste sur place avec ses 16 soldats ;
  - jouée deux fois : même somme de contrôle ; sauvegardée au milieu (2 bâtiments tombés) puis relue : même somme de
    contrôle tous les 5 ticks pendant 2 000 ticks ;
  - paix signée après le premier bâtiment : l'ordre finit au tick même, la légion est au fort 1 750 ticks plus tard,
    plus rien ne tombe.
- **Tests** : `mp_war_attack_order` (refus en paix, pendant le préavis, pour la légion d'un autre, sur sa propre cité,
  sur une terre ennemie sans bâtiment dans le rayon, sur un joueur neutre à trois ; ordre sur la statue et sur une
  case ; de bâtiment en bâtiment dans le rayon ; fin à la paix, à la paix avec la cible pendant une guerre avec un
  tiers, à un autre ordre, sans soldat ; sauvegarde au milieu, partie plus ancienne, somme de contrôle, rejeu ; rien en
  classique). 18 inversions d'une ligne : chacune fait échouer sa vérification (le refus d'une terre sans bâtiment est
  vérifié aussi à la fenêtre ; le classique n'a jamais de guerre, sa vérification garde contre une régression). Le
  bâtiment laissé au bout de 2 jours n'est pas couvert. Vrai jeu : `tools/mp-attack-test.sh` (partie à deux écrite par
  `simtool attacksave`, sans brouillard ; fenêtre de la légion, choix de la cible, légion à l'œuvre, quartier rasé chez
  les deux joueurs, sans désynchronisation).
- **À valider** :
  - un rayon de 10 cases autour de la cible choisie, plutôt qu'une marche sans fin dans la cité ennemie ;
  - le bâtiment le plus proche, de toute sorte, plutôt que les priorités des envahisseurs ;
  - à la fin de l'ordre, la légion reste sur place (elle ne rentre pas) ;
  - la paix avec la cible arrête l'ordre même si le joueur combat encore un tiers ;
  - on peut viser une case dans le brouillard : la simulation, connue de tous, dit s'il y a une cible (D-038 accepte
    déjà qu'un tricheur voie à travers).
- Pas fait : un ordre qui poursuit les soldats ennemis (l'ordre « nettoyer » existe), un curseur propre au choix de la
  cible, le bouton dans la barre militaire de Julius (option `ui_show_military_sidebar`).
- Réseau : nouvelle commande, nouvel état de la simulation : protocole à monter à la fusion.

### D-088 — Moral et totaux par camp, arcs de triomphe, batailles scriptées ; les javeliniers tirent plus vite (M10.8, MG.5, précise D-058, D-077, D-083, D-084)
- 2026-10-14 · **adoptée provisoirement**, *à valider* (tâches M10.8 et MG.5 ; Claude a tranché)
- **Le moral d'origine** (`figure/formation.c`, `figure/formation_legion.c`) :
  - plafond par troupe : légionnaires 80 (100 après l'académie), javeliniers et cavaliers 60 (80) ; une légion naît à
    50, gagne 5 par mois au fort et, dehors, perd 5 par mois après 3 mois sans combat ;
  - un soldat tué retire 5 à 20 points selon la part de la formation qu'il représentait (5 dans une légion de 16) ;
  - chaque jour (`formation_update_monthly_morale_deployed`, quotidienne malgré son nom), une formation à 20 ou moins
    fuit vers son fort ; la première fois, les autres légions de sa cité perdent 10 et ses ennemis gagnent 10
    (`change_all_morale`). Des légionnaires au plafond cèdent vers 12 morts, javeliniers et cavaliers vers 8.
- **Ce qui n'allait pas entre deux joueurs** : moral et totaux ne connaissaient que les envahisseurs de l'IA.
  1. Une légion qui cédait décourageait les autres légions de son joueur, mais l'armée ennemie n'en tirait rien.
  2. Les compteurs de combat d'une légion (tirs reçus, combat récent, tir) étaient effacés deux fois par jour dès que
     sa cité n'avait aucun envahisseur : sous les javelots d'un joueur, une légion perdait entre deux salves sa
     formation serrée et ses places.
  3. Les soldats d'un joueur sur les terres d'un autre n'y étaient pas des « ennemis dans la cité » : l'immigration
     continuait ; conseillers et habitants n'en disaient rien ; pas de musique de bataille.
  4. Les tours ne tiraient qu'en présence d'une armée de l'IA (`enemy_army_total_enemy_formations`), donc jamais sur
     un joueur (D-077 disait le contraire).
  5. Aucun total des pertes par camp, et les arcs de triomphe ne venaient que des batailles lointaines, absentes en
     multijoueur.

  Ce qui allait déjà : le moral perdu à chaque mort (formation de la victime), la fuite, les totaux de légions et de
  soldats de chaque cité (par tranche).
- **Fait** (`mp/battle.c`, appels d'une ligne ailleurs ; rien en classique, ni tant que personne ne se bat) :
  1. une légion qui cède donne +10 aux légions des joueurs qui combattent son propriétaire (après `change_all_morale`) ;
  2. les compteurs de combat restent tant qu'un joueur ennemi a des soldats (`formation_legion_update`) ;
  3. à la fin des figures de chaque cité, les soldats de ses ennemis sur ses terres comptent parmi ses ennemis
     (`city_figures_enemies`, sauvegardé avec la cité) : l'immigration s'arrête au-delà de 3, comme pour des
     envahisseurs ; conseillers, habitants et musique en parlent ;
  4. une sentinelle de tour cherche une cible dès qu'un joueur ennemi a des soldats (`figuretype/wall.c`) ;
  5. pertes de chaque camp : soldats tués et légions détruites, par paire de joueurs (morceau `mp_battle` du `.mpsav`,
     dans la somme de contrôle ; une partie plus ancienne se charge sans pertes) ;
  6. un triomphe (D-084) gagne un arc de triomphe à la cité du vainqueur (`city_buildings_earn_triumphal_arch`, comme
     une bataille lointaine gagnée) ; la commande de construction l'accepte alors (D-085 refuse un arc non gagné) ;
  7. conseiller militaire : « En guerre contre le joueur 2 ; soldats ennemis dans la cité : 16 » et, à la place des
     batailles lointaines (sans objet en multijoueur), « Pertes : 8 soldats, 1 légion ; l'ennemi : 2 soldats ».
- **Batailles scriptées** (`mp_battles`, CESAR §10.3) : carte préparée pour 2, terrain découvert de personne entre
  les cités, 16 soldats par camp au plafond de leur moral, par commandes seulement. La troupe du joueur 1 arrive du sud
  et se poste face au nord ; celle du joueur 2 part à 14 cases au nord (ou à l'est : de flanc) et charge, ou se poste à
  8 cases et tire. La colonne (`FORMATION_COLUMN`, 4 × 4, la seule que le code protège des javelots : 1 dégât) est
  réservée par le jeu aux légions passées par l'académie. Vainqueur (le camp qui tient quand l'autre est détruit ou
  fuit), morts du joueur 1 / du joueur 2, ticks depuis la déclaration :

| Bataille | Valeurs d'origine (80 × 4, tir 100) | MG.1 (136 × 10, tir 100) | MG.5 (136 × 10, tir 70) |
|----------|-------------------------------------|--------------------------|-------------------------|
| légion postée ; légion qui marche dessus | légion postée, 0 / 16, 450 | idem | idem |
| légion postée ; javeliniers qui chargent | légion, 0 / 16, 220 | légion, 0 / 16, 320 | légion, 0 / 16, 320 |
| légion postée ; javeliniers qui chargent de flanc | légion, 0 / 16, 270 | légion, 5 / 15, 370 | légion, 7 / 11, 370 |
| javeliniers postés ; légion qui marche (du nord) | légion, 16 / 2, 370 | légion, 14 / 9, 570 | **javeliniers**, 11 / 14, 570 |
| javeliniers postés ; légion qui marche (de l'est) | légion, 14 / 2, 420 | légion, 9 / 11, 570 | **javeliniers**, 6 / 14, 520 |
| légion en double ligne ; javeliniers qui tirent | javeliniers, 13 / 0, 970 | idem | javeliniers, 13 / 0, 720 |
| légion en colonne (académie) ; mêmes tirs | personne, 6 / 0, 6 000 | idem | javeliniers, 11 / 0, 5 670 |
| javeliniers postés ; cavaliers qui chargent | cavaliers, 15 / 1, 270 | cavaliers (tous deux fuient), 8 / 12, 470 | **javeliniers**, 7 / 15, 320 |
| légion postée ; cavaliers qui chargent | légion, 0 / 16, 270 | idem | idem |
| cavaliers postés ; javeliniers qui chargent | cavaliers (tous deux fuient), 11 / 11, 370 | javeliniers, 16 / 0, 270 | idem |
| caravane du joueur 2 qui rentre à la déclaration, chassée depuis 10 cases | cavaliers 50 ticks, javeliniers 95, légionnaires jamais (1 500) | idem | idem |

- **Ce qu'on en apprend** :
  - une légion postée en formation gagne toute charge : +4 en attaque et en défense de face, et ses 16 soldats frappent
    tout ce qui passe à côté, à deux contre un. Même de flanc (−4 en défense, pas de bonus), les javeliniers qui la
    chargent perdent ;
  - une légion qui marche n'a pas ses bonus ; au contact, elle s'arrête et les retrouve : seul le tir pendant
    l'approche la punit ;
  - le moral décide souvent l'issue : javeliniers et cavaliers cèdent deux fois plus tôt que les légionnaires ;
  - MG.1 a donné aux javeliniers une vraie mêlée (une légion qui les charge perd 9 soldats au lieu de 2), pas assez
    pour gagner ; le tir n'en dépend pas : 16 javeliniers abattent une légion immobile en double ligne sans perte, et
    seule la colonne les tient en respect ;
  - les cavaliers perdent toutes les mêlées (1,56 fois moins forts, D-058) mais rattrapent une caravane qui fuit, ce
    que les légionnaires ne font jamais (même vitesse qu'elle) ; les javeliniers, deux fois plus rapides, aussi.
- **MG.5** : le risque de D-058 (les javeliniers meilleure troupe en terrain découvert) **ne se vérifie pas** ; c'est
  le rôle voulu « ils gagnent contre une légion en marche » qui n'était pas tenu. **Réglage** : en multijoueur, un
  javelot toutes les **70** ticks au lieu de 100 (points de vie, attaque, portée et dégâts inchangés ; table d'origine
  en classique). Mesuré sur 8 approches d'une légion (10 à 16 cases, du nord et de l'est) : les javeliniers gagnent 7
  fois et, la huitième, les deux camps fuient (à 100 : 0 fois ; à 80 : 5 ; à 60 : 8, en écrasant la légion, 5 morts
  contre 16). La légion postée gagne toujours les charges ; immobile sous les tirs, elle tombe plus vite (720 ticks au
  lieu de 970) ; la colonne tient toujours plus de quatre fois plus longtemps.
- **Bornes** (`mp_battles`, chaque bataille rejouée identique) : une légion postée écrase une légion qui marche dessus
  (≥ 12 morts contre ≤ 4) et les javeliniers qui la chargent (≥ 12 contre ≤ 4), et les bat aussi de flanc ; les
  javeliniers battent une légion qui marche sur eux, du nord et de l'est ; ils abattent sans perte une légion immobile
  en double ligne en 1 000 ticks au plus, et la colonne tient au moins quatre fois plus ; les cavaliers perdent contre
  des javeliniers et sont écrasés par une légion ; des javeliniers qui chargent des cavaliers postés gagnent ; les
  cavaliers rattrapent la caravane en 600 ticks au plus, les légionnaires jamais ; les pertes comptées par le jeu sont
  celles des batailles.
- **Pas mesurable en l'état** : le « rapport des pertes victime / agresseur entre 3 et 5 » de CESAR §10.3 n'a pas
  d'unité commune (soldats, bâtiments, caravanes, habitants) ; à définir avec M11.1, les compteurs existent
  (`mp_battle` pour les soldats, `mp_wrath` pour les dégâts).
- **Tests** : `mp_war_morale` (chaque vérification échoue quand on inverse la ligne qui la fait passer : 7 inversions),
  `mp_battles` (le tir remis à 100 fait échouer deux bornes ; sans le décompte des morts en mêlée ou par javelot, les
  pertes ne sont plus comptées), `mp_troops_javelin` (tir à 70 en multijoueur). Captures : `tools/mp-battle-test.sh`
  (partie à deux en pleine bataille, conseiller militaire de chacun : « Pertes : 8 soldats ; l'ennemi : 3 soldats »
  chez l'un, « soldats ennemis dans la cité : 8 » et « Pertes : 3 soldats ; l'ennemi : 8 soldats » chez l'autre).
- **Constats sans suite** : un soldat qui fuit peut disparaître au premier pas (aucun chemin vers sa place au fort),
  comme dans l'original : ce n'est pas une perte infligée, elle n'est pas comptée. La commande de formation accepte la
  colonne et la tortue pour une légion sans académie, que le jeu ne propose qu'après l'académie (`mp/actions.c`). Les
  textes du jeu disent la tortue « imperméable aux attaques à distance », mais le code protège la colonne. Les sons de
  combat propres à chaque camp (DESIGN §7) ne sont pas faits : chacun n'entend que les coups portés par ses soldats.
- **À valider** :
  - +10 aux légions de l'ennemi quand une légion cède ;
  - les soldats ennemis comptent dans la cité seulement sur ses terres, et l'immigration s'arrête au-delà de 3 ;
  - les tours tirent sur les soldats d'un joueur en guerre ;
  - les pertes à la place des batailles lointaines dans le conseiller militaire ;
  - un arc de triomphe par triomphe ;
  - le tir des javeliniers toutes les 70 ticks ; leur flanc, c'est le tir : de flanc en mêlée, ils perdent.
- Réseau : nouveau morceau de sauvegarde et règles de simulation changées (ennemis de la cité, tir des javeliniers) :
  protocole à monter à la fusion. Classique : inchangé (parité verte).
- **Corrigé à la fusion** : la commande de formation d'une légion acceptait la colonne et la tortue sans l'académie.
  En multijoueur, elle n'accepte plus que ce que la fenêtre de la légion propose : doubles lignes et nettoyage pour
  tous, la colonne pour les légionnaires formés à l'académie, les lignes simples pour les autres troupes
  (`mp/actions.c`) ; `mp_war` le vérifie (échoue sans).

### D-089 — Le gouverneur choisit son salaire ; le commerce continue pendant le préavis (revoit D-076, D-077, D-080)
- 2026-10-18 · **adoptée** (Alexandre, après la version de la guerre : « salaire qu'il choisit » ; « ne coupe pas le
  commerce pendant le préavis de guerre »)
- **Salaire** (revient à D-067) : chaque cité part du rang 0, sans salaire ni épargne (inchangé). Le gouverneur
  choisit ensuite son salaire dans la fenêtre du conseiller impérial, **au plus celui de son rang de lauriers** : la
  commande `MP_ACTION_SET_SALARY` refuse un rang plus haut. Un rang gagné laisse le salaire tel qu'il a été choisi ;
  un rang perdu le ramène au plafond, au changement de mois, avant la paie. Le rang de l'original suit le rang de
  lauriers, pour que la faveur de l'original ne trouve jamais de salaire au-dessus du rang. Tests `mp_salaries`
  (refus au-dessus du rang, choix plus bas, paie choisie, rang perdu ou gagné ; échoue sans le plafond) et
  `mp_caesar_gifts`. `mp_no_caesar_request_start` fixe désormais un rang de salaire plutôt qu'un montant.
- **Commerce pendant le préavis** : une guerre honorable ne coupe plus le commerce pendant ses 3 mois de préavis ;
  les caravanes partent et livrent comme en paix. Le commerce ne s'arrête qu'au début des combats (fin du préavis,
  guerre brutale, ou riposte du défenseur) : plus aucune caravane ne part, et celles en route font demi-tour, sans être
  payées. La menace (D-080) reste : le défenseur a 3 mois pour céder avant que la guerre ne commence. Test `mp_war`
  (pendant le préavis, une caravane en route livre et est payée ; au début des combats, la suivante fait demi-tour ;
  échoue avec l'ancienne règle).
- Réseau : protocole 33.

### D-090 — Le salon, ses règles, les lettres de César et l'écran de fin à la manière de l'original (UI)
- 2026-10-18 · **adoptée provisoirement**, **à valider** par Alexandre (« plus jolis et fidèles au jeu d'origine
  (bouton / organisation…) ; pas trop saturées »). Dessin seulement : ni règle, ni état, ni commande ne changent.
- **Salon** : avant, une colonne de gauche pleine (liste, joueurs, sept règles en texte cliquable, gris quand on ne
  peut pas les changer) et la seconde expédition casée sous « Rejoindre » ; une barre de défilement qui débordait
  sur la colonne de droite au-delà de dix parties. Après, deux colonnes symétriques comme la fenêtre des fichiers
  d'origine : « Héberger une partie » (liste dans un panneau, barre de défilement à côté, « Joueurs : 2 » avec les
  flèches des options d'origine, boutons « Règles de la partie » et « Héberger ») et « Rejoindre une partie »
  (parties trouvées, adresse, « Rejoindre ») ; dessous, un panneau de texte blanc : les règles en une phrase
  (carte, difficulté, fin, puis seulement les règles qui diffèrent de celles du salon : « sans dieux », « sans
  invasions », « sans brouillard », « Rome reprend la province à la seconde expédition ») et l'état de la partie.
  En hébergeant, la ligne des joueurs devient « Joueurs présents : 1 sur 2 ». Une partie rejointe cache
  « Héberger » et « Rejoindre ». Un échec (port, hôte injoignable) s'affiche jusqu'au prochain essai ; avant, il ne
  s'affichait pas. « Nouvelle partie (forêt) » devient « Nouvelle partie » (la carte se choisit dans les règles).
- **Règles de la partie** (fenêtre par-dessus le salon, comme les options d'origine) : une ligne par règle,
  « Carte : », « Difficulté : », « Dieux : », « Invasions IA : », « Brouillard de guerre : », « Fin de la partie : »,
  « Seconde expédition : », la valeur sur une étiquette verte qui passe à la suivante (comme les options du son), les
  noms de difficulté de la fenêtre d'origine ; OK ou clic droit ferme. Une phrase dit qui peut les changer ; chez un
  joueur qui a rejoint, ou pour une partie reprise (D-063, D-073), les valeurs s'écrivent sans étiquette.
- **Lettres de César** : avant, un panneau à part avec l'icône du conseiller, un texte noir signé « César » et un
  bouton OK, sur la ville assombrie. Après, comme les messages de l'Empereur d'origine : à leur place (sur la ville,
  pas sur la barre latérale), un titre par sujet (« Avertissement de César », « Ultimatum de César », « César impose
  la paix », « L'expédition punitive », « Les légions de César vaincues » / « repartent », « Promotion », « Lettre de
  César » pour l'accueil), la date et « Pour Joueur N » en tête du texte blanc, le bouton du conseiller impérial en
  bas à gauche (il l'ouvre) et la flèche de fermeture en bas à droite ; une lettre longue agrandit la fenêtre.
  Entrée et clic droit ferment toujours.
- **Écran de fin** : avant, des lignes « Joueur 1 (vous) lauriers : 0 » et la règle en petit. Après, comme la
  fenêtre de victoire d'origine : le titre, « L'héritier de César est » puis son nom en grand (« La partie est gagnée
  par » pour la fin au score ; la phrase de Rome quand Rome reprend la province), les cités dans un panneau, la
  meilleure d'abord, sous les colonnes « Cité » et « Lauriers », et les deux choix sur de grandes étiquettes.
- **Menu principal** : inchangé (déjà dans l'idiome d'origine, D-031).
- Scripts : les clics du salon de tous les scripts du vrai jeu suivent la nouvelle place des boutons (la carte, la
  difficulté et l'expédition passent par « Règles de la partie ») ; `mp-ui-client.txt` capture aussi les règles de
  l'hôte vues par le client (`ui-client-rules.png`).
- *À valider* : les règles dans une fenêtre à part plutôt que dans le salon ; le résumé des règles (seulement ce qui
  diffère) ; « Nouvelle partie » sans « (forêt) » ; les titres des lettres ; la signature « César » retirée (les
  messages d'origine n'en ont pas).
- Classique : inchangé.

### D-091 — Les statistiques de César à la manière des conseillers d'origine : deux pages, un bandeau comme la barre du haut
- 2026-10-18 · **adoptée provisoirement**, *à valider* (Alexandre : « plus jolis et fidèles au jeu d'origine […] pas
  trop saturées ») · précise D-057, D-071, D-084, D-086, D-089
- Dessin seulement : ni règle, ni état, ni commande ne change (`windowsweep` : aucune fenêtre n'écrit dans la partie).
- **Conseiller impérial** (`window/mp_imperial.c`), à la manière des tableaux d'origine (en-têtes de colonnes en
  petite police au-dessus du cadre, lignes à 20 pixels, nombres alignés à droite) :
  - avant : une page de vingt lignes serrées (notes, barres et lauriers de chaque note, lauriers de la cité et de
    César, « fêtes et campagnes : bientôt », guerres et armée, classement, colère, parts, ennemi de Rome, ultimatum) ;
  - après, en deux pages, avec un bouton en bas à droite du cadre (comme ceux du cadeau et du don) :
    1. « Notes et classement » : les cinq notes et leurs barres, la colère de César en sixième ligne
       (jauge marquée à 40 et 70), le classement ; en rouge, seulement s'il y a lieu, le délai de l'ultimatum, les
       ennemis de Rome et la disgrâce ;
    2. « Lauriers et colère » : les lauriers par source (cinq notes, cadeaux, guerres, armée, l'expédition s'il y en a
       une), la part de chacun dans la colère avec « ennemi de Rome », les légions de la cité, celles que César tolère
       et leur coût par mois.
  - en-tête comme l'original (« Estime de l'Empereur 71 », puis le mot de l'empereur) : « Lauriers : 108 sur 1000,
    prochain rang à 200 », puis la règle en une ligne. Retirés : « Fêtes et campagnes : bientôt », les deux totaux
    (l'en-tête donne le total, la seconde page le détail) ;
  - le bouton du salaire retrouve l'infobulle de l'original, que D-076 avait retirée (D-089 a rouvert sa fenêtre) ;
    la note de cette fenêtre disait encore « Rome vous verse le salaire de votre rang » (D-076) : « Choisissez votre
    salaire, au plus celui de votre rang : les rangs plus élevés s'obtiennent avec des lauriers. ».
  La page choisie reste d'une ouverture à l'autre (état de l'affichage, jamais sauvegardé).
- **Évaluation, pilier des lauriers** (`window/mp_ratings.c`) : avant, la boîte du bas débordait (un titre qui
  répétait le pilier, puis trois lignes) ; après, comme les autres piliers : « Lauriers », puis deux lignes au plus
  (le mois dernier et la tendance, la place, et « Vos cinq notes en rapportent chaque mois. » s'il reste la place).
  « 200 Requis » a le même retrait que sous les autres piliers.
- **Bandeau** (`widget/mp_status.c`) : avant, un rectangle noir ; après, la barre du haut de l'original, au bas de la
  vue de la ville et sur toute sa largeur : sa pierre, « Joueur 2 » et « héritier à 1000 lauriers » dans sa police,
  les lauriers des joueurs dans un de ses panneaux sombres (« J1 », … dans leur couleur ; « P1 » en anglais : le « J »
  était écrit en dur), la colère et sa jauge dans un autre, la pause dans un troisième. Faute de place : l'unité du
  score part d'abord, puis le score, puis « Joueur N ». Avant le lancement et après un arrêt, l'état du réseau en haut,
  dans le même panneau sombre.
- Conseillers de la population et des finances : rien de propre au multijoueur à l'écran (les impôts passent par une
  commande, sans autre changement), laissés tels quels.
- Captures : `tools/mp-stats-test.sh` (grande cité sans réseau, puis partie seule depuis le salon ; 640 × 480,
  800 × 600 et 1024 × 736 ; avant et après l'ultimatum), `tools/mp-stats4-test.sh` (quatre joueurs).
- *À valider* :
  - une seconde page plutôt que des infobulles : celles du jeu n'affichent que les textes d'origine, et peuvent être
    désactivées dans les options ;
  - le bandeau sur toute la largeur de la vue : il cache 24 pixels du bas de la ville (22 avant, sur la largeur de son
    texte) ;
  - la colère comme une sixième ligne sous les notes, avec ses seuils.

### D-092 — Les fenêtres de la guerre à la manière de l'original ; la fenêtre de l'offre de paix (interface, précise D-077, D-080, D-084, D-087, D-088)
- 2026-10-18 · **adoptée provisoirement**, *à valider* (Alexandre : « plus jolis et fidèles au jeu d'origine (bouton /
  organisation…) », « pas trop saturées » ; « as-tu ajouté un menu pour cesser le feu et arrêter la guerre ? »)
- Dessin et textes seulement : simulation, commandes, sauvegarde et sommes de contrôle inchangées ; classique inchangé.
- **« Guerre et paix »** :
  - avant : un dialogue plein écran au titre centré, deux paragraphes de règles (5 lignes), puis pour chaque joueur
    son état, « Déclarer la guerre : » et trois boutons, « Il propose la paix » à droite, la ligne de César dessous ;
  - après : une page dans le cadre du conseiller militaire (40 × 26 blocs, par-dessus lui), son icône et le titre à
    côté ; en-têtes « Joueur, Situation, Déclarer la guerre, Paix » au-dessus d'un panneau ; une ligne encadrée par
    joueur comme les légions du conseiller : le joueur à sa couleur, « En paix » en vert, « Préavis : 46 jours » ou
    « En guerre » en rouge (comme le conseiller en chef), les boutons à droite (Brutale ; Honorable ou Se rétracter ;
    Proposer la paix, Retirer l'offre ou **Accepter la paix** quand l'autre l'a proposée) et une seule ligne dessous :
    le prix de César d'une déclaration en paix, sinon l'offre reçue (en vert) ou envoyée, sinon le motif et le mois de
    guerre ; trois puces de règles sous le panneau. Le détail passe dans des **infobulles** : la règle de chaque bouton
    et le prix complet de César, le motif d'une guerre en survolant le joueur.
  - « Confirmer ? » devient « Confirmer » (tenait mal dans le bouton) ; dans la ligne, « colère double (guerre
    récente) » devient « colère double » (en entier dans l'infobulle).
- **La fenêtre de l'offre de paix** (demandée par Alexandre) : quand un autre joueur propose la paix, la vue de la
  cité montre, comme les dialogues de l'original (480 × 160 au milieu : titre, deux lignes, deux boutons),
  « Proposition de paix — Le joueur 2 vous propose la paix. Les combats cessent et les légions rentrent au fort. »
  avec **Accepter** (la commande existante `MP_ACTION_PROPOSE_PEACE` : la paix est signée) et **Plus tard** (ou
  Échap, clic droit : l'offre reste dans « Guerre et paix ») ; Entrée accepte, comme l'OK des dialogues de
  l'original. Une fois par offre, à la prochaine vue de la cité (pas pendant un tracé de construction, après les
  lettres de César), fermée d'elle-même si l'offre est retirée ou la paix signée entre-temps. État d'affichage de cet
  ordinateur seulement (`mp/war.c`, jamais sauvegardé ni lu par la simulation, comme les annonces) : le bandeau
  « propose la paix » n'est plus montré à celui qui la reçoit (la fenêtre le remplace ; le compte des annonces ne
  change pas). Une offre en attente dans une partie chargée n'ouvre pas la fenêtre : la page et le conseiller la
  montrent.
- **Conseiller militaire** : le bouton « Guerre » devient « Guerre et paix » ; sa ligne de la guerre dit d'abord
  « Le joueur 2 vous propose la paix » quand une offre attend une réponse, puis, comme avant, la guerre en cours avec
  les soldats ennemis, ou le préavis (D-084, D-088).
- **Annonces sur la cité** (bandeaux), à la première personne pour les deux joueurs concernés : avant « Joueur 1 :
  guerre brutale contre joueur 2 », « Joueurs 1 et 2 : paix signée » ; après « Le joueur 1 vous déclare une guerre
  brutale ! », « Vous déclarez la guerre au joueur 2 : combats dans 3 mois », « Le joueur 1 retire sa déclaration de
  guerre », « Les combats commencent avec le joueur 2 », « Paix signée avec le joueur 2 », « César impose la paix avec
  le joueur 2 » ; les autres joueurs lisent « Le joueur 1 déclare la guerre au joueur 2 : combats dans 3 mois »,
  « Paix signée entre les joueurs 1 et 2 » (le français « propose la paix à joueur 2 » est corrigé).
- **Fenêtre de la légion** : avant, « Une charge d'armes par recrue. En caserne : 0 » serré sous le titre ; après, une
  ligne du tableau comme les autres, « Armes en caserne » (légionnaires) ou « Bois en caserne » (javeliniers), la
  valeur dans la colonne des valeurs, en rouge quand la légion attend des recrues, rien pour les cavaliers (le coût
  complet reste dans la fenêtre du fort) ; « Retourner au fort » et « Attaquer » / « Attaque en cours » centrés
  ensemble. Choix de la cible : « Cliquez sur un bâtiment de l'ennemi ; clic droit pour annuler ».
- **Caserne et fort** : la caserne montrait « 0 unités » pour les armes et « Bois : 0 » ; elle montre l'icône du bois et
  « 0 unités », comme les armes. Le fort met l'icône de la ressource devant la ligne du coût d'une recrue.
- **Infobulles** : `tooltip_context` accepte un texte composé (`precomposed_text`, le nom du champ dans le Julius
  d'aujourd'hui) ; les fenêtres ne pouvaient montrer que des textes du jeu d'origine.
- **Tests** : `tools/mp-war-test.sh` (deux joueurs, 640 × 480 et 1024 × 736) passe par la fenêtre de l'offre des deux
  côtés : l'hôte accepte par elle ; le client répond « Plus tard », retrouve l'offre dans le conseiller et l'accepte
  dans « Guerre et paix » ; partie sans désynchronisation. `windowsweep` ouvre aussi « Guerre et paix » (à plusieurs :
  `tools/mp-menus-test.sh`, 145 fenêtres, aucune ne change l'état). Revus aussi : `mp-attack-test.sh`,
  `mp-battle-test.sh`, `mg-barracks-test.sh` ; « Guerre et paix » à quatre joueurs (trois lignes) par un script de
  travail ; textes anglais relus. Les annonces nomment les joueurs par « %d », comme les messages du réseau :
  `mp_status_translations` vérifie aussi qu'ils sont les mêmes en français et en anglais (échoue sans).
- **À valider** :
  - une fenêtre pour l'offre de paix reçue, plutôt qu'un bandeau ; Entrée l'accepte ;
  - « Plus tard » ne la remontre pas : l'offre reste dans la page et le conseiller ;
  - le préavis en rouge, comme la guerre ;
  - les annonces à la première personne ;
  - pas de ligne de recrues pour les cavaliers dans la fenêtre de la légion.

### D-093 — Le commerce dans le style du conseiller d'origine : onglets, flèches, liste qui défile, infobulles ; l'alerte de prix comme un message (T5.6, précise D-049, D-051, D-078)
- 2026-10-18 · **adoptée provisoirement** (Alexandre : « plus jolis et fidèles au jeu d'origine (bouton /
  organisation ...) », « pas trop saturées » ; travail sans lui)
- **Page du commerce** (`window/mp_trade.c`) :
  - *Avant* : 15 lignes serrées de 18 px, des « - » et « + » dans des rectangles dessinés à la main, l'onglet choisi
    peu visible (gris foncé, texte rouge), six colonnes sur l'onglet d'un joueur (dont « Empire ») et sur l'onglet
    Empire (dont « Portorium »).
  - *Après* : les lignes du conseiller d'origine (icône, nom, valeurs alignées en colonnes, 24 px), les **flèches de
    l'original** (bas, haut, comme le salaire du conseiller du travail) pour mon prix et les deux bornes, des boutons
    à bordure de l'original (comme les ordres d'un entrepôt) pour « J'achète » et le commerce avec l'empire. Les
    flèches font 24 px et l'original n'en a pas de plus petites : 15 lignes ne tiennent pas dans 640 × 480 avec les
    onglets et les boutons du bas. **Douze lignes** sont visibles, la liste **défile** comme les listes de l'original
    (barre à droite, flèches, curseur, molette), sur les trois onglets à la fois.
  - **Onglets** posés sur le bord du tableau : celui qui est choisi est taillé dans la pierre du tableau et s'ouvre
    dessus, les autres sont des boutons de l'original.
  - **Moins de colonnes** : l'onglet d'un joueur perd « Empire » (le prix de l'empire passe dans l'infobulle de
    « Son prix », le vert reste), l'onglet Empire perd « Portorium » (dans l'infobulle du prix payé ou reçu ; la ligne
    d'aide dit 50 %).
  - **Infobulles** : prix de l'empire, Rome et portorium, rôle des flèches et de « J'achète », chargements en route,
    et **pourquoi rien ne vient** en une phrase (le mot court, « sans stock », reste écrit dans la colonne, en blanc).
    Les boutons d'origine gardent leurs infobulles d'origine (bilan de l'industrie, carte, prix). Pour cela, les
    infobulles acceptent un texte composé (`precomposed_text`) et un conseiller peut remplir la sienne lui-même
    (`get_tooltip`, facultatif) ; le classique n'en utilise aucun.
  - *Inchangé* : la route en un seul bouton dans le coin du titre, la ligne d'aide (état de la route tant qu'elle n'est
    pas ouverte), les commandes, les pas (10 Dn, 4 chargements), la carte et les prix de l'empire en bas, le clic sur
    une ressource qui ouvre sa fenêtre d'origine.
- **Alerte de prix** (`window/mp_price_alert.c`) :
  - *Avant* : boutons texte « Voir le commerce » et « OK » au milieu du bas.
  - *Après* : comme les messages de l'original : titre centré, ligne d'explication, tableau dans un panneau creux
    (vendeur, ressource, ancien et nouveau prix alignés à droite), le **bouton du conseiller au commerce** en bas à
    gauche avec « Voir le commerce » écrit à côté (cliquable aussi), la **croix** de fermeture en bas à droite ; Entrée,
    Échap et le clic droit ferment. Toujours de la taille de l'écran d'origine, ville grisée (D-049). « Voir le
    commerce » ouvre l'onglet du vendeur, la ligne de la ressource en vue.
- Mode classique : inchangé (conseiller d'origine, fenêtre de la ressource et infobulles d'origine).
- Captures : `tools/mp-ui-test.sh` (640 × 480 et 1024 × 736 : onglets, infobulles, liste défilée, alerte chez l'hôte
  et chez le client), `mp-trade-test.sh`, `mp-trade3-test.sh`, `mp-trade4-test.sh`, `mp-solo-test.sh` (clics mis à
  jour). `windowsweep` ouvre aussi l'onglet Empire.
- *À valider* :
  - la liste qui défile (12 lignes sur 15 ; poterie, meubles et armes en bas) plutôt que des « - » et « + » dessinés
    qui tiennent sur 15 lignes ;
  - le prix de l'empire (onglet d'un joueur) et le portorium (onglet Empire) dans les seules infobulles ;
  - l'alerte sans « OK », fermée par la croix de l'original.
