# 📹 DSi Video Cam

App homebrew per Nintendo DSi che registra pseudo-video dalla fotocamera e li riproduce immediatamente — tutto in RAM, niente salvataggio su scheda SD.

---

## Come funziona

| Fase | Cosa succede |
|---|---|
| **Preview** | Vedi il live della fotocamera sullo schermo superiore |
| **Registrazione** | Premi A → cattura ~10 frame/sec e li tiene in RAM |
| **Riproduzione** | Appena premi STOP (A), il video parte subito in loop |
| **Spegni il DS** | Tutto cancellato — nessun file scritto sulla SD |

### Limiti hardware
- **Max ~60 frame** in RAM (≈ 6 secondi a 10 fps)
- Risoluzione: 256×192 (nativa DSi)
- Funziona **solo su DSi / DSi XL** (serve la fotocamera)

---

## Controlli

```
MENU
  A        → Anteprima fotocamera
  B        → Riproduci ultimo video
  START    → Esci

ANTEPRIMA (live cam)
  A        → Inizia registrazione
  B        → Torna al menu

REGISTRAZIONE
  A        → Ferma e vai alla riproduzione
  (auto)   → Si ferma da solo a 60 frame

RIPRODUZIONE
  L        → Loop ON
  R        → Loop OFF
  B        → Torna al menu
```

---

## Compilazione locale

### Requisiti
- [devkitPro](https://devkitpro.org/wiki/Getting_Started) con `nds-dev` installato

```bash
# Installa il gruppo nds-dev (se non già fatto)
sudo dkp-pacman -S nds-dev

# Compila
make

# Output: dsi-video-cam.nds
```

---

## Compilazione via GitHub Actions

Fai il push su `main` — Actions compila automaticamente e carica il `.nds` come artifact scaricabile.

Per creare una release con il `.nds` allegato:
```bash
git tag v1.0.0
git push origin v1.0.0
```

---

## Installazione sul DSi

1. Scarica `dsi-video-cam.nds` dagli Artifacts di GitHub Actions
2. Copia il file nella root della scheda SD
3. Avvia con **TWiLight Menu++** o **HiyaCFW**

> ⚠️ Richiede un DSi con custom firmware. Non funziona su DS / DS Lite (niente fotocamera).

---

## Struttura progetto

```
dsi-camera-app/
├── source/
│   └── main.c              ← tutto il codice dell'app
├── Makefile                ← build con devkitARM
├── .github/
│   └── workflows/
│       └── build.yml       ← GitHub Actions
└── README.md
```
