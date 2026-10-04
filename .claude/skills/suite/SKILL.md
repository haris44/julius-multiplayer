---
name: suite
description: Reprend le projet Caesar III multijoueur là où il en est - lit la roadmap et le journal, enchaîne les tâches du jalon en cours avec leurs tests, met à jour la roadmap et le journal, commit. À utiliser quand Alexandre dit « suite », « continue », « tâche suivante » ou « on avance ».
---

# Reprendre le projet

1. **Contexte** : lire `CLAUDE.md` (règles), `doc/mp/ROADMAP.md` (jalon en cours) et la dernière entrée de `doc/mp/JOURNAL.md`. Si Alexandre a précisé une tâche ou une priorité, elle passe en premier.
2. **État** : `git status`, `git log --oneline -5`, `df -h .`, puis `tools/check.sh`. S'il est rouge, réparer d'abord : on ne construit jamais sur un état cassé.
3. **Choix** : prendre la première tâche non cochée du jalon en cours. Relire la fiche `doc/mp/code-map/` du module concerné et la section correspondante de `doc/mp/DESIGN.md`.
4. **Test d'abord** : écrire le test qui prouvera la tâche (ctest, rejeu, script `--automation`), puis implémenter par petites étapes, avec `tools/check.sh` après chaque étape.
5. **Clôture de tâche** : commit (message en anglais, préfixé par l'ID de tâche, ex. `M1.3: ...`), cocher la tâche dans `ROADMAP.md`. Une découverte qui change la conception va dans `DECISIONS.md` (et DESIGN.md si besoin), pas seulement dans le code.
6. **Enchaîner** les tâches tant que le jalon n'est pas fini et que rien ne bloque. S'arrêter pour demander à Alexandre **uniquement** quand une décision lui revient (gameplay, hypothèses H* de `VISION.md`).
7. **Fin de session** : ajouter une entrée en tête de `JOURNAL.md` (fait / appris / prochaine étape / points ouverts), commit, puis un résumé en français pour Alexandre : ce qui est fait, comment c'est testé, ce qui vient ensuite.
