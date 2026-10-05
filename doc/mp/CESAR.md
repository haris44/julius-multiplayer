# César juge — plan de la condition de victoire

> Plan demandé par Alexandre le 2026-10-06. Il remplace l'exigence E8 (« suppression complète de César ») par un
> César **arbitre** de la partie. Décision : D-050. Tâches : jalons M9 à M11 de [ROADMAP.md](ROADMAP.md).
> Faits du jeu d'origine (formules, valeurs, `fichier:ligne`) : [code-map/07](code-map/07-cesar-faveur-notes-equilibrage.md).
> Les valeurs chiffrées de ce plan sont des **réglages de départ** : la §10 dit comment on les mesure et les corrige.

## 0. En bref

- César revient. Il ne gouverne pas les cités : il **observe** la province et, à la fin, **désigne le vainqueur**.
- Chaque cité gagne chaque mois des **lauriers** (points de victoire). Ce gain vaut la **valeur de la cité**,
  mesurée par cinq notes (prospérité, commerce, habitat, culture et éducation, grandeur), multipliée par la
  **faveur** de César, de ×0,5 à ×1,5.
- La faveur se gagne en **servant César** : dons, fêtes, troupes prêtées à ses campagnes, demandes honorées, guerres
  justes. Elle se perd par l'agression.
- La guerre entre joueurs est permise, mais César la regarde : une **jauge de colère commune** monte avec la
  **durée** et la **puissance** des guerres. Au maximum, ses légions frappent **toutes** les cités, et le fautif
  paie le plus.
- Deux chemins vers la victoire :
  - se développer mieux et plus que l'autre, en se servant de César ;
  - affaiblir le rival par des frappes commerciales et militaires **courtes et ciblées**, qui restent sous le
    seuil de la colère.

## 1. La demande

> César va ré-apparaître, c'est lui qui donnera la victoire à la cité la plus prospère, celle qui marchande le mieux
> et le +, celle qui a les plus belles habitations, celle qui prête ses troupes lorsqu'il y a besoin, celle qui fait
> de nombreux festivals, celle qui a une culture et une éducation prospère, celle qui fait la guerre pour de bonnes
> raisons, celle qui fait des dons à César. A l'inverse, César pourra être en colère contre des joueurs qui sont trop
> belliqueux entre eux, et envoyer son armée pour attaquer tous les joueurs de la carte (qui seront donc tous
> « perdants » dans un sens).
> Une guerre entre joueurs sur la carte ne devra pas durer trop longtemps, ni être trop « puissante » d'un coup, car
> ça attirera l'œil de César. Pour gagner, il faudra soit se développer mieux et + que son adversaire en se servant
> de lui, ou bien assécher son concurrent avec des frappes commerciales et militaires très ciblées.
> Focus sur l'équilibrage : ce ne sont pas des mécaniques communes dans les jeux, car l'économie n'est pas aussi
> développée, mais ça va être vraiment le but ici. — Alexandre

D'où vient chaque critère dans le plan :

| Critère d'Alexandre | Où il compte |
|---------------------|--------------|
| La plus prospère | Note **Prospérité** (§5.1) |
| Marchande le mieux et le plus | Note **Commerce** : volume et réseau (§5.2) |
| Les plus belles habitations | Note **Habitat** (§5.3) |
| Culture et éducation prospères | Note **Culture et éducation** (§5.4) |
| Se développer « plus » | Note **Grandeur** (§5.5) |
| Prête ses troupes | Faveur : **campagnes de César** (§6.3), lauriers de campagne |
| Nombreuses fêtes | Faveur : **fêtes** (§6.2) |
| Dons à César | Faveur : **dons** (§6.1) |
| Guerre pour de bonnes raisons | **Motifs** de guerre (§7.1) : faveur, triomphes, colère réduite |
| Trop belliqueux : armée de César | **Colère de César** (§7.2 à §7.4) |

## 2. Principes de conception et d'équilibrage

1. **Mesurer la cité, pas les clics.** L'essentiel des lauriers vient de l'état de la cité, mesuré par des notes de
   0 à 100 comme celles de l'original. Le joueur de Caesar III les connaît déjà : prospérité et culture sont **les
   notes d'origine**, équilibrées par les auteurs du jeu.
2. **Rendements décroissants partout.** Chaque note sature : doubler son commerce ne double pas la note. Chaque
   achat de faveur (dons, fêtes) a un plafond annuel. On ne gagne pas en poussant un seul levier.
3. **L'argent seul plafonne la faveur vers 75.** Dons et fêtes compensent l'oubli de César. Au-delà de 75, il faut
   le **servir** : troupes, demandes honorées, guerres justes.
4. **La guerre est un scalpel, pas une massue.** Son coût en colère croît avec le **carré** de la durée et de la
   puissance. Une frappe courte d'une légion passe. Une guerre longue ou massive déclenche César.
5. **Une menace commune crée un dilemme.** La colère de César punit tout le monde, mais le fautif d'abord. Celui qui
   la déclenche exprès ne peut pas y gagner (§9).
6. **Cumul dans le temps.** Les lauriers s'additionnent mois après mois. Une cité ruinée deux ans par une frappe
   perd deux ans de lauriers : c'est ce qui donne du sens à « assécher son concurrent ». Comme les notes partent de
   zéro, les premières années rapportent peu : l'avance prise au début reste rattrapable.
7. **Lisible.** Chaque mois, chaque joueur voit d'où viennent ses lauriers, sa faveur et sa part de la colère. Une
   règle qu'on ne voit pas ne s'équilibre pas.
8. **Jouable seul** (Alexandre essaie chaque version seul) : notes, faveur, dons, fêtes, campagnes et lauriers
   marchent à une cité ; la colère reste à zéro sans adversaire, et `simtool` la teste.
9. **Déterministe et réglable.** Tout est entier, calculé dans la simulation, sauvegardé, et passe par des
   commandes. Les réglages sont réunis dans une seule table (`mp/caesar_rules.h`), recopiée dans DESIGN.

Repères de temps : un mois de jeu dure environ 20 à 30 secondes, une année 4 à 6 minutes selon la vitesse. Une
partie de 20 ans dure donc 1 h 30 à 2 h. Une caravane met environ 45 jours à traverser la carte pour 2.

## 3. Vue d'ensemble : quatre compteurs

```
          état de la cité                        actions envers César
  ┌──────────────────────────────┐     ┌──────────────────────────────────────┐
  │ Prospérité  Commerce  Habitat │     │ dons, fêtes, troupes, demandes,       │
  │ Culture-éducation   Grandeur  │     │ guerres justes  /  agressions          │
  └──────────────┬───────────────┘     └──────────────────┬───────────────────┘
                 │ valeur V (0-100)                         │ faveur F (0-100)
                 └──────────────┐            ┌──────────────┘
                                ▼            ▼
                 lauriers du mois = V × (50 + F) / 1000      + hauts faits (§4.2)
                                │
                                ▼
                 total des lauriers → verdict de César en fin de partie

  guerres entre joueurs ──► COLÈRE DE CÉSAR (jauge commune 0-100) ──► avertissement, ultimatum, expédition
                              └─ part de chaque joueur (sa belligérance) ─► pertes de faveur et de lauriers
```

| Compteur | Portée | Bouge comment | Sert à |
|----------|--------|---------------|--------|
| Notes (5) et valeur V | par cité | chaque mois, d'après l'état de la cité | lauriers |
| Faveur F | par cité | actions, et retour lent vers 50 | multiplicateur, motifs, sanctions |
| Lauriers | par cité | s'additionnent chaque mois, plus les hauts faits | victoire |
| Colère C | **commune** | guerres (durée, puissance, dégâts), décrue en paix | menace sur tous |
| Belligérance | par cité | sa part de la colère | qui paie quand César frappe |

## 4. Les lauriers

### 4.1 Gain mensuel

```
valeur V  = 25 % Prospérité + 25 % Commerce + 20 % Culture-éducation + 15 % Habitat + 15 % Grandeur   (0 à 100)
lauriers du mois = V × (50 + F) / 1000                                                   (0 à 15 par mois)
```

- Faveur 0 : ×0,5 ; faveur 50 : ×1 ; faveur 100 : ×1,5. César **multiplie** le développement, il ne le remplace
  pas : une cité pauvre et flatteuse ne gagne pas.
- Ordres de grandeur visés : cité moyenne en milieu de partie (V = 40, F = 60) : environ 4,4 par mois, 53 par an.
  Belle cité de fin de partie (V = 70, F = 80) : environ 9 par mois, 110 par an. Partie de 20 ans : le vainqueur
  autour de 1 200 à 1 500.
- Les lauriers sont calculés en dixièmes (entiers) pour ne pas perdre les petits gains.

### 4.2 Hauts faits (gains ponctuels)

| Haut fait | Lauriers | Remarque |
|-----------|----------|----------|
| Campagne de César gagnée | jusqu'à 60, selon sa part de la force envoyée | §6.3 |
| Triomphe (légion ennemie détruite dans une guerre **juste**) | 20 | un arc de triomphe de plus, comme après une bataille lointaine |
| Demande de César honorée (option) | 10 | §6.4 |

Les dons et les fêtes ne donnent **jamais** de lauriers directement, seulement de la faveur : l'argent ne s'achète
pas des points de victoire, il achète un multiplicateur plafonné.

### 4.3 Fin de partie et verdict

- **Durée** choisie dans le salon (10, 15, 20 ou 30 ans ; 20 par défaut). À la fin, César désigne la cité qui a le
  plus de lauriers. À égalité, la meilleure faveur.
- **Consulat anticipé** (option du salon, active par défaut) : à partir de la 10e année, une cité qui a au moins
  1,5 fois les lauriers de la deuxième pendant deux bilans annuels de suite est nommée consul, et la partie
  s'arrête. Cela évite de jouer dix ans une partie jouée d'avance.
- **Partie seule** : pas de rival. César donne un **titre** selon les lauriers, avec les rangs de l'original
  (Citoyen… Consul, César). Alexandre peut ainsi mesurer ses parties seul.
- Les **rangs** servent aussi en cours de partie : ils montent avec les lauriers et fixent le salaire autorisé
  (§6.1), comme les promotions de la campagne.
- Le mode « score à durée limitée » actuel (culture + prospérité + paix + population / 100, provisoire) disparaît
  au profit de ce verdict. « Sans fin » reste pour les parties libres.

## 5. Les cinq notes de développement

Chaque note vaut de 0 à 100, calculée chaque mois. Les notes de volume saturent selon `100 × x / (x + réf)` : elles
valent 50 à la référence, 75 à trois fois la référence, et ne plafonnent jamais net. Les références se calibrent sur
de vraies cités (§10.2).

### 5.1 Prospérité
- **Mesure** : la note de prospérité d'origine (habitat, bénéfice de l'année, salaires face à Rome, chômage…).
- **Pourquoi** : c'est exactement « la plus prospère », et le jeu d'origine l'a déjà équilibrée.
- **Comportement d'origine** (code-map/07 §1) : recalculée une fois par an, elle bouge de −5 à +10 par an. Un
  bénéfice dans l'année vaut +5, quel que soit son montant. Chômage, deux sortes de nourriture, salaires face à Rome,
  villas, tentes, tribut payé et hippodrome valent chacun ±1. Elle plafonne à la moyenne des valeurs de prospérité
  des maisons. Le tribut annuel existe toujours en multijoueur (D-026) et compte donc normalement.
- **Conséquence** : c'est une note **lente**, qui récompense la constance. Une frappe la touche peu ; ce sont le
  commerce et l'habitat qui encaissent les frappes. Il faut des années pour la monter : une bonne raison de lui
  donner un quart du poids.

### 5.2 Commerce
- **Volume (70 % de la note)** : la valeur des marchandises vendues sur les 12 derniers mois, à l'empire et aux
  joueurs, **au prix de référence de l'empire** (pas au prix négocié). Les achats auprès d'autres joueurs comptent
  pour moitié : César aime la province qui commerce avec elle-même.
- **Réseau (30 %)** : le nombre de relations actives (un partenaire et une ressource, au moins une caravane livrée
  dans l'année). Un carrefour commercial vaut plus qu'un seul gros client.
- **Garde-fous** :
  - prix de référence, pour qu'un prix de 9 999 Dn ne gonfle rien ;
  - entre deux joueurs, flux **nets** par ressource et par an : vendre du marbre à B puis le lui racheter ne compte
    pas.
- **Effet recherché** : l'embargo, la hausse de prix, l'assèchement du stock et l'interception des caravanes
  frappent directement cette note chez la cible. Ce sont les « frappes commerciales ».

### 5.3 Habitat
- **Mesure** : la qualité moyenne des maisons, pondérée par leurs habitants. Chaque niveau de maison (tente à palais,
  20 niveaux) vaut `100 × (niveau / 19)²` : la courbe est convexe, une villa compte beaucoup plus qu'une insula.
- **Pourquoi** : « les plus belles habitations » mesure la qualité ; la quantité est dans la Grandeur. Une ville
  pleine de tentes a beaucoup d'habitants et un mauvais habitat.

### 5.4 Culture et éducation
- **Mesure** : la note de culture d'origine, recalculée chaque mois : théâtres 25 points, religion 30, écoles 15,
  bibliothèques 20, académies 10. Chaque part est donnée par paliers de couverture (à partir de 30, 50, 70, 85 et
  100 %), et la couverture se calcule en places par habitant (par exemple 75 élèves par école, 800 lecteurs par
  bibliothèque).
- **Pourquoi** : déjà équilibrée par les auteurs du jeu, et l'éducation y pèse 45 points sur 100 : c'est bien
  « culture et éducation ».
- **Limite connue** : la note d'origine compte des places, pas la desserte réelle des maisons. Amphithéâtres,
  colisées et hippodromes n'y entrent pas.
- **Effet recherché** : le marbre (temples, missions) et les ressources de luxe deviennent stratégiques. Assécher le
  marbre d'un rival freine sa culture.

### 5.5 Grandeur
- **Mesure** : la population, saturée par `100 × pop / (pop + 8 000)` (réglage de départ). Cela donne 50 à 8 000
  habitants, 75 à 24 000.
- **Pourquoi** : le « plus » d'Alexandre. La saturation empêche qu'une ville géante écrase les autres critères.

## 6. La faveur de César

La faveur F va de 0 à 100 et part de 50. Dans l'original, elle ne fait que baisser de 2 par an (code-map/07 §2). En
multijoueur, elle **revient vers 50 de 5 % de l'écart chaque mois**, soit environ la moitié de l'écart par an :
César oublie, en bien comme en mal. Rester haut demande un effort régulier, et une faute finit par s'effacer.

| Source | Effet sur F | Limite |
|--------|-------------|--------|
| Don (§6.1) | formule d'origine : +3, +5 ou +10 pour un premier don modeste, généreux ou somptueux, puis usure | au plus +10 par an |
| Fête (§6.2) | petite +1, grande +2, somptueuse +3 | une seule fête comptée tous les 6 mois, soit +6 par an au plus |
| Campagne de César : troupes envoyées (§6.3) | jusqu'à +15 selon la part, +5 si victoire | — |
| Campagne : rien envoyé alors qu'on a des légions | −5 | — |
| Demande honorée / refusée ou en retard (§6.4, option) | +6 / −4 | — |
| Guerre déclarée sans motif (§7.1) | −10 | doublé contre une cité dont les troupes servent César |
| Triomphe dans une guerre juste | +5 | — |
| Armée trop puissante (§7.5) | −1 par mois et par légion de trop | — |
| Dette (règle d'origine, toujours active) | −5, puis −10, puis faveur plafonnée à 10 | — |
| Responsable de l'expédition punitive (§7.4) | F tombe à 0 | — |

**Le calcul qui fixe le plafond de l'argent (principe 3).** À 5 % de l'écart par mois, un apport régulier de A
points par an tient la faveur à environ 50 + 5/3 × A.
- Dons et fêtes au maximum : A = 16, donc F ≈ 77. L'argent seul plafonne bien vers 75.
- Ajouter une campagne tous les 3 ans et deux demandes honorées par an : A ≈ 34, donc F ≈ 100.
- Les sommets sont réservés à ceux qui servent César.

La faveur est gardée en dixièmes dans l'état multijoueur, pour que les 5 % ne s'arrondissent pas à zéro. Le jeu
recopie sa valeur entière dans la faveur d'origine, que lisent l'interface et la dette.

### 6.1 Dons, salaire et épargne
- On reprend les mécanismes d'origine. Le gouverneur touche chaque mois un **salaire** selon son rang (0, 2, 5, 8,
  12, 20, 30, 40, 60, 80 ou 100 Dn), prélevé sur le trésor de la cité et versé à son **épargne personnelle**. Les
  **dons** puisent dans cette épargne et coûtent épargne / 8 + 20 (modeste), épargne / 4 + 50 (généreux) ou
  épargne / 2 + 100 (somptueux).
- Usure d'origine : les dons suivants rapportent de moins en moins (pour un don somptueux : +10, puis +5, +3, +1,
  puis 0), et elle se remet à zéro après 12 mois sans don. On ajoute un plafond de +10 par an.
- Le **rang** monte avec les lauriers (§4.3) et fixe le salaire autorisé. Un salaire au-dessus de son rang coûte
  chaque année l'écart en faveur, comme dans l'original. Un salaire en dessous rapporte +1.
- **Choix pour le joueur** : se payer pour donner, c'est prendre de l'argent à la cité (constructions, commerce)
  pour le multiplicateur. C'est un vrai arbitrage, sans bonne réponse évidente.

### 6.2 Fêtes
- Les fêtes d'origine gardent leur coût, qui suit la population : population / 20 + 10 pour une petite fête, / 10 + 20
  pour une grande, / 5 + 40 et du vin pour une somptueuse. Une cité de 10 000 habitants paie donc 510, 1 020 ou
  2 040 Dn.
- Elles gardent aussi leurs effets : jusqu'à +40 sur l'humeur du dieu fêté, bonheur des maisons +7, +9 ou +12
  (bien moins pour une deuxième fête dans l'année).
- En plus, elles plaisent à César, mais **une seule fête compte tous les 6 mois**. Les « nombreuses fêtes »
  d'Alexandre deviennent une habitude à tenir : deux par an, régulièrement.

### 6.3 Campagnes de César : prêter ses troupes
- Tous les 3 à 4 ans environ (dates tirées au début de la partie, les mêmes pour tous), César annonce une
  **campagne** : une armée ennemie menace une ville de l'empire. C'est la **bataille lointaine** d'origine, que
  les cartes multijoueur ne contiennent pas : `mp/caesar` la crée lui-même.
- Chaque joueur a **6 mois** pour envoyer des légions depuis la carte de l'empire. Les forces de tous les joueurs
  s'additionnent face à l'ennemi : c'est un moment de **coopération** dans une partie de rivalité.
- **Règle d'origine** (code-map/07 §5) : Rome gagne si sa force atteint celle de l'ennemi. Un légionnaire formé vaut
  3, un autre soldat 2 ou 1. L'avance ne fait que réduire les pertes, de 70 % à 5 % des soldats. Une défaite tue
  **tous** les soldats envoyés.
- **Force ennemie** : 60 par joueur au départ, +5 % par année de jeu. Seule, une cité la bat avec deux légions
  formées (2 × 16 × 3 = 96). La force romaine se compte en entier : l'original la range sur 8 bits et repartirait de
  0 au-delà de 255, ce que plusieurs joueurs dépasseraient.
- **Récompense** selon la part de chacun dans la force envoyée :
  - faveur jusqu'à +15, et +5 en cas de victoire ;
  - jusqu'à 60 lauriers en cas de victoire, et un arc de triomphe comme dans l'original ;
  - pertes de soldats comme dans l'original.
- **Refus** : la cité qui avait des légions et n'a rien envoyé perd 5 de faveur, contre −50 dans l'original, où la
  bataille ne concernait qu'elle. La cité sans légion ne perd rien.
- **Tension voulue** : des légions parties pendant des mois laissent la cité exposée. Mais attaquer une cité dont les
  troupes servent César est la pire faute (§7.1).

### 6.4 Demandes de César (option du salon, à valider)
- Les demandes d'origine (« envoyez 20 chargements de vin ») reviennent, calculées selon la taille de la cité. La
  même ressource est demandée à tous en même temps.
- **Intérêt** : elles créent des pics de demande sur le marché entre joueurs. Accaparer le vin quand César en
  demande, c'est une frappe commerciale.

## 7. La guerre sous l'œil de César

La mécanique de combat (légions chez l'adversaire, ordre « attaquer », portes et murs, interception des caravanes)
est celle prévue en DESIGN §7. Ce plan ajoute le **cadre** : déclaration, motif, colère.

### 7.1 Déclaration et motif
- On ne peut attaquer un joueur (soldats, bâtiments, caravanes) qu'en **guerre déclarée**. Déclarer passe par une
  commande et s'annonce à tous. La guerre commence **un mois** après la déclaration : le préavis laisse au défenseur
  le temps de rappeler ses troupes. C'est un garde-fou contre la ruée.
- Le **motif** est déterminé par le jeu, pas choisi par le joueur :

| Motif | Condition | Effet |
|-------|-----------|-------|
| **Riposte** | la cible a déclaré la guerre au joueur, l'a attaqué ou a intercepté ses caravanes dans les 12 derniers mois | juste : colère comptée à moitié, triomphes possibles |
| **Mandat de César** | César a déclaré la cible « ennemie de Rome » : faveur sous 20, ou fautive pendant un ultimatum | juste : colère comptée au quart, +5 de faveur par triomphe |
| **Sans motif** | les autres cas | injuste : −10 de faveur à la déclaration (−20 si la cible a des troupes en campagne pour César), colère pleine, pas de triomphe |

- Paix :
  - elle se signe quand les deux camps la proposent, ou quand César l'impose (§7.3) ;
  - redéclarer la guerre au même joueur dans les 12 mois suivants est toujours « sans motif », et la colère compte
    double.

### 7.2 La colère de César (jauge commune)
Chaque mois, chaque guerre en cours ajoute à la jauge :

| Terme | Valeur par mois | Ce qu'il punit |
|-------|-----------------|----------------|
| **Durée** | le n-ième mois de guerre ajoute n (1, 2, 3…) | les guerres longues : 3 mois coûtent 6, 6 mois 21, 12 mois 78 |
| **Puissance** | 2 × L², L = légions de l'agresseur dans le territoire adverse | les coups massifs : 1 légion 2, 2 légions 8, 3 légions 18, 4 légions 32 |
| **Dégâts** | +1 par bâtiment détruit, +2 par caravane interceptée, +1 par 10 habitants tués | les ravages |

- Le motif pondère ces termes : sans motif ×1, riposte ×½, mandat ×¼.
- En paix générale, la jauge baisse de 3 par mois ; s'il y a une guerre quelque part, de 1 par mois.
- La **belligérance** d'un joueur, c'est sa part de la jauge : ce que ses guerres ont ajouté, au prorata de ses
  actes.
- **Ce que ça donne** (réglages de départ) :
  - **Frappe ciblée sans motif** : 1 légion, 3 mois de guerre dont 2 chez l'adversaire, 8 bâtiments et 3 caravanes.
    Durée 6, puissance 4, dégâts 14 : 24 de colère, effacés en 8 mois de paix, plus 10 de faveur perdus. Ça passe,
    mais pas deux fois dans l'année.
  - **Guerre totale** : 3 légions, 8 mois, dont 6 chez l'adversaire. Durée 36, puissance 108 : César intervient
    avant la fin, même avec un motif de riposte.
  - **Guerre sous mandat** : 2 légions, 6 mois, contre un ennemi de Rome. (21 + 40 + dégâts) / 4, soit environ 20 :
    César laisse faire.

### 7.3 Les étapes de la colère
| Seuil | Ce qui se passe |
|-------|-----------------|
| 40 | **Avertissement** : lettre de César à tous (« les troubles de la province me déplaisent »). Les responsables sont nommés. |
| 70 | **Ultimatum** : toutes les guerres doivent cesser dans les 3 mois (paix imposée). Le responsable principal est déclaré « ennemi de Rome » : la guerre contre lui devient un mandat. |
| 100 | **Expédition punitive** (§7.4). |

### 7.4 L'expédition punitive
- Les légions de César débarquent dans **chaque** cité : c'est l'invasion de César d'origine (code-map/07 §3). Ses
  légionnaires sont plus forts que ceux des joueurs (attaque 13 et défense 2, contre 10 et 0). Un joueur a au plus
  96 soldats.
- Dans l'original, les vagues successives comptent 32, 64, 96 puis 144 soldats. Ici, une cité innocente reçoit 32
  soldats, et le responsable principal 96 à 144 selon sa part de la colère. On ne sort pas indemne d'une guerre
  totale.
- Les règles de retraite de l'original, qui dépendent de la faveur, ne s'appliquent pas. L'expédition reste
  jusqu'à sa destruction, ou 12 mois au plus. La vaincre rapporte +10 de faveur, comme dans l'original.
- Pendant 12 mois, la province est **en disgrâce** : tous les lauriers mensuels sont divisés par deux.
- Le responsable principal tombe à 0 de faveur et perd **25 %** de ses lauriers. Les autres responsables en perdent
  au prorata de leur part. Les innocents ne perdent que la disgrâce et les dégâts.
- La jauge redescend ensuite à 30.
- **Seconde expédition dans la même partie** (option du salon, à valider) : « Rome reprend la province ». La partie
  s'arrête, et César ne désigne aucun vainqueur.

### 7.5 Une armée trop puissante attire l'œil de César
- Même en paix, César tolère environ **une légion par tranche de 5 000 habitants (au moins 2)**. Au-delà, chaque
  légion de trop coûte 1 de faveur par mois.
- Cela freine la course aux armements, et la guerre « trop puissante d'un coup » devient coûteuse avant même d'être
  déclarée.

## 8. Les deux chemins vers la victoire

**Le bâtisseur courtisan.** Il développe les cinq notes et tient sa faveur à 70-80 : deux fêtes par an, un don
annuel, des troupes à chaque campagne.
- Résultat : V autour de 65 en fin de partie, multiplicateur ×1,25 environ. C'est le chemin « normal ».
- Sa faiblesse : ses légions partent en campagne, et ses caravanes dépendent de ses voisins.

**Le prédateur ciblé.** Développement moyen, mais il frappe là où ça fait mal, au bon moment :
- couper le marbre du rival quand celui-ci construit ses temples ;
- intercepter ses caravanes pendant que les légions du rival sont parties chez César ;
- brûler un entrepôt clé en trois mois de guerre.
- Résultat : il paie un peu de faveur et de colère. Le rival perd des mois de commerce et de culture, donc des
  lauriers mensuels cumulés.
- Sa faiblesse : trop de frappes font monter la jauge, et l'expédition le désigne comme fautif.

**Objectif d'équilibrage** (vérifié en §10) :
- une frappe ciblée **réussie** doit coûter à la victime 3 à 5 fois ce qu'elle coûte à l'agresseur, en lauriers sur
  les deux années suivantes ;
- une frappe **ratée** (repoussée, ou qui déclenche l'avertissement) doit coûter plus à l'agresseur qu'à la cible ;
- à développement égal, le bâtisseur et le prédateur doivent gagner chacun environ une partie sur deux.

## 9. Exploits prévus et garde-fous

| Exploit | Garde-fou |
|---------|-----------|
| Deux joueurs se renvoient les mêmes marchandises pour gonfler le commerce | flux nets par paire et par ressource, au prix de référence (§5.2) |
| Prix de vente absurde entre complices | volume compté au prix de référence de l'empire |
| Acheter la faveur avec l'épargne | plafond de +15 par an pour les dons, retour vers 50, salaire limité par le rang |
| Fêtes à la chaîne | une fête comptée tous les 6 mois |
| Le dernier déclenche exprès la colère pour faire perdre tout le monde | le fautif perd 25 % de ses lauriers et tombe à 0 de faveur : la manœuvre le fait plonger plus que les autres |
| Le premier déclenche la colère pour figer son avance | même règle : la disgrâce touche tout le monde, la perte surtout le fautif |
| Déclarer, frapper, signer la paix, recommencer | redéclaration dans les 12 mois : sans motif, colère double ; les dégâts restent dans la jauge |
| Envoyer une poignée de soldats à la campagne pour toucher la récompense | récompense selon la part de la force (soldats × entraînement × moral), avec un minimum |
| Gonfler la population en tentes | la Grandeur sature, l'Habitat chute, la prospérité aussi |
| Thésauriser sans construire | le trésor ne compte que par le bénéfice dans la prospérité d'origine |
| Construire écoles et théâtres loin des maisons pour la note de culture | règle d'origine (des places, pas la desserte) : on la garde, car ces bâtiments coûtent des ouvriers et de l'entretien ; à surveiller en télémétrie |
| Se liguer à deux contre un (3 ou 4 joueurs) | permis (c'est de la diplomatie), mais chaque agresseur porte sa part de la colère |
| Attaquer un joueur dont les légions sont chez César | sans motif double (−20 de faveur), colère pleine : la pire faute du jeu |

## 10. Méthode d'équilibrage

L'équilibrage est le but de ce chantier : il a ses outils, ses mesures et ses critères écrits, comme la parité.

### 10.1 Une table de réglages
Tous les nombres de ce plan sont dans `mp/caesar_rules.h` : poids des notes, références, plafonds, termes de la
colère, seuils. Chaque changement de réglage est noté dans DECISIONS avec la mesure qui l'a motivé.

### 10.2 Calibrer les notes sur de vraies cités
- Le dossier `test/data` contient une trentaine de vraies cités de Caesar III (Lugdunum, Massilia, Valentia…), à
  des stades variés.
- `simtool notes SAVE` affichera les cinq notes et leurs mesures brutes (valeur des ventes, population, niveaux des
  maisons).
- On choisit les références pour qu'une bonne cité de milieu de partie ait 40 à 55 par note, et les meilleures
  cités de fin 75 à 85.
- On vérifie aussi qu'aucune note ne domine : chacune doit varier autant que les autres d'une cité à l'autre.

### 10.3 Simulations sans tête
- **Trajectoires** : faire avancer ces cités plusieurs années (autopilot) et tracer V, F et les lauriers. Le gain
  doit croître avec le développement, sans palier qui enferme.
- **Duels scriptés** : deux cités copiées sur une carte préparée (déjà possible avec `gotocity`). On les pilote par
  commandes (dons, fêtes, déclaration, envoi d'une légion vers un bâtiment) :
  - frappe ciblée contre bâtisseur ;
  - guerre totale ;
  - campagne de César avec et sans troupes.
  On mesure les lauriers des deux sur 2 à 5 ans.
- Aucun compteur d'origine ne cumule les pertes, les victimes ou les destructions par attaquant : la colère (§7.2)
  et ces mesures demandent de nouveaux compteurs dans `mp/caesar`.
- Ces mesures deviennent des **tests ctest** avec des bornes, pour que tout réglage futur qui casse l'équilibre
  voulu échoue :
  - « frappe ciblée type : colère < 30 » ;
  - « guerre totale type : expédition » ;
  - « dons seuls : faveur < 78 au bout de 5 ans » ;
  - « rapport des pertes victime / agresseur entre 3 et 5 ».

### 10.4 Télémétrie des vraies parties
- Chaque mois, la sauvegarde garde le détail des gains de chaque joueur : chaque note, faveur, lauriers par source,
  colère et parts.
- `simtool laurels PARTIE.mpsav` le sort en tableau. Après chaque partie d'Alexandre, on lit la sauvegarde au lieu
  de deviner, et on ajuste.
- Le journal macOS (`julius-log.txt`) note aussi les événements de César (avertissements, expéditions).

### 10.5 Critères chiffrés d'une partie équilibrée
- Aucune note ne fait plus de 30 % des lauriers du vainqueur.
- La faveur du vainqueur vient pour moins de la moitié des dons et fêtes.
- Le joueur en tête à mi-partie gagne dans 60 à 75 % des cas : l'avance compte, mais le retour reste possible.
- Une partie à 2 joueurs déclenche une expédition punitive dans moins d'une partie sur trois, quand les deux
  jouent « normalement ».

## 11. Interface

- **Conseiller impérial** (fenêtre d'origine, contenu multijoueur) :
  - faveur et sa tendance, rang, lauriers ;
  - les cinq notes, avec barres et conseils à la manière du conseiller des notes ;
  - salaire et épargne, dons ;
  - campagne et demande en cours.
- **Bandeau multijoueur** (en bas à gauche) : les lauriers remplacent le score, et une **jauge de colère** colorée
  est toujours visible.
- **Lettres de César** : messages en plein écran, comme l'alerte de prix, pour l'avertissement, l'ultimatum,
  l'expédition, les campagnes et le verdict.
- **Fenêtre de la province** :
  - lauriers et faveur de chacun (classement public, option du salon) ;
  - guerres en cours et leur motif ;
  - part de chacun dans la colère.
- **Salon** :
  - mode « Jugement de César » et sa durée ;
  - options : consulat anticipé, demandes de César, seconde expédition fatale, classement public.

## 12. Découpage et pièges techniques

Les jalons de [ROADMAP.md](ROADMAP.md) suivent cet ordre :
- **M9, César juge** : jouable en paix et **seul**, livré à Alexandre avant la guerre ;
- **M10, la guerre sous l'œil de César** : l'ancien M9, auquel s'ajoute l'interception des caravanes M8.7 ;
- **M11, équilibrage et finitions** : l'ancien M10.

Pièges relevés dans le code d'origine (code-map/07), à traiter dans ces jalons :
- **Faveur** : en partie libre, l'original la remet à 50 chaque mois (`scenario_is_open_play`). Il faut donc que
  `mp/caesar` tienne la faveur lui-même, au lieu de rallumer la mise à jour d'origine.
- **Usure des dons** : elle ne se remet à zéro que dans cette mise à jour mensuelle, éteinte en multijoueur ;
  `mp/caesar` doit aussi compter les mois sans don.
- **Dette** : la pénalité de dette (−5, −10, plafond 10) tourne déjà en multijoueur (`city/emperor.c`
  `update_debt_state`). Elle reste, et se cumule avec les règles de ce plan.
- **Batailles lointaines et demandes** : elles viennent des données du scénario, vides sur les cartes préparées.
  `mp/caesar` les crée, à partir de la graine de la partie (déterminisme).
- **Force romaine sur 8 bits** : `distant_battle.roman_strength` repart de 0 au-delà de 255. Le cumul de plusieurs
  joueurs se compte en entier.
- **Paix** : la pénalité « bâtiment détruit par un ennemi » va à la cité dont c'est le tour de simulation. En
  guerre entre joueurs, il faudra l'imputer à la victime (M10).

## 13. Points à valider par Alexandre

1. Lauriers **cumulés** mois après mois, plutôt qu'un classement sur l'état final ?
2. Faveur en **multiplicateur** (×0,5 à ×1,5) ?
3. Poids des notes : prospérité 25, commerce 25, culture-éducation 20, habitat 15, grandeur 15 ?
4. **Préavis d'un mois** avant une guerre ?
5. Seconde expédition punitive : fin de partie sans vainqueur ?
6. **Demandes de biens** de César : oui ou non ?
7. Classement public des lauriers ?
8. Durée par défaut de 20 ans, et consulat anticipé ?
