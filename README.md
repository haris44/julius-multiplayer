# Caesar III Multijoueur

[![Compilation](https://github.com/haris44/julius-multiplayer/actions/workflows/multiplayer.yml/badge.svg?branch=master)](https://github.com/haris44/julius-multiplayer/actions/workflows/multiplayer.yml)

Caesar III à plusieurs, sur un réseau local. De 2 à 4 joueurs bâtissent chacun leur cité sur une même carte,
commercent entre eux et, bientôt, se font la guerre. À l'intérieur de sa cité, chacun joue exactement au Caesar III
d'origine. Ce qui change, c'est la province autour : des voisins, des frontières, une eau à partager, et César qui
juge.

Le projet est un fork de [Julius](https://github.com/bvschaik/julius), la réimplémentation libre et fidèle de
Caesar III.

> *In English: a fork of Julius that turns Caesar III into a 2 to 4 player LAN game. Each player builds a city on a
> shared map, and the gameplay inside a city is exactly the original one. Players expand through missions, trade
> with each other, depend on Caesar's aqueduct for water and compete for Caesar's laurels. War between players is
> in progress. The documentation is in French.*

**État** : en développement. On joue déjà en paix : zones, missionnaire, commerce entre joueurs, César qui juge. La
guerre entre joueurs est le prochain grand chantier. Les versions Linux et Windows sont récentes et encore peu
essayées.

## La partie en bref

- Deux cartes préparées pour 2 joueurs, et deux pour 3 ou 4. L'hôte choisit dans le salon « Carte 1 », « Carte 2 »
  ou « Carte au hasard » (par défaut). Un bras de mer traverse chaque carte. La route de César relie tout le monde et
  franchit la mer par un pont. Les emplacements sont tirés au sort à chaque partie.
- Deux sortes d'emplacements :
  - **les terres** : le fer et le marbre (pas de bois), les porcs. Ces joueurs vivent loin de l'eau ;
  - **la côte** : le bois, l'argile, la pêche et les fruits, avec les docks et les quais de pêche ;
  - le blé et les légumes pour tous. À 2 joueurs, les olives vont aux terres et les vignes à la côte ; à 4, les
    olives à un joueur des terres, les vignes à l'autre.
- À 2 joueurs, un joueur des terres et un de la côte. À 4, deux de chaque. À 3, une place de la côte reste libre.
  Personne ne peut tout produire.
- On peut aussi jouer seul, pour découvrir la carte et ses règles.

## Missions et zone de construction

- On commence avec une mission, déjà bâtie au bord de la route de César, et la zone qu'elle donne : 20 cases autour
  d'elle. On ne construit que dans sa zone.
- La zone grandit toute seule : 20 cases autour des maisons habitées et des bâtiments qui ont des employés. Elle
  est tracée à la couleur du joueur. Routes, aqueducs et murs se construisent partout.
- Un bâtiment qui se retrouve hors de la zone s'effondre au bout de 3 mois.

## Le missionnaire

- Chaque joueur a un missionnaire, qu'on déplace comme une légion : un clic sur lui, puis un clic là où il doit
  aller.
- Il fonde de nouvelles missions (menu Éducation), à 20 cases de lui au plus. Chacune coûte 30 chargements de marbre
  et ouvre une nouvelle zone : c'est ainsi qu'on s'étend vers une forêt, une côte ou un gisement lointain.
- Il peut mourir. Une mission en forme un nouveau pour 300 deniers.

## L'eau et l'aqueduc de César

- La seule eau de la carte est le bras de mer. Les joueurs des terres en vivent loin : seul l'aqueduc de César les
  alimente, depuis un réservoir de César sur la côte, un par joueur des terres.
- Les routes de César sont indestructibles. Son aqueduc l'est aussi, tant que la guerre n'existe pas : ensuite, les
  soldats pourront le couper. Privé de sa source, un réservoir garde son eau environ 5 minutes, puis les fontaines
  s'assèchent.

## Le commerce

- Chaque joueur n'a le droit d'exploiter que certaines ressources : il faut commercer pour le reste, avec l'empire
  ou avec les autres joueurs.
- Chaque ressource a un **prix de Rome**, celui du jeu d'origine. **L'empire vend toujours**, mais avec le
  **portorium**, une douane de 50 % : on lui achète au prix de Rome + 50 %, on lui vend au prix de Rome − 50 %. Le
  marbre, à 200 à Rome, s'achète 300 à l'empire et se vend 100.
- Chacun commerce avec l'empire pour lui seul : ses routes, ses achats et ses ventes ne changent rien chez les
  autres.
- Entre joueurs, pas de douane. Chacun part du prix de Rome, puis fixe ses prix de vente comme il veut, joueur par
  joueur. Un changement de prix déclenche une alerte chez l'acheteur.
- Une route entre deux joueurs s'ouvre quand les deux l'ont proposée. Chaque mois, le vendeur envoie une caravane
  par ressource achetée, 8 chargements au plus. L'acheteur paie à l'arrivée ce qui entre dans ses entrepôts.
- Le conseiller au commerce réunit sur une page l'empire et les joueurs : stocks, prix de Rome, prix payés et reçus,
  achats et chargements en route. La source la moins chère est en vert.
- Son onglet « Stocks » règle, pour chaque ressource, un stock minimum et un stock maximum, valables pour l'empire et
  pour les joueurs : « vendre au-dessus de » (la cité garde ce stock et ne vend rien en dessous) et « acheter
  jusqu'à » (plus aucun achat dès que le stock l'atteint).

## César juge

- Chaque mois, César donne des lauriers à chaque cité selon cinq notes : prospérité, commerce, habitat, culture et
  éducation, grandeur.
- Les **cadeaux à César** rapportent aussi des lauriers : 4, 7 ou 10 selon leur taille, un seul compté par an. Ils
  se paient sur l'épargne personnelle, que remplit le salaire du gouverneur. Tout le monde part du même salaire,
  nul au début, puis Rome paie à chacun le salaire de son rang de lauriers.
- La première cité qui atteint le score choisi dans le salon (1 000 lauriers par défaut) devient son héritière et
  gagne la partie.
- Le conseiller impérial montre les notes, les lauriers, le rang, le salaire et le classement, avec les boutons du
  cadeau et du don à la cité. César écrit à chaque nouveau rang.
- Dans l'évaluation de la cité, le quatrième pilier est celui des **lauriers** : sa hauteur dit où l'on en est du
  score. Un clic donne les lauriers du mois dernier, la tendance et la place dans la province. L'ancienne faveur de
  César n'est plus affichée.

## Le salon et les règles

- L'hôte règle la partie jusqu'au lancement : carte, difficulté (« facile » par défaut), dieux, fin de partie,
  invasions de l'IA, brouillard de guerre. Les autres joueurs voient ses réglages, tenus à jour, sans pouvoir les
  changer. En partie, la difficulté est la même pour tous.
- « Invasions IA : non » coupe toutes les attaques de l'IA, Mars compris. Les révoltes de gladiateurs restent.
- Brouillard de guerre, en option : on ne voit que ce qu'on a découvert.
- Pause pour tout le monde (touche P). La partie se sauvegarde et se reprend depuis le salon, avec ses règles.

## À venir

- **La guerre entre joueurs** : guerre honorable, avec 3 mois de préavis, ou brutale et immédiate, que César
  n'apprécie pas. Légions chez l'adversaire, caravanes interceptées, aqueduc coupé. Une colère de César commune à
  tous : des joueurs trop belliqueux s'attirent une expédition punitive.
- **Trois troupes, trois coûts** : légionnaires payés en armes, javeliniers renforcés et payés en bois (réservé aux
  joueurs de la côte), cavaliers gratuits et rapides.
- Fêtes et campagnes de César qui rapportent des lauriers, et ses demandes adressées à toute la province.
- La hausse des prix de l'empire quand un joueur arrête le commerce.

La feuille de route complète est dans [doc/mp/ROADMAP.md](doc/mp/ROADMAP.md).

## Jouer

1. **Il faut sa propre copie de Caesar III** (GOG ou Steam). Ce dépôt ne contient aucun fichier du jeu original, qui
   est toujours vendu.
2. **Récupérer le programme** dans l'onglet
   [Actions](https://github.com/haris44/julius-multiplayer/actions/workflows/multiplayer.yml), en bas de chaque
   compilation réussie (il faut être connecté à GitHub) :
   - Linux : une AppImage ;
   - Windows (64 bits) : un dossier avec le `.exe` ;
   - Mac (puce Apple) : une image disque `.dmg`, que `tools/package-mac.sh` fabrique aussi en local.
3. **Tous les joueurs** sont sur le même réseau local, avec exactement la même version. Le pare-feu doit laisser
   passer les ports 27400 (TCP, la partie) et 27401 (UDP, le salon).
4. **Menu principal, « Multijoueur »** : l'hôte crée la partie, les autres la voient apparaître et la rejoignent.

Le mode d'emploi complet (installation sur chaque système, pare-feu, règles) est dans
[tools/dist/LISEZMOI.txt](tools/dist/LISEZMOI.txt), joint à chaque version. Le jeu classique, campagne et
scénarios, reste disponible et identique à celui de Julius.

## Pour les développeurs

- Tout se passe sur la branche `master`. Julius reste la source des correctifs : remote `origin`, tag
  `upstream-base`.
- Principes :
  - la simulation est déterministe et avance au même pas sur chaque ordinateur, à partir des mêmes commandes des
    joueurs ;
  - le mode classique n'est pas modifié ;
  - des tests de parité comparent la simulation au Caesar III d'origine (`tools/check.sh`).
- Documentation, en français, dans [doc/mp/](doc/mp/) : [VISION](doc/mp/VISION.md) (exigences),
  [DESIGN](doc/mp/DESIGN.md) (architecture), [DECISIONS](doc/mp/DECISIONS.md), [ROADMAP](doc/mp/ROADMAP.md),
  [JOURNAL](doc/mp/JOURNAL.md), [TESTING](doc/mp/TESTING.md).
- Compilation : comme Julius, avec CMake et SDL2 ([doc/BUILDING.md](doc/BUILDING.md)).

## Crédits et licence

Ce projet dérive de [Julius](https://github.com/bvschaik/julius), de Bianca van Schaik et ses contributeurs. Il est
sous licence GNU AGPL 3.0, comme Julius ([LICENSE.txt](LICENSE.txt)). Caesar III appartient à ses ayants droit : ce
projet n'y est pas affilié.
