/**
 * @file MathUtils.hpp
 * @brief Algorithmes mathématiques génériques et réutilisables.
 */

#pragma once
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace utils {

	/**
	 * @struct Edge
	 * @brief Représente un lien pondéré entre une entité et une cible.
	 * @tparam EntityType Le type d'identifiant de l'entité (ex: int, hlt::EntityId).
	 * @tparam TargetType Le type d'identifiant de la cible (ex: hlt::Position).
	 */
	template <typename EntityType, typename TargetType>
	struct Edge {
		EntityType source_id;
		TargetType target;
		double score;
	};

	/**
	 * @brief Résout le problème d'affectation biparti (Bipartite Matching) de manière gloutonne optimisée.
	 * @param ref_edges Liste de tous les liens (Edges) possibles entre entités et cibles.
	 * @return Une table de hachage associant chaque entité à sa meilleure cible disponible.
	 * @note Complexité : O(N log N) dominée par le tri des arêtes.
	 */
	template <typename EntityType, typename TargetType>
	std::unordered_map<EntityType, TargetType> solve_greedy_assignment(std::vector<Edge<EntityType, TargetType>>& ref_edges) {

		// Tri décroissant par score
		std::sort(ref_edges.begin(), ref_edges.end(), [](const Edge<EntityType, TargetType>& a, const Edge<EntityType, TargetType>& b) {
			return a.score > b.score;
			});

		std::unordered_map<EntityType, TargetType> final_assignments;
		std::unordered_set<TargetType> taken_targets;
		std::unordered_set<EntityType> assigned_entities;

		for (const auto& edge : ref_edges) {
			// Si l'entité n'a pas encore de mission ET que la cible est libre
			if (assigned_entities.find(edge.source_id) == assigned_entities.end() &&
				taken_targets.find(edge.target) == taken_targets.end()) {

				final_assignments[edge.source_id] = edge.target;
				assigned_entities.insert(edge.source_id);
				taken_targets.insert(edge.target);
			}
		}
		return final_assignments;
	}
}