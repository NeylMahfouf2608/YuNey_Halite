/**
 * @file GeneticEngine.hpp
 * @brief Moteur d'Algorithme Génétique agnostique et multithreadé.
 */
#pragma once

#include <vector>
#include <functional>
#include <random>
#include <algorithm>
#include <future>
#include <iostream>

namespace utils {

	struct Genome {
		std::vector<int> genes;
		int fitness = 0;
	};

	class GeneticEngine {
	private:
		int m_population_size;
		int m_generations;
		std::vector<std::pair<int, int>> m_gene_bounds;
		std::function<int(const std::vector<int>&, int)> m_evaluate_func;
		std::mt19937 m_rng;

		int random_int(int min, int max) {
			std::uniform_int_distribution<int> dist(min, max);
			return dist(m_rng);
		}

		Genome generate_random_genome() {
			Genome g;
			g.genes.reserve(m_gene_bounds.size());
			for (const auto& bounds : m_gene_bounds) {
				g.genes.push_back(random_int(bounds.first, bounds.second));
			}
			return g;
		}

		Genome crossover_and_mutate(const Genome& p1, const Genome& p2) {
			Genome child;
			child.genes.reserve(m_gene_bounds.size());

			for (size_t i = 0; i < m_gene_bounds.size(); ++i) {
				// Croisement uniforme (50/50 d'hériter de l'un ou l'autre parent)
				int inherited_gene = (random_int(0, 1) == 0) ? p1.genes[i] : p2.genes[i];

				// Mutation de +/- 5% (10% de chance d'occurrence)
				if (random_int(1, 100) <= 10) {
					double variation = 1.0 + (random_int(-5, 5) / 100.0);
					int mutated_val = static_cast<int>(inherited_gene * variation);

					// Contrainte stricte dans les limites définies (utilise min/max plutot que clamp pour ne pas avoir à changer la version de C++ (bizarrement je peux utiliser les lambda et les std::function par contre)
					mutated_val = std::max(m_gene_bounds[i].first, std::min(m_gene_bounds[i].second, mutated_val));
					child.genes.push_back(mutated_val);
				}
				else {
					child.genes.push_back(inherited_gene);
				}
			}
			return child;
		}

	public:
		GeneticEngine(int pop_size, int gens, const std::vector<std::pair<int, int>>& bounds, std::function<int(const std::vector<int>&, int)> eval_func)
			: m_population_size(pop_size), m_generations(gens), m_gene_bounds(bounds), m_evaluate_func(eval_func) {
			std::random_device rd;
			m_rng = std::mt19937(rd());
		}

		Genome run() {
			std::vector<Genome> population(m_population_size);
			for (int i = 0; i < m_population_size; ++i) {
				population[i] = generate_random_genome();
			}

			for (int gen = 0; gen < m_generations; ++gen) {
				std::cout << "\n=== GENERATION " << gen + 1 << " ===\n";
				std::vector<std::future<int>> futures;

				// Évaluation asynchrone pour maximiser l'usage CPU
				for (int i = 0; i < m_population_size; ++i) {
					futures.push_back(std::async(std::launch::async, m_evaluate_func, population[i].genes, i + 1));
				}

				for (int i = 0; i < m_population_size; ++i) {
					population[i].fitness = futures[i].get();
				}

				// Tri par élitisme (décroissant)
				std::sort(population.begin(), population.end(), [](const Genome& a, const Genome& b) {
					return a.fitness > b.fitness;
					});
				//affichage bugué par le multithreading, rigolo donc je laisse
				std::cout << ">> Meilleur ADN: Score " << population[0].fitness << " | Genes: [";
				for (size_t i = 0; i < population[0].genes.size(); ++i) {
					std::cout << population[0].genes[i] << (i < population[0].genes.size() - 1 ? ", " : "");
				}
				std::cout << "]\n";

				std::vector<Genome> next_generation;
				next_generation.reserve(m_population_size);

				// Conservation intacte des 4 meilleurs individus
				for (int i = 0; i < 4; ++i) {
					next_generation.push_back(population[i]);
				}

				// Reproduction pour repeupler la génération
				while (next_generation.size() < static_cast<size_t>(m_population_size)) {
					int p1_idx = random_int(0, 3);
					int p2_idx = random_int(0, 3);
					next_generation.push_back(crossover_and_mutate(population[p1_idx], population[p2_idx]));
				}

				population = std::move(next_generation);
			}
			return population[0];
		}
	};
}