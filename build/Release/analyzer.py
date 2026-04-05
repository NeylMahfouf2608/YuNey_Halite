# -*- coding: cp1252 -*-
import zstandard as zstd
import json
import sys
import csv

def analyze_replay(file_path):
    try:
        with open(file_path, 'rb') as f:
            dctx = zstd.ZstdDecompressor()
            replay = json.loads(dctx.decompress(f.read()))
    except Exception as e:
        print(f"Erreur : {e}"); return

    player_names = {str(p['player_id']): p['name'] for p in replay['players']}
    frames = replay['full_frames']
    
    # Préparation du fichier CSV
    with open('telemetry.csv', mode='w', newline='') as csv_file:
        fieldnames = ['Turn', 'Player', 'Bank', 'Cargo', 'Ships', 'Mined_This_Turn', 'Allied_Collisions', 'Combat_Deaths']
        writer = csv.DictWriter(csv_file, fieldnames=fieldnames)
        writer.writeheader()

        prev_entities = {pid: {} for pid in player_names.keys()}
        
        print(f"Analyse de {len(frames)} tours en cours...")

        for turn, frame in enumerate(frames):
            current_entities = frame.get('entities', {})
            events = frame.get('events', [])

            # Extraction des collisions pour ce tour
            collisions = {pid: 0 for pid in player_names.keys()}
            combats = {pid: 0 for pid in player_names.keys()}
            
            for event in events:
                if event['type'] == 'shipwreck':
                    ships_involved = event.get('ships', [])
                    owners = set()
                    for sid in ships_involved:
                        for pid, p_ships in prev_entities.items():
                            if str(sid) in p_ships: owners.add(pid)
                    
                    if len(owners) == 1: # Collision alliée (Accident)
                        collisions[list(owners)[0]] += len(ships_involved)
                    else: # Combat
                        for pid in owners: combats[pid] += 1

            # Écriture des données pour chaque joueur
            for pid, name in player_names.items():
                p_entities = current_entities.get(pid, {})
                
                bank = frame.get('energy', {}).get(pid, 0)
                cargo = sum(e.get('energy', 0) for e in p_entities.values())
                ships_count = len(p_entities)
                
                # On estime le minage (différence de cargo + dépôts)
                # Note: Simplifié pour le CSV
                
                writer.writerow({
                    'Turn': turn,
                    'Player': name,
                    'Bank': bank,
                    'Cargo': cargo,
                    'Ships': ships_count,
                    'Mined_This_Turn': 0, # Donnée complexe à extraire précisément, on se base sur Cargo/Bank
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
