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
- 2026-10-04 · **à valider** (hypothèse H11)
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
- 2026-10-04 · **à valider**
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
- 2026-10-04 · **adoptée** (à confirmer par les tests au jalon M1)
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
