/**
 * @file trainer.cpp
 * @brief Boîte à outils d'entraînement et de benchmark pour l'IA YuNey_v2.
 * * @details
 * FONCTIONNEMENT DE L'ALGORITHME GÉNÉTIQUE :
 * 1. Population : Génère des "individus" (bots) avec un set de gènes (paramètres) aléatoires.
 * 2. Évaluation (Fitness) : Fait jouer chaque individu contre un bot de référence (3 matchs).
 * Le score d'un individu = (Halite récolté) + (Bonus massif de 10 000 pts si Victoire).
 * 3. Évolution : Les meilleurs survivent, se croisent et mutent au fil des générations.
 * * SPÉCIFICITÉS DU PROJET :
 * - Spécialisation (Overfitting) : La carte est verrouillée en 32x32 sans aléatoire de taille.
 * - Multi-threading : Les matchs d'évaluation et le benchmark s'exécutent en parallèle
 * pour ne pas brider le processeur.
 */

#include "utils/genetic_engine.hpp"
#include <iostream>
#include <fstream>
#include <string>
#include <regex>
#include <vector>
#include <future>
#include <random>

const int POPULATION_SIZE = 10;
const int GENERATIONS = 15;
const int MATCHES_PER_EVAL = 3;

const std::string COLOR_GREEN = "\033[32m";
const std::string COLOR_RED = "\033[31m";
const std::string COLOR_RESET = "\033[0m";

// Correspondance stricte avec les arguments attendus par MyBot.cpp
const std::vector<std::string> GENE_NAMES = {
	"DROPOFF_WEALTH", "DROPOFF_DIST", "SPAWN_WEALTH_RATIO", "KAMIKAZE_PROFIT",
	"ENDGAME_TURN_OFFSET", "STOP_SPAWN_TURN", "KAMIKAZE_MIN_DANGER",
	"MINING_THRESHOLD_MIN", "MINING_THRESHOLD_MAX", "INSPIRATION_BONUS",
	"GENE_SPAWN_MAX_SHIPS_RATIO", "GENE_ECO_ABANDON_PENALTY",
	"GENE_RETURN_THRESHOLD", "GENE_CONGESTION_PENALTY", "GENE_DROPOFF_MIN_SHIPS"
};

/**
 * @brief Évalue la viabilité d'un set de gènes en lançant de vraies parties.
 */
int evaluate_halite_bot(const std::vector<int>& genes, int thread_id) {
	int total_fitness = 0;
	std::string match_results = "";

	// Construction de la ligne de commande avec injection des gènes
	std::string my_bot_args = "MyBot.exe";
	for (int gene : genes) my_bot_args += " " + std::to_string(gene);

	std::random_device rd;
	std::mt19937 rng(rd());
	std::uniform_int_distribution<int> seed_dist(1000, 99999);

	for (int m = 0; m < MATCHES_PER_EVAL; ++m) {
		int seed = seed_dist(rng); // Variation de la topologie (toujours en 32x32)
		std::string temp_file = "result_thread" + std::to_string(thread_id) + "_match" + std::to_string(m) + ".txt";

		std::string cmd = ".\\halite.exe --no-replay --no-logs --seed " + std::to_string(seed) +
			" --width 32 --height 32 \"" + my_bot_args + "\" \"ReferenceBot.exe\" > " + temp_file + " 2>&1";

		system(cmd.c_str());

		// Lecture et suppression propre du fichier de log temporaire
		std::ifstream file(temp_file);
		std::string output((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();
		std::remove(temp_file.c_str());

		// Extraction de la position et du score via Regex
		std::regex score_regex("Player 0.*rank ([0-9]+) with ([0-9]+) halite");
		std::smatch match;

		if (std::regex_search(output, match, score_regex) && match.size() > 2) {
			int rank = std::stoi(match.str(1));
			int halite = std::stoi(match.str(2));

			total_fitness += halite;
			if (rank == 1) {
				total_fitness += 10000; // Prime de victoire massive
				match_results += COLOR_GREEN + "V" + COLOR_RESET + " ";
			}
			else {
				match_results += COLOR_RED + "D" + COLOR_RESET + " ";
			}
		}
		else {
			match_results += COLOR_RED + "E" + COLOR_RESET + " ";
		}
	}

	std::cout << "[Thread " << thread_id << "] Matchs: [" << match_results << "] Score: " << total_fitness << "\n";
	return total_fitness;
}

/**
 * @brief Lance le processus d'évolution génétique.
 */
void run_genetic_training() {
	std::cout << "\n--- DEMARRAGE DE L'ENTRAINEMENT GENETIQUE (SPECIALISATION 32x32) ---\n";

	// Plages de valeurs autorisées pour chaque gène
	std::vector<std::pair<int, int>> gene_bounds = {
		{8000, 20000}, {10, 25}, {2000, 8000}, {50, 600},
		{10, 30}, {50, 250}, {0, 500}, {0, 40}, {60, 150}, {10, 100},
		{10, 50}, {-10, 20}, {500, 990}, {0, 15}, {10, 50}
	};

	utils::GeneticEngine engine(POPULATION_SIZE, GENERATIONS, gene_bounds, evaluate_halite_bot);
	utils::Genome best_dna = engine.run();

	std::cout << "\n=========================================\n";
	std::cout << "Score final : " << best_dna.fitness << "\nProfil ADN Champion :\n";
	for (size_t i = 0; i < best_dna.genes.size(); ++i) {
		std::cout << "  -> " << GENE_NAMES[i] << " : " << best_dna.genes[i] << "\n";
	}
}

/**
 * @brief Joue un match unique pour le mode Benchmark.
 */
bool play_single_benchmark_match(int match_id) {
	std::string temp_file = "bench_match_" + std::to_string(match_id) + ".txt";

	// Sans arguments génétiques, le bot utilise les valeurs hardcodées dans MyBot.cpp
	std::string cmd = ".\\halite.exe --no-replay --no-logs --width 32 --height 32 \"MyBot.exe\" \"ReferenceBot.exe\" > " + temp_file + " 2>&1";
	system(cmd.c_str());

	std::ifstream file(temp_file);
	std::string output((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	std::remove(temp_file.c_str());

	std::regex score_regex("Player 0.*rank ([0-9]+)");
	std::smatch match;

	if (std::regex_search(output, match, score_regex) && match.size() > 1) {
		return std::stoi(match.str(1)) == 1; // Retourne Vrai si Victoire
	}
	return false;
}

/**
 * @brief Lance 100 parties pour évaluer le Win Rate actuel du bot.
 */
void run_benchmark() {
	int total_matches = 100;
	int batch_size = 10; // Limitation des threads simultanés