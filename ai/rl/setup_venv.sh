#!/bin/bash

set -e

VENV_DIR=".venv"

echo "[SETUP] Vérification de python3-venv..."

if ! python3 -m venv --help > /dev/null 2>&1; then
    echo "[SETUP] python3-venv manquant, installation..."
    sudo apt update
    sudo apt install -y python3-venv python3-pip
fi

echo "[SETUP] Création du venv dans $VENV_DIR..."

if [ ! -d "$VENV_DIR" ]; then
    python3 -m venv "$VENV_DIR"
else
    echo "[SETUP] Le venv existe déjà."
fi

echo "[SETUP] Activation du venv..."
source "$VENV_DIR/bin/activate"

echo "[SETUP] Mise à jour de pip..."
python -m pip install --upgrade pip

echo "[SETUP] Installation des dépendances..."
pip install numpy gymnasium torch

echo ""
echo "[SETUP] Terminé ✅"
echo ""
echo "Pour activer ton venv :"
echo "source $VENV_DIR/bin/activate"
echo ""
echo "Pour lancer ton script :"
echo "python train_rl.py"