# Lute fusion — goal checklist

This file is the durable checklist for the "Librum as base, all Lute features"
goal. Every round picks the first unchecked item in phase order, implements it,
verifies it with the stated evidence, commits, and then ticks it here.

Status values: `[ ]` open, `[~]` in progress, `[x]` done (evidence linked).

## Acceptance criteria

- [x] **C1 — One repo, one build**
  Product is `librum/` only. The standalone prototype was frozen into
  `../attic/standalone/` and is no longer built or referenced by the product
  (see `../FORK.md`, `../README.md`). Fork docs live in `librum/docs/`.
  Evidence: outer repo commit "Freeze the standalone prototype…"; `attic/README.md`.

- [ ] **C2 — Lute hub**
  Sixth sidebar entry "Lute" opens `MLuteHubPage` with the secondary sidebar:
  Texts, Terms, Statistics, Languages, Tags & Parents, Import/Export, Settings.

- [ ] **C3 — Library**
  Create books/texts by paste, .txt, .pdf, URL; Lute-conform pagination
  (250 tokens, paragraphs/sentences, `---` breaks); active/archived; edit,
  archive, delete; per-book statistics.

- [ ] **C4 — Text reader**
  Read pages with clickable words, status colours/underlines, phrase spans,
  RTL, romanisation; popup with status/translation/romanisation/sentence/tags/
  parents; mark page read, prev/next, slider, bookmarks; progress survives a
  restart.

- [ ] **C5 — Terms**
  List with search/filter (status/language/tags), sorting, bulk status, edit,
  delete, sentences view, TSV export.

- [ ] **C6 — Languages**
  CRUD, predefined languages from the Lute template, all parser fields
  (regexps, substitutions, RTL, romanisation, parser type), dictionaries.

- [ ] **C7 — Statistics**
  Status distribution, words read (day/week/month), terms created, book/text
  statistics, language selector, charts in Librum style.

- [ ] **C8 — Tags & word families**
  Tag CRUD and assignment; parent/child term map and book export.

- [ ] **C9 — Import/export**
  Term import (TSV/CSV) with preview/error report; Anki export specs + TSV;
  database backup export/import.

- [ ] **C10 — PDF integration**
  PDF word popup uses the same vocabulary; a PDF can be imported into the Lute
  library; the "Lute library" action works with and without an open PDF,
  reports its result and leads into the library.

- [ ] **C11 — Database compatibility**
  Existing `lute.db` opens without migration; schema untouched; Lute v3 can
  still open the file; roundtrip covered by a test.

- [ ] **C12 — Tests & QA**
  gtest suites for repositories/paginator/import/stats/export green; headless
  integration test (create text → read → set statuses → stats → export);
  `--lute-demo-*` screenshots of every page; packaged smoke test.

- [ ] **C13 — Docs & package**
  `LUTE.md`, `FORK.md`, this file complete; portable Windows package built and
  tested.

## Phase log

Newest entries first; each entry: what changed, evidence, commit.

- Phase 0 (in progress) — the "Lute library" action is functional without an
  open PDF: Settings > Lute has an "Add a PDF…" picker, imports via
  `LuteController::addPdfToLuteLibrary`, reports success/failure in a banner
  and stores the book+text with its real word count. The action no longer
  depends on `documentOpen`. QA: `--lute-demo-library <pdf> --out <png>`
  (works without a document). Evidence: `build/shot-library-demo3.png`,
  demo log "adding … true".
- Phase 0 — language selection fixed for real Lute databases: the selected
  language is now stored and matched by *name* (ids collide between different
  `lute.db` files), and a DB without English falls back to its first language.
  Verified with the user's Polish-only database copy: language "Polish"; fresh
  template: "English". New `--lute-db <path>` CLI flag selects a database.
- Phase 0 (started) — consolidation done: standalone frozen into
  `attic/standalone/`, root README/FORK.md rewritten, this checklist added.
  Next in Phase 0: the Lute library page (C2/C3) in Phase 2.
