YuNey (Yuna/Neyl)

YuNey est un bot Halite III développé dans le cadre d'un TP d'Intelligence Artificielle pour l'ENJMIN.

🛠 Stack Technique
Langage Principal : C++17.
Script d'analyse : Python 3 (Télémétrie et décompression de replays Zstandard).

Outils : 
- **Doxygen** pour la documentation technique.
- **Genetic Engine** intégré pour le fine-tuning des paramètres.
- **Analyzer custom** pour lire les replays et en faire une analyse custom.

🧠 Architecture et Algorithmes
Le bot repose sur une séparation stricte des responsabilités :

1. Prise de Décision (FSM & Bipartite Matching)
Chaque vaisseau est piloté par une Machine à États Finis:

- **MINING** : Recherche de ressources et récolte.
- **RETURNING** : Retour à la base la plus proche via Flow Field.
- **ENDGAME** : Procédure de crash final pour sécuriser le Halite.
- **BLOCKING** : Harcèlement tactique des structures ennemies.

L'assignation des tâches de minage utilise un Greedy Bipartite Matching. Au lieu que chaque vaisseau choisisse la case la plus proche, le bot calcule un score global pour maximiser la rentabilité de toute la flotte et éviter que deux vaisseaux ne ciblent le même gisement.

2. Navigation et Pathfinding
- Flow Field (Dijkstra multi-sources) : Utilisé pour le retour aux bases. Il génère une "carte de courants" permettant à tous les vaisseaux de rentrer de manière fluide, en intégrant des pénalités de congestion près du Shipyard.
- A* : Utilisé pour la navigation de précision. Implémente une optimisation "Zero-Allocation" via des callbacks pour minimiser les accès à la mémoire.

3. Analyse Spatiale (Influence Maps)
Le MapAnalyzer génère des cartes de chaleur (Heatmaps) :
- Inspiration : Attire les vaisseaux là où le bonus x3 de Halite est possible (proximité ennemie).
- Danger : Répulsion immédiate autour des vaisseaux ennemis pour éviter les abordages non désirés.

🚀 Optimisations Majeures
- Mouvement "Pare-balles" : Système de réservation de position à double passe. Un vaisseau ne libère sa position actuelle que si sa position future est garantie libre. Cela élimine 100% des collisions alliées.
- Gestion Mémoire : Utilisation intensive de static et thread_local pour les conteneurs de pathfinding afin d'éviter la fragmentation de la mémoire pendant les 400 tours.
- Kamikaze Tactique : En phase finale (ENDGAME), le bot désactive la détection de collisions alliées sur ses propres bases, permettant un dépôt massif et simultané de ressources.

🧬 Algorithme Génétique (Training)
Le comportement de YuNey est piloté par une quinzaine de gène (paramètres). Plutôt que de régler les seuils à la main, un moteur génétique multithreadé fait s'affronter des centaines de versions du bot. 
(Les résultats sont mitigés)

Gènes clés : Seuil de retour, ratio de densité de flotte, agressivité de l'éco-abandon, pénalité de congestion.

Spécialisation : Le moteur est configuré pour "Overfitter" la carte 32x32, garantissant une efficacité maximale sur ce format spécifique.

📊 Outils de Diagnostic
Télémétrie (analyzer.py)
Ce script extrait les données des replays (.hlt) pour générer un fichier telemetry.csv. Il permet de tracer :
- Les courbes de banque vs cargo.
- Les pics de collisions (Accidents vs Combats).
- L'efficacité du spawning par rapport à la richesse de la carte.


Précisions concernant l'usage de LLM : Le README.md et le script Analyzer.py ont été écrit par Gemini
