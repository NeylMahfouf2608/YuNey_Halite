/**
 * @file navigator.cpp
 * @brief Implémentation du pathfinding A* et du Flow Field (Dijkstra multi-sources).
 */

#include "navigator.hpp"
#include <queue>

namespace bot {

	/**
	 * @brief Nœud pour la file de priorité A*.
	 * @details Le tie-breaker sur g_score privilégie les chemins qui ont déjà
	 * parcouru le plus de distance en cas d'égalité sur f_score, évitant l'exploration de zigzag.
	 */
	struct AStarNode {
		hlt::Position pos;
		int g_score;
		int f_score;

		bool operator>(const AStarNode& ref_other) const {
			if (f_score == ref_other.f_score) {
				return g_score < ref_other.g_score;
			}
			return f_score > ref_other.f_score;
		}
	};

	void Navigator::clear_reservations() {
		m_reserved_positions.clear();
	}

	void Navigator::stay_still(const std::shared_ptr<hlt::Ship>& p_ship) {
		m_reserved_positions.insert(p_ship->position);
	}

	/**
	 * @brief Génère une carte de directions (Flow Field) vers la base la plus proche.
	 * @details Utilise un Dijkstra inversé (depuis les bases vers la carte).
	 */
	void Navigator::compute_return_flow_field(const std::shared_ptr<hlt::Player>& p_player, hlt::GameMap& ref_game_map, const MapAnalyzer& ref_analyzer) {
		const int width = ref_game_map.width;
		const int height = ref_game_map.height;

		static std::vector<std::vector<int>> s_dist_map;

		// Réutilisation de la mémoire allouée (Optimisation industrielle)
		if (m_return_flow_field.empty() || m_return_flow_field.size() != static_cast<size_t>(height)) {
			m_return_flow_field.assign(height, std::vector<hlt::Direction>(width, hlt::Direction::STILL));
			s_dist_map.assign(height, std::vector<int>(width, 999999));
		}
		else {
			for (int y = 0; y < height; ++y) {
				std::fill(m_return_flow_field[y].begin(), m_return_flow_field[y].end(), hlt::Direction::STILL);
				std::fill(s_dist_map[y].begin(), s_dist_map[y].end(), 999999);
			}
		}

		std::priority_queue<AStarNode, std::vector<AStarNode>, std::greater<AStarNode>> pq;

		auto add_source = [&](const hlt::Position& ref_pos) {
			s_dist_map[ref_pos.y][ref_pos.x] = 0;
			pq.push({ ref_pos, 0, 0 });
			};

		// Les cibles du Dijkstra inversé sont le Shipyard et tous les Dropoffs
		add_source(p_player->shipyard->position);
		for (const auto& dropoff_pair : p_player->dropoffs) {
			add_source(dropoff_pair.second->position);
		}

		// --- PROPAGATION DE L'ONDE ---
		while (!pq.empty()) {
			AStarNode current_node = pq.top();
			pq.pop();

			hlt::Position current_pos = current_node.pos;
			int current_dist = current_node.g_score;

			if (current_dist > s_dist_map[current_pos.y][current_pos.x]) continue;

			for (const auto& dir : hlt::ALL_CARDINALS) {
				hlt::Position next_pos = ref_game_map.normalize(current_pos.directional_offset(dir));
				hlt::Direction return_dir = hlt::invert_direction(dir);

				// ÉCO-CONDUITE INVERSÉE : 
				// Coût appliqué sur la case qu'on s'apprête à quitter (next_pos dans la timeline réelle)
				int ship_move_cost = ref_game_map.at(next_pos)->halite / 10;
				int base_cost = ship_move_cost + 1;

				double influence = ref_analyzer.get_influence(next_pos);
				int danger_penalty = (influence < 0) ? static_cast<int>(-influence) : 0;

				// Application du gène de congestion près des bases
				int congestion_penalty = 0;
				int dist_to_base = ref_analyzer.get_distance_to_nearest_base(next_pos, p_player, ref_game_map);
				if (dist_to_base > 0 && dist_to_base <= 3) {
					congestion_penalty = (4 - dist_to_base) * hlt::constants::GENE_CONGESTION_PENALTY;
				}

				int new_dist = current_dist + base_cost + danger_penalty + congestion_penalty;

				if (new_dist < s_dist_map[next_pos.y][next_pos.x]) {
					s_dist_map[next_pos.y][next_pos.x] = new_dist;
					m_return_flow_field[next_pos.y][next_pos.x] = return_dir;
					pq.push({ next_pos, new_dist, new_dist });
				}
			}
		}
	}

	int Navigator::heuristic(const hlt::Position& ref_a, const hlt::Position& ref_b, hlt::GameMap& ref_game_map) const {
		return ref_game_map.calculate_distance(ref_a, ref_b);
	}

	/**
	 * @brief Pathfinding A* dynamique pour la navigation offensive ou de minage.
	 */
	hlt::Direction Navigator::navigate_to(const std::shared_ptr<hlt::Ship>& p_ship, const hlt::Position& ref_destination, hlt::GameMap& ref_game_map, const MapAnalyzer& ref_analyzer) {
		if (p_ship->position == ref_destination) {
			stay_still(p_ship);
			return hlt::Direction::STILL;
		}

		int cost_to_move = ref_game_map.at(p_ship->position)->halite / 10;
		if (p_ship->halite < cost_to_move) {
			stay_still(p_ship);
			return hlt::Direction::STILL;
		}

		auto heuristic_lambda = [&ref_game_map](const hlt::Position& a, const hlt::Position& b) {
			return ref_game_map.calculate_distance(a, b);
			};

		// --- DEFINITION DES VOISINS ET COUTS ---
		// Utilisation d'un callback pour éviter l'allocation de vecteurs dynamiques dans la boucle A*
		auto neighbors_lambda = [&](const hlt::Position& current_ref, std::function<void(const hlt::Position&, int)> add_neighbor) {

			// Copie locale requise par le starter_kit
			hlt::Position current = current_ref;

			int cost_to_leave = ref_game_map.at(current)->halite / 10;

			for (const auto& next_pos_raw : current.get_surrounding_cardinals()) {
				hlt::Position next_pos = ref_game_map.normalize(next_pos_raw);

				if (m_reserved_positions.find(next_pos) != m_reserved_positions.end() && next_pos != ref_destination) continue;

				int base_cost = cost_to_leave + 1;
				hlt::MapCell* p_target_cell = ref_game_map.at(next_pos);

				// Impossible de traverser un ennemi
				if (p_target_cell->is_occupied() && p_target_cell->ship->owner != p_ship->owner && next_pos != ref_destination) {
					continue;
				}

				// Fort malus pour les cases occupées par des alliés (Swapping découragé)
				if (p_target_cell->is_occupied() && p_target_cell->ship->owner == p_ship->owner && next_pos != ref_destination) {
					base_cost += 1000;
				}

				double influence = ref_analyzer.get_influence(next_pos);
				double danger_penalty = (influence < 0) ? -influence : 0.0;

				int total_cost = base_cost + static_cast<int>(danger_penalty);

				// Envoi direct du voisin valide à A*
				add_neighbor(next_pos, total_cost);
			}
			};

		std::vector<hlt::Position> path = utils::find_path<hlt::Position>(
			p_ship->position,
			ref_destination,
			neighbors_lambda,
			heuristic_lambda
		);

		if (path.empty()) {
			stay_still(p_ship);
			return hlt::Direction::STILL;
		}

		// Traduction de la prochaine position en direction
		hlt::Position next_step = path.front();
		hlt::Direction move_dir = hlt::Direction::STILL;

		for (const auto& dir : hlt::ALL_CARDINALS) {
			if (ref_game_map.normalize(p_ship->position.directional_offset(dir)) == next_step) {
				move_dir = dir;
				break;
			}
		}

		return move_dir;
	}
} // namespace bot