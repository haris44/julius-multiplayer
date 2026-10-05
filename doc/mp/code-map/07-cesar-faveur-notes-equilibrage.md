# 07 — César, faveur, notes et indicateurs : faits pour l'équilibrage

Cartographie en lecture seule (branche `multiplayer`). Complète `04` §2.7 et §3 (liste des interventions de César)
sans les répéter : ici, les **formules et les nombres**. Références `chemin:ligne` sous `src/`. Ce qui est marqué
**[LC]** vient de la lecture du code seule, sans vérification en jeu.

Unités : 50 ticks par jour, 16 jours par mois, 12 mois par an (`game/time.c:5-7`). Donc 192 jours = 1 an.

## 0. Quand tout cela tourne

| Fréquence | Appel |
|---|---|
| tick 1, chaque jour | `city_gods_calculate_moods(1)` (`game/tick.c:141`) |
| tick 4, chaque jour | `city_emperor_update` : dette, puis colère de César (`tick.c:148`) |
| tick 33, chaque jour | `city_culture_update_coverage` (`tick.c:189`) |
| tick 45, chaque jour | `figure_generate_criminals` (`tick.c:199`) |
| tick 49, chaque jour | `city_culture_calculate`, puis coût des fêtes (`tick.c:202`, `city/culture.c:187-188`) |
| jours 0 et 8 | `city_sentiment_update` (`tick.c:130-132`) |
| chaque mois | finances (`tick.c:88`), batailles lointaines (`:91-93`), demandes (`:95-97`), moral au repos (`:101`), `city_ratings_update(0)` (`:114`), fêtes (`:118`) |
| chaque année | `city_finance_handle_year_change` (`tick.c:76`), **puis** `city_ratings_update(1)` (`:79`) |

Le mois du changement d'année appelle `city_ratings_update(1)` **à la place de** `(0)` (`tick.c:111-115`). Le code
« mensuel » de la faveur tourne donc exactement une fois par mois.

Hors victoire (`city/victory.c:44-67`) et score multijoueur (`mp/endgame.c:32-35`), **aucune** des quatre notes n'a
d'effet sur la simulation. Seule exception : les seuils de colère de César lisent la faveur (`city/emperor.c:94-124`).

---

## 1. Les quatre notes

### 1.1 Culture — recalculée chaque mois (`city/ratings.c:307-397`, appelée en `:601`)

Points selon le taux de couverture de chaque service :

| Couverture | ≥ 100 | > 85 | > 70 | > 50 | > 30 | sinon | Lignes |
|---|---|---|---|---|---|---|---|
| Théâtre | 25 | 18 | 12 | 8 | 3 | 0 | `ratings.c:315-329` |
| Religion | 30 | 22 | 14 | 9 | 3 | 0 | `:331-345` |
| École | 15 | 10 | 6 | 4 | 1 | 0 | `:347-361` |
| Académie | 10 | 7 | 4 | 2 | 1 | 0 | `:363-377` |
| Bibliothèque | 20 | 14 | 8 | 4 | 2 | 0 | `:379-393` |

- Total au plus 100 (25 + 30 + 15 + 10 + 20), borné entre 0 et 100 (`:395`). Si la population vaut 0, la note vaut 0 (`:311-313`).
- La couverture est une **capacité divisée par la population**, plafonnée à 100. Elle est calculée au tick 33 (`city/culture.c:96-160`) :
  - théâtre : 500 places par théâtre actif (`:101`) ;
  - religion : pour chaque dieu, 500 par oracle (compte **total**, pas actif), 750 par petit temple et 1500 par grand temple. `religion_coverage` est la **moyenne des 5 dieux** (`:111-145`) ;
  - école : 75 par école, rapporté à la population d'âge scolaire (`:150-151`) ;
  - bibliothèque : 800 par bibliothèque, rapporté à la population totale (`:152-153`) ;
  - académie : 100 par académie, rapporté à la population d'âge académique (`:154-155`).
- « Actif » veut dire `building_count_active` (`building/count.c:214-217`).
- Amphithéâtre (800), colisée (1500), hippodrome (100 % dès qu'il y en a un) et hôpital (1000) sont calculés (`culture.c:102-108,158-159`) mais **ne comptent pas** dans la note.

Remarques pour l'équilibrage :
- La note ignore la desserte réelle des maisons par les marcheurs. Seul compte le nombre de bâtiments actifs par habitant.
- Elle réagit en un mois et sature à 100. Pour la religion, il faut couvrir les cinq dieux.

### 1.2 Prospérité — variation une fois par an (`ratings.c:399-448`, `:606-608`), plafond recalculé chaque mois (`:605`)

| Critère (année écoulée) | Effet | Lignes |
|---|---|---|
| chômage < 5 % / ≥ 15 % | +1 / −1 | `ratings.c:403-407` |
| « a gagné de l'argent » : `last_year.expenses.construction + treasury > prosperity_treasury_last_year` (`:154-158`) | +5, sinon −1 | `:409-413` |
| au moins 2 sortes de nourriture consommées | +1 | `:416-418` |
| salaire moyen (`wage_rate_paid_last_year / 12`) ≥ salaire de Rome + 2 / < salaire de Rome | +1 / −1 | `:420-425` |
| plus de 30 % de la population en tentes ou baraques (niveau ≤ `HOUSE_LARGE_SHACK`, `city/population.c:333-334`) | −1 | `:427-429` |
| plus de 10 % en villas ou palais (niveau ≥ `HOUSE_SMALL_VILLA`) | +1 | `:430-432` |
| tribut impayé l'an dernier | −1 | `:434-436` |
| hippodrome avec des courses | +1 | `:438-440` |

- Variation annuelle comprise entre −5 et +10.
- La note est plafonnée par `prosperity_max` (`:441-444`), puis bornée entre 0 et 100 (`:445`). Le trésor de référence est mis à jour juste après (`:414`).
- `prosperity_max` est la **moyenne, par bâtiment-maison** (et non par habitant), de `model_get_house(level)->prosperity` (`:450-466`). Ces valeurs viennent de `c3_model.txt` (`building/model.c:83,147`), une donnée du jeu absente du dépôt et non lue ici.
- Prêt de secours : −3 tout de suite (`ratings.c:59-66`, appelé par `emperor.c:49`).
- Le tribut est déjà payé quand « a gagné de l'argent » est évalué (`tick.c:76` puis `:79`).
- Toutes les notes partent de 0 (`city/data.c:17`), et le trésor de référence aussi.

Remarques pour l'équilibrage :
- Avec au plus +10 par an, il faut au moins 10 ans pour passer de 0 à 100.
- Le bonus de profit vaut +5 quel que soit le montant gagné. Les dépenses de construction sont neutres.
- Comptent comme recettes : les dons (prêt de secours compris, `finance.c:78-82`) et les exportations vers un autre joueur (`mp/trade.c:386-391`).
- Beaucoup de petites maisons font baisser le plafond.

### 1.3 Paix — une fois par an (`ratings.c:468-496`)

- **Gain de base** : +2 tant que `years_of_peace` < 2, puis +5 (`:471-475`).
- **Malus** :
  - −1 s'il y a eu au moins un criminel dans l'année, manifestant ou voleur (`:476-478`, enregistrés par `figuretype/crime.c:83,106`) ;
  - −5 s'il y a eu une émeute (`:479-481`, `crime.c:67`) ;
  - −N pour N bâtiments détruits par des ennemis, au plus 12 par an (`:482-484`, plafond `:85-87`). Ne comptent pas : tentes, préfecture, poste d'ingénieurs, puits, fort, terrain de fort, porte fortifiée et tour (`:71-80`). Source : `building_destroy_by_enemy` (`building/destruction.c:209-216`), appelée par les dégâts ennemis (`figure/movement.c:183`) et par la case d'apparition d'une invasion (`scenario/invasion.c:288`).
- **Remise à zéro** : `years_of_peace` revient à 0 après une émeute ou un bâtiment détruit, sinon il augmente de 1 (`:485-489`). Les compteurs sont remis à zéro (`:490-492`). La note est bornée entre 0 et 100 (`:494`).
- **Trajectoire** : 2, 4, 9, 14… Il faut 22 ans calmes pour atteindre 100. **Au bout de 10 ans, la note vaut au plus 44**, ou 34 avec un criminel chaque année.
- **Criminels** (tick 45, `crime.c:110-150`) : il en apparaît seulement si une maison a un bonheur inférieur à 50.
  - Avec un sentiment de la cité inférieur à 30, le tirage doit être ≥ sentiment + 50. Une émeute n'est possible que si la maison la plus malheureuse est à 10 ou moins.
  - Avec un sentiment inférieur à 60, le tirage doit être ≥ sentiment + 40, et il n'y a que des voleurs ou des manifestants.
  - Une émeute donne +20 de bonheur à toutes les maisons (`crime.c:68`).

Remarques pour l'équilibrage :
- La paix est lente : sur une partie au score de 10 ans (`game/rules.c:31`), elle ne dépasse pas 44.
- **[LC]** Le malus de destruction va dans le `city_data` de la cité **dont c'est le tour** (contexte courant). Les ennemis IA d'une cité sont dans sa tranche, donc ils la pénalisent bien. Pour la guerre entre joueurs (M9), les soldats de l'attaquant tourneraient dans le contexte de l'attaquant : il faudra imputer la destruction à la victime.

### 1.4 Faveur
Voir le §2. Elle n'évolue plus en multijoueur (`ratings.c:602-604`).

---

## 2. Faveur de César en détail

- **Valeur** : `city_data.ratings.favor` (`city/data_private.h:233`), bornée entre 0 et 100 à chaque modification (`ratings.c:101-104,595`).
- **Départ** : `scenario_starting_favor` (`emperor.c:19`), qui vient de la difficulté (`game/difficulty.c:13-17`) :

  | Très facile | Facile | Normal | Difficile | Très difficile |
  |---|---|---|---|---|
  | 70 | 60 | 50 | 50 | 40 |

  En multijoueur, la difficulté par défaut est « difficile » (`game/rules.c:25`), donc la faveur part de 50. Chaque cité ajoutée est une copie du joueur 0 (`game/player_context.h:31-41`).
- **Il n'y a ni cible ni rappel vers une cible.** La faveur est un simple cumul. La seule dérive automatique est **−2 par an** (`ratings.c:512-514`, sauf dans les tutoriels 1 et 2). `favor_last_year` et `favor_change` servent seulement au texte d'explication (`:586-593`, `:290-293`).
- Ni l'épargne personnelle ni le trésor positif n'interviennent : `ratings.c` ne lit jamais `personal_savings`.

| Source | Effet | Lignes |
|---|---|---|
| Chaque année | −2 | `ratings.c:512-514` |
| Tribut impayé (1ʳᵉ, 2ᵉ, 3ᵉ année consécutive et au-delà) | −3, −5, −8 | `:516-524`. Compteur `tribute_not_paid_total_years` : `finance.c:352-356`, remis à 0 quand le tribut est payé (`:359,369`) |
| Salaire, par an. `delta = salary_rank − player_rank` | si `player_rank ≠ 0` : −delta si delta > 0, **+1** si delta < 0. Si `player_rank = 0` : −delta si delta > 0 | `:526-539` |
| Années jalons 25, 50 et 75 % | +5 si tous les objectifs actifs sont atteints au prorata, sinon −2 | `:541-584` |
| Cadeaux | de +10 à 0, dégressif (§4) | `emperor.c:220-256` |
| Demande livrée à temps / en retard | +`favor` / +`favor`/2 | `scenario/request.c:38,41` |
| Demande refusée / ignorée | −3 / −5 | `request.c:58,65` |
| Bataille lointaine : pas d'envoi / trop tard / défaite / victoire | −50 / −25 / −10 / **+25** | `city/military.c:241,245,250,256` |
| Armée de César entièrement tuée, si faveur < 35 | +10 | `emperor.c:108-114` |
| Dette : 2ᵉ passage en négatif | −5 | `emperor.c:50-54` |
| Dette : 12 mois de plus | −10, sauf si des soldats impériaux sont présents | `emperor.c:63-69` |
| Dette : encore 12 mois | faveur plafonnée à 10 | `emperor.c:79-84` |
| Jeu libre (`is_open_play`) | faveur forcée à 50 | `ratings.c:500-503` |
| Changement d'empereur | **aucun** : `city_ratings_reset_favor_emperor_change` (mise à 50) n'est jamais appelée | `ratings.c:106-109`, `scenario/emperor_change.c:23-34` |

- Le prêt de secours n'est accordé **qu'une fois par partie** : `debt_state` ne revient jamais à 0, seul `months_in_debt` est remis à −1 (`emperor.c:36-39`).
- Le prêt vaut `difficulty_adjust_money(scenario_rescue_loan())` (`:42`), soit 100 % en difficile (`difficulty.c:16`).
- Intérêts de la dette : 10 % par an, prélevés par douzièmes chaque mois (`finance.c:250-257`).

Remarques pour l'équilibrage :
- Sans demandes ni batailles, la faveur ne fait que baisser (−2 par an, plus les malus).
- Les seules hausses régulières sont les cadeaux, qui s'épuisent, et un salaire inférieur au rang (+1 par an).
- Le plafond à 100 et le plancher à 0 sont appliqués à chaque variation, donc une perte de −50 est absorbée par le plancher.

---

## 3. Colère de César (`emperor.c:89-155`, chaque jour au tick 4)

Une seule machine à états par cité (`city_data.emperor.invasion`, `data_private.h:78-86`). Les branches sont évaluées dans l'ordre suivant :

1. **Armée impériale présente** (`figure.imperial_soldiers` > 0) : `duration_day_countdown` diminue de 1 par jour (`:93`).
   - Si la faveur est ≥ 35 et que le compte à rebours est < 176 (plus de 16 jours, soit un mois, de présence), l'armée est **en pause** : `formation_caesar_pause`, `wait_ticks = 20` chaque jour (`:94-95`, `figure/formation.c:190-197`).
   - Sinon, si la faveur est ≥ 22 et que le compte à rebours est > 0, l'armée **se retire** : `months_low_morale = 1` (`formation.c:199-206`), avec un message une seule fois (`:96-102`). Si le compte à rebours vaut 0, le message est « le siège continue » (`:103-106`).
   - Si la faveur est < 22, l'armée continue d'attaquer.
2. **Armée vaincue** (`soldiers_killed ≥ size`) : remise à zéro. Si la faveur est < 35 : +10, avec un message de respect n° 1, 2 ou 3 selon le nombre d'invasions (`:108-122`). Chaque légionnaire de César tué incrémente `soldiers_killed` (`figure/figure.c:80-82` → `emperor.c:333-336`).
3. **Pas de compte à rebours en cours**, et faveur ≤ 10 : `warnings_given` augmente de 1, `days_until_invasion` passe à **192 jours (1 an)**. Le message de colère n'est montré qu'au 1ᵉʳ avertissement (`:123-131`).
4. **Compte à rebours en cours** : il diminue de 1 par jour. À 0, l'invasion part, **sans revérifier la faveur** (`:132-153`).
   - Taille selon `invasion.count` (0, 1, 2, ≥ 3) : **32, 64, 96, 144** (`:136-145`).
   - Une invasion réussie incrémente `count`, fixe `duration_day_countdown = 192` et enregistre la taille.

**L'armée** (`scenario_invasion_start_from_caesar`, `scenario/invasion.c:422-429` → `start_invasion`, `:193-317`) :
- Taille × difficulté : 40, 60, 80, 100 ou 120 % (`difficulty.c:13-17`), plafonnée à 150 (`invasion.c:203-206`).
- Elle apparaît au **point d'entrée de la carte** (`:230-233`) et détruit le bâtiment présent sur cette case (`:288`).
- Elle cible les « plus beaux bâtiments », id 24 (`:424`).
- Composition : 100 % de `FIGURE_ENEMY_CAESAR_LEGIONARY`, en colonne (`:70`). Une formation jusqu'à 16 soldats, 2 jusqu'à 32, 3 au-delà (`:174-191`). Une formation n'a que 16 cases de figures : au-delà, les soldats existent mais ne sont pas indexés (`formation.c:459-465`).
- Caractéristiques (`figure/properties.c`, colonnes dégâts max / attaque / défense) :

  | Unité | Dégâts max | Attaque | Défense | Ligne |
  |---|---|---|---|---|
  | Légionnaire de César | 150 | 13 | 2 | `:62` |
  | Légionnaire du joueur | 150 | 10 | 0 | `:18` |
  | Javelinier | 80 | 4 | 0 (projectile 4) | `:16` |
  | Cavalier | 120 | 8 | 0 | `:17` |

- Moral maximal 100 (`formation.c:272-273`).
- À l'arrêt et en colonne, les légionnaires de César ont le même bonus de défense que ceux du joueur : +7 en colonne, +4 en double ligne (`figure/combat.c:87-96`).
- Le joueur a au plus 6 légions de 16 soldats, soit 96 (`figure/formation.h:9,22`).

**Fin** : quand l'armée est tuée (branche 2) ou qu'elle se retire. Si la faveur reste ≤ 10, un nouvel avertissement relance un an de compte à rebours (branche 3).

**Renvoi** : la faveur seule ne fait **jamais** perdre. La défaite (`VICTORY_STATE_LOST`) arrive dans deux cas :
- les envahisseurs (impériaux et ennemis) sont plus nombreux que 2 + les soldats **et** la population est tombée sous le quart de son record (`victory.c:92-96`) ;
- il y a des envahisseurs et la population est nulle (`:97-101`).

On voit alors le message `MESSAGE_FIRED`, puis l'écran de renvoi (`:123-129`).

Remarques pour l'équilibrage :
- Le délai d'un an n'est pas annulé si la faveur remonte avant l'invasion. Ce n'est qu'une fois l'armée sur place que la faveur ≥ 35 ou ≥ 22 la freine.
- La taille ne dépend que du nombre d'invasions passées, pas de la richesse ni de l'armée du joueur. 144 légionnaires plafonnés à 150, contre 96 soldats au plus pour le joueur.

---

## 4. Cadeaux, épargne personnelle, salaire et rangs

- **Salaire** (`emperor.c:15`), en deniers par mois selon le rang 0 à 10 :

  | Rang | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 |
  |---|---|---|---|---|---|---|---|---|---|---|---|
  | Salaire | 0 | 2 | 5 | 8 | 12 | 20 | 30 | 40 | 60 | 80 | 100 |

  - Noms des rangs **[LC, d'après le jeu original]** : citoyen, scribe, ingénieur, architecte, questeur, procurateur, édile, préteur, consul, proconsul, César. L'interface les lit dans le groupe de textes 32 (`window/advisor/imperial.c:176`).
  - Le salaire est payé chaque mois si le trésor est > −5000 : il passe du trésor à l'épargne et compte en `expenses.salary` (`finance.c:259-266,322`).
  - Le joueur choisit son rang de salaire librement (`window/set_salary.c:102`). « Continuer à gouverner » remet le salaire à 0 (`victory.c:151-159`).
- **Rang du joueur** : rang de la campagne, ou `scenario_property_player_rank` pour un scénario personnalisé, ce qui est le cas du multijoueur (`scenario_set_custom(2)`, `mp/mapgen.c:236`, `mp/lockstep.c:338`). Le rang de salaire part au même niveau, plafonné à 10 (`emperor.c:21-31`).
- **Épargne personnelle** :
  - Elle vaut 0 en scénario personnalisé (`emperor.c:23-24`). En campagne, elle est reportée de mission en mission (`scenario/scenario.c:452-456`, `window/mission_end.c:124`).
  - Elle ne rapporte pas d'intérêts.
  - Elle n'a que deux usages : les cadeaux, et le don à la cité, qui la verse au trésor comme `income.donated` (`emperor.c:321-326`, `finance.c:78-82`).
- **Coût des cadeaux** : modeste `épargne/8 + 20`, généreux `épargne/4 + 50`, somptueux `épargne/2 + 100` (`emperor.c:200-206`).
  - Le coût est fixé à l'ouverture du conseiller impérial (`imperial.c:94-96`).
  - Un cadeau n'est possible que si son coût est ≤ l'épargne (`emperor.c:195-198`).
  - Le type d'objet (0 à 3) change à chaque envoi, sans effet sur le jeu (`:259-263`).
- **Gain de faveur et usure** (`emperor.c:220-256`) : le compteur `gift_overdose_penalty` augmente à chaque cadeau.

  | Compteur avant l'envoi | Modeste | Généreux | Somptueux |
  |---|---|---|---|
  | 0 | +3 | +5 | +10 |
  | 1 | +1 | +3 | +5 |
  | 2 | 0 | +1 | +3 |
  | 3 | 0 | 0 | +1 |
  | 4 et plus | 0 | 0 | 0 |

  - Il n'y a **pas de délai d'attente** entre deux cadeaux. `months_since_gift` repasse à 0 à chaque envoi (`:258`).
  - L'usure est remise à 0 quand 12 mois sont passés sans cadeau (`ratings.c:504-507`, vérifié chaque mois).

Remarques pour l'équilibrage :
- En multijoueur, `update_favor_rating` ne tourne plus (`ratings.c:602-604`). `months_since_gift` n'augmente donc jamais et l'usure ne serait jamais remise à zéro si l'on réactivait les cadeaux seuls.
- Le coût est proportionnel à l'épargne, donc un cadeau somptueux coûte toujours plus de la moitié de l'épargne.
- Sans salaire, l'épargne reste à 0 : les cadeaux coûteraient seulement 20, 50 ou 100 Dn.

---

## 5. Demandes impériales et batailles lointaines

**Demandes** :
- Données du scénario (`scenario/data.h:88-99`), au plus 20 (`:9`) :
  - `year`, compté depuis l'année de départ ;
  - `resource` : une marchandise, `RESOURCE_DENARII` = 16 ou `RESOURCE_TROOPS` = 17 (`game/resource.h:29-30`) ;
  - `amount`, au plus 999 dans l'éditeur (`window/editor/edit_request.c:132-133`) ;
  - `deadline_years`, 5 par défaut (`scenario/editor.c:89`) ;
  - `favor`, 8 par défaut et au plus 100 (`editor.c:90`, `edit_request.c:160`).
- Initialisation (`request.c:15-24`) : le mois est tiré entre 2 et 9, et `months_to_comply = 12 × deadline_years`.
- Déroulement (`request.c:26-96`) :
  1. apparition à l'année et au mois prévus, avec un message (`:79-92`) ;
  2. rappel quand il reste 12 mois (`:51-53`) ;
  3. à l'échéance : refus, faveur −3, et 24 mois de sursis (`:54-59`) ;
  4. à la fin du sursis : demande ignorée, faveur −5 (`:60-66`).
- Envoi (`request.c:98-116`) : l'arrivée a lieu de 1 à 4 mois plus tard (`:105`). Elle rapporte +`favor`, ou +`favor`/2 si l'envoi était en retard (`:33-45`).
  - Argent : compté en `sundries` (`:109`).
  - Troupes : retire `amount` habitants (`city/population.c:197-203`) et `amount` armes (`request.c:112`).
  - Marchandises : retirées des entrepôts (`:114`).
- Conditions dans l'interface : pour l'argent, trésor > montant ; pour les marchandises, stock ≥ montant (`imperial.c:157-166`).
- **[LC]** Pour les troupes, la vérification lit `stored_in_warehouses[17]` alors que `RESOURCE_MAX` vaut 16 : c'est une lecture hors du tableau (`city/resource.c:20-23`, `resource.h:30,33`).

**Batailles lointaines** :
- **Déclenchement** (`scenario/distant_battle.c:39-56`) : une invasion de type 4 (`scenario/types.h`) à l'année et au mois prévus, si les temps de trajet romain et ennemi dépassent tous deux 4 mois et qu'aucune bataille n'est en cours.
  - Les temps de trajet viennent des objets `ROMAN_ARMY` et `ENEMY_ARMY` de l'empire (`:27-37`).
  - Il en résulte `months_until_battle = 24` et `enemy_strength = amount` de l'invasion (`city/military.c:148-157`). `enemy_strength` est un `uint8` (`data_private.h:100`).
- **Envoi** : le joueur coche « au service de l'empire » sur des légions (`formation.c:154-157`), puis les envoie depuis le conseiller impérial (`imperial.c:143-153,234-240`).
  - `roman_strength = Σ facteur × soldats` (`figure/formation_legion.c:168-203`). Le facteur vaut 3 pour un légionnaire formé à l'académie militaire et 2 sinon, 2 pour un javelinier ou un cavalier formé et 1 sinon.
  - **[LC]** `roman_strength` est un `uint8` (`data_private.h:101`), alors que le maximum possible est 6 × 16 × 3 = 288 : au-delà de 255, la valeur repart de 0.
  - Les soldats marchent jusqu'à la sortie et deviennent des fantômes (`figuretype/soldier.c:368-403`).
- **Trajet** (`military.c:159-188`) : le compteur aller part du temps de trajet romain (`:105`). Il baisse de 1 par mois, ou de 2 si les Romains sont en retard sur la progression de l'ennemi, sans descendre sous 1.
- **Résolution** quand `months_until_battle` atteint 0 (`military.c:237-266`) :

  | Situation | Faveur | Conséquences |
  |---|---|---|
  | Aucune légion envoyée | −50 | la ville devient étrangère 24 mois |
  | Arrivée trop tardive (compteur aller > 2) | −25 | ville étrangère ; les troupes reviennent **sans pertes** |
  | `roman < enemy` : défaite | −10 | ville étrangère ; **tous** les soldats envoyés meurent (`:205-211,248-253`) |
  | Sinon : victoire | **+25** | **+1 arc de triomphe** disponible (`city/buildings.c:122-125`, seule source d'arcs, constructible si disponibles > posés, `:114`) ; `won_count` +1 ; retour des troupes |

  - Pertes en cas de victoire, selon l'avantage `(R − E) / R` (`:212-232`) :

    | Avantage | < 10 % | < 25 % | < 50 % | < 75 % | < 100 % | = 100 % (E = 0) |
    |---|---|---|---|---|---|---|
    | Pertes | 70 % | 50 % | 25 % | 15 % | 10 % | 5 % |

  - Toute légion engagée perd aussi 75 points de moral (`formation_legion.c:207`).
  - Les survivants reviennent après `roman_months_traveled` mois (`military.c:261,268-283`).
  - La ville étrangère redevient vulnérable après 24 mois (`:284-290`, `empire/city.c:347-355`).

Remarques pour l'équilibrage :
- La victoire exige seulement R ≥ E : la marge ne réduit que les pertes.
- Les écarts de faveur sont énormes par rapport au reste : −50 / +25, contre −2 par an.

---

## 6. Fêtes (`city/festival.c`)

- **Coûts**, recalculés chaque jour au tick 49 (`festival.c:137-150`) :

  | Fête | Coût en deniers | Vin | Délai (mois) |
  |---|---|---|---|
  | Petite | population/20 + 10 | — | 2 |
  | Grande | population/10 + 20 | — | 3 |
  | Somptueuse | population/5 + 40 | population/500 + 1 | 4 |

  S'il manque du vin, la fête somptueuse est indisponible et la sélection retombe sur la grande (`:143-149`).
- **Programmation** (`:69-90`) :
  - L'argent est payé tout de suite en `sundries` (`:85`) et le vin est retiré (`:87-89`). Les délais sont aux lignes `:74-83`.
  - Une seule fête peut être prévue à la fois (`:10-13`, `window/advisor/entertainment.c:172-186`).
  - Impossible si le trésor est ≤ −5000 (`window/hold_festival.c:147-149`).
  - **Aucun prérequis** : ni temple ni oracle (rien dans `city_festival_schedule`).
  - Il n'y a pas de limite annuelle ; seul l'effet s'use.
  - Une commande multijoueur existe (`mp/actions.c:233-236`).
- **Effet sur le bonheur** (`throw_party`, `:92-118`) : +7, +9 ou +12 sur **chaque maison** si la première fenêtre de 12 mois est libre. Sinon +2, +3 ou +5 si la seconde fenêtre est libre. Sinon aucun effet. Le bonheur de chaque maison reste borné entre 0 et 100 (`city/sentiment.c:30-38`).
- **Effet sur le dieu choisi** : son `months_since_festival` repasse à 0 (`festival.c:110`).
  - Cible d'humeur du dieu : couverture du dieu + 12 − min(mois depuis sa fête, 40) (`city/gods.c:252-305`). Puis +50 (ou 100) pour le dieu qui a le plus de temples, et −25 pour celui qui en a le moins (`:307-317`, Vénus ne reçoit jamais le bonus). Enfin, un plancher selon la population : 50 sous 100 habitants, puis 0 au-delà de 500 (`:318-335`).
  - Une fête vaut donc jusqu'à **+40** sur la cible de ce dieu.
  - L'humeur se rapproche de la cible d'un point par jour (`gods.c:156-160`).
  - Aucune malédiction tant que le dieu a eu une fête il y a 3 mois ou moins (`:213,218`).
- **Aucun effet direct sur les notes** : `ratings.c` ne lit rien des fêtes. L'effet est indirect, par le bonheur qui fait baisser la criminalité, donc monter la paix.

---

## 7. Indicateurs économiques déjà suivis

| Domaine | Données | Où |
|---|---|---|
| Finances | `this_year` et `last_year` : recettes {impôts, exportations, dons, total} ; dépenses {importations, salaires, construction, intérêts, salaire du gouverneur, divers, tribut, total} ; `net_in_out` ; `balance` | `city/finance.h:46-65`, report annuel `finance.c:296-334`. Aussi : `treasury`, `tax_percentage` (0 à 25, `:30`), `stolen_*`, `cheated_money`, `tribute_not_paid_*`, `wage_rate_paid_*` (`data_private.h:108-125`) |
| Exportations | empire : `city_finance_process_export`, ×2 sous la bénédiction de Neptune (`finance.c:54-62`) ; entre joueurs : `mp/trade.c:386-391`. Prix d'achat à l'empire ×1,5 en multijoueur (`empire/trade_prices.c:29`) | |
| Impôts | chaque mois : habitants × `tax_multiplier` du modèle (ajusté par la difficulté) / 2 × taux ; plèbe et patriciens séparés | `finance.c:167-240` |
| Population | `population`, `population_last_year`, `highest_ever`, `average_per_year`, naissances et décès de l'année, `lost_*` ; historique de 2400 mois | `data_private.h:147-184` ; historique `population.c:228-235` |
| Population par niveau de maison | `at_level[20]`, recalculé chaque mois ; tentes, baraques, grande insula et plus, villas | `finance.c:178-191` ; `population.c:312-346` |
| Bâtiments | nombre actif et total par type | `building/count.c:214-222` |
| Travail, sentiment, culture | `wages`, `wages_rome`, `unemployment_percentage` ; `sentiment.value` (`sentiment.c:260-264`) ; couvertures et moyennes par maison (`culture.c:13-24,162-189`) | |
| Commerce par ressource | `traded` et `limit` par route et ressource, remis à 0 chaque année (`empire/trade_route.c:6-11`, `empire/city.c:176-183`) ; achats et ventes par marchand, par figure (`figure/trader.c:11-17`) | |
| Commerce entre joueurs | aucun compteur par partenaire ; seulement les finances, et les prix demandés (`mp/trade.c:30-31`) | |
| Stocks | entrepôts, greniers, mois de nourriture, nourriture produite ou consommée le mois dernier | `data_private.h:310-336` |

**Par joueur, et sauvegardé** :
- Toute mémoire enregistrée par `player_context_register` est permutée à chaque changement de cité, entre autres :
  - `city_data` (`city/data.c:1062`) : notes, faveur, empereur, finances, population, armée, bataille lointaine ;
  - `culture_coverage` (`culture.c:229`) ;
  - `scenario`, qui contient les demandes et les invasions (`scenario/scenario.c:494`) ;
  - `formation_totals` (`formation.c:761`), `enemy_armies` (`figure/enemy_army.c:205-206`), `building_count` (`count.c:431`) ;
  - `trade_routes` (`trade_route.c:90`), `trade_prices` (`trade_prices.c:76`), les villes de l'empire (`empire/city.c:421`) ;
  - `invasions` (`invasion.c:493`), `emperor_change` (`emperor_change.c:53`), `victory` (`victory.c:175`).
- La sauvegarde multijoueur écrit chaque cité avec les morceaux classiques plus son « extra_state » (`mp/savegame.c:71-77`). Exemples vérifiés : `at_level`, `highest_ever`, `prosperity_max`, `peace_years_of_peace`, `won_count`, `invasion.count` (`data.c:89,508,511,468,478,504`).

**Commun au monde** : scores de fin (`mp/endgame.c:9-17`, sauvegardés par `mp/savegame.c:67`), `game_rules`, grilles de propriété, de territoire et de brouillard, et les bâtiments de César (`savegame.c:66-70`). Le score en direct est recalculé chaque jour et **n'est pas sauvegardé** (`endgame.c:16,19-30`).

---

## 8. Armée : ce qui se mesure

- `city_data.military` (`data_private.h:88-94`) : `total_legions`, `total_soldiers` (`uint8`), `empire_service_legions`. Recalculé aux ticks 5 et 29 par `formation_calculate_figures` puis `city_military_update_totals` (`formation.c:482-543`, `city/military.c:49-64`).
- Force vue par l'IA :
  - légion = soldats + soldats/2 pour les légionnaires (`formation.c:519-524`), cumulée dans `enemy_army` `totals.legion_strength` ;
  - ennemi = nombre de figures (`enemy_army.c:12-19`) ;
  - l'ennemi ignore les légions si sa force dépasse le double de la leur (`:123-126`).
- Carte d'influence : rayon de 7 à 2 cases selon la taille de la légion, recalculée tous les 5 jours (`enemy_army.c:90-121`).
- Champs d'une formation (`formation.h`) : `morale` (`:78`), `months_from_home` (`:79`), `num_figures` (`:85`), `total_damage` et `max_total_damage` (`:88-89`), `recent_fight` (`:107`), `missile_fired`, `empire_service`, `in_distant_battle`, `has_military_training` (`:117`), `is_at_fort`.
- Moral maximal (`formation.c:267-295`) :

  | Unité | Formée | Non formée |
  |---|---|---|
  | Légionnaire | 100 | 80 |
  | Javelinier ou cavalier | 80 | 60 |

  Ennemis : de 70 à 90 selon le peuple.
- Une légion hors du fort, sans combat récent depuis plus de 3 mois, perd 5 de moral par mois (`formation.c:377-384`). Un combat met `recent_fight` à 6, puis il baisse de 1 par mois (`:170-173,405-407`).
- Événements de combat :
  - une mort passe la figure à l'état cadavre et fait baisser le moral de sa formation de 5 à 20 selon la proportion perdue (`combat.c:105-112`, `formation.c:297-316`) ;
  - quand une formation tombe à 20 ou moins, toutes les formations de son camp perdent 10 de moral et l'autre camp en gagne 10 (`formation.c:318-361`).
- Compteurs existants :
  - `city_data.figure.{enemies, imperial_soldiers, rioters, soldiers, attacking_natives}`, **instantanés**, remis à zéro chaque tick (`city/figures.c:5-50`) ;
  - `emperor.invasion.soldiers_killed`, pour les soldats de César seulement ;
  - `peace_destroyed_buildings`, par an et plafonné à 12 ;
  - `distant_battle.total_count` et `won_count`.
  - **Il n'existe aucun compteur cumulé** de soldats perdus, d'ennemis tués ou de bâtiments détruits par attaquant.
- Recrutement : un légionnaire consomme 1 arme de la caserne (`building/barracks.c:100-104`). Au plus 6 légions de 16 soldats.

---

## 9. Ce que le multijoueur neutralise aujourd'hui

| Mécanisme | Neutralisé par | Ligne |
|---|---|---|
| Évolution de la faveur : −2 par an, tribut, salaire, jalons, usure des cadeaux | `!game_rules_is_multiplayer()` autour de `update_favor_rating` | `city/ratings.c:602-604` |
| Colère et invasions de César | idem autour de `process_caesar_invasion` | `city/emperor.c:160-162` |
| Invasions de César prévues par le scénario (et toutes les invasions IA si la règle les interdit) | l'avertissement est effacé | `scenario/invasion.c:330-334` |
| Batailles lointaines | `scenario_distant_battle_process` n'est pas appelé | `game/tick.c:90-93` |
| Demandes impériales | `scenario_request_process` n'est pas appelé | `tick.c:95-97` |
| Changement d'empereur | idem | `tick.c:219-221` |
| Salaire du gouverneur, donc l'épargne | `pay_monthly_salary` n'est pas appelé | `city/finance.c:273-275` |
| Victoire, défaite et renvoi | `city_victory_check` sort tout de suite | `city/victory.c:110-112` |
| Conseiller impérial : cadeaux, salaire, dons, envoi des demandes | redirigé vers les notes ; bouton inactif | `window/advisors.c:103-105,220-222` |

**Toujours actif en multijoueur** (D-026 « gardés », ou pas encore traité) :
- **Dette** (`emperor.c:159`, sans condition) :
  - un seul prêt de secours, avec prospérité −3 ;
  - puis **faveur −5, −10, et plafond à 10** (`:54,68,83`).
  - La faveur n'est donc **pas tout à fait figée**, contrairement à ce que dit D-026.
- **Tribut annuel** (`finance.c:397-402`, sans condition) : 25 % du bénéfice ou un forfait de 50 à 500 Dn selon la population (`:357-387`). S'il est impayé, prospérité −1 (`ratings.c:434-436`).
  - **Écart avec DESIGN §5.1**, qui range le tribut et le prêt de secours parmi les mécanismes désactivés : le code suit D-026, qui les garde.
- **Salaires de Rome** : variation aléatoire de 1 à 4, entre 5 et 45 (`city/labor.c:99-118`, `scenario/random_event.c`).
- **Blé fourni par Rome**, si la carte l'active (`city/resource.c:292,301,351`).
- **Affichage de la faveur** : conseiller des notes (`window/advisor/ratings.c:92`), barre latérale (`widget/sidebar/extra.c:161`), info-bulle (`graphics/tooltip.c:289`), drapeau du sénat (`widget/city_without_overlay.c:262`).
- Commandes « service de l'empire » et « envoi » des légions, sans effet en multijoueur (`mp/actions.c:310-320`).

Pour réactiver : tout l'état de César est déjà **par cité** (`city_data.emperor`, `ratings`, `distant_battle`, `scenario`, §7). Retirer une condition suffit donc à le rendre actif cité par cité. Mais :
- les demandes, les invasions et les batailles lointaines viennent des **données du scénario**, que les cartes multijoueur ne remplissent pas (aucune trace dans `src/mp/*.c`) ;
- l'armée de César entre par `scenario_map_entry()` de la cité courante (`invasion.c:230-233`). **[LC]** Il reste à vérifier que c'est bien le point d'arrivée du joueur sur une carte composée.
