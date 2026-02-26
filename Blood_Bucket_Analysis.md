# Analyse de Blood Bucket VST

## Type de plugin
- **Format**: VST 2.x (32-bit)
- **Moteur**: SynthEdit (référence `se_vst.dll` confirmée)
- **Architecture**: Intel 80386 (x86)

## Paramètres identifiés

### Paramètres confirmés
1. **Saturation** - Contrôle principal de saturation/distorsion
2. **Brightness** - Contrôle de tonalité (filtre passe-haut ou EQ haute fréquence)
3. **mixDryWet** - Mélange signal sec/traité (Mix)

### Modules SynthEdit détectés
- **ADSR** - Enveloppe (possiblement pour modulation)
- **Filter obs / Filter2 obsolete** - Modules de filtrage
- **SV Filter** - State Variable Filter (filtre multi-mode)
- **WaveShaper** - Module de saturation/distorsion
- **Clipper** - Module d'écrêtage

### Contrôles GUI
- **Slider** - Curseurs
- **Knob** - Boutons rotatifs
- Interface avec contrôles d'animation et bitmap

## Architecture probable

D'après l'analyse, Blood Bucket semble être un plugin de **saturation/distorsion** avec:

1. **Signal Path**:
   - Input → WaveShaper/Clipper (Saturation) → SV Filter (Brightness) → Mix (Dry/Wet) → Output

2. **Paramètres utilisateur**:
   - **Saturation**: Intensité de la distorsion/saturation
   - **Brightness**: Filtre tonal (probablement filtre passe-haut ou boost hautes fréquences)
   - **Mix**: Balance entre signal original et traité

3. **Modulation possible**: 
   - ADSR détecté, pourrait moduler la saturation ou le filtre

## Reconstruction dans SynthEdit

Pour recréer ce plugin:

### Modules nécessaires:
1. **IO Mod** (entrée/sortie audio)
2. **WaveShaper** ou **Clipper** pour la saturation
3. **SV Filter** (State Variable) en mode passe-haut pour Brightness
4. **Dry/Wet Mixer** pour le paramètre Mix
5. **3 x Float3 GUI** (sliders/knobs) pour les 3 paramètres

### Configuration suggérée:
```
Input → Saturation (WaveShaper) → Filter (SV HP) → Mixer (Dry/Wet) → Output
          ↑                          ↑                ↑
       [Saturation]            [Brightness]        [Mix]
       (paramètre)             (paramètre)      (paramètre)
```

## Notes techniques

- Le plugin est compilé en 32-bit, donc fonctionnera mieux dans des DAW 32-bit ou avec un bridge
- Pas de projet .se1/.se2 embarqué détecté dans la DLL
- Les ressources sont encodées/compressées
- Interface graphique basée sur bitmaps/animation

## Recommandation

Pour porter ce plugin sur macOS, l'option la plus viable est de:
1. **Recréer le patch dans SynthEdit** avec la structure identifiée ci-dessus
2. **Compiler pour macOS** (SynthEdit supporte la compilation multi-plateforme)
3. Ajuster les paramètres et le comportement audio pour correspondre à l'original

Alternativement, utiliser un bridge comme CrossOver ou Wine pour faire tourner la version Windows sur macOS.
