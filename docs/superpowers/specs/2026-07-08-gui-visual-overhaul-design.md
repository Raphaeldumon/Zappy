# Refonte visuelle « atmosphérique » du GUI Zappy

Date : 2026-07-08
Statut : validé (approche A + cycle jour/nuit)

## Objectif

Remplacer les effets de saison actuels (overlays 2D écran : rectangles teintés,
cercles, lignes dans `Interface::drawWeatherOverlay`) et l'environnement statique
(skybox `Background.png`, soleil codé en dur dans `lighting.fs`) par un rendu
« cinématique » entièrement dynamique : ciel procédural, cycle jour/nuit,
lumière et ombres pilotées par la saison, particules 3D dans le monde,
post-process filmique.

## Contraintes et décisions

- **Moteur : raylib conservé** (GLSL 330, wrapper `raylibWrapper.cpp` existant).
- **Performance : full max**, aucun mode dégradé requis. Pas de toggle qualité.
- **Direction artistique : espace stylisé vivant** — le tore reste dans l'espace,
  nébuleuses animées, étoiles, soleil/lune visibles.
- **Soleil = `assets/luan.png`, lune = `assets/palasse.png`** (billboards).
  `palasse.png` a un fond blanc : masque circulaire appliqué dans le shader.
- **Cycle jour/nuit : temps réel, ~3 min par jour complet**, indépendant du
  serveur. La saison module la durée du jour et la hauteur de l'arc solaire.
- Chaque étape d'implémentation laisse le GUI jouable.

## Architecture

### Module central : `EnvironmentState` (`gui/environment.hpp/.cpp`)

Possède tous les paramètres visuels ambiants. Personne d'autre ne connaît les
saisons : le rendu (ciel, lumière, particules, post, sol) ne lit que l'état
interpolé.

```
struct ParticleProfile {
    enum Kind { None, Snow, Leaves, Petals, Fireflies, GoldenDust,
                Embers, Rain, Splash, Spores };
    // kinds actifs + densités + paramètres de vent
};

struct EnvironmentProfile {           // une entrée par (saison, météo)
    Vec3 sunTint;                     // modulateur appliqué sur le cycle jour/nuit
    Vec3 ambientTint;
    Vec3 skyHorizon, skyZenith, nebulaTint;
    Vec3 fogColor;  float fogDensity;
    Vec3 gradeLift, gradeGain;        // color grading
    Color groundDark, groundLight;    // teinte du damier
    float dayFraction;                // part de jour dans le cycle (été 0.65, hiver 0.35)
    float sunArcHeight;               // élévation max du soleil
    ParticleProfile particles;
};
```

- À chaque événement saison/météo du serveur (déjà parsé, `interface.cpp:2050`),
  on change le **profil cible**.
- `EnvironmentState::update(dt)` interpole tous les paramètres vers la cible
  sur ~4 s (smoothstep). Aucun switch visuel brutal.
- Horloge `timeOfDay ∈ [0,1)` en temps réel (~180 s/cycle).

### Cycle jour/nuit

- Position du soleil calculée depuis `timeOfDay` + `dayFraction` +
  `sunArcHeight` : lever à l'est, coucher à l'ouest.
- La lune suit en opposition de phase. La nuit, la lune devient la source
  lumineuse principale (ombres comprises) : lumière bleu froid, ambient réduit.
- Courbe de couleur du cycle : nuit bleue → aube rose/or (golden hour) →
  zénith blanc → crépuscule orange → nuit. La saison multiplie cette base
  (`couleurFinale = cycleJourNuit(t) × sunTint`).
- La nuit : étoiles et nébuleuses s'intensifient, lucioles actives,
  aurores boréales si hiver.

## Couches de rendu

### 1. Ciel procédural (`assets/shaders/sky.fs`, remplace `Background.png`)

Même géométrie skybox, rendu 100 % shader :
- Étoiles : hash 3D + scintillement temporel, intensité liée à la nuit.
- Nébuleuses : 3-4 octaves de FBM simplex animé lentement, teintées
  `nebulaTint` (violet-rose printemps, or été, cuivre automne, bleu glacé hiver).
- Dégradé horizon→zénith piloté par le profil et l'heure.
- Aurores boréales : bandes de noise vert/cyan ondulantes (nuits d'hiver).
- Météo : orage = ciel assombri + éclairs illuminant les nébuleuses ;
  brouillard = ciel délavé.
- **Soleil et lune** : billboards texturés (`luan.png`, `palasse.png`) dessinés
  à leur position céleste, halo/couronne procédurale additive derrière,
  masque circulaire pour `palasse.png`. Leurs pixels dépassent 1.0 (HDR)
  pour alimenter bloom et god rays.

### 2. Lumière dynamique + ombres

- `lighting.fs` : `kSunDir`/`kSunColor`/`kAmbient` (constantes) deviennent des
  uniforms alimentés par `EnvironmentState`. Ajout : rim light (contre-jour)
  et fog de profondeur.
- **Shadow mapping** : passe de profondeur ortho depuis la source active
  (soleil le jour, lune la nuit), shadow map 4096² depth-only créée via
  `rlgl`, échantillonnage PCF 3×3 dans `lighting.fs` et le sol batché.
- Éclair d'orage : flash 3D réel (boost `sunColor`/ambient sur ~2 frames)
  en plus du flash 2D actuel.

### 3. Particules 3D monde (nouveau `ParticleSystem`, remplace `drawWeatherOverlay`)

- Simulation CPU, pools fixes (~4000), rendu billboards face caméra
  (additive ou alpha selon le type).
- Hiver : neige dérivant avec le vent, fondu au contact du sol.
- Automne : feuilles tourbillonnantes (rotation 3D, champ de vent sinusoïdal).
- Printemps : pétales + lucioles émissives nocturnes (au-dessus du seuil de
  bloom → elles brillent).
- Été : poussières dorées ; canicule = braises montantes.
- Pluie/orage : traits verticaux 3D + éclaboussures (anneaux) à l'impact
  sur le tore ; fertile = spores vertes lumineuses.
- L'overlay 2D `drawWeatherOverlay` est supprimé ; la touche `V` toggle
  particules + post saisonnier.

### 4. Post-process cinématique

Chaîne existante étendue : `sceneRT (RGBA16F via rlgl) → bright extract →
blur → composite`. Le composite fait en un shader :
- Tonemapping ACES (le RT float permet aux émissifs de dépasser 1.0).
- God rays : radial blur du masque brillant depuis la position écran du
  soleil (ou de la lune la nuit).
- Color grading lift/gain par profil, vignette douce, léger grain.
- Distorsion de chaleur (canicule) : offset UV sinusoïdal, remplace les
  lignes 2D.

### 5. Habillage du sol

Les couleurs du damier codées en dur (`interface.cpp:747`) sont pilotées par
le profil : vert printemps → doré été → roux automne → enneigé hiver.
Le sol reçoit ombres et fog.

## Gestion d'erreur

Chaque capacité (shadow map, RT float, sky shader, textures soleil/lune) a un
fallback : échec de compilation/FBO/chargement → log warning + chemin de rendu
actuel conservé (même stratégie que `enableBloom`).

## Outillage debug

Touche debug pour forcer localement saison, météo et heure du jour (cycle
rapide), afin de vérifier chaque combinaison sans dépendre du serveur.

## Vérification

- Build complet + lancement serveur/GUI réels.
- Captures d'écran : 4 saisons × météos clés × jour/aube/nuit via la touche
  debug ; comparaison visuelle.
- Vérifier l'absence de régression : sélection/hover des tuiles, HUD,
  broadcast, incantation, météorites restent lisibles par-dessus le nouveau
  rendu.

## Ordre d'implémentation

1. `EnvironmentState` + uniforms lumière dynamique (soleil saisonnier statique).
2. Ciel procédural + billboards soleil/lune + cycle jour/nuit.
3. Post-process (RT float, ACES, grading, god rays, vignette).
4. Particules 3D + suppression de l'overlay 2D.
5. Shadow mapping.
6. Sol saisonnier + polish (transitions, orage, aurores) + touche debug.
