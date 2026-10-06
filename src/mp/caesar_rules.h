#ifndef MP_CAESAR_RULES_H
#define MP_CAESAR_RULES_H

/**
 * @file
 * Every setting of Caesar the judge (doc/mp/CESAR.md, D-050, D-053), in one place. They are starting values:
 * CESAR.md §10 says how they are measured and corrected, and every change goes in DECISIONS.md with its reason.
 *
 * Laurels are counted in tenths, the gauge of Caesar's wrath in tenths of a point (0 to 1000), so that weights of
 * one half or one quarter lose nothing. Durations are in game months.
 */

// ---------- Laurels of the city, every month (CESAR §4.1, §5) ----------

// Tenths of a laurel a note of 100 brings every month: 120 laurels a year when all are at 100
#define MP_CAESAR_WEIGHT_PROSPERITY 25
#define MP_CAESAR_WEIGHT_TRADE 25
#define MP_CAESAR_WEIGHT_CULTURE 20
#define MP_CAESAR_WEIGHT_HOUSING 15
#define MP_CAESAR_WEIGHT_GREATNESS 15

// Trade note: share of the volume and of the network, in percent; purchases from players count for half
#define MP_CAESAR_TRADE_VOLUME_SHARE 70
#define MP_CAESAR_TRADE_NETWORK_SHARE 30
#define MP_CAESAR_TRADE_PURCHASE_PERCENT 50

// Greatness note: 100 * population / (population + reference), 50 at the reference
#define MP_CAESAR_GREATNESS_REFERENCE 8000

// ---------- Victory (CESAR §4.3, D-053) ----------

// The first city to reach the score wins; the lobby offers these, the second one by default
#define MP_CAESAR_NUM_SCORES 4
#define MP_CAESAR_SCORES { 500, 1000, 1500, 2000 }
#define MP_CAESAR_DEFAULT_SCORE 1000

// Ranks of the original game, one for each tenth of the score; the last one, Caesar, is the victory
#define MP_CAESAR_NUM_RANKS 11
// Highest salary of each rank, in denarii a month (original salaries)
#define MP_CAESAR_RANK_SALARIES { 0, 2, 5, 8, 12, 20, 30, 40, 60, 80, 100 }

// ---------- Laurels of Caesar (CESAR §6), in tenths ----------

// Festivals: small, large, grand; one counted every 6 months
#define MP_CAESAR_FESTIVAL_LAURELS { 20, 40, 60 }
#define MP_CAESAR_FESTIVAL_PERIOD 6

// Gifts: modest, generous, lavish; one counted every 12 months
#define MP_CAESAR_GIFT_LAURELS { 40, 70, 100 }
#define MP_CAESAR_GIFT_PERIOD 12

// Campaigns of Caesar (distant battles shared by every city)
#define MP_CAESAR_CAMPAIGN_WON_LAURELS 600     // at most, by share of the strength sent
#define MP_CAESAR_CAMPAIGN_LOST_LAURELS 150    // at most, by share of the strength sent
#define MP_CAESAR_CAMPAIGN_REFUSED_LAURELS -100 // had legions and sent none
#define MP_CAESAR_CAMPAIGN_PERIOD_MIN 36       // months between two campaigns
#define MP_CAESAR_CAMPAIGN_PERIOD_MAX 48
#define MP_CAESAR_CAMPAIGN_DELAY 6             // months to send legions
#define MP_CAESAR_CAMPAIGN_ENEMY_STRENGTH 60   // per player at the start
#define MP_CAESAR_CAMPAIGN_ENEMY_GROWTH 5      // percent more each year

// Requests of Caesar to the whole province (D-053): a pot shared by the loads each city sent
#define MP_CAESAR_REQUEST_POT_PER_PLAYER 100
#define MP_CAESAR_REQUEST_NOTHING_SENT -50      // when the province misses the deadline
#define MP_CAESAR_REQUEST_PERIOD_MIN 24
#define MP_CAESAR_REQUEST_PERIOD_MAX 36
#define MP_CAESAR_REQUEST_DELAY 12
#define MP_CAESAR_REQUEST_LOADS_PER_1000 2      // loads for each 1000 inhabitants of the province
#define MP_CAESAR_REQUEST_MIN_LOADS 10

// War (CESAR §7)
#define MP_CAESAR_TRIUMPH_LAURELS 200            // enemy legion destroyed in a just war
#define MP_CAESAR_EXPEDITION_DEFEATED_LAURELS 200
#define MP_CAESAR_UNJUST_WAR_LAURELS -150
#define MP_CAESAR_UNJUST_WAR_CAMPAIGN_LAURELS -400 // the target has legions away for Caesar
#define MP_CAESAR_BRUTAL_WAR_LAURELS -150          // on top of the motive
#define MP_CAESAR_HONOURABLE_WAR_NOTICE 3          // months between the declaration and the fighting
#define MP_CAESAR_REDECLARATION_DELAY 12           // a new war within it is without motive, wrath doubled

// Army too powerful (CESAR §7.5): one legion for each 5000 inhabitants, at least 2
#define MP_CAESAR_TOLERATED_LEGIONS_POPULATION 5000
#define MP_CAESAR_TOLERATED_LEGIONS_MIN 2
#define MP_CAESAR_EXTRA_LEGION_LAURELS -20        // every month, for each legion too many

// ---------- Wrath of Caesar (CESAR §7.2 to §7.4), in tenths of a point ----------

#define MP_CAESAR_WRATH_MAX 1000
#define MP_CAESAR_WRATH_WARNING 400
#define MP_CAESAR_WRATH_ULTIMATUM 700
#define MP_CAESAR_WRATH_EXPEDITION 1000
#define MP_CAESAR_WRATH_AFTER_EXPEDITION 300

// Every month of war: the n-th month adds n, legions L of the aggressor in enemy land add 2 L * L
#define MP_CAESAR_WRATH_MONTH 10
#define MP_CAESAR_WRATH_POWER 20
// Damage: a building destroyed, a caravan intercepted (none for the resource of a request), ten inhabitants killed
#define MP_CAESAR_WRATH_BUILDING 10
#define MP_CAESAR_WRATH_CARAVAN 20
#define MP_CAESAR_WRATH_TEN_KILLED 10

// Weight of the motive, in quarters: without motive, retaliation, mandate of Caesar; a brutal war, in halves
#define MP_CAESAR_WRATH_UNJUST_QUARTERS 4
#define MP_CAESAR_WRATH_RETALIATION_QUARTERS 2
#define MP_CAESAR_WRATH_MANDATE_QUARTERS 1
#define MP_CAESAR_WRATH_BRUTAL_HALVES 3

// Monthly decrease: in general peace, and while a war goes on somewhere
#define MP_CAESAR_WRATH_DECAY_PEACE 30
#define MP_CAESAR_WRATH_DECAY_WAR 10

#define MP_CAESAR_ULTIMATUM_DELAY 3              // months to make peace
#define MP_CAESAR_DISGRACE_MONTHS 12             // monthly laurels halved after an expedition
#define MP_CAESAR_CULPRIT_LOSS_PERCENT 25        // laurels lost by the main culprit of an expedition
#define MP_CAESAR_EXPEDITION_MONTHS 12           // at most
#define MP_CAESAR_EXPEDITION_SOLDIERS_INNOCENT 32
#define MP_CAESAR_EXPEDITION_SOLDIERS_CULPRIT_MIN 96
#define MP_CAESAR_EXPEDITION_SOLDIERS_CULPRIT_MAX 144

#endif // MP_CAESAR_RULES_H
