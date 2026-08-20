# Feature Specification: Steerable Discovery & Training (Phase 3)

**Feature Branch**: `004-steerable-discovery-training`

**Created**: 2026-08-20

**Status**: Ready for planning / tasks

**Input**: User description: "Phase 3 — Steerable Discovery & Training (Steinmetz & Reiss steerable NAfx). Implement inside OpenYourBox, guided by ml_forge training UX patterns. Scope: (1) FiLM conditioning input on TCN, residual checkbox on TCN, PReLU in activation and TCN; (2) Capture Samples via paired VST instances recording clean x + processed y, copyright acknowledgment before Train; (3) non-blocking train worker with Adam + multiresolution STFT loss, ca=0 during steer, LR schedule, ~2500 steps, Run/Pause/Stop + live loss; audio thread keeps prior model; (4) on success auto-load trained model as Gold BlackBox with free c via Knob/XY."

## Clarifications

### Session 2026-08-20

- Q: When two plugin instances are paired for Capture Samples, what audio should the Clean and Processed roles record? → A: Clean = instance input (dry); Processed = instance input (wet from upstream in the DAW). During capture, both instances bypass their own graph processing by default so the DAW sound is unaffected; users may opt out of bypass to keep processing active.
- Q: Which plugin instance owns the trainable graph, the Train controls, and the Gold BlackBox auto-load after a successful run? → A: The instance where recording/capture was **initiated** is the master: it owns the steerable graph, Train UI, **training library**, and auto-load. The peer is a **slave** with a reduced recording menu (pairing/sync/role/record affordances only, no full Train workflow).
- Q: After a successful train on the master, what part of the master’s graph should auto-load replace with the Gold BlackBox? → A: Replace only the **trainable processing chain** (elements **armed** for training); keep Knob/XY and similar control sources outside as Blue nodes feeding conditioning. Each element has an arm/disarm control for training; elements are **armed by default**.
- Q: What maximum length should a single Capture Samples recording be allowed to reach before it auto-stops? → A: **No fixed maximum**; the user stops recording manually.
- Q: Should elements expose weight identity and browsing? → A: Yes. Each weight-bearing element has a **Weights** property that shows **seed N** when weights are random, or the **file path** when weights come from a trained/loaded file. From that property the user can **browse** and select other compatible weight files to load into the element.
- Q: Can Knob Input, XY Trackpad, and other non-processing control sources be armed into the trained Gold BlackBox, or must they always stay outside? → A: **Never absorbed** — only elements with **trainable parameters** participate in arm/train/auto-load. Control sources always remain Blue outside the Gold BlackBox.
- Q: Can a Gold BlackBox produced by train auto-load be unfrozen afterward? → A: **Yes** — same Phase 2 Unfreeze policy: user can unfreeze back to an editable modular (Blue) graph. Unfrozen elements **keep the trained weights** until the user **randomizes** or **retrains**.
- Q: How should training data be sourced and selected? → A: A **training library** holds sample pairs. **File import** of clean/processed pairs is allowed. **Capture Samples** recording **adds** pairs into that library. Before Train, the user **chooses** which library pairs to include.
- Q: Must users be able to reproduce steerable-nafx architecture and loss? → A: Other armed graphs are allowed, but the graph MUST be able to express the reference steerable TCN (FiLM per block, residual, PReLU, **dilation growth** schedule). Default train loss MUST match the reference multiresolution STFT configuration (FFT/window sizes 32/128/512/2048, hops 16/64/256/1024).
- Q: Best UI for dilation growth, cropping, and segment length? → A: **Growth**: integer Dilation growth slider (RONN-style) + live dilations/RF readout + optional presets (2/8/10); default G=2. **Cropping**: always RF-aware, no toggle; info line only. **Segment length**: hidden default ~228k samples (clamped); show Train window ≈ N s; long-term expose seconds, not sample counts.
- Q: Training library UX for the long term? → A: Dedicated Library panel (list+detail): multi-select, import, capture-add, rename, delete, x/y preview; roadmap for search/tags/collections, saved selections, export, disk usage, SR mismatch gating — see `contracts/training-library-ui-contract.md`.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Capture Paired Clean/Processed Samples Across Two Plugin Instances (Priority: P1)

A sound designer loads **two instances** of the plugin in their DAW: one on a clean (dry) path and one **after** the reference effect chain they want to learn (wet). They open **Capture Samples** on the instance that will own training (the **master**), which reveals a menu **inside the same plugin window**. The menu discovers and pairs with the other instance (**slave**) so both stay synchronized while recording; the slave shows a **reduced** recording menu (pairing/sync/role/record only—no full Train workflow). When recording, **both roles capture that instance’s audio input**: Clean records dry **x**, Processed records already-wet **y** from upstream DAW processing. **By default, both instances bypass their own graph processing** during capture so monitoring stays unaffected; the user may disable bypass to keep processing active. Captured pairs accumulate in the master’s **training library**. Before **Train** is available on the master, a blocking copyright acknowledgment must be completed (constitution). The user may also **import file-based** clean/processed pairs into the same library and **select** which library entries participate in a given training run.

**Why this priority**: Without x/y training data, steerable training cannot start. Dual-instance capture and file import both feed the training library.

**Independent Test**: (a) Import a file pair into the Library, select it, confirm preview; (b) optionally capture via two instances into the same library. Confirm Train is master-only and disabled until copyright acknowledgment.

**Acceptance Scenarios**:

1. **Given** a single plugin instance with no peer available, **When** the user opens Capture Samples, **Then** the in-window menu explains that a second instance is required and does not start recording
2. **Given** two plugin instances on the same machine/session, **When** the user opens Capture Samples on one instance and selects the peer, **Then** that instance becomes the **master**, the peer becomes the **slave**, and both show a clear paired/synced state
3. **Given** a paired slave instance, **When** the user opens its recording menu, **Then** only reduced capture controls are shown (no full Train / library management workflow)
4. **Given** paired instances, **When** the user assigns Clean (x) and Processed (y) roles, **Then** each instance displays its role and recording uses that assignment
5. **Given** paired and role-assigned instances, **When** the user starts recording while audio plays, **Then** both instances capture time-aligned clips of their **inputs** forming one x/y pair (Clean = dry input, Processed = wet input) until the user stops (no automatic maximum duration)
6. **Given** capture is active with default settings, **When** audio passes through either instance, **Then** each instance bypasses its own graph processing so the DAW-heard signal is unaffected by that instance’s model
7. **Given** the user turns off capture bypass, **When** recording or monitoring continues, **Then** that instance’s graph processing remains active while capture still records the configured input
8. **Given** a successful capture, **When** recording stops, **Then** the pair is **added** to the master’s **training library** and can be removed or kept
9. **Given** the training library UI, **When** the user imports a clean/processed file pair, **Then** that pair appears in the library alongside captured pairs
10. **Given** multiple library pairs, **When** the user prepares Train, **Then** they can select which pairs are included in the next run (at least one required)
11. **Given** no copyright acknowledgment on file, **When** the user views Train on the master, **Then** Train is disabled and a blocking modal requires certification that samples are original or royalty-free before Train enables
12. **Given** the user checks the acknowledgment, **When** the modal is confirmed, **Then** a local acknowledgment record is stored and Train becomes available on the master (when at least one **selected** library pair exists)

---

### User Story 2 - Train a Steerable Model Without Interrupting Live Audio (Priority: P1)

With at least one **selected** training-library pair and copyright acknowledgment complete, the user starts training from the **master** instance. Training runs in the background worker and does not stall or glitch the live audio path. The training panel (ml_forge-style) shows **Run**, **Pause**, and **Stop**, plus a live loss readout. During steering/training, the conditioning channel used for steerability is held at a neutral value (**ca = 0**). The optimizer and loss follow the steerable NAfx recipe: Adam; **multiresolution STFT loss** with FFT/window lengths **{32, 128, 512, 2048}** and hop sizes **{16, 64, 256, 1024}**; learning rate **1e-3 → 1e-4 at 80% of steps → 1e-5 at 95%**; approximately **2500** steps. Training uses **receptive-field-aware** audio crops (context of past samples equal to the model receptive field before each target segment). While training, the audio engine continues using the **previously loaded** model. On failure or stop, the user sees a clear status and the prior model remains active.

**Why this priority**: Non-blocking training with controllable progress is the core Phase 3 capability and a constitution non-negotiable (worker must never block the audio thread).

**Independent Test**: With ≥1 selected library pair and copyright ack, start Train, confirm audio keeps playing with the prior model, verify live loss updates and train-window info, Pause then Resume, then Stop without model replace. Run a full successful job and proceed to Story 3.

**Acceptance Scenarios**:

1. **Given** copyright acknowledgment and at least one **selected** library pair on the master, **When** the user presses Run/Train on the master, **Then** training starts in the background and the plugin UI remains responsive
2. **Given** training is running, **When** the DAW continues playback, **Then** audio uses the previously loaded model with no dropouts attributable to training load on the audio path
3. **Given** training is running, **When** the user views the training panel on the master, **Then** live loss (and step progress) update continuously
4. **Given** training is running, **When** the user presses Pause, **Then** optimization suspends and Resume continues from the same run
5. **Given** training is running or paused, **When** the user presses Stop, **Then** the run ends without loading a new model and the prior model stays active
6. **Given** a training run, **When** steering/conditioning is applied in the training recipe, **Then** the steer conditioning channel is held at ca = 0 for that phase as specified by the steerable NAfx method
7. **Given** a full training run, **When** it proceeds without user interrupt, **Then** it follows the prescribed learning-rate schedule (high → mid at ~80% of steps → low at ~95%) over ~2500 steps with the specified multiresolution STFT loss
8. **Given** a paired slave instance, **When** the user looks for Train controls, **Then** the full Train workflow is not available on the slave
9. **Given** only file-imported pairs (no DAW capture), **When** the user selects them and Runs Train, **Then** training proceeds with the same recipe as captured pairs

---

### User Story 3 - Auto-Load Trained Effect as Gold BlackBox and Steer with Knob/XY (Priority: P1)

When training completes successfully on the **master**, that instance automatically loads the trained model as a **Gold** frozen BlackBox that **replaces the armed trainable processing chain** (locked/optimized appearance). **Knob Input**, **XY Trackpad**, and similar control sources remain outside as Blue nodes and continue to feed conditioning **c**. Before training, the user can **arm or disarm** individual elements for inclusion in the training subgraph; elements are **armed by default**. The user can then drive free perceptual conditioning **c** with Knob/XY to morph the effect character in real time without retraining.

**Why this priority**: Delivering a steerable Gold effect is the user-visible payoff of capture + train; without auto-load and free-c control, Phase 3 is incomplete.

**Independent Test**: Arm the intended processing elements (or leave defaults), complete a successful training run, confirm those armed nodes become one Gold BlackBox while Knob/XY remain Blue and wired for conditioning, and verify moving the controls changes the effect timbre while audio continues.

**Acceptance Scenarios**:

1. **Given** training finishes successfully, **When** the result is ready, **Then** the armed trainable processing chain on the master is automatically replaced by a Gold BlackBox without requiring a separate Freeze action
2. **Given** the new Gold BlackBox is loaded, **When** audio plays, **Then** processing uses the newly trained model
3. **Given** Knob/XY (or similar) sources existed outside the armed chain, **When** auto-load completes, **Then** those sources remain Blue live nodes connected into the BlackBox conditioning path
4. **Given** a trained steerable Gold BlackBox, **When** the user adjusts Knob and/or XY Trackpad conditioning, **Then** the perceptual character of the effect changes in real time
5. **Given** auto-load succeeds, **When** the user inspects the node, **Then** Gold styling (and locked affordance) matches other frozen BlackBoxes from Phase 2
6. **Given** any element with **trainable parameters** on the master, **When** the user views its controls, **Then** an arm/disarm-for-training control is available and defaults to **armed**
7. **Given** a control source without trainable parameters (e.g., Knob Input, XY Trackpad, Audio Input), **When** the user inspects it, **Then** it is not armable for training and cannot be absorbed into the Gold BlackBox
8. **Given** the user disarms a trainable element, **When** Train runs, **Then** that element is excluded from the trained subgraph and is not absorbed into the resulting Gold BlackBox
9. **Given** the user re-arms a previously disarmed trainable element, **When** a later Train runs, **Then** that element is included again in the armed processing chain
10. **Given** a Gold BlackBox created by train auto-load, **When** the user chooses Unfreeze, **Then** the node reverts to an editable modular (Blue) graph per Phase 2 Unfreeze behavior and each restored trainable element **retains the trained weights** (Weights property continues to show the trained path/provenance)
11. **Given** an unfrozen graph still carrying trained weights, **When** the user randomizes weights or completes a new training run that replaces them, **Then** those prior trained weights are superseded; until then they remain active

---

### User Story 4 - Configure FiLM, Residual, and PReLU for Steerable TCN/Activation Graphs (Priority: P2)

A user building a trainable structure for steerable discovery configures a **TCN** that can match the reference steerable-nafx design: **FiLM** conditioning (wired from Knob, XY Trackpad, or Merge), **residual** path, **PReLU**, and a **dilation growth** factor so layer dilations follow growth^layer (same exponential schedule family as RONN’s dilation growth). Other armed architectures remain allowed; users who want paper parity can build the same conditional TCN. These options make the live graph capable of expressing the steerable NAfx-style conditional architecture before capture and train.

**Why this priority**: Architecture readiness enables Stories 1–3 but can be validated independently in the live graph without completing a full train.

**Independent Test**: Place TCN and Activation nodes, set residual on, PReLU, dilation growth (e.g. 8 or 10), connect a Knob/XY (or Merge) to the TCN FiLM conditioning input, and verify live processing accepts the graph and shape/connection rules hold.

**Acceptance Scenarios**:

1. **Given** a TCN element, **When** the user inspects its ports/properties, **Then** a dedicated FiLM/conditioning input is available for connection from Knob, XY Trackpad, or Merge
2. **Given** a TCN element, **When** the user enables the residual checkbox, **Then** the TCN uses a residual connection path; when disabled, it does not
3. **Given** a TCN element, **When** the user sets dilation growth G, **Then** successive layers use dilations following the exponential growth schedule G^layer (layer 0 → 1, then G, G², …)
4. **Given** an Activation Function element, **When** the user opens activation type choices, **Then** PReLU is available alongside existing activations
5. **Given** a TCN element, **When** the user configures its internal activation, **Then** PReLU is available
6. **Given** FiLM conditioning is connected, **When** the user adjusts the upstream Knob or XY Trackpad, **Then** live (pre-train) behavior reflects conditioning where the live engine supports it, without breaking audio
7. **Given** the user wants steerable-nafx parity, **When** they configure TCN with FiLM + residual + PReLU + dilation growth and feed 2D conditioning (e.g. XY), **Then** the graph expresses the reference conditional TCN family without requiring a separate non-graph tool

---

### User Story 5 - Inspect and Browse Element Weights (Priority: P2)

A user inspecting any weight-bearing element (live Blue modules or Gold BlackBox) sees a **Weights** property. If the element uses randomized weights, the property shows **seed N**. If the element uses trained or file-backed weights, it shows the **path** of those weights. From the same property the user can **browse** other weight files on disk and load a compatible file into that element without leaving the plugin.

**Why this priority**: Makes random vs trained provenance visible and lets users reuse trained weights across graphs; supports post-train workflows and RONN-style seed inspection.

**Independent Test**: Create a TCN with random weights and confirm Weights shows a seed; after a successful train/auto-load, confirm the Gold BlackBox Weights shows a path; browse and select another compatible weight file and verify the property updates and audio reflects the load (or a clear incompatibility error).

**Acceptance Scenarios**:

1. **Given** a weight-bearing element with randomized weights, **When** the user views its Weights property, **Then** it displays the seed identifier (e.g., seed N)
2. **Given** a weight-bearing element loaded from a trained or saved weight file, **When** the user views its Weights property, **Then** it displays the file path of those weights
3. **Given** the Weights property is visible, **When** the user chooses browse, **Then** they can select another weight file from the filesystem inside the plugin workflow
4. **Given** the user selects a compatible weight file, **When** load completes, **Then** the element uses those weights and the Weights property shows the new path
5. **Given** the user selects an incompatible weight file, **When** load is attempted, **Then** the element keeps its prior weights and the user sees a clear error

---

### Edge Cases

- What happens if the slave opens Capture Samples first? That instance becomes master for the new pairing session; the other becomes slave (initiator = master).
- What happens if the peer instance disconnects mid-recording? Recording stops, the incomplete pair is discarded or marked invalid, and both UIs show unpaired status.
- What happens if the user starts Train with zero **selected** library pairs? Train remains disabled with a clear reason.
- What happens if training fails (worker crash, invalid graph, out of memory)? User sees a non-blocking error; prior model and library remain; audio uninterrupted.
- What happens if selected pairs have mismatched sample rates? Train is blocked with a clear message (v1).
- What happens if the user edits the graph while training? Training continues against the snapshot submitted at Run; live graph edits do not mutate the in-flight job (Train may be disabled while a job is active).
- What happens if only one role is assigned or both instances claim Clean? Pairing UI refuses Record until roles are complementary (one Clean, one Processed).
- What happens when capture ends with bypass still on? Bypass restores the user’s pre-capture processing state when leaving capture/record (default).
- What happens on copyright acknowledgment across sessions? Stored local acknowledgment persists so the modal is not re-required every launch (first-training gate per constitution); Train still requires ≥1 selected pair.
- What happens if auto-load fails after a successful train artifact is produced? User sees an error; prior model stays active; a retry-load action is offered without re-running the full train when possible.
- What happens if no elements are armed when Train is pressed? Train is refused with a clear message requiring at least one armed element with trainable parameters.
- What happens to disarmed elements on successful auto-load? They remain as live Blue nodes outside the new Gold BlackBox.
- What happens to Knob/XY/Audio Input on train auto-load? They are never part of the armed trainable set and always remain outside the Gold BlackBox.
- What happens to weights on Unfreeze after train? Trained weights remain on the restored Blue elements until the user randomizes or retrains (successful new train / explicit weight reload that replaces them).
- What happens if the user browses an incompatible weights file? Prior weights remain; clear error; no audio-thread stall.
- What happens to the Weights property after successful train auto-load? The new Gold BlackBox shows the path of the trained weights artifact.
- What happens if dilation growth and depth yield an impractically large receptive field? UI RF readout warns; live engine may refuse unsafe configs per existing performance/shape gates (planning detail).

## Out of Scope

- Cloud training, marketplace, and remote sample packs (Phase 4)
- Embedding ml_forge / Dear PyGui as a separate application
- User-editable optimizer/loss/LR/step-count experiment panel in v1 (recipe is fixed)
- Per-layer manual dilation editors; disabling RF-aware cropping
- Cross-machine instance pairing
- In-library destructive audio editing (trim/fade) in v1
- Automatic resample of mixed-SR library selections in v1 (block instead)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The plugin MUST provide a Capture Samples control that opens an in-window menu (not a separate application) for pairing, role assignment, and recording.
- **FR-001a**: The instance that **initiates** Capture Samples / recording pairing MUST become the **master**; the paired peer MUST be the **slave**. The master MUST own the **training library**, Train controls, steerable architecture submitted to training, and Gold BlackBox auto-load. The slave MUST show a **reduced** recording menu (pairing, sync, role, record/bypass) without the full Train workflow.
- **FR-002**: Capture MUST support discovering and pairing two plugin instances so recording of clean **x** and processed **y** stays synchronized.
- **FR-003**: Each paired instance MUST be assignable as Clean (x) or Processed (y), and Record MUST require complementary roles.
- **FR-004**: Recording MUST produce time-aligned x/y sample pairs from each instance’s **audio input** (Clean = dry input; Processed = wet input after upstream DAW processing), with duration controlled by user start/stop and **no fixed maximum** length, and **add** them to the master’s **training library**.
- **FR-004a**: During capture, both paired instances MUST **bypass their own graph processing by default** so the audible DAW path is unaffected; the UI MUST offer an option to disable bypass and keep processing active while still recording inputs.
- **FR-004b**: Users MUST be able to **import file-based** clean/processed sample pairs into the same training library (without dual-instance capture).
- **FR-004c**: Before Train, users MUST be able to **select** which training-library pairs participate in the run; Train MUST require at least one selected pair.
- **FR-004d**: The master MUST provide a dedicated **Training Library** UI (list + detail) supporting at least: browse entries (name, duration, sample rate, channels, source Capture|Import, created time), multi-select for Train, import, rename, delete (with confirm), and preview playback of x and/or y inside the plug-in.
- **FR-004e**: Library entries MUST persist across sessions in local user data. Capture and import MUST both appear in the same library. Mixed sample-rate selections MUST be blocked at Train with a clear message (v1).
- **FR-004f**: Library UI MUST be designed for long-term growth: reserved support for tags/notes, search/filter, named collections or saved selection sets, export of pairs, and disk-usage visibility (may ship incrementally after the FR-004d core).
- **FR-005**: Before the first training session, a blocking copyright acknowledgment modal MUST appear with certification text that samples are original work or royalty-free; Train MUST stay disabled until acknowledged; acknowledgment MUST be stored in a local log.
- **FR-006**: On the master, Train MUST send the training job (snapshot of **armed** elements on the master’s graph + **selected** library pairs) to the background worker and MUST NOT block, stall, or interrupt the real-time audio thread; while training, audio MUST continue with the previously loaded model.
- **FR-007**: The master’s training UI MUST provide Run, Pause, and Stop controls and a live loss (and progress) display consistent with ml_forge-style training UX.
- **FR-008**: Training MUST use the steerable NAfx recipe: Adam optimizer; **multiresolution STFT loss** with FFT/window sizes **{32, 128, 512, 2048}** and hop sizes **{16, 64, 256, 1024}**; steer conditioning held at **ca = 0**; learning-rate schedule **1e-3 → 1e-4 at 80% of steps → 1e-5 at 95%**; approximately **2500** steps (user Stop/Pause may end early).
- **FR-008a**: Training MUST apply **receptive-field-aware cropping**: each optimization step feeds a target **segment** of audio of configured segment length, with preceding context of at least the model’s receptive field so every target sample has a full causal history.
- **FR-008b**: Other armed architectures MAY be trained, but the product MUST allow users to construct a graph equivalent to the reference steerable conditional TCN (FiLM-conditioned blocks, residual, PReLU, dilation growth schedule).
- **FR-008c**: RF-aware cropping MUST always be applied (no user disable in v1). Segment length MUST default to the steerable-nafx-scale value (~228308 samples), clamped to available audio after RF context. Primary UI MUST NOT require users to enter sample counts; Train panel SHOULD show an informational train-window duration in seconds.
- **FR-009**: On successful training completion, the **master** MUST automatically replace the **armed trainable processing chain** with a Gold BlackBox; control sources without trainable parameters (Knob Input, XY Trackpad, Audio Input, and similar) MUST always remain outside as live Blue nodes feeding conditioning.
- **FR-009a**: Only elements with **trainable parameters** MUST expose an **arm/disarm for training** control; those elements MUST be **armed by default**. Elements without trainable parameters MUST NOT be armable and MUST NEVER be absorbed into the auto-loaded Gold BlackBox. Disarmed trainable elements MUST be excluded from the training snapshot and MUST NOT be absorbed into the Gold BlackBox. Train MUST require at least one armed trainable element.
- **FR-009b**: A Gold BlackBox produced by train auto-load MUST support **Unfreeze** using the same Phase 2 policy as manually frozen BlackBoxes, restoring an editable modular (Blue) graph. Unfreeze MUST **preserve trained weights** on restored elements; those weights MUST remain active until the user **randomizes** them or **retrains** (or otherwise loads replacement weights).
- **FR-010**: After successful load, users MUST be able to control free perceptual conditioning **c** via Knob Input and/or XY Trackpad connected in the graph (remaining outside the Gold BlackBox).
- **FR-011**: TCN elements MUST expose a FiLM conditioning input intended for connection from Knob, XY Trackpad, or Merge.
- **FR-012**: TCN elements MUST offer a residual connection option via checkbox.
- **FR-012a**: TCN elements MUST expose a **dilation growth** parameter such that layer dilations follow an exponential growth schedule (growth^layer), enabling RONN/steerable-nafx-style receptive-field scaling (not limited to fixed power-of-two-only schedules).
- **FR-012b**: Dilation growth UI MUST use an integer control labeled consistently with RONN (**Dilation growth**), show a live readout of the resulting dilation sequence and receptive field (samples and ms), and MAY offer presets that set common values (including 2, 8, and 10). Default growth for new TCN elements MUST be **2**.
- **FR-013**: Activation Function and TCN elements MUST include PReLU as a selectable activation.
- **FR-014**: UI responsiveness MUST remain usable during training (constitution: UI remains fluid under backend load); audio path MUST meet existing live/frozen latency expectations with the active model.
- **FR-015**: Weight-bearing elements MUST expose a **Weights** property that displays **seed N** when weights are random, or the **file path** when weights are trained/file-backed.
- **FR-016**: From the Weights property, users MUST be able to browse and load other compatible weight files into that element; incompatible files MUST be rejected without changing the active weights, with a clear error.

### Key Entities

- **Plugin Instance Pairing**: Master/slave link between two plugin instances with sync state and complementary Clean/Processed roles; master is the initiator of the capture session.
- **Sample Pair (x, y)**: Aligned clips of clean **x** and processed **y** (from capture and/or file import), plus metadata (duration, source: capture|import, capture time/bypass when applicable); duration is user-gated with no fixed maximum for captures.
- **Training Library**: Master-owned collection of sample pairs; capture and file import both **add** entries; user selects which entries feed a Train run.
- **Copyright Acknowledgment**: Local record that the user certified sample rights before training.
- **Training Job**: Submitted **armed**-element architecture snapshot, **selected** library pairs, recipe settings (including segment length and RF-aware cropping), run state (running/paused/stopped/failed/succeeded), live loss, step index.
- **Training Arm State**: Per **trainable-parameter** element flag indicating inclusion in the next training subgraph; default armed; user-toggleable; not applicable to control/source elements without trainable parameters.
- **Element Weights**: Per weight-bearing element provenance — either random **seed N** or a **file path** to trained/loaded weights; browsable for alternate compatible files.
- **Steerable Gold BlackBox**: Trained frozen effect node replacing the armed processing chain; accepts audio **x** and conditioning **c** for perceptual control; Weights property shows the trained artifact path.
- **TCN (steerable)**: Temporal block with FiLM conditioning input, optional residual path, PReLU option, and **dilation growth** (exponential per-layer dilations).
- **Activation (extended)**: Activation element including PReLU among types, retaining existing gain-as-slope behavior from Phase 2.2.
- **Train Segment Length**: Number of target audio samples optimized per training step (after RF context); controls how much of the pair is scored each iteration.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A user can either (a) pair two instances and capture a pair into the library, or (b) import a file pair into the library, then select it for Train—each path completable in under 5 minutes without leaving the plugin window.
- **SC-002**: Train remains unavailable until copyright acknowledgment is completed; after acknowledgment, Train enables when at least one **selected** library pair exists.
- **SC-003**: During an active training run, continuous DAW playback shows no training-induced audio dropouts, and the audible model remains the pre-train model until success auto-load.
- **SC-004**: Users can Pause and Resume a run and Stop a run so that Stop never replaces the active model; at least 95% of interrupted-stop tests leave the prior model active.
- **SC-005**: A successful default-length training run (~2500 steps) surfaces live loss updates at least once per second on average during the run, and on completion the armed processing chain is replaced by a Gold BlackBox without a separate manual freeze step while external Knob/XY sources remain usable.
- **SC-006**: After auto-load, adjusting Knob and/or XY Trackpad conditioning produces an audible change in effect character within one control gesture for 90% of test attempts.
- **SC-007**: In the graph editor, users can enable TCN residual, set dilation growth, select PReLU on Activation and TCN, and connect FiLM conditioning from Knob/XY/Merge with shape-legal connections on the first attempt in guided tests.
- **SC-008**: Users can disarm an element before Train and verify it remains a live Blue node outside the auto-loaded Gold BlackBox; re-arming includes it in a subsequent run.
- **SC-009**: For a random-weight element, Weights shows a seed; after train auto-load or file load, Weights shows a path; browsing a compatible file updates the property and active weights in under 30 seconds in guided tests.
- **SC-010**: With a steerable-nafx-equivalent TCN graph (FiLM + residual + PReLU + dilation growth) and a selected library pair, a default train run uses the specified multiresolution STFT configuration and completes with free-c steering via XY/Knob.
- **SC-011**: In the Training Library, a user can import a pair, rename it, preview x and y, multi-select for Train, and delete with confirm—completing that loop in under 3 minutes in guided tests.

## Assumptions

- Phase 2 (embedded builder, freeze/BlackBox, Python worker IPC) and Phase 2.2 (Knob Input, XY Trackpad, Merge, activation/TCN gain) are available baselines this feature extends.
- Pairing is limited to two instances on the same machine/session (same DAW project); cross-machine pairing is out of scope.
- Captures are user-gated record start/stop with **no fixed maximum** duration; users are responsible for practical clip lengths; captures **add** to the training library (they do not exclusively define the train set).
- File-imported pairs and captured pairs share one library; Train uses only **user-selected** entries.
- Default **segment length** ≈ **228308** samples (steerable-nafx), clamped to available audio after RF; **hidden** from primary UI; Train shows `Train window ≈ N s`. Long-term: optional seconds control.
- RF-aware cropping is **always on** (no toggle).
- Dilation growth UI: RONN-style integer control, default **2**, presets 2/8/10, live RF/dilation readout.
- Training library UX: `contracts/training-library-ui-contract.md`.
- Complementary Clean/Processed roles are explicitly chosen in the Capture menu (not inferred solely from graph contents). Master/slave is determined by which instance initiated Capture Samples pairing, independent of Clean/Processed role assignment (master may be Clean or Processed).
- Reference effects to clone live **upstream** of the Processed instance in the DAW; OpenYourBox records inputs rather than substituting for that reference during capture.
- Capture bypass defaults on for both roles; opt-out is per capture session and does not change what is recorded (always instance input).
- Training submits a snapshot of the master’s **armed** steerable processing elements at Run; live graph edits during a run do not alter that job (Train may be disabled while a job is active).
- The steerable NAfx training hyperparameters named in FR-008 / FR-008a (including MR-STFT sizes and RF-aware crops) are product requirements for V1 of this phase, not optional experimenter knobs—advanced overrides are out of scope unless added later.
- FiLM conditioning input on TCN is a deliberate Phase 3 addition relative to Phase 2.2’s “no new ports” guidance; Phase 3 supersedes that constraint for TCN FiLM only.
- **Dilation growth** on TCN aligns with RONN’s exponential dilation schedule (layer dilations 1, G, G², …). OpenYourBox’s prior power-of-two-only shift schedule is insufficient alone for arbitrary G (e.g. 8 or 10 as in steerable-nafx demos).
- Control sources (Knob Input, XY Trackpad, Audio Input, and similar elements **without trainable parameters**) are **never** included in training or the Gold BlackBox; only elements with trainable parameters are armable.
- Auto-load replaces the armed trainable processing chain with one Gold BlackBox on the master consistent with Phase 2 freeze presentation; **Unfreeze is supported** afterward under the same Phase 2 policy as manually frozen BlackBoxes, and **keeps trained weights** until randomize or retrain.
- Weight-bearing elements include at least TCN, other parameterized processing modules, and Gold BlackBox nodes; source-only elements without weights omit the Weights property.
- ml_forge guides training-panel UX patterns (Run/Pause/Stop, live loss), not a requirement to embed the ml_forge application itself.
- Marketplace/cloud training (Phase 4) remains out of scope.
