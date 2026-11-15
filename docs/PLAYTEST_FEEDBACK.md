# Playtest feedback log

Owner's feedback after each checkpoint, verbatim where possible, plus what we did about it.
Format:

## CPx.y — YYYY-MM-DD
- **Feedback:** ...
- **Action:** what changed / which CP it went into / declined (why)

---

## CP0.0 — 2026-11-14
- **Feedback (verbatim):** "it looks okayish, I think we can have better and a bit more human looking avatars for
  the players, maybe taking inspiration from sensible soccer or maybe google research football like characters,
  think thoroughly on this, as it will be part of the user experience? Also, make sure the player can jump for
  headers also, even if it is in further checkpoints, but atleast make sure it is taken care of."
- **Action:** CP0.0b "Human avatars" inserted before CP0.1 — procedural 3D-skeleton players (D-009), presets to
  compare by eye, pose gallery + demo mode. Headers: jump state added to the sim now (D-010); playable header
  in CP0.2, AI aerial duels CP0.3, corners CP0.4.

## CP0.0b — 2026-11-14
- **Feedback:** "modern look, wide zoom, contextual headers fine." Asked whether the architecture supports (1) bigger /
  per-format pitch sizes and (2) future training of flair moves (bicycle kicks, rabonas, "Messi + Neymar" style).
- **Action:** defaults set (D-011). (1) answered: pitch is config; formations/markings/camera to be made format-aware
  (D-002 note). (2) answered with walkthrough; committed to parametric moves (D-012) starting CP0.1.
