/**
 * @file Pathfinder.hpp
 * @brief Algorithme de recherche de chemin A* universel, optimisé "Zero-Allocation".
 */
#pragma once
#include <vector>
#include <functional>
#include <queue>
#include <unordered_map>
#include <algorithm>

namespace utils {

	template<typename StateType>
	struct AStarNode {
		StateType state;
		int g_score;
		int f_score;

		// Tie-breaker : En cas d'égalité sur le coût total (f_score), on privilégie 
		// le nœud qui a déjà parcouru le plus de chemin (g_score élevé).
		// Cela empêche A* d'explorer inutilement en largeur.
		bool operator>(const AStarNode& ref_other) const {
			if (f_score == ref_other.f_score) return g_score < ref_other.g_score;
			return f_score > ref_other.f_score;
		}
	};

	/**
	 * @brief Trouve le chemin optimal via A* avec injection de dépendances.
	 * * @details
	 * POURQUOI UTILISER std::function ET DES LAMBDAS ?
	 * Cet algorithme est "agnostique" : il ne connaît ni les règles de Halite, ni la carte.
	 * Il délègue l'évaluation du terrain au code appelant via ces fonctions :
	 * * 1. L'Heuristique (get_heuristic) : Permet de changer la façon dont la distance est
	 * estimée (ex: distance de Manhattan) sans toucher au cœur de l'algorithme.
	 * * 2. Le Callback "Zéro Allocation" (get_neighbors) :
	 * Habituellement, une fonction `get_neighbors` retourne un `std::vector<Voisins>`.
	 * Cela force la création et la destruction d'un tableau à chaque case explorée.
	 * Ici, `get_neighbors` prend une autre fonction en paramètre. Dès qu'elle trouve un
	 * voisin valide, elle l'injecte DIRECTEMENT dans le moteur A*, sans tableau intermédiaire.
	 */
	template<typename StateType, typename HashFunc = std::hash<StateType>>
	std::vector<StateType> find_path(
		const StateType& ref_start,
		const StateType& ref_goal,
		std::function<void(const StateType&, std::function<void(const StateType&, int)>)> get_neighbors,
		std::function<int(const StateType&, const StateType&)> get_heuristic)
	{
		// thread_local conserve l'optimisation (ne pas réallouer la map à chaque appel)
		// tout en empêchant les crashs (Race Conditions) si le bot multithread ses déplacements.
		// assurément très overkill. Motivé par le multithreading du généticien.
		thread_local std::unordered_map<StateType, StateType, HashFunc> s_came_from;
		thread_local std::unordered_map<StateType, int, HashFunc> s_g_score;

		s_came_from.clear();
		s_g_score.clear();

		std::priority_queue<AStarNode<StateType>, std::vector<AStarNode<StateType>>, std::greater<AStarNode<StateType>>> open_set;

		open_set.push({ ref_start, 0, get_heuristic(ref_start, ref_goal) });
		s_g_score[ref_start] = 0;

		while (!open_set.empty()) {
			StateType current = open_set.top().state;
			open_set.pop();

			if (current == ref_goal) break;

			// Appel de la lambda d'exploration.
			// Pour chaque voisin valide trouvé, la lambda interne ci-dessous est exécutée.
			get_neighbors(current, [&](const StateType& next, int move_cost) {
				int tentative_g_score = s_g_score[current] + move_cost;

				if (s_g_score.find(next) == s_g_score.end() || tentative_g_score < s_g_score[next]) {
					s_came_from[next] = current;
					s_g_score[next] = tentative_g_score;
					int f_score = tentative_g_score + get_heuristic(next, ref_goal);
					open_set.push({ next, tentative_g_score, f_score });
				}
				});
		}

		std::vector<StateType> path;

		// Cible inaccessible (bloquée par d'autres vaisseaux ou murs)
		if (s_came_from.find(ref_goal) == s_came_from.end()) return path;

		// Reconstruction du chemin à l'envers
		StateType current_trace = ref_goal;
		while (current_trace != ref_start) {
			path.push_back(current_trace);
			current_trace = s_came_from[current_trace];
		}

		std::reverse(path.begin(), path.end());
		return path;
	}
}