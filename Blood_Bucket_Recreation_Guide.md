# Guide de reconstruction de Blood Bucket dans SynthEdit

## Préparation

### Outils nécessaires
- **SynthEdit 1.4+** (version récente supportant macOS)
- Téléchargeable sur: https://www.synthedit.com

### Modules requis
Tous les modules suivants sont disponibles dans SynthEdit par défaut ou via le package manager.

---

## Étape 1: Créer un nouveau projet

1. Ouvrir SynthEdit
2. File → New Project
3. Sauvegarder sous "Blood_Bucket_Recreation.se2"

---

## Étape 2: Ajouter les modules de base

### 2.1 Modules d'entrée/sortie
1. Ajouter **IO Mod** (2x):
   - Un pour l'entrée audio
   - Un pour la sortie audio

### 2.2 Module de saturation
Ajouter un **WaveShaper** ou **Clipper**:
- **Option 1: WaveShaper** (recommandé)
  - Plus de contrôle sur la courbe de saturation
  - Chercher "WaveShaper" dans la liste des modules
  
- **Option 2: Clipper**
  - Saturation hard-clipping plus agressive
  - Chercher "Clipper" dans la liste

### 2.3 Module de filtrage
Ajouter un **SV Filter** (State Variable Filter):
- Ce filtre multi-mode sera configuré en passe-haut
- Chercher "SV Filter" ou "State Variable Filter"

### 2.4 Module de mixage Dry/Wet
Ajouter un **Dry/Wet Mixer**:
- Permet de mélanger le signal original et traité
- Chercher "Mixer" ou "Dry/Wet"

---

## Étape 3: Créer les contrôles utilisateur

### 3.1 Paramètre "Saturation"
1. Ajouter un **Float3 GUI** (ou Slider/Knob)
2. Renommer: "Saturation"
3. Configurer:
   - Range: 0.0 à 1.0
   - Default: 0.5
   - Appearance: Knob ou Slider

### 3.2 Paramètre "Brightness"
1. Ajouter un **Float3 GUI**
2. Renommer: "Brightness"
3. Configurer:
   - Range: 0.0 à 1.0 (mappé à fréquence de coupure)
   - Default: 0.5
   - Appearance: Knob

### 3.3 Paramètre "Mix"
1. Ajouter un **Float3 GUI**
2. Renommer: "Mix" ou "Dry/Wet"
3. Configurer:
   - Range: 0.0 à 1.0 (0 = 100% dry, 1 = 100% wet)
   - Default: 1.0 (100% wet)

---

## Étape 4: Câblage du signal

### 4.1 Connexions audio principales
```
Audio Input → [Split vers 2 chemins]
   ↓                    ↓
   |              WaveShaper (Saturation)
   |                    ↓
   |              SV Filter (Brightness)
   |                    ↓
   └─────→ Dry/Wet Mixer ←──┘
                ↓
          Audio Output
```

### 4.2 Connexions des contrôles

**Saturation → WaveShaper:**
- Connecter la sortie du contrôle "Saturation" à l'entrée de gain/drive du WaveShaper
- Vous devrez peut-être ajouter un **Volts to Float** converter si les types ne matchent pas

**Brightness → SV Filter:**
- Connecter "Brightness" à l'entrée "Cutoff Frequency" du filtre
- Convertir la valeur 0-1 en fréquence (par exemple 200 Hz - 20 kHz)
- Utiliser un **Float Scaler** pour mapper la plage:
  - Input: 0.0 - 1.0
  - Output: 200 - 20000 (Hz)

**Mix → Dry/Wet Mixer:**
- Connecter "Mix" à l'entrée "Mix Amount" du mixer

---

## Étape 5: Configuration des modules

### 5.1 WaveShaper
Configuration suggérée:
- **Curve Type**: Soft Clip ou Tube
- **Gain**: Contrôlé par le paramètre "Saturation"
- **DC Offset**: 0

### 5.2 SV Filter
Configuration:
- **Filter Type**: High Pass (passe-haut)
- **Cutoff**: Contrôlé par "Brightness" (via Float Scaler)
- **Resonance**: Fixe à 0.5 (ou ajoutez un paramètre si souhaité)

### 5.3 Dry/Wet Mixer
- **Input A**: Signal original (dry)
- **Input B**: Signal traité (wet)
- **Mix**: Contrôlé par paramètre "Mix"

---

## Étape 6: Interface utilisateur

### 6.1 Layout des contrôles
Organiser les 3 knobs/sliders:
```
┌─────────────────────┐
│   BLOOD BUCKET      │
├─────────────────────┤
│                     │
│  [Saturation]       │
│                     │
│  [Brightness]       │
│                     │
│  [Mix]              │
│                     │
└─────────────────────┘
```

### 6.2 Apparence (optionnel)
- Ajouter un fond bitmap
- Customiser les knobs avec des images
- Ajouter des labels de texte

---

## Étape 7: Test et ajustement

### 7.1 Tests audio
1. Construire le projet: Build → Build VST Plugin
2. Charger dans votre DAW
3. Tester avec différents signaux:
   - Guitare
   - Basse
   - Drums
   - Synthés

### 7.2 Ajustements possibles
Si le son ne correspond pas exactement:
- **Saturation trop douce?** → Augmenter le gain dans le WaveShaper
- **Saturation trop dure?** → Changer le type de courbe (soft clip, tube, tanh)
- **Brightness pas assez prononcé?** → Ajuster la plage de fréquences du scaler
- **Manque de corps?** → Ajouter un filtre passe-bas en parallèle

---

## Étape 8: Compilation pour macOS

### 8.1 Export VST3/AU pour Mac
1. Dans SynthEdit: Build → Export Settings
2. Sélectionner:
   - ☑ macOS (ARM64) pour Mac M1/M2/M3
   - ☑ macOS (x86_64) pour Mac Intel
   - ☑ Format VST3
   - ☑ Format AU (Audio Unit)

3. Build → Build All Platforms

### 8.2 Installation sur Mac
Les plugins seront exportés dans:
- **VST3**: `/Library/Audio/Plug-Ins/VST3/`
- **AU**: `/Library/Audio/Plug-Ins/Components/`

---

## Améliorations optionnelles

### Fonctionnalités avancées
1. **ADSR Modulation**:
   - Ajouter un module ADSR
   - Connecter à la saturation ou au filtre pour modulation dynamique

2. **Paramètres supplémentaires**:
   - **Output Gain**: Pour compenser le niveau
   - **Input Gain**: Pour drive supplémentaire
   - **Resonance**: Pour le filtre

3. **Presets**:
   - Créer des presets dans SynthEdit
   - Exporter avec le plugin

---

## Résolution de problèmes

### Le son est trop distordu
- Réduire le gain dans le WaveShaper
- Ajouter un limiteur en sortie

### Pas assez de saturation
- Augmenter la plage du paramètre "Saturation"
- Utiliser un Clipper au lieu de WaveShaper

### Le filtre ne fait rien
- Vérifier que le Float Scaler convertit correctement 0-1 en Hz
- S'assurer que le filtre est en mode High Pass

### Le plugin ne se charge pas dans le DAW
- Vérifier l'architecture (32-bit vs 64-bit)
- Rescanner les plugins dans le DAW
- Vérifier les permissions du dossier de plugins

---

## Ressources

- **SynthEdit Forum**: https://www.synthedit.com/forum/
- **Module Documentation**: Dans SynthEdit, clic droit sur module → Help
- **Tutoriels**: https://www.synthedit.com/tutorials/

---

## Notes finales

Ce guide recrée la fonctionnalité de base de Blood Bucket. Le son exact dépendra:
- Du type de waveshaping utilisé
- Des courbes de saturation choisies
- Des réglages fins des paramètres

N'hésitez pas à expérimenter avec différents modules et configurations pour obtenir le son souhaité !
