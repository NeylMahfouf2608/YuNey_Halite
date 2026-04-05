/**
 * @file InfluenceGrid.hpp
 * @brief Moteur générique de carte d'influence (Heatmap) 2D.
 */
#pragma once
#include <vector>
#include <algorithm>
#include <functional>

namespace utils {

	class InfluenceGrid {
	private:
		int m_width;
		int m_height;
		std::vector<std::vector<double>> m_grid;

	public:
		InfluenceGrid(int width = 0, int height = 0) : m_width(width), m_height(height) {
			if (width > 0 && height > 0) {
				m_grid.assign(height, std::vector<double>(width, 0.0));
			}
		}

		/**
		 * @brief Réinitialise la grille (Zéro allocation si les dimensions sont identiques).
		 */
		void reset(int width, int height) {
			if (m_width != width || m_height != height) {
				m_width = width;
				m_height = height;
				m_grid.assign(height, std::vector<double>(width, 0.0));
			}
			else {
				for (auto& row : m_grid) {
					std::fill(row.begin(), row.end(), 0.0);
				}
			}
		}

		/**
		 * @brief Ajoute un score (attrait ou danger) sur une case.
		 * @param is_toric Gère automatiquement le wrap-around (la carte boucle sur elle-même).
		 */
		void add_influence(int x, int y, double value, bool is_toric = true) {
			if (is_toric) {
				x = (x % m_width + m_width) % m_width;
				y = (y % m_height + m_height) % m_height;
			}
			else if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
				return;
			}
			m_grid[y][x] += value;
		}

		double get_value(int x, int y) const {
			// Sécurité anti-crash si on déborde hors mode torique
			if (x < 0 || x >= m_width || y < 0 || y >= m_height) return 0.0;
			return m_grid[y][x];
		}

		/**
		 * @brief Applique une transformation sur toute la carte via une Lambda.
		 * @param filter Fonction injectée par l'appelant (ex: lissage, dégradation temporelle).
		 */
		void apply_filter(const std::function<double(int x, int y, double current_value)>& filter) {
			for (int y = 0; y < m_height; ++y) {
				for (int x = 0; x < m_width; ++x) {
					m_grid[y][x] = filter(x, y, m_grid[y][x]);
				}
			}
		}
	};
}