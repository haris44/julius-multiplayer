# Maquettes : statistiques et décisions

> Demande d'Alexandre (T4.6, 2026-10-06) : « trouver une manière propre de faire évoluer ces vues. On voudra des
> statistiques et un commerce plus fin, sans surcharger l'écran. » Il veut d'abord des maquettes, à regarder
> ensemble, **avant tout code**. Ce document est donc une proposition : rien n'est décidé, tout est **à valider**.
> Liens : [CESAR.md](CESAR.md) §11 (interface de César), D-051 (page du commerce), T4.1 (César dans l'évaluation),
> T4.2 (dons), T4.3 et T4.12 (prix et portorium), T4.5 (stock minimum et maximum), M9.9 (télémétrie mensuelle).

## 0. En bref

- Trois façons de faire, dessinées à la taille réelle de l'écran (§4 à §6) :
  - **A. Chaque chose à sa place** : des onglets dans les conseillers existants ;
  - **B. Les Annales** : une seule page de courbes, ouverte depuis les conseillers ; **recommandée** ;
  - **C. La vue d'ensemble** : un tableau de vignettes, chacune ouvre sa courbe.
- Pour les **décisions** du commerce, une **fiche par ressource** (§7), la même pour les trois options : la page du
  commerce reste un résumé, le détail et les réglages fins (prix par joueur, stock minimum et maximum de T4.5) vont
  dans la fiche.
- Statistiques proposées en premier (§3) : **lauriers et notes dans le temps**, puis **échanges par partenaire**,
  puis **prix payés**.
- Il faut d'abord **enregistrer** ces chiffres chaque mois, dans la sauvegarde (§8). C'est le même travail que la
  télémétrie de M9.9 : on le fait une fois, pour les courbes et pour `simtool`.

## 1. Les contraintes de l'écran

- **640 × 480, toujours.** Les conseillers sont dessinés dans un cadre fixe de 640 × 480, centré. Une fenêtre plus
  grande montre plus de ville autour, pas un conseiller plus grand. Plus de pixels ne donnent donc pas plus de
  place : tout doit tenir dans 640 × 432, au-dessus de la barre des icônes.
- **La barre des 13 icônes est pleine** : 12 conseillers et le retour à la ville. Une nouvelle vue ne peut pas
  avoir sa propre icône. Elle devient une **sous-page** d'un conseiller, ou remplace le contenu d'un conseiller
  en multijoueur (comme le conseiller impérial aujourd'hui).
- **Les pièces du jeu d'origine** (on n'invente pas de nouveaux dessins, I4) :
  - le panneau de bois (`outer_panel`), le panneau sombre (`inner_panel`), les boutons à bordure, les flèches
    − et + ;
  - les onglets de la page du commerce (un par joueur, D-051) ;
  - les courbes du conseiller de la population : une grande (400 × 200) et deux vignettes (104 × 55) qu'un clic
    échange avec la grande ;
  - les piliers de marbre de l'évaluation ; les barres colorées du conseiller impérial multijoueur ;
  - la petite fenêtre des réglages d'une ressource (seuil d'export, mise en sommeil) ; les infobulles.
- **Les polices** : environ 7 pixels par lettre, 16 à 18 pixels par ligne. Un panneau sombre de la hauteur de la
  page tient 18 à 20 lignes, soit les 16 ressources et un total.
- **L'interface ne calcule rien.** Les conseillers d'origine écrivent dans l'état de la simulation (code-map/01
  §3.5). Les nouvelles vues ne font que **lire** des chiffres enregistrés par la simulation, au bilan du mois.
- **Une décision = une commande réseau** (I3), comme les boutons de prix d'aujourd'hui.

**Convention des maquettes** : un caractère vaut 8 × 16 pixels, une maquette fait 80 × 30 caractères, soit
640 × 480. Les proportions sont donc les vraies.

| Signe | Pièce du jeu |
|-------|--------------|
| `+---+` et `\|` | panneau de bois (cadre du conseiller) |
| `:::` et `:` | panneau sombre (texte blanc) |
| `[ texte ]` | bouton à bordure ; `[-]` `[+]` : flèches |
| `(Co)` | icône du conseiller (ici : commerce) |
| `(o)` | icône de la ressource |
| `*` `o` `+` `.` `=` `#` | une courbe ou des barres, une par joueur ou par série |
| `[Tr] … [<-]` | la barre des 13 icônes, en bas |

## 2. Ce qui existe aujourd'hui

| Écran | Ce qu'il montre | En multijoueur |
|-------|-----------------|----------------|
| Évaluation (piliers) | culture, prospérité, paix, faveur | la faveur est figée (D-026) ; T4.1 veut y mettre César |
| Impérial | lauriers, rang, les cinq notes en barres, classement (D-057) | une photo du mois : pas d'évolution |
| Commerce | une ligne par ressource : stock, empire, joueur choisi (D-051) | page pleine : 16 lignes, 7 colonnes |
| Population | trois courbes : historique, recensement, société | seule vraie courbe dans le temps |
| Finances | recettes et dépenses, cette année et l'an passé | — |
| Chef | 16 lignes d'état (travail, nourriture, santé…) | — |

**Chiffres déjà gardés** dans la sauvegarde :
- la population de chaque mois (2 400 mois, `city_data.population.monthly`) ;
- les finances de l'année en cours et de l'an passé ;
- les chargements échangés avec chaque ville de l'empire dans l'année (`trade_route_traded`, remis à zéro chaque
  année) ;
- les lauriers de chaque cité, totaux par source (`mp/caesar`).

**Rien n'est gardé** pour l'évolution des notes et des lauriers, les échanges entre joueurs (on annonce chaque
livraison, sans la compter), les prix demandés dans le temps, le trésor mois par mois.

## 3. Les statistiques à montrer d'abord

Classées par intérêt pour la partie (le jugement de César et le commerce), pas par facilité.

| # | Statistique | La question du joueur | À enregistrer chaque mois, par cité | Taille |
|---|-------------|-----------------------|--------------------------------------|--------|
| 1 | **Lauriers** : total, de la cité, de César | « qui approche du score, et à quel rythme ? » | 3 nombres | 12 octets |
| 2 | **Les cinq notes** | « quelle note me fait perdre des lauriers ? » | 5 nombres de 0 à 100 | 5 octets |
| 3 | **Échanges par partenaire** (empire, chaque joueur) | « qui m'achète quoi, et combien ça rapporte ? » | par ressource et par partenaire : chargements vendus et achetés ; par partenaire : deniers reçus et payés | 300 octets |
| 4 | **Prix** : payé en moyenne, demandé par chaque vendeur, portorium | « qui me fait payer cher, depuis quand ? » | par ressource : le prix que chaque autre joueur me demande, et le taux du portorium (T4.3) | 100 octets |
| 5 | **Trésor, recettes, dépenses** | « est-ce que je m'enrichis ? » | 3 nombres | 12 octets |
| 6 | **Population** comparée aux autres | « suis-je le plus grand ? » | rien : déjà gardée | — |
| 7 | Plus tard (M10) : colère de César, part de chacun, légions, pertes | « qui fâche César ? » | 4 à 6 nombres, dont la colère, commune | 20 octets |
| 8 | Plus tard : production par ressource | « mes ateliers suivent-ils ? » | de nouveaux compteurs dans les industries | 32 octets |

- Le **prix payé en moyenne** se déduit de la ligne 3 (deniers payés divisés par chargements) : rien de plus à
  garder.
- **Taille** : environ 450 octets par cité et par mois. Une partie de 20 ans à 4 joueurs : environ 430 Ko avant
  compression, surtout des zéros. Une grille de la carte à 4 pèse déjà 67 Ko. Pour réduire, le détail par
  ressource peut ne garder que les 24 derniers mois, puis des totaux par année.
- **Ce que l'on voit des autres** (*à valider*, question 3) :
  - les lauriers de tous, toujours (classement public, D-053) ;
  - sa propre cité en entier ;
  - des autres, le reste seulement sans brouillard de guerre, comme les scores aujourd'hui (D-038) ;
  - les échanges avec un joueur, on les connaît des deux côtés : on les voit toujours.

## 4. Option A — Chaque chose à sa place

Chaque conseiller garde son icône et gagne **deux onglets** en haut à droite : « aujourd'hui » (la page actuelle)
et « dans le temps ». Pas de nouvelle fenêtre.

**A1. Commerce, onglet « Bilan 12 mois »** : tous les partenaires d'un coup, en chargements. Un clic sur un nom de
joueur revient à l'onglet des prix, sur ce joueur.

```
+------------------------------------------------------------------------------+
| (Co) Commerce                            [ Prix et achats ][ Bilan 12 mois ] |
| Chargements vendus (+) et achetés (-) sur les 12 derniers mois               |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
| : Ressource     Stock   Empire   Joueur 2   Joueur 3   Joueur 4       Net  : |
| : (o) Blé          12       .         .          .          .          0   : |
| : (o) Légumes       4       .        +8          .          .         +8   : |
| : (o) Fruits        0     -16         .          .          .        -16   : |
| : (o) Olives       20     +24         .          .        -40        -16   : |
| : (o) Vignes        0       .         .        -32          .        -32   : |
| : (o) Viande        8       .         .          .          .          0   : |
| : (o) Vin           0       .         .          .          .          0   : |
| : (o) Huile        16     +16         .          .          .        +16   : |
| : (o) Fer          24       .       +48        +16          .        +64   : |
| : (o) Bois          0     -24       -16          .          .        -40   : |
| : (o) Argile        0       .         .          .        -24        -24   : |
| : (o) Marbre       32       .       +40        +24        +16        +80   : |
| : (o) Armes         8       .         .          .        +16        +16   : |
| : (o) Meubles       0       .         .          .          .          0   : |
| : (o) Poterie       4       .         .          .          .          0   : |
| :                                                                          : |
| : Deniers reçus      +3 200  +16 560     +7 520     +5 120                 : |
| : Deniers payés      -4 100   -1 800     -3 600     -6 900                 : |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
|   Un clic sur une ressource : sa fiche. Un clic sur un joueur : ses prix.    |
|          [   Carte de l'empire   ]            [  Prix de l'empire   ]        |
+------------------------------------------------------------------------------+

  [Tr]  [Mi]  [Im]  [Év]  [Co]  [Po]  [Sa]  [Éd]  [Di]  [Re]  [Fi]  [Ch]  [<-]

```

**A2. Évaluation, onglet « Sur 3 ans »** : les cinq notes en courbes. Les cinq cases en dessous reprennent la place
des piliers ; un clic sur une case met sa courbe en avant et affiche son conseil en bas.

```
+------------------------------------------------------------------------------+
| (Év) Évaluation de la cité                 [ Aujourd'hui ][  Sur 3 ans  ]    |
|                                                                              |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
| : 100 |                                                    Prospérité ──   : |
| :     |                                         ......     Commerce   ..   : |
| :     |                              ...........      ..   Habitat    ++   : |
| :  50 |               ..........          ++++++++++++++   Culture    **   : |
| :     |      +++++++++     ++++++++++++++++          ****  Grandeur   ==   : |
| :     |  ++++    ******************************************                : |
| :     | ========================================== ──────                  : |
| :   0 +---------------------------------------------------                 : |
| :      jan. 3              jan. 4              jan. 5                      : |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
|                                                                              |
|   +----------+   +----------+   +----------+   +----------+   +----------+   |
|   |Prospérité|   | Commerce |   | Habitat  |   | Culture  |   | Grandeur |   |
|   |  52  (+3)|   |  61  (-8)|   |  44  (+1)|   |  70  (+2)|   |  38  (+2)|   |
|   +----------+   +----------+   +----------+   +----------+   +----------+   |
|                                                                              |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
| : Commerce en baisse ce mois : le joueur 3 n'achète plus votre marbre.     : |
| : Lauriers de la cité ce mois : 5,4 (dont commerce 1,5).                   : |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
|                                                                              |
|                                                                              |
+------------------------------------------------------------------------------+

  [Tr]  [Mi]  [Im]  [Év]  [Co]  [Po]  [Sa]  [Éd]  [Di]  [Re]  [Fi]  [Ch]  [<-]

```

- **Navigation** : rien de nouveau à apprendre, l'information est là où on la cherche déjà.
- **Pour** : peu de code par écran ; chaque conseiller garde son sens.
- **Contre** :
  - chaque conseiller double ses modes : 12 pages à tenir au lieu d'une ;
  - les courbes ne se comparent pas entre elles (les lauriers dans l'impérial, le commerce ailleurs) ;
  - la page du commerce, déjà pleine, devient deux pages pleines ;
  - chaque nouvelle statistique demande de trouver « son » conseiller.

## 5. Option B — Les Annales (recommandée)

Une seule page de courbes, **les Annales de la cité**, ouverte depuis plusieurs conseillers par un bouton
« Annales » (un bouton de 200 × 23, comme « Carte de l'empire »). Elle reprend la grande courbe du conseiller de la
population, avec :
- en haut, **le sujet** : Lauriers, Notes, Échanges, Prix, Cité ;
- à droite, **les séries** du sujet : un bouton par série, comme les vignettes de la population ;
- en haut à droite, **les joueurs** à comparer (chacun dans sa couleur), seulement pour les séries publiques ;
- en bas à droite, **la période** : 2 ans, 5 ans, toute la partie ;
- en bas, **un tableau des chiffres du mois**, pour lire sans deviner sur la courbe.

**B1. Sujet « Lauriers »**, ouvert depuis le conseiller impérial, deux joueurs comparés :

```
+------------------------------------------------------------------------------+
| (An) Annales de Lindum                         [Moi][ J2 ][ J3 ][ J4 ]       |
|  [Lauriers][ Notes ][Échanges][ Prix ][ Cité ]                               |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: +-------------+ |
| : 600 |                                              *     : |Lauriers     | |
| :     |                                         *****  o   : |  total    * | |
| :     |                                   ******    ooo    : +-------------+ |
| :     |                             ******      oooo       : |  de la cité | |
| : 300 |                       ******        ooooo          : +-------------+ |
| :     |               ********      oooooooo               : |  de César   | |
| :     |        *******   ooooooooooo                       : +-------------+ |
| :     |  ******  oooooooo                                  : |Rang         | |
| :   0 +--------------------------------------------------- : +-------------+ |
| :       an 1          an 3           an 5          an 7    :                 |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: Période :       |
|   * Moi (Lindum) 512      o Joueur 3 (Tarraco) 431          [2 ans][5][Tout] |
|                                                                              |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
| : Ce mois         Moi     J2      J3      J4                               : |
| : Lauriers        512     388     431     205                              : |
| : Gagnés en 12 m  +86     +71     +90     +40                              : |
| : Score : 1 000.  Au rythme actuel, le joueur 3 l'atteint en l'an 13.      : |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
|                                                                              |
|                                                                              |
|                                                    [ Retour au conseiller ]  |
+------------------------------------------------------------------------------+

  [Tr]  [Mi]  [Im]  [Év]  [Co]  [Po]  [Sa]  [Éd]  [Di]  [Re]  [Fi]  [Ch]  [<-]

```

**B2. Sujet « Échanges »**, ouvert depuis le commerce, partenaire « Joueur 3 » : vendu au-dessus de la ligne,
acheté en dessous, et le détail des 12 derniers mois.

```
+------------------------------------------------------------------------------+
| (An) Annales de Lindum                                                       |
|  [Lauriers][ Notes ][Échanges][ Prix ][ Cité ]                               |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: +-------------+ |
| :  80 |                                                    : |Tous         | |
| :     |                      ##                            : |partenaires  | |
| :     |          ##          ##          ##  ##            : +-------------+ |
| :  40 |    ##    ##  ##  ##  ##  ##      ##  ##  ##        : |Empire       | |
| :     |    ##    ##  ##  ##  ##  ##  ##  ##  ##  ##  ##    : +-------------+ |
| :   0 +--------------------------------------------------- : |Joueur 2     | |
| :     |    ==    ==  ==      ==      ==  ==      ==  ==    : +-------------+ |
| : -40 |    ==    ==          ==          ==                : |Joueur 3   * | |
| :       fév.  avr.  juin  août  oct.  déc.  fév.  avr.     : +-------------+ |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |Joueur 4     | |
|   ## vendu au joueur 3   == acheté au joueur 3   (chargem.)  +-------------+ |
|   Ressource : [ Toutes v ]                                  [2 ans][5][Tout] |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
| : Avec le joueur 3, 12 mois  Chargements    Deniers   Prix moyen           : |
| : Vendu : marbre                   +24      +4 800        200              : |
| : Vendu : fer                      +16      +2 720        170              : |
| : Acheté : vignes                  -32      -3 600        113              : |
| : Solde                                     +3 920                         : |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
|                                                                              |
|                                                    [ Retour au conseiller ]  |
|                                                                              |
+------------------------------------------------------------------------------+

  [Tr]  [Mi]  [Im]  [Év]  [Co]  [Po]  [Sa]  [Éd]  [Di]  [Re]  [Fi]  [Ch]  [<-]

```

- **Navigation** :
  - le bouton « Annales » s'ouvre **sur le sujet du conseiller d'où l'on vient** : Lauriers depuis l'impérial,
    Notes depuis l'évaluation, Échanges depuis le commerce, Cité (trésor, population) depuis les finances et la
    population ;
  - la barre des icônes reste active ; l'icône du conseiller d'origine reste allumée ;
  - « Retour au conseiller » ou le clic droit ramène à la page d'origine ;
  - une touche du clavier ouvre les Annales sur le dernier sujet vu.
- **Pour** :
  - **une seule pièce** à construire et à tester : une courbe, des séries, une période ; toute nouvelle statistique
    est une série de plus, pas un écran de plus ;
  - les conseillers d'origine ne changent pas, à un bouton près : moins de risque pour le classique (I2) ;
  - la comparaison entre joueurs se fait au même endroit pour tout ;
  - le tableau du bas donne les chiffres exacts que la courbe ne montre pas.
- **Contre** :
  - une page de plus, à un clic des conseillers ;
  - 50 colonnes de courbe pour 20 ans de partie : on lit une tendance, pas chaque mois (d'où le tableau et la
    période).

## 6. Option C — La vue d'ensemble

Le **conseiller en chef**, en multijoueur, ouvre sur une page de **six vignettes** choisies par le joueur. Chacune
montre une valeur, sa tendance et une petite courbe ; un clic l'ouvre en grand (la grande courbe de l'option B,
sans les séries). Le rapport d'origine du conseiller reste à un bouton.

**C1. Vue d'ensemble** :

```
+------------------------------------------------------------------------------+
| (Ch) Vue d'ensemble de Lindum                   [ Rapport du conseiller ]    |
|                                                                              |
|  +---------------------+  +---------------------+  +---------------------+   |
|  |Lauriers   512  +86/a|  |Commerce   61     -8 |  |Trésor  4 210  +900/a|   |
|  |               ..**  |  |  ..****             |  |        .....****    |   |
|  |        ..****       |  |**      ***          |  |  ******             |   |
|  |  ******             |  |           ********  |  |**                   |   |
|  +---------------------+  +---------------------+  +---------------------+   |
|                                                                              |
|  +---------------------+  +---------------------+  +---------------------+   |
|  |Population 6 840  +4%|  |Échanges +16 000 /a  |  |Marbre  stock 32     |   |
|  |            ...****  |  |  J2 ###### +14 760  |  |  J2 210  J3 200     |   |
|  |     ..*****         |  |  J3 ###     +3 920  |  |  J4 220             |   |
|  |*****                |  |  J4 =       -1 780  |  |  empire achète 100  |   |
|  +---------------------+  +---------------------+  +---------------------+   |
|                                                                              |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
| : (!) Le joueur 3 n'achète plus votre marbre depuis 2 mois.                : |
| : (!) Réservoir de l'ouest : 40 jours d'eau.                               : |
| : (i) Prochaine fête comptée par César : dans 3 mois.                      : |
| :::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: |
|                                                                              |
|  Un clic sur une vignette ouvre sa courbe en grand.  [ Choisir les vignettes]|
|                                                                              |
|                                                                              |
+------------------------------------------------------------------------------+

  [Tr]  [Mi]  [Im]  [Év]  [Co]  [Po]  [Sa]  [Éd]  [Di]  [Re]  [Fi]  [Ch]  [<-]

```

- **Navigation** : l'icône du conseiller en chef ; un clic sur une vignette ouvre sa courbe ; « Choisir les
  vignettes » change les six sujets.
- **Pour** : tout l'essentiel en un coup d'œil ; les alertes (prix, eau, fêtes) au même endroit.
- **Contre** :
  - des vignettes de 22 × 4 caractères : on y voit une pente, pas des chiffres ;
  - cache le rapport d'origine du chef, que les joueurs de Caesar III connaissent ;
  - il faut tout de même la grande courbe de l'option B derrière chaque vignette : C coûte B, et plus.

## 7. Les décisions : la fiche d'une ressource (pour les trois options)

Aujourd'hui, un clic sur le nom d'une ressource, dans la page du commerce, ouvre ses réglages d'origine (seuil
d'export, mise en sommeil). En multijoueur, cette petite fenêtre devient **la fiche de la ressource** : tout ce
qu'on décide pour elle, avec tous les partenaires en même temps. La page du commerce reste un résumé d'une ligne
par ressource.

**D1. Fiche du marbre** (le joueur des terres le vend à trois joueurs), par-dessus la page du commerce grisée :

```
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . .+-------------------------------------------------------------------+ . . .
. . .| (o) Marbre                     Stock : 32 chargements (2 entrep.) | . . .
. . .|                                                                   | . . .
. . .|  Vendre au-dessus de [-]  8 [+]    Acheter jusqu'à   [-] 40 [+]   | . . .
. . .|  (stock gardé pour soi)            (au-delà, plus d'achat)        | . . .
. . .| ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: | . . .
. . .| : Vendre à     Mon prix      Il achète   Vendu (12 mois)        : | . . .
. . .| : Empire       100 (200-100)   [oui]        0                   : | . . .
. . .| : Joueur 2   [-] 210 [+]        oui        40                   : | . . .
. . .| : Joueur 3   [-] 200 [+]        oui        24                   : | . . .
. . .| : Joueur 4   [-] 220 [+]        oui        16                   : | . . .
. . .| :                                                               : | . . .
. . .| : Acheter à    Prix   Douane  Payé    J'achète                  : | . . .
. . .| : Empire        200    +100    300    [ non ]                   : | . . .
. . .| ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::: | . . .
. . .|  12 mois : vendu 80 chargements, 16 720 Dn ; prix moyen 209       | . . .
. . .|                                                                   | . . .
. . .|  [ Mettre en réserve ]   [ Courbe dans les Annales ]    [  OK  ]  | . . .
. . .+-------------------------------------------------------------------+ . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
. . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .
```

- **En haut**, les deux bornes de T4.5 :
  - « vendre au-dessus de » : le seuil d'export d'origine, valable pour l'empire et pour les joueurs ;
  - « acheter jusqu'à » : on n'achète plus au-delà de ce stock.
  C'est une lecture provisoire de T4.5 (*à valider*, question 5).
- **Vendre à** : une ligne par acheteur, avec mon prix (flèches − et +, une commande par clic, comme aujourd'hui) et
  ce qu'il m'a acheté en 12 mois.
- **Acheter à** : une ligne par vendeur, avec son prix, la douane (le portorium de l'empire, D-060) et le prix payé.
  « J'achète » (oui ou non) par vendeur ; l'empire vend toujours (T4.3).
- **En bas**, le bilan de 12 mois et un lien vers la courbe de la ressource dans les Annales.
- Ce que la fiche **ne fait pas** : choisir entre plusieurs vendeurs à la fois. La règle reste celle du jeu : le
  moins cher parmi ceux à qui « j'achète » (D-048, revue par T4.3).
- Les **décisions envers César** (dons, fêtes, salaire : T4.2, M9.3, M9.4) restent dans le conseiller impérial,
  avec leurs lauriers affichés à côté du bouton (CESAR §6). Leur effet dans le temps se lit dans les Annales,
  sujet Lauriers.

## 8. Les données à enregistrer

- **Un nouveau module de la simulation** (nom provisoire `mp/annals`), une pièce de plus du `.mpsav`, comptée dans
  la somme de contrôle. Une partie classique n'en a pas (I2).
- **Quand** : au bilan du mois, juste après les lauriers (`mp_caesar_update_city_month`), pour chaque cité. Jamais
  depuis l'interface (I3).
- **Ce qu'il garde** : les lignes 1 à 5 du §3, une entrée par mois et par cité, depuis le début de la partie.
- **Les compteurs à ajouter** pendant le mois, remis à zéro au bilan :
  - à chaque livraison d'une caravane entre joueurs (`mp/trade`) : chargements et deniers, des deux côtés ;
  - à chaque vente ou achat avec l'empire (caravanes et navires de l'empire) : chargements et deniers, partenaire
    « empire » ;
  - le prix demandé par chaque vendeur et le portorium : une photo au bilan du mois, rien à compter.
- **Les lecteurs** : les Annales (option B), la fiche (§7), `simtool annals PARTIE.mpsav` en tableau (M9.9 : lire
  les parties d'Alexandre au lieu de deviner, CESAR §10.4).
- **Format** : version de sauvegarde multijoueur relevée ; une partie d'avant reprend avec des annales vides.
- **Tests prévus** : un ctest par compteur (une livraison connue donne les bons chargements et deniers chez les
  deux joueurs) ; partie sauvegardée et reprise, annales identiques ; partie classique sans annales.

## 9. Recommandation

**L'option B, les Annales, avec la fiche de ressource du §7.**
- Une seule pièce à construire, qui sert toutes les statistiques à venir, sans toucher aux pages d'origine.
- La fiche de ressource donne le « commerce plus fin » sans charger la page du commerce.
- La vue d'ensemble (C) pourra venir plus tard comme page d'accueil des Annales, si le besoin s'en fait sentir.

**Ordre de travail proposé**, chaque étape jouable seule :
1. enregistrer les chiffres (§8) et `simtool annals` ;
2. les Annales, sujets Lauriers et Notes ;
3. sujets Échanges et Prix ;
4. la fiche de ressource, avec les bornes de T4.5 ;
5. sujet Cité (trésor, population comparée), puis la guerre (M10).

## 10. Questions pour Alexandre

1. **Quelle option** : A, B (recommandée) ou C ?
2. **Quelles statistiques en premier** ? Proposé : lauriers et notes, puis échanges par partenaire, puis prix.
3. **Que voit-on des autres joueurs** ? Proposé : les lauriers toujours ; les notes, la population et le trésor des
   autres seulement sans brouillard de guerre ; les échanges avec un joueur, toujours.
4. **Combien de temps garder** ? Proposé : toute la partie, mois par mois ; ou 24 mois en détail puis par année.
5. **Les bornes de T4.5** : « vendre au-dessus de » et « acheter jusqu'à », les mêmes pour l'empire et pour les
   joueurs ? Ou une paire de bornes par partenaire ?
6. **Les échanges en chargements ou en deniers** sur les courbes ? Proposé : chargements par ressource, deniers
   pour les totaux.
7. **Le nom** : « Annales » vous va-t-il ? (« Registres », « Tabularium »…)
