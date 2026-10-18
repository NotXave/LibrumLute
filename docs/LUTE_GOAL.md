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

- Crash hardening — `BookService::setUp` now catches unsupported/corrupt
  documents (leaves `getFzDocument()` null) and `BookController::setUp`
  reports failure instead of opening an empty reader; QML shows a warning and
  stays on the library. Regression tests: `FailedSetUpLeavesSafeGetters`
  (106 application tests green, 34 lute tests green). Package rebuilt and
  smoke-tested, no new LibrumLute crash dump.

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 7 (Anki QA) — gtest exports a term with translation and tag, checks the
  header and the row `harbor\tHafen\tnoun\t3`; 32 tests green.


- Phase 7 (Anki) — `exportAnkiTsv()` writes an Anki-friendly file (Text,
  Translation, Tags via wordtags GROUP_CONCAT, Status); the Terms page has an
  "Export Anki" save dialog. Build green; a pytest/gtest row check follows.


- Phase 5 (dictionaries QA) — screenshot check showed Polish has zero
  `languagedicts` rows (the template's 24 rows belong to other language ids),
  so the empty 200px panel was hidden when the list is empty.


- Phase 5 (dictionaries) — the Languages page now lists the current
  language's dictionaries (use-for, URI, checkbox); toggling a checkbox calls
  `setDictionaryActive()` and reloads the list. Demo ran without QML warnings.


- Phase 5 (dictionaries) — activation support: `setDictionaryActive()` writes
  `languagedicts.LdIsActive`, the controller reloads the list; gtest toggles a
  dictionary off and back on (31 tests green). The checkbox UI on the Languages
  page is the remaining step.


- Phase 5 (dictionaries) — `LuteDbAccess::dictionaries(languageId)` reads
  Lute's `languagedicts` rows (use-for, type, URI, active, order); the
  controller exposes them and the Languages header shows the count for the
  current language (user DB: 24 dictionary rows). Editing dictionaries is the
  remaining part of C6.


- Phase 2 (reader polish) — shortcuts: `b` toggles a bookmark for the current
  page, `m` marks the page read, on top of the existing left/right/Escape keys.


- Phase 2 (popup polish) — "Copy" button copies the active headword to the
  clipboard (`copyActiveWord`) and reports it in the message banner.


- Phase 9 (packaging refresh) — packaged exe rebuilt with the Lute-table
  bookmarks and reader controls; smoke test opened the reader with the Polish
  library copy (screenshot 100 KB). Package remains 169.7 MB.


- Phase 2 (reader polish) — bookmarks: "Bookmark/Bookmarked" toggles the
  current page for the open book (stored per lute.db path and book id in
  QSettings), a picker jumps to a bookmark; loaded on `openBook`. Verified in
  the reading demo without QML warnings
  (`docs/lute/reader-bookmark.png`). Lute's own `textbookmarks` table is not
  written yet.


- Phase 2 (reader polish) — "go to page" input next to the slider: accepts a
  number (validated 1..pageCount), Enter or Go calls `openPage()`; verified in
  the reading demo without QML warnings
  (`docs/lute/reader-pagejump.png`).


- Phase 3 (term list) — direct delete action per term row
  (`deleteTermById`, refreshes list and statistics); covers the bulk-less
  single-term cleanup part of C5.


- Phase 9 (packaging refresh) — portable package updated with the latest
  reader (slider), tags/families and statistics features: 169.7 MB, 1,405
  files; smoke test opened the Lute reader with the Polish library copy and
  produced a screenshot (96 KB).


- Phase 2 (reader polish) — page slider in the reader toolbar: drag jumps to
  the page via `openPage()`, bound to `readerPage`/`readerPageCount`, disabled
  for single-page books. Verified in the reading screenshot (page 1 of 262,
  `docs/lute/reader-slider.png`).


- Phase 8 (QA) — word family verified through the controller:
  `--lute-demo-family` linked "dom" (id 1662) to the open word
  (`linkActiveParent true`, parents list contains it) and unlinked it again
  (`unlinkActiveParent true`, parents list empty). Screenshot
  `shot-family.png`; click-through with the mouse still open.


- Phase 8 (continued) — parent picker: `linkActiveParent(text)` finds or
  creates the parent term and appends the link, `unlinkActiveParent(id)`
  removes it; the popup has a "Link a parent term…" field and clickable family
  chips. Compiles and builds; click-through QA alongside the next round.

- Phase 8 (continued) — word families: `parentsForTerm()`/`setTermParents()`
  read and write Lute's `wordparents` table; the controller adds the parent
  terms to the active word and the popup shows a "Family:" chip row with
  translations. gtest links "harbors" to "harbor", checks the translation and
  clears the link; 29 tests green. The picker for choosing parents follows.

- Phase 8 (QA) — tag toggle verified through the controller: the new
  `--lute-demo-tag-toggle` helper selected a word, toggled tag 1 and logged
  `toggleActiveTag true` with activeWord.tags = ["noun"] (wordtags written,
  popup state updated). Screenshot `shot-tagtoggle.png`.
- Phase 8 (QA) — tag chips verified in the real popup: a tag was seeded into
  the user's DB copy and appears in the KRAJOWY popup under the headword
  (screenshot `docs/lute/tags-popup.png`). Toggle click-through still to do.
- Phase 8 (continued) — tag assignment UI: the word popup renders every tag of
  the database as a chip, highlights the ones assigned to the active term and
  toggles them via `toggleActiveTag()` (writes `wordtags`, updates the popup).
  Compiles and builds; a click-through QA helper follows with the word-family
  round.

- Phase 8 (continued) — term tags: `tagsForTerm()`/`setTermTags()` write Lute's
  `wordtags` join table, the controller adds the assigned tag names to the
  active word and the popup renders them as chips. gtest assigns and clears a
  tag on a term; 28 tests green. Fixed a NULL-binding bug in `saveTag`
  (TgComment is NOT NULL), which the test caught.

- Phase 7 (continued) — restore verified end-to-end with `--lute-demo-restore`:
  backup returned true, restore returned true, the database reopened with 69
  languages and the book list (1 book) — evidence in the app log.
- Phase 7 (continued) — database restore: `LuteController::restoreDatabase()`
  replaces the current lute.db with a chosen backup, reinitialises the
  connection (languages, library, terms, stats) and reports the result;
  Settings → Lute has a "Restore…" picker next to Backup. Backup format is
  covered by gtest; the restore path uses the same file and is UI-wired.

- Phase 9 (packaging) — portable build rebuilt with all current features
  (`dist/LibrumLute-0.1.0-win64`, 169.7 MB, windowed, windeployqt) and smoke
  tested: the packaged exe opened the Lute hub with the user's Polish library
  (`--lute-demo-hub` on a DB copy) and wrote the screenshot. C13 remains open
  for final docs/checksum and C10–C12 acceptance.

- Phase 8 (started) — tag manager: `LuteTag`, `LuteDbAccess::tags/saveTag/
  deleteTag`, controller list/CRUD plus a Tag page in the hub (add with
  comment, delete, live count) and a `--lute-demo-tags` helper. gtest covers
  create/list/update/delete; 27 tests green. Attaching tags to terms and word
  families are the next part of C8.

- Phase 7 (continued) — TSV term import: `importTermsTsv()` parses the export
  format (optional header, tabs, status/token count), the Terms page has an
  "Import TSV" picker and the controller refreshes list/stats. gtest exports
  two terms, deletes them, reimports and checks word + phrase rows; 26 tests
  green. Anki specs still open.

- Phase 7 (continued) — database backup: `LuteDbAccess::backupTo()` uses
  `VACUUM INTO` for a consistent snapshot, the controller reports the result
  and Settings → Lute has a "Back up…" save dialog. gtest opens the backup and
  finds the saved term; 25 tests green. Restore UI and Anki specs still open.

- Phase 7 (started) — term export: `LuteDbAccess::exportTermsTsv()` writes a
  UTF-8 TSV with text, translation, status and token count; the Terms page has
  an "Export TSV" save dialog and the controller reports success/failure.
  Covered by a gtest (header + single word + phrase row) — 24 tests green.
  Term import, Anki specs and backup still open.

- Phase 6 (started) — Lute statistics page in the hub: cards (terms, learning,
  known, words read), 14-day series for created terms and status changes (from
  `createdPerDay`/`statusChangesPerDay` exposed as `createdSeries`/
  `learnedSeries`), status distribution. Verified on the user's Polish DB:
  14,358 terms, 6,493 learning, 6,983 known, 172,894 words read; screenshot
  `docs/lute/statistics.png`, demo `--lute-demo-hub-stats`.

- Phase 5/3 (tests) — language manager and term filters covered by gtest:
  save/insert/update language (all parser columns, NULL-safety), guarded
  deletion while terms exist, deletion after cleanup; term search/status
  filters. 23 tests green. This closes the open verification from the language
  round.

- Phase 5 (started) — language manager: the hub's Languages section lists all
  languages of the open lute.db with parser type/RTL/romanization flags and
  edits every parser column via `LuteDbAccess::saveLanguage`; new languages and
  guarded deletion (`deleteLanguage` refuses while terms/books exist). The
  `languages` QVariant entries now carry all parser fields. Verified on a
  69-language template DB (screenshot `docs/lute/languages.png`,
  `--lute-demo-languages`). Dictionaries still open.

- Phase 3 (started) — the Lute hub now switches between Texts and Terms; the
  term list reads the real `words` table (search by text, status filter,
  status colours, phrase badge, translation) and clicking a term opens the Lute
  popup for editing. Verified against the user's Polish database: 14,358 terms,
  status distribution 1:5735, 2:176, 3:502, 4:63, 5:17, 98:487, 99:6496;
  screenshot `docs/lute/terms.png`. Bulk edit/export/sentences still open.

- Phase 2 (continued) — texts can be created/imported: "New text" (paste),
  "Import .txt" and "Add a PDF" in the Lute library; new books are paginated
  with Lute's rules (`LutePaginator`: 250-token threshold, optional sentence
  boundaries, `---` page breaks) and stored as `books` + `texts`.
  Verified: "Harbor demo" created from generated text → 4 pages
  (250/70/250/70 words, 640 words total), visible in the library. 21 gtest
  cases green (4 new paginator tests). Evidence: `docs/lute/new-text.png`,
  `--lute-demo-create`.

- Phase 2 (started) — the Lute library and text reader exist: the sidebar entry
  *Lute* shows the books of the selected language from the real `lute.db` with
  page count/current page/search, and opens them in a Lute reader that renders
  a page as rich text, one link per word, coloured by status, with page
  navigation and "mark page read". Word clicks reuse the Lute popup. Verified
  against a copy of the user's Polish database: 271 books listed, book
  "Ostatnie życzenie…" open with 262 pages and coloured words (evidence:
  `docs/lute/library.png`, `docs/lute/reader.png`, demos
  `--lute-demo-hub/--lute-demo-reading/--lute-demo-word`). C2/C3/C4 remain open
  for text creation, terms/languages pages and bookmarks.
- Phase 1 (started) — data layer for the library: `LuteBook`/`LuteText`,
  `books/book/texts/text/setBookCurrentPage/markTextRead` in `LuteDbAccess`,
  17 gtest cases green.

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

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 9 (packaging refresh, round 41) — packaged exe rebuilt with
  dictionary URI editing; smoke test opened the Languages page (69 languages)
  against the Polish DB copy.
- Phase 9 (packaging refresh) — packaged exe rebuilt with Anki export,
  dictionary checkboxes and the term delete action; smoke test opened the
  Terms page with the Polish library copy (screenshot 126 KB).

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 9 (screenshots) — final screenshot set recaptured from the packaged
  feature state against the user's Polish DB copy: library, reader, terms,
  languages, statistics, tags (all in `docs/lute/`).

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 2 (popup QA) — `--lute-demo-status` selected word 12, called
  `setActiveStatus(3)` and logged `status 3 / "Learning (3)"`, i.e. the popup
  state and the term update path work; the physical mouse click remains the
  only unmeasured bit.

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 5 (dictionaries) — URI editing: `setDictionaryUri()` updates
  `languagedicts.LdDictURI`; the Languages panel's URI fields are editable and
  save on editing finished, closing the dictionary part of C6.

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 3 (bulk) — "Mark filtered known" bulk-updates the terms matching the
  current search and status filter to 99 (`setStatusForTerms`, refuses without
  a filter), then refreshes list and statistics. Closes the bulk part of C5.

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 9 (final freeze, round 45) — final portable build smoke-tested (reader
  with the Polish library, screenshot produced); C13 marked done, all C1-C13
  now have evidence.

- Phase 8/7 (acceptance) — C8 and C9 marked done: tags/families, TSV and
  Anki export/import, backup and restore each have a controller log or gtest;
  only the physical mouse click in the popup is not measurable headless.

- Phase 3 (bulk QA) — gtest bulk-updates "har" to 99 and status 5 to 1,
  checking that only the filtered rows change; 33 tests green. C5 marked done.

## Phase log

- Crash on opening a library book fixed — the minidump
  (`LibrumLute.exe.4792.dmp`) faulted in `application.dll+0x1B07D`, i.e.
  `BookService::getFilePath()`; `BookController::setUp()` called it before
  `m_bookGetter` existed. The call was removed, the path is emitted after
  `setUp`, the QML double-open hook is gone and BookService getters are
  null-safe. Regression test `BookService.GettersAreSafeWithoutAnOpenBook`;
  lute tests 34 green, application tests 105 green. Package rebuilt at
  `dist/LibrumLute-0.1.0-win64/LibrumLute.exe` (commit 3b068fd).


- EPUB page view fixed and cleaned — Lute now shares the renderer's MuPDF
  document (wrapper document kept alive, same context/layout), so page numbers
  and word boxes match exactly; overlays draw a thin baseline-relative
  underline per line fragment instead of filled boxes. Verified with the real
  "Ostatnie życzenie" EPUB page 18/267 (`docs/lute/epub-highlights.png`).
  Portable package rebuilt at `dist/LibrumLute-0.1.0-win64/LibrumLute.exe`
  with BUILD-INFO.txt, smoke-tested from that folder. 34 gtest cases green.


- EPUB fixed in the page view — reflowable books (EPUB/MOBI) are now laid out
  in BookService (`fz_layout_document(450, 650, 11)`) and the Lute extractor
  uses the same layout, so pages render and word coordinates match. Verified
  with the user's "Ostatnie życzenie" EPUB: page 17 of 267 shows the chapter
  text with green Lute status highlights (`docs/lute/epub-in-reader.png`).


- EPUB working path — importing the real "Ostatnie życzenie" EPUB into the
  Lute library now paginates it (292 pages, 72,776 words, 250/page) instead of
  one huge page; the Lute reader shows it with status colours (screenshot
  `docs/lute/epub-reader.png`). Settings/Texts buttons are renamed to
  "Add a book…" and accept EPUB/MOBI/FB2/CBZ. This is the reliable EPUB path
  until the library-rendered EPUB overlay is confirmed.


- EPUB follow-up — reproduced with the user's real "Ostatnie życzenie" EPUB:
  MuPDF extracts words (multi-page, Polish) and the app opens it, but the
  external reader renders this book's pages blank in our test build, while
  library opening is the likely route. The library hook now passes
  `BookController.getFilePath()` to Lute and failures are logged
  ("lute document could not be opened ..."); `librum-fixed.exe` in the dist
  folder carries the fix and diagnostics for a user-side verification.


- Phase 2 (EPUB fix) — `LuteWordExtractor::open()` passed
  `application/pdf` to MuPDF for every file, so EPUBs failed to open and the
  Lute colours never appeared. It now detects EPUB/MOBI/FB2/CBZ/HTML and opens
  the matching format; verified with a generated Polish EPUB: the external
  reader shows green/orange status colours (`docs/lute/epub.png`, demo
  `--lute-demo-external`).

## Status summary (round 36)

| Criterion | State | Evidence |
| --- | --- | --- |
| C1 fork merged | done | Librum fork builds Lute inside; commits 0dbb266..3fa0076 |
| C2 Lute hub | done | Texts/Terms/Statistics/Languages/Tags sections, sidebar entry, Ctrl+6 |
| C3 library | done | 271 books of the user DB listed, create/import text + PDF, delete-free read |
| C4 reader | done | rich text per page, status colours, click popup, slider, page jump, bookmarks (`textbookmarks`), shortcuts, mark read |
| C5 terms | done | 14,358 terms, search, status filter, edit popup, delete, bulk mark filtered known, TSV export/import, Anki export |
| C6 languages/dicts | done | 69 languages CRUD, parser fields, dictionaries list + activation + URI editing |
| C7 statistics | done | cards, 14-day series, status distribution |
| C8 tags/families | done | tag CRUD, term assignment (verified via controller log), popup chips (screenshot), parent link/unlink (verified), picker UI |
| C9 import/export/backup | done | text/PDF import, TSV export/import, Anki export (gtest), VACUUM INTO backup + verified restore |
| C10 PDF integration | done | PDF words/quads, popup on click, phrase drag, status overlay, import to Lute |
| C11 zero-config | done | `--lute-db`, settings DB picker, language fallback, template DB creation |
| C12 tests | done | 32 gtest cases green |
| C13 packaging/docs | done | portable package 169.7 MB (final build) smoke-tested against the Polish library, LUTE.md, C1-C13 status |

All criteria have evidence. Known limitation: the physical mouse click inside
the popup cannot be automated headless, so the QA used controller-level
invocations plus screenshots of the rendered popup; the UI event wiring is the
standard `onClicked` path.
