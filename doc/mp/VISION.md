# Vision — Caesar III multijoueur (fork de Julius)

> Document de référence des **exigences**. Il change rarement, et seulement sur décision d'Alexandre.
> Le *comment* est dans [DESIGN.md](DESIGN.md), le *quand* dans [ROADMAP.md](ROADMAP.md).

## Demande d'origine (Alexandre, 2026-10-04)

> Le but serait de faire une grosse évolution du jeu pour avoir une carte multijoueur (multi local
> uniquement, chacun sur son pc). A la manière d'un AoE 2, et jusqu'à 4 joueurs. On atterrirait sur
> le terrain à un endroit différent chacun, et nous développerions chacun notre cité. Les mécanismes
> de commerce et de guerre devraient être adaptés également. Les limites de ressources devraient
> être rapportées au joueur et plus à la carte, et les tailles de cartes réadaptées pour être plus
> grandes et plus larges. L'avantage est que les PC / Mac que nous avons maintenant sont beaucoup
> plus puissants qu'à l'époque. Concernant tout le gameplay "intérieur" à une ville hors
> multijoueur, il doit rester exactement le même que l'original.
> On supprimera complètement les interventions de César, ainsi que la campagne : on se focus
> uniquement sur le jeu libre, en multijoueur.
> L'objectif est de faire un setup full vibe-coding, [...] en autonomie complète [...]. Il faudra
> travailler de manière structurée.

## Exigences

| ID | Exigence |
|----|----------|
| E1 | Multijoueur **réseau local uniquement** (LAN), chacun sur sa machine, **2 à 4 joueurs**. |
| E2 | Une **carte partagée** ; chaque joueur démarre à un **endroit différent** et développe **sa propre cité**. |
| E3 | **Commerce** adapté au multijoueur. |
| E4 | **Guerre** adaptée au multijoueur. |
| E5 | Les **limites** sont **par joueur** et non plus par carte. |
| E6 | **Cartes plus grandes** (largeur et hauteur), en exploitant la puissance des machines actuelles. |
| E7 | Le **gameplay intérieur d'une cité reste exactement celui de l'original**. |
| E8 | **Suppression complète des interventions de César**. |
| E9 | **Suppression de la campagne** : uniquement du **jeu libre, en multijoueur**. |
| E10 | Développement **autonome et structuré** par Claude, avec des **tests exécutables par Claude** (le jeu doit pouvoir être testé sans humain). |

## Interprétations retenues (hypothèses à confirmer par Alexandre)

Claude a tranché ces points pour pouvoir avancer. Chacun peut être remis en cause : il suffit de le dire.

| ID | Hypothèse | Conséquence si elle est fausse |
|----|-----------|--------------------------------|
| H1 | « Limites de ressources » désigne les **limites techniques du moteur** : nombre max de bâtiments, de figures, de légions, etc. Ce ne sont pas les ressources du jeu (blé, argile…), qui dépendent toujours du terrain de la carte. | Revoir E5 dans DESIGN. |
| H2 | « Interventions de César » couvre : demandes impériales, faveur et ses conséquences (légions de César, destitution), rangs et promotions, cadeaux à César, messages et changement d'empereur. Le **tribut annuel** à Rome et le **salaire du gouverneur** disparaissent aussi (voir DESIGN). | Réactiver la mécanique concernée. |
| H3 | Les notes **culture, prospérité et paix** restent (indicateurs, et base possible d'un score). La note de **faveur** disparaît. | — |
| H4 | « Jeu libre » : pas d'objectifs imposés ; on choisit une carte et des réglages de partie dans un salon (lobby) avant de lancer. | — |
| H5 | La condition de victoire n'est pas précisée. Par défaut, le salon proposera **partie sans fin**, **conquête** et **score à durée limitée** (détails dans DESIGN). | Changer les modes proposés. |
| H6 | Les menaces non-joueurs (invasions barbares, indigènes, loups) ne sont pas mentionnées : elles restent possibles, **désactivables dans le salon**. | — |
| H7 | Les dieux et les événements internes (séismes, révolte de gladiateurs, épidémies…) font partie du gameplay intérieur : ils sont **conservés**, et chaque cité les vit séparément. | — |
| H8 | Les villes de l'empire (non-joueurs) restent des partenaires commerciaux ; s'y ajoute le **commerce entre joueurs**. | — |
| H9 | LAN = un joueur **héberge**, les autres rejoignent par découverte automatique ou adresse IP. Pas d'Internet, pas de serveur dédié. | — |
| H10 | Parties **mixtes Mac / PC** : la simulation doit être déterministe sur toutes les plateformes. | — |
| H11 | Chaque joueur a un **territoire** autour de son point d'arrivée et ne construit que dessus. Une bande neutre sépare les territoires, ce qui empêche les cités de se « brancher » l'une sur l'autre (DECISIONS D-006). | Construction libre partout, comme dans AoE2 : beaucoup plus de travail d'isolement. |
| H12 | En multijoueur, le temps ne s'arrête pas quand un joueur ouvre une fenêtre, et l'annulation n'existe plus (D-010). | — |

## Hors périmètre (pour l'instant)

- Jeu via Internet, matchmaking, serveurs dédiés.
- Adversaires gérés par l'ordinateur (IA).
- Compatibilité des sauvegardes multijoueur avec Caesar III ou Julius d'origine.
- Campagne et scénarios à objectifs. L'éditeur n'est adapté que pour créer des cartes multijoueur.

## Critères de réussite du projet

1. Une partie à 4 joueurs en LAN, Mac et PC mélangés, sur une grande carte, tient au moins 2 heures sans désynchronisation ni plantage.
2. Les tests de parité avec le jeu original (`ctest`, harnais *autopilot*) restent verts à chaque commit.
3. Chaque cité se joue comme dans Caesar III. Seules différences : l'absence de César et la présence des voisins (commerce, guerre).
4. Le tout est vérifiable par des tests automatisés exécutés par Claude, sans intervention humaine.
