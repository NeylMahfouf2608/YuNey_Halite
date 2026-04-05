# -*- coding: cp1252 -*-
"""
@file telemetry_expert.py
@brief Analyseur de replays Halite III pour extraction de télémétrie CSV.
@details
LOGIQUE DE L'ANALYSEUR :
1. Décompression : Lit le JSON brut via Zstandard pour accéder aux données du match.
2. Détection de Collision : Identifie les "Accidents" (collisions alliées) en vérifiant 
   si tous les vaisseaux impliqués dans un 'shipwreck' appartiennent au même joueur.
3. Télémétrie temporelle : Génère un état complet (Banque, Cargo, Flotte) tour par tour 
   pour permettre une analyse graphique des performances.
"""

import zstandard as zstd
import json
import sys
import csv

def analyze_replay(file_path):
    """
    @brief Extrait les données de performance d'un fichier .hlt.
    """
    try:
        with open(file_path, 'rb') as f:
            dctx = zstd.ZstdDecompressor()
            replay = json.loads(dctx.decompress(f.read()))
    except Exception as e:
        print(f"Erreur de lecture : {e}")
        return

    player_names = {str(p['player_id']): p['name'] for p in replay['players']}
    frames = replay['full_frames']
    
    with open('telemetry.csv', mode='w', newline='') as csv_file:
        fieldnames = ['Turn', 'Player', 'Bank', 'Cargo', 'Ships', 'Mined_This_Turn', 'Allied_Collisions', 'Combat_Deaths']
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

        # Stockage de la frame précédente pour identifier les propriétaires des vaisseaux détruits
        prev_entities = {pid: {} for pid in player_names.keys()}
        
        print(f"Analyse de {len(frames)} tours en cours...")

        for turn, frame in enumerate(frames):
            current_entities = frame.get('entities', {})
            events = frame.get('events', [])

            collisions = {pid: 0 for pid in player_names.keys()}
            combats = {pid: 0 for pid in player_names.keys()}
            
            for event in events:
                if event['type'] == 'shipwreck':
                    ships_involved = event.get('ships', [])
                    owners = set()
                    for sid in ships_involved:
                        for pid, p_ships in prev_entities.items():
                            if str(sid) in p_ships: 
                                owners.add(pid)
                    
                    # Si un seul propriétaire est impliqué : collision entre alliés (Bug de navigation)
                    if len(owners) == 1:
                        collisions[list(owners)[0]] += len(ships_involved)
                    # Sinon : perte au combat contre un ennemi
                    else:
                        for pid in owners: 
                            combats[pid] += 1

            for pid, name in player_names.items():
                p_entities = current_entities.get(pid, {})
                
                bank = frame.get('energy', {}).get(pid, 0)
                cargo = sum(e.get('energy', 0) for e in p_entities.values())
                ships_count = len(p_entities)
                
                writer.writerow({
                    'Turn': turn,
                    'Player': name,
                    'Bank': bank,
                    'Cargo': cargo,
                    'Ships': ships_count,
                    'Mined_This_Turn': 0, # Calcul complexe (Delta Bank + Delta Cargo), non implémenté ici
                    'Allied_Collisions': collisions[pid],
                    'Combat_Deaths': combats[pid]
                })

            prev_entities = current_entities

    print("Terminé ! Le fichier 'telemetry.csv' a été généré.")

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python telemetry_expert.py <fichier.hlt>")
    else:
        analyze_replay(sys.argv[1])
