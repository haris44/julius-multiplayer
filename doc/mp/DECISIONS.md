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
- 2026-10-04 · **adoptée** (décision d'Alexandre) ; la règle « chacun ne sert que sa cité » est **à valider** (H11)
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
