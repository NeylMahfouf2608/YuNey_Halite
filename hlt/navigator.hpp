/**
 * @file navigator.hpp
 * @brief Déclaration de la classe Navigator gérant le pathfinding et la prévention des collisions.
 */
#pragma once

#include "game_map.hpp"
#include "ship.hpp"
#include <unordered_set>
#include <vector>
#include "player.hpp"
#include "MapAnalyzer.hpp"
#include <memory>
#include "utils/pathfinder.hpp"

namespace bot {

	/**
	 * @class Navigator
	 * @brief Moteur de déplacement utilisant A* pour le ciblage et un Flow Field pour les retours.
	 */
	class Navigator {
	private:
		/// @brief Tableau global retenant les positions verrouillées pour le tour actuel.
		std::unordered_set<hlt::Position> m_reserved_positions;

		/// @brief Matrice directionnelle pointant vers la base la plus proche depuis n'importe quelle case.
		std::vector<std::vector<hlt::Direction>> m_return_flow_field;

		/**
		 * @brief Heuristique de distance de Manhattan pour l'algorithme A*.
		 * @param ref_a Position de départ.
		 * @param ref_b Position d'arrivée.
		 * @param ref_game_map Référence constante à la carte pour le wraparound (torique).
		 * @return La distance minimale calculée.
		 */
		int heuristic(const hlt::Position& ref_a, const hlt::Position& ref_b, hlt::GameMap& ref_game_map) const;

	public:
		Navigator() = default;

		/**
		 * @brief Vérifie si une position est déjà réservée par un autre vaisseau allié.
		 * @param ref_pos La position à tester.
		 * @return true si la case est réservée, false sinon.
		 */
		bool is_reserved(const hlt::Position& ref_pos) const {
			return m_reserved_positions.find(ref_pos) != m_reserved_positions.end();
		}

		/// @brief Nettoie l'ensemble des réservations (à appeler au début de chaque tour).
		void clear_reservations();

		/**
		 * @brief Calcule le meilleur mouvement sécurisé vers une cible (Algorithme A*).
		 * @param p_ship Pointeur constant vers le vaisseau à déplacer.
		 * @param ref_destination La position cible à atteindre.
		 * @param ref_game_map Référence à la carte du jeu.
		 * @param ref_analyzer Référence à l'analyseur pour lire les dangers (Influence).
		 * @return La direction hlt::Direction optimale.
		 */
		hlt::Direction navigate_to(const std::shared_ptr<hlt::Ship>& p_ship, const hlt::Position& ref_destination, hlt::GameMap& ref_game_map, const MapAnalyzer& ref_analyzer);

		/**
		 * @brief Ordonne à un vaisseau de rester immobile en réservant sa case actuelle.
		 * @param p_ship Pointeur constant vers le vaisseau.
		 */
		void stay_still(const std::shared_ptr<hlt::Ship>& p_ship);

		/**
		 * @brief Calcule la carte des flux de retour pour toute la flotte (Dijkstra Multi-Sources).
		 * @param p_player Pointeur constant vers le joueur actuel (pour lire ses bases).
		 * @param ref_game_map Référence à la carte du jeu.
		 * @param ref_analyzer Référence à l'analyseur de la carte.
		 */
		void compute_return_flow_field(const std::shared_ptr<hlt::Player>& p_player, hlt::GameMap& ref_game_map, const MapAnalyzer& ref_analyzer);

		/**
		 * @brief Verrouille manuellement une position sur la carte.
		 * @param ref_pos La position à réserver.
		 */
		void reserve_position(const hlt::Position& ref_pos) {
			m_reserved_positions.insert(ref_pos);
		}

		/**
		 * @brief Libère une position préalablement réservée (utilisé pour les swaps).
		 * @param ref_pos La position à déverrouiller.
		 */
		void unreserve_position(const hlt::Position& ref_pos) {
			m_reserved_positions.erase(ref_pos);
		}

		/**
		 * @brief Récupère la direction de retour précalculée pour une case spécifique.
		 * @param ref_pos La position où se trouve le vaisseau.
		 * @return La direction optimale pour rentrer à la base.
		 */
		hlt::Direction get_return_direction(const hlt::Position& ref_pos) const {
			return m_return_flow_field[ref_pos.y][ref_pos.x];
		}
	};
}