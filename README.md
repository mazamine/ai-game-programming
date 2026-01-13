# Awalé AI Bot – AI Game Programming

## Description
Ce projet implémente un bot autonome pour le jeu d’Awalé dans le cadre du module AI Game Programming. Le bot communique exclusivement via l’entrée et la sortie standard (stdin/stdout) et joue automatiquement contre un arbitre ou d’autres bots étudiants.  
Il utilise un algorithme Minimax avec élagage alpha-bêta pour choisir ses coups.

## Protocole de communication
À chaque tour, le bot lit une ligne sur l’entrée standard :
- START : le bot est le joueur 1 et joue le premier coup.
- Un coup adverse (ex. : 3R, 12TB).
- END : fin de la partie.

Le bot répond par un seul coup valide, par exemple :
```text
3R
12TB
```

Important :
- Le bot n’affiche rien d’autre sur la sortie standard.
- Il effectue un flush de stdout après chaque coup.

## Algorithme
- Algorithme : Minimax avec élagage alpha-bêta.
- Profondeur maximale de recherche : 6.

Le bot génère tous les coups valides possibles (R, B, TR, TB) et utilise une fonction d’évaluation basée sur :
- le nombre de graines capturées ;
- le contrôle du plateau ;
- les opportunités de capture (trous contenant 2 ou 3 graines) ;
- la pression de famine exercée sur l’adversaire.

## Compilation
Windows :
```sh
gcc -O2 -std=c11 -o bot.exe bot.c
```

Linux :
```sh
gcc -O2 -std=c11 -o bot bot.c
```

## Exécution avec l’arbitre
Exemple de match local entre deux bots :
```sh
java Arbitre bot.exe bot.exe 3
```

Paramètres :
- Timeout par coup : 2 secondes.
- Limite de coups : 400.

## Tests
Le bot a été testé :
- contre lui-même ;
- avec un arbitre Java simulant une partie complète ;
- sans timeout ni disqualification.

## Auteur
[Mohamed El Amine MAZOUZ](mailto:mohamed-el-amine.mazouz@etu.univ-cotedazur.fr)

Projet réalisé dans le cadre du Master – AI Game Programming
