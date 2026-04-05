/**
 * @file MapAnalyzer.hpp
 * @brief Déclaration de la classe MapAnalyzer gérant l'analyse spatiale et l'évaluation des cibles.
 */
#pragma once

#include "game.hpp" 
#include "game_map.hpp"
#include "ship.hpp"
#include <memory>
#include <unordered_set>
#include <vector> 
#include "utils/influence_grid.hpp"

namespace bot {

	/**
	 * @class MapAnalyzer
	 * @brief Outil d'analyse stratégique de la carte (Influence spatiale, richesse, danger).
	 */
	class MapAnalyzer {
	private:
		/// @brief Ensemble des cibles déjà assignées (pour éviter la sur-concentration).
		std::unordered_set<hlt::Position> m_claimed_targets;

		/// @brief Carte d'influence globale (grille 2D) stockant les valeurs de danger et de bonus.
		utils::InfluenceGrid m_influence_engine;

	public:
		MapAnalyzer() = default;

		/// @brief Nettoie la liste des cibles réservées (à appeler chaque tour).
		void clear_claims();

		/**
		 * @brief Renvoie le niveau d'influence d'une case.
		 * @param ref_pos La position à évaluer.
		 * @return La valeur d'influence (positive = bonus, négative = danger mortel).
		 */
		double get_influence(const hlt::Position& ref_pos) const;

		/**
		 * @brief Trouve la distance vers la base ou le dropoff le plus proche appartenant au joueur.
		 * @param ref_pos La position de départ.
		 * @param p_player Le joueur concerné.
		 * @param ref_game_map La carte pour le calcul de distance torique.
		 * @return La distance minimale.
		 */
		int get_distance_to_nearest_base(const hlt::Position& ref_pos, const std::shared_ptr<hlt::Player>& p_player, hlt::GameMap& ref_game_map) const;

		/**
		 * @brief Calcule la quantité totale de halite dans un rayon donné autour d'un centre.
		 * @param ref_center Le centre de la zone de recherche.
		 * @param radius Le rayon de recherche.
		 * @param ref_game_map La carte pour récupérer les valeurs.
		 * @return La somme de Halite dans la zone.
		 */
		int get_area_wealth(const hlt::Position& ref_center, int radius, hlt::GameMap& ref_game_map) const;

		/**
		 * @brief Calcule le Halite total restant sur toute la carte.
		 * @param ref_game_map La carte du jeu.
		 * @return La somme globale du Halite.
		 */
		int get_total_map_halite(const hlt::GameMap& ref_game_map) const;

		/**
		 * @brief Génère la carte d'influence globale (Inspiration et Dangers).
		 * @param ref_game Référence au jeu actuel (pour analyser la position des ennemis).
		 */
		void compute_influence(hlt::Game& ref_game);

		/**
		 * @brief Sélectionne la meilleure cible de minage pour un vaisseau donné (Legacy, remplacé par Bipartite).
		 * @param p_ship Le vaisseau cible.
		 * @param ref_game_map La carte du jeu.
		 * @return La position optimale à miner.
		 */
		hlt::Position get_best_halite_target(const std::shared_ptr<hlt::Ship>& p_ship, hlt::GameMap& ref_game_map);

		/**
		 * @brief Récupère les N meilleures zones de la carte absolues (Valeur brute + Influence).
		 * @param count Le nombre de cibles désirées.
		 * @param ref_game_map La carte du jeu.
		 * @return Un vecteur contenant les meilleures positions.
		 */
		std::vector<hlt::Position> get_top_global_targets(int count, const hlt::GameMap& ref_game_map, const hlt::Game& ref_game) const;

		/**
		 * @brief Évalue la carte pour trouver l'emplacement optimal de construction d'un Dropoff.
		 * @param p_player Le joueur construisant le dropoff.
		 * @param ref_game_map La carte du jeu.
		 * @return Une paire contenant la position idéale et un booléen indiquant si l'investissement est rentable.
		 */
		std::pair<hlt::Position, bool> find_best_dropoff_location(const std::shared_ptr<hlt::Player>& p_player, hlt::GameMap& ref_game_map) const;
	};

} // namespace bot