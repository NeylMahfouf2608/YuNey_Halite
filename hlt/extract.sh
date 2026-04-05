#!/bin/bash

# Nom du fichier de sortie
OUTPUT_FILE="code_complet.txt"

# On vide le fichier de sortie s'il existe déjà
> "$OUTPUT_FILE"

echo "Recherche des fichiers .cpp et .hpp..."

# On cherche tous les fichiers .cpp et .hpp et on les parcourt
find . -type f \( -name "*.cpp" -o -name "*.hpp" \) | while read -r file; do
    echo "Ajout de : $file"
    
    # Écriture d'un en-tête clair pour séparer les fichiers dans le document final
    echo "================================================================================" >> "$OUTPUT_FILE"
    echo "FICHIER : $file" >> "$OUTPUT_FILE"
    echo "================================================================================" >> "$OUTPUT_FILE"
    
    # Ajout du contenu du fichier
    cat "$file" >> "$OUTPUT_FILE"
    
    # Ajout de quelques sauts de ligne pour aérer
    echo -e "\n\n" >> "$OUTPUT_FILE"
done

echo "Terminé ! Tout ton code a été regroupé dans le fichier '$OUTPUT_FILE'."