# Czechmate — AI Trenér & lokální LLM: kontext pro asistenta

Jeden soubor se **veškerým relevantním kontextem** z práce na projektu (coach, MediaPipe/Gemma, prompty, tok, soubory, rozhodnutí). Cesta workspace: `free_chess_v1_mqtt_HA` / podsložka **`CZECHMATE/`** (iOS aplikace).

---

## 1. Účel funkce „AI Trenér“

- **Učící mód** mění chování hry (bez stresových prvků, trenérské UI).
- Hráč může otevřít **plán pozice**: tlačítko **„Zhodnoť moji pozici“** v `GameView` → sheet s `CoachBubbleView`.
- Analýza pozice: **Stockfish** přes existující API (`BoardConnectionStore.fetchCoachStockfishAnalysis()`) → **FEN**, **eval** (přepočet do perspektivy **bílého** v pěšácích), **nejlepší tah v UCI** (např. `e2e4`).
- Lokální **Gemma 2B** přes **MediaPipe LLM Inference** generuje český výklad; při nedostupnosti modelu běží **záložní mock** (streamovaný text).

---

## 2. Architektura (soubory)

| Soubor | Role |
|--------|------|
| `CZECHMATE/Features/Coach/ChessCoachManager.swift` | Orchestrace: Stockfish → prompt → stream do `@Published advice`, `Task.detached` pro inference, stav „Trenér se připravuje…“, sync s `ModelDownloadManager` / učícím módem, `weak learningMode` → `registerCoachAdviceStreamCompleted()`. |
| `CZECHMATE/Features/Coach/ChessGemmaInference.swift` | Obal nad `LlmInference` (MediaPipe): load `.bin`, `LlmInference.Session`, `addQueryChunk` + `generateResponseAsync`, delta do callbacku. Bez CocoaPods: **`#if canImport(MediaPipeTasksGenAI)`** → stub (nekompletní build). |
| `CZECHMATE/Features/Coach/LLMPromptBuilder.swift` | `coachPlayerLevelHint`, `buildMediaPipeGemmaCoachPrompt` (persona **Czechmate AI Trenér**, sekce ROLE / KONTEXT / ÚKOL / STYL / DATA), `buildPromptForLLM` (legacy/konzistence). |
| `CZECHMATE/Features/Coach/ModelDownloadManager.swift` | Stažení / staging; očekávaný soubor modelu: **`coach_gemma_mediapipe.bin`** v `Application Support/…/ChessCoachModels/`. |
| `CZECHMATE/Features/Coach/LearningModeManager.swift` | Učící mód, blunder brzda, **`coachAdviceStreamsCompleted`** + **`registerCoachAdviceStreamCompleted()`**. |
| `CZECHMATE/Features/Coach/CoachBubbleView.swift` | Bublina; při `isStreamingLLM` vypnutý typewriter (živý text). |
| `CZECHMATE/Features/Coach/CoachOnboardingView.swift` | Onboarding ke stažení modelu. |
| `CZECHMATE/Features/Game/GameView.swift` | Zapojení trenéra, `@AppStorage("czechmate.coach.userLevel")`, volání `streamPositionPlanSummary(…, userLevel:)`. |
| `CZECHMATE/ContentView.swift` | `ChessCoachManager`, `LearningModeManager`, `ModelDownloadManager` jako `environmentObject`. |

**Odstraněno ze závislostí projektu:** Swift Package **LLM.swift** (llama.cpp) — nahrazeno MediaPipe / stubem.

---

## 3. MediaPipe & integrace

- Oficiální distribuce pro iOS: **CocoaPods** `MediaPipeTasksGenAI` (+ často **`MediaPipeTasksGenAIC`** pro session API podle ukázek).
- V kořenu **`CZECHMATE/Podfile`**; po integraci otevírat **`CZECHMATE.xcworkspace`**, ne jen `.xcodeproj`.
- Krátký postup je také v `context/phase5_coach/MEDIAPIPE_COCOAPODS.txt`.
- **Swift Package** s názvem MediaPipe GenAI pro iOS **není** primární cesta v dokumentaci Google AI Edge; kód je připraven na **`canImport(MediaPipeTasksGenAI)`**.

---

## 4. Model na disku

- Očekávaný název: **`coach_gemma_mediapipe.bin`** (FlatBuffer formát pro MediaPipe LLM Inference, ne GGUF).
- Umístění: adresář z `ModelDownloadManager` → typicky  
  `~/Library/Application Support/.../ChessCoachModels/coach_gemma_mediapipe.bin`.
- Zdroj modelu: např. Gemma 2B IT kvantizace podle [AI Edge – LLM Inference iOS](https://ai.google.dev/edge/mediapipe/solutions/genai/llm_inference/ios) / Kaggle.

---

## 5. Inference parametry (kód)

- **`LlmInference.Options`**: `maxTokens = 512` — u MediaPipe jde o **limit vstup + výstup**; krátký výstup vyžaduje rezervu pro dlouhý předprompt.
- **`LlmInference.Session.Options`**: `topk = 40`, `temperature = 0.7`, `topp = 0.9`.

---

## 6. Tok (krok za krokem)

1. `syncLearningModeAndModel` při zapnutí učícího módu + přítomnosti souboru modelu spustí **`loadModel`** (async).
2. Uživatel: **„Zhodnoť moji pozici“** → `requestPositionCoachEvaluation()` (nutný internet pro Stockfish dle stávající kontroly).
3. `streamPositionPlanSummary`: načte FEN / eval / UCI → **`buildMediaPipeGemmaCoachPrompt`** (`userLevel` z `UserDefaults`, režim např. **`wholePositionPlan`**).
4. Pokud model ještě není v RAM: text **„Trenér se připravuje…“** + `await loadModel`.
5. Pokud `gemma.isLoaded`: **`Task.detached`** → `streamResponse` → delta přes **`CoachAdviceDeltaBridge`** na MainActor do **`advice`**.
6. Po dokončení: **`registerCoachAdviceStreamCompleted()`**; při chybě záložní **`streamFallbackMock`** (text ve znění Czechmate + vysvětlení záložního režimu).

---

## 7. Prompting (strategie)

- **Persona:** oficiální **„AI Trenér“ aplikace Czechmate** — ne anonymní chatbot; mluví jménem produktu.
- **Data:** FEN, eval z pohledu bílého, tah UCI — vždy v bloku **DATA** na konci promptu (instrukce první, fakta poslední).
- **Úroveň hráče:** `coachPlayerLevelHint` (1–10) ladí hloubku a slovní zásobu.
- **Styl:** čeština, bez „jsem AI“, bez markdown nadpisů v odpovědi, délka na jednu bublinu (několik krátkých vět).
- **`buildPromptForLLM`** používá stejnou linku identity Czechmate pro případné další režimy.

---

## 8. Pravidla od uživatele (pro budoucí úpravy)

- Nemazat soubory bez schválení.
- Preferovat **`context/`** pro podklady; tento soubor je centrální kontext.
- Staging vs produkce: do staging kódu debug výpisy (`AppDebugLog.staging` kde už je zavedeno).
- Minimální rozsah změn — ne „refactor celého projektu“ bez důvodu.

---

## 9. Rychlé odkazy v repu

- Plánování: `docs/planning/` (MASTERPLAN, roadmap, gap web vs app).
- Starší poznámka k LLM: `context/phase5_coach/` (vč. MEDIAPIPE_COCOAPODS).

---

*Tento dokument byl sepsán jako konsolidovaný kontext pro AI asistenta v Cursoru; při větších změnách architektury ho aktualizuj.*
