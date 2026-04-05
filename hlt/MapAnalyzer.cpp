/**
 * @file MapAnalyzer.cpp
 * @brief Moteur d'analyse spatiale (Influence, Ciblages, Dropoffs).
 */

#include "MapAnalyzer.hpp"
#include "constants.hpp"
#include <algorithm>

namespace bot {

	void MapAnalyzer::clear_claims() {
		m_claimed_targets.clear();
	}

	double MapAnalyzer::get_influence(const hlt::Position& ref_pos) const {
		return m_influence_engine.get_value(ref_pos.y, ref_pos.x);
	}

	/**
	 * @brief Génère une "Heatmap" tactique de la carte.
	 * Valorise les zones de regroupement ennemi (Bonus d'Inspiration)
	 * et pénalise les cases adjacentes aux ennemis (Risque de collision).
	 */
	void MapAnalyzer::compute_influence(hlt::Game& ref_game) {
		const int width = ref_game.game_map->width;
		const int height = ref_game.game_map->height;

		m_influence_engine.reset(width, height);

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				hlt::Position current_pos(x, y);
				int enemies_close_range = 0;
				int min_dist_to_enemy = 999;

				for (const auto& p_player : ref_game.players) {
					if (p_player->id == ref_game.my_id) continue;

					for (const auto& ship_pair : p_player->ships) {
						int dist = ref_game.game_map->calculate_distance(current_pos, ship_pair.second->position);
						if (dist <= 4) enemies_close_range++;
						if (dist < min_dist_to_enemy) min_dist_to_enemy = dist;
					}
				}

				// Attraction vers les zones à potentiel de bonus x3
				if (enemies_close_range >= 2) {
					int cell_halite = ref_game.game_map->at(current_pos)->halite;
					m_influence_engine.add_influence(x, y, cell_halite * hlt::constants::GENE_INSPIRATION_BONUS);
				}

				// Répulsion de survie immédiate
				if (min_dist_to_enemy == 0) {
					m_influence_engine.add_influence(x, y, -10000.0);
				}
				else if (min_dist_to_enemy == 1) {
					m_influence_engine.add_influence(x, y, -150.0);
				}
			}
		}
	}

	/**
	 * @brief Cible locale individuelle (Fallback si Bipartite échoue).
	 */
	hlt::Position MapAnalyzer::get_best_halite_target(const std::shared_ptr<hlt::Ship>& p_ship, hlt::GameMap& ref_game_map) {
		hlt::Position best_target = p_ship->position;
		double best_score = -999999.0;

		for (int y = 0; y < ref_game_map.height; ++y) {
			for (int x = 0; x < ref_game_map.width; ++x) {
				hlt::Position target(x, y);
				int halite = ref_game_map.at(target)->halite;
				if (halite < 50) continue;

				// Pénalité massive si la cible est déjà verrouillée par un allié
				double target_multiplier = (m_claimed_targets.find(target) != m_claimed_targets.end()) ? 0.05 : 1.0;

				int distance = ref_game_map.calculate_distance(p_ship->position, target);
				double influence = m_influence_engine.get_value(target.y, target.x);
				double score = ((static_cast<double>(halite) + influence) / (distance + 1)) * target_multiplier;

				if (score > best_score) {
					best_score = score;
					best_target = target;
				}
			}
		}

		m_claimed_targets.insert(best_target);
		return best_target;
	}

	/**
	 * @brief Extrait les N cellules les plus rentables du jeu pour l'assignation globale.
	 */
	std::vector<hlt::Position> MapAnalyzer::get_top_global_targets(int count, const hlt::GameMap& ref_game_map, const hlt::Game& ref_game) const {
		struct CellValue { hlt::Position pos; double value; };
		std::vector<CellValue> all_cells;
		all_cells.reserve(ref_game_map.height * ref_game_map.width);

		// Le seuil d'exigence baisse à mesure que la carte se vide
		int dynamic_threshold = std::max(10, 80 - (ref_game.turn_number / 5));

		for (int y = 0; y < ref_game_map.height; ++y) {
			for (int x = 0; x < ref_game_map.width; ++x) {
				int halite = ref_game_map.cells[y][x].halite;
				if (halite < dynamic_threshold) continue;

				double influence = m_influence_engine.get_value(y, x);
				all_cells.push_back({ hlt::Position(x, y), static_cast<double>(halite) + influence });
			}
		}

		std::sort(all_cells.begin(), all_cells.end(), [](const CellValue& a, const CellValue& b) { return a.value > b.value; });

		std::vector<hlt::Position> top_targets;
		int limit = std::min(count, static_cast<int>(all_cells.size()));
		top_targets.reserve(limit);

		for (int i = 0; i < limit; ++i) top_targets.push_back(all_cells[i].pos);

		return top_targets;
	}

	/**
	 * @brief Identifie la zone optimale pour construire une base avancée.
	 * Doit respecter la distance génétique minimale vis-à-vis des autres bases.
	 */
	std::pair<hlt::Position, bool> MapAnalyzer::find_best_dropoff_location(const std::shared_ptr<hlt::Player>& p_player, hlt::GameMap& ref_game_map) const {
		hlt::Position best_pos;
		double best_score = -1.0;
		bool found_good_spot = false;

		for (int y = 0; y < ref_game_map.height; ++y) {
			for (int x = 0; x < ref_game_map.width; ++x) {
				hlt::Position current(x, y);

				int dist_to_nearest = get_distance_to_nearest_base(current, p_player, ref_game_map);
				if (dist_to_nearest < hlt::constants::GENE_DROPOFF_DIST) continue;

				int area_wealth = get_area_wealth(current, 5, ref_game_map);
				double score = (static_cast<double>(area_wealth) * dist_to_nearest) / 1000.0;

				if (area_wealth > hlt::constants::GENE_DROPOFF_WEALTH && score > best_score) {
					best_score = score;
					best_pos = current;
					found_good_spot = true;
				}
			}
		}

		return { best_pos, found_good_spot };
	}

	int MapAnalyzer::get_distance_to_nearest_base(const hlt::Position& ref_pos, const std::shared_ptr<hlt::Player>& p_player, hlt::GameMap& ref_game_map) const {
		int min_dist = ref_game_map.calculate_distance(ref_pos, p_player->shipyard->position);
		for (const auto& dropoff_pair : p_player->dropoffs) {
			int dist = ref_game_map.calculate_distance(ref_pos, dropoff_pair.second->position);
			if (dist < min_dist) min_dist = dist;
		}
		return min_dist;
	}

	int MapAnalyzer::get_area_wealth(const hlt::Position& ref_center, int radius, hlt::GameMap& ref_game_map) const {
		int total_halite = 0;
		for (int dy = -radius; dy <= radius; ++dy) {
			for (int dx = -radius; dx <= radius; ++dx) {
				hlt::Position target = ref_game_map.normalize(hlt::Position(ref_center.x + dx, ref_center.y + dy));
				total_halite += ref_game_map.at(target)->halite;
			}
		}
		return total_halite;
	}

	int MapAnalyzer::get_total_map_halite(const hlt::GameMap& ref_game_map) const {
		int total = 0;
		for (int y = 0; y < ref_game_map.height; ++y) {
			for (int x = 0; x < ref_game_map.width; ++x) {
				total += ref_game_map.cells[y][x].halite;
			}
		}
		return total;
	}

} // namespace bot