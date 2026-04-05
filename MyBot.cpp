/**
 * @file MyBot.cpp
 * @brief Point d'entrée de l'IA YuNey_v2 pour Halite III.
 * @details Implémente une architecture par Machine à États, assignation globale (Bipartite),
 * navigation Flow Field et sécurité stricte anti-collision.
 */

#include "game.hpp"
#include "navigator.hpp"
#include "MapAnalyzer.hpp"
#include "log.hpp"
#include "utils/math.hpp"

#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>

enum class ShipState { MINING, RETURNING, ENDGAME, BLOCKING };

int main(int argc, char* argv[]) {
	hlt::Game game;
	game.ready("YuNey");

	// Chargement des paramètres génétiques via arguments CLI
	if (argc >= 16) {
		hlt::constants::GENE_DROPOFF_WEALTH = std::atoi(argv[1]);
		hlt::constants::GENE_DROPOFF_DIST = std::atoi(argv[2]);
		hlt::constants::GENE_SPAWN_WEALTH_RATIO = std::atoi(argv[3]);
		hlt::constants::GENE_KAMIKAZE_PROFIT = std::atoi(argv[4]);
		hlt::constants::GENE_ENDGAME_TURN_OFFSET = std::atoi(argv[5]) / 10.0;
		hlt::constants::GENE_STOP_SPAWN_TURN = std::atoi(argv[6]);
		hlt::constants::GENE_KAMIKAZE_MIN_DANGER = std::atoi(argv[7]);
		hlt::constants::GENE_MINING_THRESHOLD_MIN = std::atoi(argv[8]);
		hlt::constants::GENE_MINING_THRESHOLD_MAX = std::atoi(argv[9]);
		hlt::constants::GENE_INSPIRATION_BONUS = std::atoi(argv[10]) / 10.0;
		hlt::constants::GENE_SPAWN_MAX_SHIPS_RATIO = std::atoi(argv[11]);
		hlt::constants::GENE_ECO_ABANDON_PENALTY = std::atoi(argv[12]);
		hlt::constants::GENE_RETURN_THRESHOLD = std::atoi(argv[13]);
		hlt::constants::GENE_CONGESTION_PENALTY = std::atoi(argv[14]);
		hlt::constants::GENE_DROPOFF_MIN_SHIPS = std::atoi(argv[15]);
	}

	bot::Navigator navigator;
	bot::MapAnalyzer map_analyzer;
	std::unordered_map<hlt::EntityId, ShipState> m_ship_states;

	for (;;) {
		game.update_frame();
		auto me = game.me;
		auto& world = game.game_map;

		std::vector<hlt::Command> command_queue;
		command_queue.reserve(me->ships.size() + 1);

		// Baisse progressive de l'exigence de minage au fil du temps
		int dynamic_threshold = std::max(
			hlt::constants::GENE_MINING_THRESHOLD_MIN,
			hlt::constants::GENE_MINING_THRESHOLD_MAX - (game.turn_number / 5)
		);

		navigator.clear_reservations();
		map_analyzer.clear_claims();
		map_analyzer.compute_influence(game);
		navigator.compute_return_flow_field(me, *world, map_analyzer);

		std::vector<std::shared_ptr<hlt::Ship>> all_ships;
		for (const auto& ship_pair : me->ships) {
			// Réservation préventive pour figer la carte avant les déplacements
			navigator.reserve_position(ship_pair.second->position);
			all_ships.push_back(ship_pair.second);
		}

		// --- 1. PLANIFICATION DE FIN DE PARTIE ---
		// Allocation des créneaux de retour pour éviter l'embouteillage final
		std::unordered_map<hlt::EntityId, int> m_scheduled_arrival_turn;
		int endgame_trigger = hlt::constants::MAX_TURNS - static_cast<int>(world->width * hlt::constants::GENE_ENDGAME_TURN_OFFSET);

		if (game.turn_number > endgame_trigger) {
			std::vector<bool> slots(hlt::constants::MAX_TURNS + 1, false);
			std::vector<std::shared_ptr<hlt::Ship>> endgame_ships = all_ships;

			std::sort(endgame_ships.begin(), endgame_ships.end(), [&](const auto& a, const auto& b) {
				return world->calculate_distance(a->position, me->shipyard->position) < world->calculate_distance(b->position, me->shipyard->position);
				});

			for (const auto& p_ship : endgame_ships) {
				int arrival = hlt::constants::MAX_TURNS;
				while (arrival > 0 && slots[arrival]) arrival--;
				if (arrival > 0) {
					slots[arrival] = true;
					m_scheduled_arrival_turn[p_ship->id] = arrival;
				}
			}
		}

		// --- 2. FSM ---
		for (const auto& p_ship : all_ships) {
			hlt::EntityId id = p_ship->id;

			// Calcul de la distance vers la base la plus proche (Shipyard ou Dropoff)
			int t_time = world->calculate_distance(p_ship->position, me->shipyard->position);
			for (const auto& drop : me->dropoffs) {
				t_time = std::min(t_time, world->calculate_distance(p_ship->position, drop.second->position));
			}

			if (m_scheduled_arrival_turn.count(id) && game.turn_number >= m_scheduled_arrival_turn[id] - t_time) {
				double local_danger = map_analyzer.get_influence(p_ship->position);
				m_ship_states[id] = (p_ship->halite < 300 && local_danger < -hlt::constants::GENE_KAMIKAZE_MIN_DANGER) ? ShipState::BLOCKING : ShipState::ENDGAME;
			}
			else if (m_ship_states.find(id) == m_ship_states.end()) m_ship_states[id] = ShipState::MINING;
			else if (m_ship_states[id] == ShipState::MINING && p_ship->halite >= hlt::constants::GENE_RETURN_THRESHOLD) m_ship_states[id] = ShipState::RETURNING;
			else if (m_ship_states[id] == ShipState::RETURNING && t_time == 0) m_ship_states[id] = ShipState::MINING;
		}

		// --- 3. ORDONNANCEMENT ---
		// Les vaisseaux rentrant (RETURNING/ENDGAME) se déplacent en premier pour libérer les passages
		std::vector<std::shared_ptr<hlt::Ship>> sorted_ships = all_ships;
		std::sort(sorted_ships.begin(), sorted_ships.end(), [&](const auto& a, const auto& b) {
			auto prio = [](ShipState s) { return (s == ShipState::RETURNING) ? 1 : (s == ShipState::ENDGAME ? 2 : (s == ShipState::BLOCKING ? 3 : 4)); };
			int pa = prio(m_ship_states[a->id]), pb = prio(m_ship_states[b->id]);

			if (pa != pb) return pa < pb;
			if (pa == 1 || pa == 2) return world->calculate_distance(a->position, me->shipyard->position) < world->calculate_distance(b->position, me->shipyard->position);
			return a->halite > b->halite;
			});

		// --- 4.MINAGE ---
		std::vector<utils::Edge<hlt::EntityId, hlt::Position>> all_edges;
		std::vector<std::shared_ptr<hlt::Ship>> mining_ships;
		std::unordered_map<hlt::EntityId, hlt::Position> local_assignments;

		for (const auto& p_ship : sorted_ships) {
			int cost = world->at(p_ship->position)->halite / 10;
			if (m_ship_states[p_ship->id] == ShipState::MINING && p_ship->halite >= cost) {
				int h = world->at(p_ship->position)->halite;
				if (h <= dynamic_threshold) {
					mining_ships.push_back(p_ship);
				}
				else {
					// Éco-abandon : vérification des cases adjacentes plus riches
					int best_adj = 0; hlt::Position best_pos = p_ship->position;
					for (const auto& dir : hlt::ALL_CARDINALS) {
						hlt::Position adj = world->normalize(p_ship->position.directional_offset(dir));
						if (world->at(adj)->halite > best_adj) { best_adj = world->at(adj)->halite; best_pos = adj; }
					}
					if ((h * 0.25) < (best_adj * 0.25) - cost - hlt::constants::GENE_ECO_ABANDON_PENALTY) {
						local_assignments[p_ship->id] = best_pos;
					}
				}
			}
		}

		// Complétion des cibles globales via algorithme Bipartite
		auto targets = map_analyzer.get_top_global_targets(mining_ships.size() * 2, *world, game);
		for (const auto& s : mining_ships) {
			for (const auto& t : targets) {
				int d = world->calculate_distance(s->position, t);
				double score = (world->at(t)->halite + map_analyzer.get_influence(t)) / ((d * d) + 1.0);
				all_edges.push_back({ s->id, t, score });
			}
		}
		auto final_assignments = utils::solve_greedy_assignment(all_edges);

		// --- 4.5. LOGIQUE DROPOFF ---
		hlt::Position target_dropoff_pos;
		bool need_dropoff = false;
		hlt::EntityId builder_id = -1;

		auto dropoff_info = map_analyzer.find_best_dropoff_location(me, *world);
		bool can_afford = me->halite >= hlt::constants::DROPOFF_COST + 1000;
		bool enough_ships = me->ships.size() >= static_cast<size_t>(std::max(1, hlt::constants::GENE_DROPOFF_MIN_SHIPS));
		bool enough_time = game.turn_number < (hlt::constants::MAX_TURNS - 150);

		if (can_afford && enough_ships && enough_time && dropoff_info.second) {
			need_dropoff = true;
			target_dropoff_pos = dropoff_info.first;
			int min_d = 9999;
			for (const auto& s : sorted_ships) {
				if (m_ship_states[s->id] == ShipState::ENDGAME) continue;
				int d = world->calculate_distance(s->position, target_dropoff_pos);
				if (d < min_d) { min_d = d; builder_id = s->id; }
			}
		}

		// --- 5. EXECUTION DES MOUVEMENTS ---
		std::unordered_set<hlt::EntityId> has_moved;

		for (const auto& p_ship : sorted_ships) {
			hlt::EntityId id = p_ship->id;
			if (has_moved.count(id)) continue;

			navigator.unreserve_position(p_ship->position);
			hlt::Direction next_dir = hlt::Direction::STILL;
			int cost = world->at(p_ship->position)->halite / 10;

			hlt::Position target_base = me->shipyard->position;
			int d_base = world->calculate_distance(p_ship->position, target_base);
			for (const auto& d : me->dropoffs) {
				if (world->calculate_distance(p_ship->position, d.second->position) < d_base) {
					d_base = world->calculate_distance(p_ship->position, d.second->position);
					target_base = d.second->position;
				}
			}

			if (need_dropoff && id == builder_id) {
				if (p_ship->position == target_dropoff_pos) {
					command_queue.push_back(p_ship->make_dropoff());
					has_moved.insert(id);
					navigator.stay_still(p_ship);
					me->halite -= hlt::constants::DROPOFF_COST;
					need_dropoff = false;
					continue;
				}
				else {
					next_dir = navigator.navigate_to(p_ship, target_dropoff_pos, *world, map_analyzer);
				}
			}
			else if (p_ship->halite < cost) {
				next_dir = hlt::Direction::STILL;
			}
			else if (m_ship_states[id] == ShipState::ENDGAME) {
				if (d_base <= 1 && p_ship->position != target_base) {
					next_dir = world->get_unsafe_moves(p_ship->position, target_base)[0];
				}
				else {
					next_dir = navigator.navigate_to(p_ship, target_base, *world, map_analyzer);
				}
			}
			else if (m_ship_states[id] == ShipState::BLOCKING) {
				next_dir = hlt::Direction::STILL;
			}
			else if (m_ship_states[id] == ShipState::RETURNING) {
				next_dir = navigator.get_return_direction(p_ship->position);
			}
			else if (m_ship_states[id] == ShipState::MINING) {
				if (local_assignments.count(id)) {
					next_dir = navigator.navigate_to(p_ship, local_assignments[id], *world, map_analyzer);
				}
				else if (world->at(p_ship)->halite > dynamic_threshold) {
					next_dir = hlt::Direction::STILL;
				}
				else if (final_assignments.count(id)) {
					next_dir = navigator.navigate_to(p_ship, final_assignments[id], *world, map_analyzer);
				}
			}

			// Gestion de la sécurité anti-collision stricte
			hlt::Position next_pos = world->normalize(p_ship->position.directional_offset(next_dir));

			bool is_target_my_base = (next_pos == me->shipyard->position);
			for (const auto& drop : me->dropoffs) {
				if (next_pos == drop.second->position) is_target_my_base = true;
			}

			// Autorisation d'écraser la réservation si fin de partie sur une base alliée
			bool is_kamikaze = (m_ship_states[id] == ShipState::ENDGAME && is_target_my_base);

			if (!is_kamikaze && next_dir != hlt::Direction::STILL && navigator.is_reserved(next_pos)) {
				next_dir = hlt::Direction::STILL;
				next_pos = p_ship->position;
			}

			navigator.reserve_position(next_pos);
			command_queue.push_back(p_ship->move(next_dir));
			has_moved.insert(id);
		}

		// --- 6.SPAWNING ---
		bool safe = !navigator.is_reserved(me->shipyard->position);
		size_t max_ships = static_cast<size_t>((world->width * world->height) / std::max(1.0f, hlt::constants::GENE_SPAWN_MAX_SHIPS_RATIO));
		bool rich = map_analyzer.get_total_map_halite(*world) > (me->ships.size() * hlt::constants::GENE_SPAWN_WEALTH_RATIO);

		if (game.turn_number < hlt::constants::MAX_TURNS - hlt::constants::GENE_STOP_SPAWN_TURN &&
			me->ships.size() < max_ships && rich &&
			me->halite >= hlt::constants::SHIP_COST && safe) {

			command_queue.push_back(me->shipyard->spawn());
			navigator.reserve_position(me->shipyard->position);
		}

		if (!game.end_turn(command_queue)) break;
	}
	return 0;
}