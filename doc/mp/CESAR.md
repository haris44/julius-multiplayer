# César juge — plan de la condition de victoire

> Plan demandé par Alexandre le 2026-10-06. Il remplace l'exigence E8 (« suppression complète de César ») par un
> César **arbitre** de la partie. Décision : D-050. Tâches : jalons M9 à M11 de [ROADMAP.md](ROADMAP.md).
> Faits du jeu d'origine (formules, valeurs, `fichier:ligne`) : [code-map/07](code-map/07-cesar-faveur-notes-equilibrage.md).
> Les valeurs chiffrées de ce plan sont des **réglages de départ** : la §10 dit comment on les mesure et les corrige.

## 0. En bref

- César revient. Il ne gouverne pas les cités : il **observe** la province et **désigne le vainqueur** : la première
  cité qui atteint le **score** de **lauriers** (points de victoire) fixé dans le salon.
- Les lauriers viennent de deux sources, qui **s'additionnent** :
  - les **lauriers de la cité**, gagnés chaque mois d'après cinq notes : prospérité, commerce, habitat, culture et
    éducation, grandeur ;
  - les **lauriers de César**, gagnés en le **servant** (dons, fêtes, troupes prêtées à ses campagnes, demandes
    honorées, guerres justes) et perdus par l'agression.
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
| Prête ses troupes | Lauriers de César : **campagnes** (§6.3) |
| Nombreuses fêtes | Lauriers de César : **fêtes** (§6.2) |
| Dons à César | Lauriers de César : **dons** (§6.1) |
| Guerre pour de bonnes raisons | **Motifs** de guerre (§7.1) : triomphes, pas de pénalité, colère réduite |
| Trop belliqueux : armée de César | **Colère de César** (§7.2 à §7.4) |

## 2. Principes de conception et d'équilibrage

1. **Mesurer la cité, pas les clics.** L'essentiel des lauriers vient de l'état de la cité, mesuré par des notes de
   0 à 100 comme celles de l'original. Le joueur de Caesar III les connaît déjà : prospérité et culture sont **les
   notes d'origine**, équilibrées par les auteurs du jeu.
2. **Rendements décroissants partout.** Chaque note sature : doubler son commerce ne double pas la note. Chaque
   action envers César a un plafond par période. On ne gagne pas en poussant un seul levier.
3. **L'argent ne fait pas gagner.** Dons et fêtes rapportent au plus 22 lauriers par an, environ un cinquième de ce
   que rapporte une belle cité. Le reste des lauriers de César se gagne en le **servant** : troupes, demandes
   honorées, guerres justes.
4. **La guerre est un scalpel, pas une massue.** Son coût en colère croît avec le **carré** de la durée et de la
   puissance. Une frappe courte d'une légion passe. Une guerre longue ou massive déclenche César.
5. **Une menace commune crée un dilemme.** La colère de César punit tout le monde, mais le fautif d'abord. Celui qui
   la déclenche exprès ne peut pas y gagner (§9).
6. **Cumul dans le temps.** Les lauriers s'additionnent mois après mois. Une cité ruinée deux ans par une frappe
   perd deux ans de lauriers : c'est ce qui donne du sens à « assécher son concurrent ». Comme les notes partent de
   zéro, les premières années rapportent peu : l'avance prise au début reste rattrapable.
7. **Lisible.** Un seul compteur de victoire, les lauriers, additionné et détaillé par source. Chaque joueur voit
   ce que vaut chaque action et sa part de la colère. Une règle qu'on ne voit pas ne s'équilibre pas.
8. **Jouable seul** (Alexandre essaie chaque version seul) : notes, dons, fêtes, campagnes et lauriers marchent à une
   cité ; la colère reste à zéro sans adversaire, et `simtool` la teste.
9. **Déterministe et réglable.** Tout est entier, calculé dans la simulation, sauvegardé, et passe par des
   commandes. Les réglages sont réunis dans une seule table (`mp/caesar_rules.h`), recopiée dans DESIGN.

Repères de temps : un mois de jeu dure environ 20 à 30 secondes, une année 4 à 6 minutes selon la vitesse. Une
partie au score par défaut (1 000 lauriers, atteint vers la 12e année, §4.3) dure donc 50 minutes à 1 h 15. Une
caravane met environ 45 jours à traverser la carte pour 2.

## 3. Vue d'ensemble : trois compteurs

```
          état de la cité                        actions envers César
  ┌──────────────────────────────┐     ┌──────────────────────────────────────┐
  │ Prospérité  Commerce  Habitat │     │ dons, fêtes, troupes, demandes,       │
  │ Culture-éducation   Grandeur  │     │ guerres justes  /  agressions          │
  └──────────────┬───────────────┘     └──────────────────┬───────────────────┘
                 │ chaque mois                              │ à chaque action
                 ▼                                          ▼
       lauriers de la cité (0 à 10 par mois)      lauriers de César (+ ou −)
                 └──────────────────┬───────────────────────┘
                                    ▼
                  total des lauriers → verdict de César en fin de partie

  guerres entre joueurs ──► COLÈRE DE CÉSAR (jauge commune 0-100) ──► avertissement, ultimatum, expédition
                              └─ part de chaque joueur (sa belligérance) ─► pertes de lauriers
```

| Compteur | Portée | Bouge comment | Sert à |
|----------|--------|---------------|--------|
| Notes (5) | par cité | chaque mois, d'après l'état de la cité | lauriers de la cité |
| Lauriers | par cité | notes chaque mois, plus ou moins les actions envers César | victoire |
| Colère | **commune** | guerres (durée, puissance, dégâts), décrue en paix | menace sur tous |
| Belligérance | par cité | sa part de la colère | qui paie quand César frappe |

La **faveur** d'origine (0 à 100) n'est plus utilisée en multijoueur : elle reste figée et cachée, comme aujourd'hui
(D-026). Tout ce qu'elle mesurait devient des lauriers.

## 4. Les lauriers

### 4.1 Lauriers de la cité (chaque mois)

Chaque note rapporte des lauriers en proportion de sa valeur. Une note tenue à 100 pendant un an rapporte :

| Note | Lauriers par an à 100 |
|------|-----------------------|
| Prospérité | 30 |
| Commerce | 30 |
| Culture et éducation | 24 |
| Habitat | 18 |
| Grandeur | 18 |
| **Total** | **120** (10 par mois) |

- Calcul : lauriers du mois = (25 × Prospérité + 25 × Commerce + 20 × Culture + 15 × Habitat + 15 × Grandeur)
  / 1 000. Il est fait en dixièmes (entiers), pour ne pas perdre les petits gains.
- Ordres de grandeur visés :
  - cité moyenne en milieu de partie (notes autour de 40) : environ 48 lauriers par an ;
  - belle cité de fin de partie (notes autour de 70) : environ 84 par an ;
  - en 20 ans : environ 1 000 lauriers de la cité pour une belle cité. Avec ceux de César, le score par défaut de
    1 000 lauriers est atteint vers la 12e année (§4.3).

### 4.2 Lauriers de César (actions)

Détail en §6 et §7. Un gouverneur assidu gagne environ 40 lauriers de César par an :
- deux fêtes comptées (12) ;
- un don (10) ;
- sa part des campagnes (environ 17 par an en moyenne).

Les lauriers de César font donc environ un tiers du total, comme dans la liste d'Alexandre, où la moitié des critères
concerne le service de César. La part achetée par l'argent (dons et fêtes) reste vers un cinquième.

### 4.3 Fin de partie et verdict

- **Victoire au score** (Alexandre, D-053) : le salon fixe un score de lauriers (500, 1 000, 1 500 ou 2 000 ; 1 000
  par défaut). La première cité qui l'atteint, au bilan d'un mois, gagne : César en fait son **héritier**, et la
  partie s'arrête.
  - Si plusieurs cités l'atteignent le même mois, gagne celle qui a le plus de lauriers ; à égalité exacte, celle
    qui a le plus de lauriers de la cité.
  - Ordres de grandeur visés pour une cité bien menée (mesurés en M11) : 500 lauriers vers la 8e année, 1 000 vers
    la 12e, 1 500 vers la 15e, 2 000 vers la 19e.
  - Le classement est **public** (§11) : chacun voit qui approche du score, et peut le freiner.
- Pas de durée fixe ni de consulat anticipé : le score les remplace. Une partie jouée d'avance s'arrête d'elle-même,
  quand le premier atteint le score.
- **Rangs** : ceux de l'original (Citoyen… Proconsul, César), un par dixième du score. Le dernier, César, c'est la
  victoire. En cours de partie, le rang fixe le salaire maximal (§6.1), comme les promotions de la campagne.
- **Partie seule** : pas de rival. Atteindre le score est la victoire, et le rang mesure le chemin parcouru.
  Alexandre peut ainsi mesurer ses parties seul : en combien d'années il atteint le score.
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

## 6. Les lauriers de César

Chaque action rapporte un nombre fixe de lauriers, affiché à côté du bouton qui la déclenche.

| Action | Lauriers | Limite |
|--------|----------|--------|
| Fête petite / grande / somptueuse (§6.2) | +2 / +4 / +6 | une seule fête comptée tous les 6 mois : 12 par an au plus |
| Don modeste / généreux / somptueux (§6.1) | +4 / +7 / +10 | un seul don compté tous les 12 mois : 10 par an au plus |
| Campagne de César gagnée (§6.3) | jusqu'à +60, selon sa part de la force | — |
| Campagne perdue, troupes envoyées | jusqu'à +15, selon sa part | — |
| Campagne : rien envoyé alors qu'on a des légions | −10 | — |
| Demande de César (§6.4, option) | une part de la cagnotte (10 par joueur), selon ses envois | échéance manquée : moitié de la cagnotte, −5 à qui n'a rien envoyé |
| Triomphe : légion ennemie détruite dans une guerre juste (§7.1) | +20 | — |
| Expédition de César vaincue sur ses terres (§7.4) | +20 | — |
| Guerre déclarée sans motif (§7.1) | −15 | −40 si la cible a des troupes en campagne pour César |
| Guerre brutale, sans préavis (§7.1) | −15 de plus | et la colère de cette guerre compte une fois et demie |
| Armée trop puissante (§7.5) | −2 par mois et par légion de trop | — |
| Responsable de l'expédition punitive (§7.4) | −25 % de ses lauriers | les autres responsables au prorata de leur part |

- **Régularité** : les plafonds sont **par période**. Une fête manquée dans un semestre ne se rattrape pas au
  suivant. C'est ce qui fait des « nombreuses fêtes » une habitude à tenir, sans jauge à surveiller.
- Ces actions donnent des lauriers en multijoueur, et plus de faveur : la faveur d'origine reste figée et cachée.

### 6.1 Dons, salaire et épargne
- On reprend les mécanismes d'origine. Le gouverneur touche chaque mois un **salaire** selon son rang (0, 2, 5, 8,
  12, 20, 30, 40, 60, 80 ou 100 Dn), prélevé sur le trésor de la cité et versé à son **épargne personnelle**. Les
  **dons** puisent dans cette épargne et coûtent épargne / 8 + 20 (modeste), épargne / 4 + 50 (généreux) ou
  épargne / 2 + 100 (somptueux).
- Un seul don compte tous les 12 mois. Il remplace l'usure d'origine, plus difficile à lire.
- Le **rang** monte avec les lauriers (§4.3) et fixe le salaire maximal : on ne peut pas choisir un salaire
  au-dessus de son rang. C'est plus simple que la pénalité d'origine.
- **Choix pour le joueur** : se payer pour donner, c'est prendre de l'argent à la cité (constructions, commerce)
  pour quelques lauriers. C'est un vrai arbitrage, sans bonne réponse évidente.

### 6.2 Fêtes
- Les fêtes d'origine gardent leur coût, qui suit la population : population / 20 + 10 pour une petite fête, / 10 + 20
  pour une grande, / 5 + 40 et du vin pour une somptueuse. Une cité de 10 000 habitants paie donc 510, 1 020 ou
  2 040 Dn.
- Elles gardent aussi leurs effets : jusqu'à +40 sur l'humeur du dieu fêté, bonheur des maisons +7, +9 ou +12
  (bien moins pour une deuxième fête dans l'année).
- En plus, elles rapportent des lauriers de César, mais **une seule fête compte tous les 6 mois**. Les
  « nombreuses fêtes » d'Alexandre deviennent une habitude à tenir : deux par an, régulièrement.

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
  - jusqu'à 60 lauriers en cas de victoire, et un arc de triomphe comme dans l'original ;
  - jusqu'à 15 lauriers en cas de défaite : César salue l'effort ;
  - pertes de soldats comme dans l'original.
- **Refus** : la cité qui avait des légions et n'a rien envoyé perd 10 lauriers. Dans l'original, elle perdait 50 de
  faveur, mais la bataille ne concernait qu'elle. La cité sans légion ne perd rien.
- **Tension voulue** : des légions parties pendant des mois laissent la cité exposée. Mais attaquer une cité dont les
  troupes servent César est la pire faute (§7.1).

### 6.4 Demandes de César (option du salon, active par défaut)
- Tous les 2 à 3 ans (dates et ressources tirées de la graine de la partie), César demande à **la province** des
  chargements d'une ressource qu'elle produit ou que l'empire vend : vin, huile, meubles, poterie, armes, marbre…
  - Le total suit la population de toute la province (réglage de départ : 2 chargements par 1 000 habitants, au
    moins 10).
  - Délai : 12 mois.
- **Chacun participe** (Alexandre, D-053) : chaque joueur envoie ce qu'il veut depuis ses entrepôts, en une ou
  plusieurs fois. L'original exigeait tout le chargement d'un coup. La demande se ferme dès que la province a tout
  envoyé, ou à l'échéance.
- **Lauriers selon ce que chacun donne** : une cagnotte de 10 lauriers par joueur, partagée au prorata des
  chargements envoyés.
  - Demande remplie : la cagnotte entière. Échéance manquée : la moitié, et −5 à qui n'a rien envoyé.
  - Exemple à 2 joueurs, 20 chargements de vin : A en envoie 15 et B 5 ; A gagne 15 lauriers, B 5.
  - Seul, on gagne les 10 lauriers en remplissant la demande, comme avant.
- **Tout est permis**, puisque c'est pour César. Pendant la demande, les frappes sur la ressource demandée ne le
  fâchent pas :
  - accaparer, surenchérir, ne plus vendre aux autres (le commerce ne compte jamais dans la colère) ;
  - intercepter les caravanes qui la transportent n'ajoute rien à la jauge de colère. L'interception reste un acte
    de guerre : elle demande une guerre déclarée (M10).
- **Intérêt** : un pic de demande sur le marché entre joueurs, et une course. Celui qui tient la ressource prend
  la cagnotte.

## 7. La guerre sous l'œil de César

La mécanique de combat (légions chez l'adversaire, ordre « attaquer », portes et murs, interception des caravanes)
est celle prévue en DESIGN §7. Ce plan ajoute le **cadre** : déclaration, motif, colère.

### 7.1 Déclaration et motif
- On ne peut attaquer un joueur (soldats, bâtiments, caravanes) qu'en **guerre déclarée**. Déclarer passe par une
  commande et s'annonce à tous. On choisit la forme de la guerre (Alexandre, D-053) :

| Forme | Début des combats | Ce qu'en pense César |
|-------|-------------------|----------------------|
| **Guerre honorable** | **3 mois** après la déclaration : le défenseur a le temps de rappeler ses troupes et de fermer ses portes | règles du motif (ci-dessous) |
| **Guerre brutale** | **tout de suite** | il n'aime pas ça : −15 lauriers de plus à la déclaration, et la colère de cette guerre compte une fois et demie |

- Pendant le préavis, personne n'attaque, ni l'un ni l'autre. Le défenseur peut frapper le premier en déclarant à
  son tour une guerre brutale : c'est une riposte, mais il paie le prix de la brutalité.
- Le **motif** est déterminé par le jeu, pas choisi par le joueur :

| Motif | Condition | Effet |
|-------|-----------|-------|
| **Riposte** | la cible a déclaré la guerre au joueur, l'a attaqué ou a intercepté ses caravanes dans les 12 derniers mois | juste : colère comptée à moitié, triomphes possibles |
| **Mandat de César** | César a déclaré la cible « ennemie de Rome » : elle a déclaré une guerre sans motif dans les 12 derniers mois, ou elle est la principale fautive d'un ultimatum | juste : colère comptée au quart, triomphes possibles |
| **Sans motif** | les autres cas | injuste : −15 lauriers à la déclaration (−40 si la cible a des troupes en campagne pour César), colère pleine, pas de triomphe |

- La forme et le motif se cumulent : une guerre brutale sans motif coûte 30 lauriers, et sa colère compte ×1,5.

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
| **Dégâts** | +1 par bâtiment détruit, +2 par caravane interceptée (rien si elle porte la ressource d'une demande de César en cours, §6.4), +1 par 10 habitants tués | les ravages |

- Le motif pondère ces termes : sans motif ×1, riposte ×½, mandat ×¼. Une guerre brutale les multiplie encore par
  1,5.
- En paix générale, la jauge baisse de 3 par mois ; s'il y a une guerre quelque part, de 1 par mois.
- La **belligérance** d'un joueur, c'est sa part de la jauge : ce que ses guerres ont ajouté, au prorata de ses
  actes.
- **Ce que ça donne** (réglages de départ) :
  - **Frappe ciblée sans motif** : 1 légion, 3 mois de guerre dont 2 chez l'adversaire, 8 bâtiments et 3 caravanes.
    Durée 6, puissance 4, dégâts 14 : 24 de colère, effacés en 8 mois de paix, plus 15 lauriers perdus. Ça passe,
    mais pas deux fois dans l'année. La même frappe en guerre brutale : 36 de colère et 30 lauriers perdus, juste
    sous l'avertissement.
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
  jusqu'à sa destruction, ou 12 mois au plus. La vaincre rapporte 20 lauriers (+10 de faveur dans l'original).
- Pendant 12 mois, la province est **en disgrâce** : tous les lauriers mensuels sont divisés par deux.
- Le responsable principal perd **25 %** de ses lauriers. Les autres responsables en perdent au prorata de leur
  part. Les innocents ne perdent que la disgrâce et les dégâts.
- La jauge redescend ensuite à 30.
- **Seconde expédition dans la même partie** : elle frappe comme la première. Option du salon, désactivée par
  défaut (D-053) : « Rome reprend la province », la partie s'arrête et César ne désigne aucun vainqueur.

### 7.5 Une armée trop puissante attire l'œil de César
- Même en paix, César tolère environ **une légion par tranche de 5 000 habitants (au moins 2)**. Au-delà, chaque
  légion de trop coûte 2 lauriers par mois.
- Cela freine la course aux armements, et la guerre « trop puissante d'un coup » devient coûteuse avant même d'être
  déclarée.

### 7.6 Le pont de César (T4.8, D-068, *à valider*)

**Le lieu.**
- Sur la carte à 4, le bras de mer coupe la carte d'ouest en est : deux joueurs sur chaque rive (D-047). La seule
  route d'une rive à l'autre passe sur le **pont de César**, au milieu de la carte : un pont pour navires d'environ
  25 cases, à environ 70 cases de chaque cité.
- Il appartient à César, comme ses routes : il est **indestructible** (D-034, D-047). Les navires passent dessous.
- Sur la carte pour 2, les deux joueurs vivent sur la même rive. Le pont ne mène qu'à la rive nord, sauvage : il
  ne compte que pour s'y installer (missions).

**Ce qui passe par le pont.**

| Qui | Par le pont ? | Pourquoi |
|-----|---------------|----------|
| Caravanes entre joueurs de rives différentes | **oui**, seul chemin | elles ne marchent que sur les routes. À 4 joueurs, 4 paires sur 6 ; à 3, 2 sur 3 |
| Commerce avec l'étranger | non | chaque cité a son point d'arrivée sur sa rive ; les navires passent sous le pont |
| Missionnaire qui s'installe sur l'autre rive | **oui** | il suit les règles de terrain des soldats (D-037) |
| Armées d'une rive à l'autre | **oui**, seul passage | le jeu n'a pas de transport de troupes par mer |
| Troupes prêtées aux campagnes de César (§6.3) | non | elles partent de leur cité, par la carte de l'empire |
| Expédition punitive (§7.4), invasions de l'IA | non | elles arrivent dans chaque cité |
| Immigrants | non | ils arrivent au point d'arrivée de leur cité |
| Eau | **non** | un aqueduc ne se pose ni sur l'eau ni sur un pont (règle d'origine) : l'aqueduc de César reste sur sa rive |

**Peut-on le couper, le bloquer, le tenir ? Avec le code d'aujourd'hui :**
- **le couper** : non, il est indestructible ;
- **le bloquer en paix** :
  - des soldats garés sur le pont n'arrêtent personne : les personnages ne se gênent pas, comme dans l'original ;
  - un mur ne se pose pas sur une route ;
  - mais une **porte** se pose sur une route. Rien n'interdit aujourd'hui d'en bâtir une sur la route de César, à
    l'entrée du pont, si l'endroit est dans sa zone (une mission à moins de 20 cases). Quand les portes ne laisseront
    passer que leur propriétaire (M10.2), elle fermera le pont à tout le monde. *À vérifier par un test* ;
- **le tenir en guerre** : oui, avec des soldats. M10 leur fait attaquer les soldats, les civils et les caravanes de
  l'ennemi.

**Que faire, et que fait César si un joueur le ferme ?** Trois options :

| Option | Règle | Pour | Contre |
|--------|-------|------|--------|
| **1. Terre de César** (recommandée) | personne ne bâtit ni ne revendique de zone près du pont. En paix, il est ouvert à tous. En guerre, on le tient avec des soldats, et des légions sur la terre de César comptent dans la colère comme si elles étaient chez l'adversaire | simple et lisible ; les joueurs neutres ne sont jamais gênés ; tenir le pont coûte de la colère, comme une invasion | le défenseur garde l'avantage : il attend juste derrière la terre de César, sans colère |
| **2. Blocus permis, puni** | une porte ou un fort à l'entrée du pont est permis. Fermer le pont aux autres est un **blocus** : lettre de César, puis colère chaque mois, puis « ennemi de Rome » | plus de diplomatie | plus de règles ; le blocus frappe aussi les neutres ; une tête de pont fortifiée fige la carte |
| **3. Pont destructible** | en guerre, les soldats peuvent casser le pont, comme l'aqueduc (D-034). César le rebâtit en 6 mois, aux frais du fautif (deniers et lauriers), et sa colère monte | un vrai coup de guerre, spectaculaire | coupe toute la province en deux, neutres compris, pendant 6 mois ; il faut savoir reconstruire un pont pour navires |

**Recommandation : l'option 1** (*à valider*, D-068).
- **Terre de César** : 15 cases autour de chaque entrée du pont (réglage de départ). Aucune zone ne s'y étend. On n'y
  bâtit rien : ni mission, ni porte, ni mur, ni tour, ni fort. Seules les routes y sont permises. Le pont ne peut
  donc jamais être fermé par une construction.
- **En paix**, tout le monde passe. Rien ne ferme le pont.
- **En guerre**, les deux camps peuvent s'y battre. Les soldats n'attaquent que leurs ennemis (hostilité par paire de
  joueurs, M10.1) : une guerre entre J1 et J3 ne gêne ni les caravanes ni le missionnaire de J2 et de J4.
- **Ce qu'en pense César** :
  - des légions d'un belligérant sur la terre de César comptent dans la **puissance** (§7.2) comme si elles étaient
    dans le territoire adverse. Tenir le pont 6 mois avec 2 légions coûte autant qu'une invasion de 2 légions :
    8 de colère par mois ;
  - une caravane interceptée sur le pont compte comme ailleurs (+2) ;
  - à la paix (signée ou imposée par l'ultimatum, §7.3), les soldats restés sur sa terre rentrent d'office à leur
    fort.
- **Conséquences voulues** :
  - **le commerce entre rives est le plus exposé** : une guerre entre rives coupe d'abord ces caravanes. C'est la
    frappe commerciale naturelle du prédateur (§8), et une raison de commercer aussi avec son voisin de rive ;
  - **l'eau** : couper l'aqueduc d'un joueur de l'autre rive oblige à passer le pont. Le pont protège donc l'eau du
    joueur des terres contre l'autre rive, pas contre son voisin de rive ;
  - **s'étendre sur l'autre rive** : une mission au-delà du pont est une enclave. En guerre, l'ennemi peut la
    couper de sa cité ;
  - **franchir le pont en force** est difficile : 25 cases à découvert, en colonne, face à des javeliniers qui
    tirent à 10 cases (D-058). La guerre entre rives est une guerre de siège. C'est voulu : elle doit rester courte
    et rare (principe 4, §2).
- **Lien avec la carte à 4 refaite** (D-062, T4.14) : deux joueurs des terres vivent de l'aqueduc de César. Comme un
  aqueduc ne traverse pas la mer :
  - s'ils sont sur la même rive, un seul réservoir suffit, et l'autre rive ne peut l'atteindre que par le pont ;
  - s'ils sont sur deux rives, il faut un réservoir de César sur chaque rive.
  Question pour Alexandre ci-dessous.

**Questions pour Alexandre** :
1. Option 1, 2 ou 3 ?
2. La terre de César (15 cases autour du pont, rien n'y est bâti) vous va-t-elle ?
3. Faut-il que tenir le pont coûte de la colère, ou seulement les combats qui s'y déroulent ?
4. Carte à 4 refaite : les deux joueurs des terres sur la même rive, ou un sur chaque rive ?

## 8. Les deux chemins vers la victoire

**Le bâtisseur courtisan.** Il développe les cinq notes et sert César : deux fêtes par an, un don annuel, des troupes
à chaque campagne.
- Résultat : des notes autour de 65 en fin de partie, et environ 40 lauriers de César par an. C'est le chemin
  « normal ».
- Sa faiblesse : ses légions partent en campagne, et ses caravanes dépendent de ses voisins.

**Le prédateur ciblé.** Développement moyen, mais il frappe là où ça fait mal, au bon moment :
- couper le marbre du rival quand celui-ci construit ses temples ;
- accaparer le vin quand César en demande, et intercepter les caravanes de vin du rival : César ne s'en fâche pas
  (§6.4) ;
- brûler un entrepôt clé en trois mois de guerre.
- Résultat : il paie 15 lauriers par guerre sans motif, et de la colère. Le rival perd des mois de commerce et de
  culture, donc des lauriers mensuels cumulés. Une riposte ou un mandat ne coûtent même pas les 15 lauriers.
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
| Acheter des lauriers avec l'épargne | un don compté par an (10 lauriers au plus), salaire limité par le rang |
| Fêtes à la chaîne | une fête comptée tous les 6 mois |
| Fêtes très bon marché en début de partie (leur coût suit la population) | plafond par semestre ; si la télémétrie montre qu'elles pèsent trop au début, les points suivront la population |
| Le dernier déclenche exprès la colère pour faire perdre tout le monde | le fautif perd 25 % de ses lauriers : la manœuvre le fait plonger plus que les autres |
| Le premier déclenche la colère pour figer son avance | même règle : la disgrâce touche tout le monde, la perte surtout le fautif |
| Déclarer, frapper, signer la paix, recommencer | redéclaration dans les 12 mois : sans motif, colère double ; les dégâts restent dans la jauge |
| Envoyer une poignée de soldats à la campagne pour toucher la récompense | récompense selon la part de la force (soldats × entraînement × moral), avec un minimum |
| Gonfler la population en tentes | la Grandeur sature, l'Habitat chute, la prospérité aussi |
| Thésauriser sans construire | le trésor ne compte que par le bénéfice dans la prospérité d'origine |
| Construire écoles et théâtres loin des maisons pour la note de culture | règle d'origine (des places, pas la desserte) : on la garde, car ces bâtiments coûtent des ouvriers et de l'entretien ; à surveiller en télémétrie |
| Se liguer à deux contre un (3 ou 4 joueurs) | permis (c'est de la diplomatie), mais chaque agresseur porte sa part de la colère |
| Attaquer un joueur dont les légions sont chez César | −40 lauriers, colère pleine : la pire faute du jeu |
| Frapper avant que le défenseur ait rappelé ses troupes | guerre brutale : −15 lauriers de plus, colère ×1,5 |
| Fermer le pont de César par une porte ou un fort, pour couper l'autre rive | terre de César : rien ne s'y bâtit ; le tenir en guerre coûte de la colère comme une invasion (§7.6, à valider) |
| Envoyer un seul chargement à une demande pour toucher sa part | part au prorata : un chargement sur vingt rapporte un vingtième |
| Une cité file seule vers le score | classement public : les autres la voient venir, peuvent la frapper ou se liguer |

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
- **Trajectoires** : faire avancer ces cités plusieurs années (autopilot) et tracer les notes et les lauriers. Le gain
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
  - « dons et fêtes au maximum : moins de 25 % des lauriers d'une bonne cité sur 10 ans » ;
  - « rapport des pertes victime / agresseur entre 3 et 5 ».

### 10.4 Télémétrie des vraies parties
- Chaque mois, la sauvegarde garde le détail des gains de chaque joueur : chaque note, lauriers par source,
  colère et parts.
- `simtool laurels PARTIE.mpsav` le sort en tableau. Après chaque partie d'Alexandre, on lit la sauvegarde au lieu
  de deviner, et on ajuste.
- Le journal macOS (`julius-log.txt`) note aussi les événements de César (avertissements, expéditions).

### 10.5 Critères chiffrés d'une partie équilibrée
- Aucune note ne fait plus de 30 % des lauriers du vainqueur.
- Les lauriers de César font de 20 à 40 % du total du vainqueur, dont moins de la moitié en dons et fêtes.
- Le joueur en tête quand le premier atteint la moitié du score gagne dans 60 à 75 % des cas : l'avance compte,
  mais le retour reste possible.
- Une partie à 2 joueurs déclenche une expédition punitive dans moins d'une partie sur trois, quand les deux
  jouent « normalement ».

## 11. Interface

- **Conseiller impérial** (fenêtre d'origine, contenu multijoueur) :
  - lauriers (de la cité, de César, total) et rang ;
  - les cinq notes, avec barres et conseils à la manière du conseiller des notes ;
  - salaire et épargne, dons, et la date où la prochaine fête et le prochain don compteront ;
  - campagne en cours ; demande en cours, avec ce que chacun a envoyé.
- **Bandeau multijoueur** (en bas à gauche) : les lauriers remplacent le score, et une **jauge de colère** colorée
  est toujours visible.
- **Lettres de César** : messages en plein écran, comme l'alerte de prix, pour l'avertissement, l'ultimatum,
  l'expédition, les campagnes et le verdict.
- **Fenêtre de la province** :
  - lauriers de chacun (classement public, option du salon) ;
  - guerres en cours et leur motif ;
  - part de chacun dans la colère.
- **Salon** :
  - mode « Jugement de César » et son score ;
  - options : demandes de César (active), classement public (actif), seconde expédition fatale (inactive).

## 12. Découpage et pièges techniques

Les jalons de [ROADMAP.md](ROADMAP.md) suivent cet ordre :
- **M9, César juge** : jouable en paix et **seul**, livré à Alexandre avant la guerre ;
- **M10, la guerre sous l'œil de César** : l'ancien M9, auquel s'ajoute l'interception des caravanes M8.7 ;
- **M11, équilibrage et finitions** : l'ancien M10.

Pièges relevés dans le code d'origine (code-map/07), à traiter dans ces jalons :
- **Faveur** : on ne rallume pas sa mise à jour d'origine. En partie libre, elle la remettrait à 50 chaque mois
  (`scenario_is_open_play`). Le don en multijoueur passe par une commande de `mp/caesar` qui garde le coût d'origine
  (`city/emperor.c`) et donne des lauriers au lieu de faveur.
- **Dette** : la pénalité de dette tourne encore en multijoueur (`city/emperor.c` `update_debt_state`), mais
  seulement sur la faveur figée : sans effet. Si une dette doit coûter des lauriers, ce sera une règle explicite.
- **Batailles lointaines et demandes** : elles viennent des données du scénario, vides sur les cartes préparées.
  `mp/caesar` les crée, à partir de la graine de la partie (déterminisme).
- **Force romaine sur 8 bits** : `distant_battle.roman_strength` repart de 0 au-delà de 255. Le cumul de plusieurs
  joueurs se compte en entier.
- **Paix** : la pénalité « bâtiment détruit par un ennemi » va à la cité dont c'est le tour de simulation. En
  guerre entre joueurs, il faudra l'imputer à la victime (M10).

## 13. Points validés par Alexandre

Tout est réglé (2026-10-06, D-053). D'abord : pas de multiplicateur de faveur, seulement des lauriers qui
s'additionnent (« je trouve le multiplicateur de faveur trop compliqué, je préfèrerais juste les points »). Puis :

| # | Question | Réponse |
|---|----------|---------|
| 1 | Lauriers cumulés mois après mois ? | oui |
| 2 | Valeurs des lauriers (§4.1 et §6) ? | celles du plan, ajustées en M11 |
| 3 | Préavis d'un mois avant une guerre ? | deux formes : guerre **honorable** (préavis de 3 mois) ou **brutale** (immédiate, César mécontent), §7.1 |
| 4 | Seconde expédition : fin sans vainqueur ? | option du salon, désactivée par défaut |
| 5 | Demandes de biens de César ? | oui, pour **toute la province** : chacun participe, lauriers selon ce qu'il donne, frappes sur la ressource permises (§6.4) |
| 6 | Classement public des lauriers ? | oui |
| 7 | Durée de 20 ans et consulat anticipé ? | non : **victoire au score**, la première cité à tant de lauriers (§4.3) |
