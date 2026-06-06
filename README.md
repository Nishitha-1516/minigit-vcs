# minigit — A Git-Inspired Mini Version Control System

> A from-scratch, production-quality implementation of core version control concepts in **C++17**.  
> No external VCS libraries. No shortcuts. Every byte of storage designed intentionally.

Built as a systems programming deep-dive — demonstrating file system design, cryptographic hashing, graph traversal, OOP architecture, and classic algorithms. Resume-worthy and interview-ready.

---

## Table of Contents

1. [Project Goals](#project-goals)
2. [Quick Start](#quick-start)
3. [Architecture Overview](#architecture-overview)
4. [Design Patterns Used](#design-patterns-used)
5. [Engineering Decisions & Trade-offs](#engineering-decisions--trade-offs)
6. [Storage Design](#storage-design)
7. [Data Flow Diagrams](#data-flow-diagrams)
8. [Command Reference](#command-reference)
9. [Example Workflow](#example-workflow)
10. [Build Instructions](#build-instructions)
11. [File Structure](#file-structure)
12. [Complexity Analysis](#complexity-analysis)
13. [Future Work](#future-work)

---

## Project Goals

| Goal | How it's achieved |
|---|---|
| Understand how Git works internally | Reimplemented every concept from scratch |
| Content-addressed storage | SHA-256 keyed object store — identical to Git's loose object model |
| No external dependencies | Pure C++17 STL + POSIX only |
| Modular, maintainable code | One class per responsibility, clean header/source split |
| Interview-ready systems design | Every decision documented with rationale and trade-offs |

---

## Quick Start

```bash
# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Use
cd /your/project
minigit init
echo "hello" > main.cpp
minigit add main.cpp
minigit commit -m "Initial commit"
minigit log
```

---

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│                            main.cpp                                  │
│                    Entry point — argc/argv only                      │
└─────────────────────────────┬────────────────────────────────────────┘
                              │
                    ┌─────────▼──────────┐
                    │   CommandParser    │  ← Facade + Command pattern
                    │                   │    Parses argv, dispatches
                    └─────────┬──────────┘    to handler methods
                              │
          ┌───────────────────┼────────────────────┐
          │                   │                    │
┌─────────▼────────┐ ┌────────▼────────┐ ┌────────▼────────┐
│ RepositoryManager│ │  CommitManager  │ │  StagingManager │
│                  │ │                 │ │                 │
│ • init()         │ │ • saveCommit()  │ │ • stageFile()   │
│ • findRepoRoot() │ │ • loadCommit()  │ │ • unstageFile() │
│ • HEAD r/w       │ │ • getHistory()  │ │ • clearAll()    │
│ • branch refs    │ │ • resolveHash() │ │ • load/save     │
│ • activity log   │ │ • commitExists()│ │   index         │
└─────────┬────────┘ └────────┬────────┘ └────────┬────────┘
          │                   │                    │
          │         ┌─────────▼──────────┐         │
          │         │    ObjectStore     │◄────────┘
          │         │                   │
          │         │ Content-addressed │  SHA-256 hash → blob
          │         │ deduplicating     │  .minigit/objects/ab/cd…
          │         │ blob storage      │
          │         └─────────┬──────────┘
          │                   │
┌─────────▼────────┐ ┌────────▼────────┐ ┌─────────────────┐
│   FileTracker    │ │  HashUtility    │ │    FileUtils    │
│                  │ │                 │ │                 │
│ • listFiles()    │ │ Pure SHA-256    │ │ POSIX wrappers  │
│ • computeStatus()│ │ (no OpenSSL)    │ │ stat/readdir/IO │
│ • hashFile()     │ │ sha256(string)  │ │ path joins      │
│ • shouldIgnore() │ │ hashFile(path)  │ │ timestamps      │
└──────────────────┘ └─────────────────┘ └─────────────────┘

                    ┌─────────────────────┐
                    │     DiffEngine      │
                    │                     │
                    │ Myers LCS algorithm │
                    │ Unified diff output │
                    │ ANSI colour render  │
                    │ Snapshot diffing    │
                    └─────────────────────┘
```

### Class Responsibilities

| Class | Pattern | Role |
|---|---|---|
| `CommandParser` | Facade + Command | Parse CLI args, single entry point for all operations |
| `RepositoryManager` | Singleton-style static | Own `.minigit/`, HEAD, branch refs, activity log |
| `CommitManager` | Repository | Persist and traverse the commit graph (DAG) |
| `StagingManager` | Repository | R/W staging index; enforces atomic saves |
| `ObjectStore` | Flyweight | Content-addressed blob storage; automatic dedup |
| `FileTracker` | Strategy | Classify working-tree files vs HEAD/staging |
| `DiffEngine` | Strategy | Myers LCS diff + coloured unified-diff rendering |
| `HashUtility` | Utility (static) | Pure C++ SHA-256; no side effects |
| `FileUtils` | Utility namespace | All POSIX I/O in one place; rest of code stays clean |
| `Commit` | Value Object | Plain data + serialise/deserialise; no behaviour |

---

## Design Patterns Used

### 1. Command Pattern — `CommandParser`

Each CLI verb (`init`, `add`, `commit`, …) maps to a private static handler method.  
`execute()` is the single dispatcher. Adding a new command means adding one handler — nothing else changes.

```cpp
// Dispatch table (conceptually):
if (cmd.verb == "add")    return handleAdd(cmd, root);
if (cmd.verb == "commit") return handleCommit(cmd, root);
// ...
```

**Why**: Decouples the CLI surface from business logic. Each handler is independently testable.

---

### 2. Facade Pattern — `CommandParser` (dual role)

`CommandParser::execute()` hides the complexity of coordinating `ObjectStore`, `StagingManager`, `CommitManager`, and `RepositoryManager` behind a single method call. The caller (`main.cpp`) knows nothing about internal subsystems.

```cpp
// main.cpp sees only this:
ParsedCommand cmd = CommandParser::parse(argc, argv);
return CommandParser::execute(cmd);
```

---

### 3. Repository Pattern — `CommitManager` & `StagingManager`

Both classes abstract the persistence layer behind a clean domain interface. The caller never touches file paths or serialization — just calls `saveCommit()`, `loadCommit()`, `stageFile()`, etc.

```cpp
// Caller doesn't know commits live in .minigit/commits/<hash>
CommitManager commits(mgDir);
Commit c = commits.loadCommit(someId);  // just works
commits.saveCommit(newCommit);
```

**Why**: If you swap the storage backend (e.g. SQLite, or a binary format), only the Repository class changes.

---

### 4. Flyweight Pattern — `ObjectStore`

The object store ensures each unique file content blob exists **exactly once** on disk, regardless of how many commits reference it. The hash IS the identity.

```cpp
// Storing the same file twice — second call is a no-op
std::string h1 = store.storeObject(content);  // writes to disk
std::string h2 = store.storeObject(content);  // existence check only
assert(h1 == h2);  // same hash, one blob
```

**Why**: Classic Flyweight — share immutable objects to reduce memory/storage overhead.

---

### 5. Strategy Pattern — `DiffEngine` & `FileTracker`

Both classes encapsulate an interchangeable algorithm behind a stable interface.  
`DiffEngine::diffContent()` can be swapped for a patience diff or histogram diff with zero changes to callers. `FileTracker::computeStatus()` accepts any two snapshots.

```cpp
// FileTracker doesn't care where the snapshots came from
WorkingTreeStatus s = tracker.computeStatus(headSnapshot, stagingIndex);
```

---

### 6. Value Object Pattern — `Commit`

`Commit` is a plain, immutable-by-convention data struct. Its identity is its content hash. Two commits with the same fields produce the same ID — there is no mutable state, no external references.

```cpp
Commit c;
c.message   = "fix bug";
c.parentId  = prevId;
c.files     = staging.stagedFiles();
c.id        = c.computeId();  // derived from content — not assigned externally
```

**Why**: Matches the domain perfectly. Commits in a VCS are immutable facts.

---

### 7. Template Method (implicit) — `HashUtility`

`sha256(string)` and `hashFile(path)` share the same compression loop. `hashFile` simply reads the file and delegates to `sha256` — the core algorithm is written once.

---

### 8. Namespace-as-Utility — `FileUtils`

All raw filesystem I/O is collected in one place. No class owns it; any module can use it. This avoids the God Object anti-pattern while still centralising I/O concerns.

---

## Engineering Decisions & Trade-offs

### Decision 1: SHA-256 Implemented from Scratch

**What**: Pure C++ SHA-256 — no OpenSSL, no Botan, no system crypto.

**Why**:
- Zero dependencies — the binary is fully self-contained
- Demonstrates understanding of the algorithm (bitwise ops, Merkle–Damgård construction)
- VCS hashing is not a security boundary (no timing-attack risk here)

**Trade-off**: ~120 lines of low-level bit manipulation code. A production system would use a vetted library. The implementation is correct but not constant-time.

**Interview angle**: Can explain the Merkle–Damgård construction, padding scheme, and why SHA-256 is collision-resistant.

---

### Decision 2: Content-Addressed Object Store

**What**: Every file blob stored at `.minigit/objects/<2>/<62>` where the full path IS the SHA-256 of the content.

**Why**:
- Automatic deduplication: 1000 commits referencing `README.md` = 1 blob if unchanged
- O(1) existence check (just `stat()` the path)
- Mirrors Git's exact model — validated at scale

**2-char prefix design**: Limits inodes per directory. ext4 struggles above ~65k files per directory. The prefix splits the object namespace into 256 buckets.

**Trade-off**: No delta compression. Git stores deltas for efficiency; minigit stores full content. Acceptable for a teaching implementation; fixable with zlib.

---

### Decision 3: Snapshot Model, Not Delta Model

**What**: Every commit records a full map of `{ relPath → contentHash }` for all tracked files.

**Why**:
- `checkout` is O(files) with zero chain traversal — just restore each hash
- Simpler mental model: "a commit IS a snapshot of the repo at a point in time"
- No risk of a broken delta chain corrupting history

**Trade-off**: Commit metadata grows linearly with tracked files. In Git, a tree object only stores changed subtrees (tree diffing). minigit stores the full flat map every time.

**Numbers**: A repo with 500 files, each path averaging 30 chars + 64-char hash = ~47KB per commit. Negligible for most use cases.

---

### Decision 4: Flat Staging Index

**What**: `.minigit/staging/index` is a plain text file with one `<path> <hash>` entry per line.

**Why**:
- Human-readable — you can `cat` it and immediately understand the state
- Trivial to parse (split on last space)
- Atomic writes: the whole file is rewritten on every mutation (crash-safe)

**Trade-off**: Git uses a binary index format (with stat cache, flags, extensions) for performance at scale. For minigit's scope, text is the right call.

**Alternative considered**: SQLite. Rejected because it would add a dependency and is overkill for a flat key-value store.

---

### Decision 5: Myers LCS Diff Algorithm

**What**: The `diff` command uses the Myers O(ND) algorithm — the same family as GNU diff and Git's diff.

**Why**:
- Produces **minimal** edit scripts (fewest insertions + deletions)
- Well-understood algorithm with clear complexity bounds
- Standard in the industry — interviewers recognize it

**How it works**:
1. Build a dynamic programming LCS table of size `m × n`
2. Backtrack to mark which lines are in the LCS (unchanged)
3. Lines not in LCS are either Added or Removed
4. Apply context window (±3 lines) around changes

**Trade-off**: O(m × n) space for the DP table. For files >10k lines this becomes memory-intensive. Production diffs use a space-optimised O(min(m,n)) variant. Acceptable for this scope.

---

### Decision 6: Git-Identical Ref Model

**What**: `HEAD` contains either `"ref: refs/heads/main"` (symbolic) or a raw SHA-256 (detached).  Branch refs live at `refs/heads/<name>`.

**Why**:
- Identical to Git — anyone who knows Git internals understands the model immediately
- Symbolic refs allow HEAD to "follow" a branch as commits are added
- Detached HEAD is a natural consequence of checking out a raw commit

**How HEAD resolution works**:
```
HEAD → "ref: refs/heads/main"
     → .minigit/refs/heads/main
     → "b9b6e8c9f2bd2..."   (the actual commit SHA)
```

**Trade-off**: No packed-refs file. Git consolidates many loose refs into one packed-refs file for performance. Not needed at minigit scale.

---

### Decision 7: Commit ID = SHA-256 of Commit Content

**What**: The commit hash is derived from `SHA256(parentId + branch + timestamp + message + sorted file entries)`.

**Why**:
- Deterministic: same inputs always produce the same ID
- Tamper-evident: changing any field changes the ID (and breaks the parent chain)
- No external ID generator needed — the content IS the identity

**Why sorted file entries**: Map iteration in C++ is ordered by key, so the serialized form is deterministic across runs and machines.

---

### Decision 8: Carry-Forward Snapshot on Commit

**What**: When committing, the staged files are merged on top of the parent's snapshot. Unstaged files are carried forward automatically.

```
parent snapshot:  { a.cpp→h1, b.cpp→h2, c.cpp→h3 }
staged:           { b.cpp→h4 }           ← only b.cpp was `add`-ed
commit snapshot:  { a.cpp→h1, b.cpp→h4, c.cpp→h3 }  ← a and c carried forward
```

**Why**: Users expect that files they didn't touch are still in the repo after a commit. This matches Git's behaviour exactly.

---

### Decision 9: Single-Header / Single-Source Per Class

**What**: Every class has exactly one `.h` and one `.cpp`. No mega-headers, no implementation in headers (except the Commit struct which is trivially small).

**Why**:
- Faster incremental compilation (change one `.cpp`, recompile one TU)
- Clear ownership: the header IS the public interface contract
- Interview-friendly: any file can be read and understood in isolation

---

### Decision 10: POSIX-Only (No `<filesystem>`)

**What**: All file operations use `stat()`, `readdir()`, `mkdir()`, `getcwd()` — not `std::filesystem`.

**Why**:
- `std::filesystem` requires GCC ≥ 8 with `-lstdc++fs` on older systems
- POSIX calls are universal across Linux and macOS
- Shows understanding of the underlying OS API

**Trade-off**: More verbose code in `FileUtils.cpp`. A modern codebase would prefer `std::filesystem` for portability.

---

## Storage Design

```
.minigit/
├── HEAD
│   └── "ref: refs/heads/main"          ← symbolic ref (normal mode)
│   └── "<sha256>"                       ← raw hash (detached HEAD)
│
├── objects/                             ← content-addressed blob store
│   ├── ab/
│   │   └── cdef1234…(62 chars)         ← raw file content, keyed by SHA-256
│   ├── f7/
│   │   └── 8a910b…
│   └── …                               ← 256 possible prefix buckets
│
├── commits/                             ← serialised commit objects
│   └── <full-64-char-sha256>           ← one file per commit
│
├── refs/
│   └── heads/
│       ├── main                         ← contains latest commit SHA on main
│       ├── feature-auth                 ← contains latest commit SHA on branch
│       └── …
│
├── staging/
│   └── index                           ← path↔hash pairs for staged files
│
└── logs/
    └── activity.log                    ← append-only human-readable audit log
```

### Commit File Format

```
id      b9b6e8c9f2bd2bb6e2db7fe24d7ed11eb232bcbd...
parent  580dedc0271569c6f5a4d501c0a041d45f81829a...
branch  main
time    2024-01-15 14:30:22
message Add multiply function
files   4
README.md       94cb5e309c9c08e0e846294b5c276d16b1de66ad...
hello.cpp       42f778313030d8d29d02ef553c8b4f5f05474bdf...
src/math.cpp    81c0411f8ebc4b2e04585879f3b948eff66e536b...
src/utils.cpp   23cf718c49184de97b2e966b9426178d82ba04d4...
```

### Staging Index Format

```
README.md       94cb5e309c9c08e0e846294b5c276d16b1de66ad...
src/main.cpp    42f778313030d8d29d02ef553c8b4f5f05474bdf...
```

### Object Deduplication

```
Commit A snapshot:  { main.cpp → hash_v1,  utils.cpp → hash_u1 }
Commit B snapshot:  { main.cpp → hash_v2,  utils.cpp → hash_u1 }
                                                         ↑
                              utils.cpp unchanged — same hash, SAME blob on disk
                              No duplicate storage. One read serves both commits.
```

---

## Data Flow Diagrams

### `minigit add <file>`

```
User runs: minigit add src/main.cpp
                │
                ▼
        CommandParser::handleAdd()
                │
                ├─► Read file from disk (FileUtils::readFile)
                │
                ├─► Hash content (HashUtility::sha256)
                │       └─ returns: "42f77831..."
                │
                ├─► Store blob (ObjectStore::storeObject)
                │       ├─ Check: does .minigit/objects/42/f77831... exist?
                │       ├─ YES → skip (deduplication)
                │       └─ NO  → write raw content to that path
                │
                └─► Update staging index (StagingManager::stageFile)
                        └─ Append "src/main.cpp 42f77831..." to index
                        └─ Atomically rewrite index file
```

### `minigit commit -m "message"`

```
User runs: minigit commit -m "Add feature"
                │
                ▼
        CommandParser::handleCommit()
                │
                ├─► Load staging index (StagingManager::stagedFiles)
                │       └─ { "src/main.cpp" → "42f77831..." }
                │
                ├─► Load HEAD commit (CommitManager::loadCommit)
                │       └─ parent snapshot: { "README.md"→"...", "src/main.cpp"→"old_hash" }
                │
                ├─► Merge: parent_snapshot + staged_overrides = new_snapshot
                │       └─ { "README.md"→"...", "src/main.cpp"→"42f77831..." }
                │
                ├─► Build Commit object
                │       ├─ parentId  = current HEAD SHA
                │       ├─ timestamp = now
                │       ├─ message   = "Add feature"
                │       ├─ files     = merged snapshot
                │       └─ id        = SHA256(all of the above)
                │
                ├─► Persist commit (CommitManager::saveCommit)
                │       └─ Write to .minigit/commits/<id>
                │
                ├─► Advance branch ref (RepositoryManager::writeBranchRef)
                │       └─ .minigit/refs/heads/main ← new commit id
                │
                └─► Clear staging index (StagingManager::clearAll)
```

### `minigit checkout <id>`

```
User runs: minigit checkout 546fb016
                │
                ▼
        CommandParser::handleCheckout()
                │
                ├─► Resolve short hash → full hash (CommitManager::resolveShortHash)
                │       └─ scan .minigit/commits/ for prefix match
                │
                ├─► Load commit snapshot (CommitManager::loadCommit)
                │       └─ { "README.md"→"hash1", "src/main.cpp"→"hash2", ... }
                │
                ├─► For each file in snapshot:
                │       ├─ Load blob (ObjectStore::loadObject(hash))
                │       └─ Write to working tree path (FileUtils::writeFile)
                │
                ├─► Update HEAD (RepositoryManager::setHeadDetached)
                │       └─ .minigit/HEAD ← "546fb016d17c31bb..."
                │
                └─► Clear staging index
```

---

## Command Reference

### `init [dir]`
Initialises a new repository. Creates the `.minigit/` directory scaffold.

```bash
$ minigit init
Initialized empty minigit repository in /myproject/.minigit
```

### `add <file>… | add .`
Stages files. Hashes content, stores blob, updates index.

```bash
$ minigit add README.md src/main.cpp
add 'README.md'
add 'src/main.cpp'

$ minigit add .          # stage everything in working tree
```

### `commit -m "<message>"`
Records staged changes. Merges staged files onto parent snapshot.

```bash
$ minigit commit -m "Initial commit"
[main 4a3f1b2c] Initial commit
 3 file(s) tracked
```

### `status`
Three-zone output: staged / unstaged / untracked.

```
On branch main

Changes to be committed:
  (use "minigit restore <file>" to unstage)
	new file:   README.md
	modified:   src/main.cpp

Changes not staged for commit:
  (use "minigit add <file>" to stage)
	modified:   src/utils.cpp

Untracked files:
  (use "minigit add <file>" to include)
	notes.txt
```

### `log [--oneline]`
Traverses the parent chain from HEAD to root.

```bash
$ minigit log --oneline
b9b6e8c9 Add multiply function
580dedc0 Update hello.cpp
546fb016 Initial commit

$ minigit log            # full format with date, branch, file count
```

### `checkout <commit-id|branch>`
Restores working tree. Supports short hashes and branch names.

```bash
$ minigit checkout feature-auth     # switch branch
$ minigit checkout 546fb016         # detached HEAD
```

### `branch [name]`
Lists or creates branches. New branch points to current HEAD.

```bash
$ minigit branch          # list (current marked with *)
* main
  feature-auth

$ minigit branch feature-auth    # create
Created branch 'feature-auth' at b9b6e8c9
```

### `diff [c1 [c2]]`
Line-level diff with ANSI colour output.

```bash
$ minigit diff                     # HEAD vs working tree
$ minigit diff 546fb016            # commit vs working tree
$ minigit diff 546fb016 b9b6e8c9   # commit vs commit
```

### `restore <file>`
Unstages a file and restores its working-tree copy from HEAD.

```bash
$ minigit restore src/main.cpp
Restored 'src/main.cpp'
```

---

## Example Workflow

```bash
# ── Bootstrap ──────────────────────────────────────────────────────────
$ mkdir myproject && cd myproject
$ minigit init
Initialized empty minigit repository in /myproject/.minigit

# ── First commit ────────────────────────────────────────────────────────
$ echo '#include <stdio.h>' > main.c
$ echo '# My Project' > README.md
$ minigit add .
add 'README.md'
add 'main.c'
$ minigit commit -m "Initial commit"
[main a1b2c3d4] Initial commit
 2 file(s) tracked

# ── Feature branch ─────────────────────────────────────────────────────
$ minigit branch feature-utils
Created branch 'feature-utils' at a1b2c3d4

$ minigit checkout feature-utils
Switched to branch 'feature-utils'
Restored 2 file(s)

$ echo 'int add(int a, int b){return a+b;}' > utils.c
$ minigit add utils.c
$ minigit commit -m "Add utility functions"
[feature-utils 9f8e7d6c] Add utility functions
 3 file(s) tracked

# ── Check history ───────────────────────────────────────────────────────
$ minigit log --oneline
9f8e7d6c Add utility functions
a1b2c3d4 Initial commit

# ── Go back to main ─────────────────────────────────────────────────────
$ minigit checkout main
Switched to branch 'main'
$ ls
main.c  README.md     # utils.c is gone — correct!

# ── See what changed between commits ───────────────────────────────────
$ minigit diff a1b2c3d4 9f8e7d6c
--- utils.c
+++ utils.c
  (new file)
+ int add(int a, int b){return a+b;}

# ── Time travel to first commit ─────────────────────────────────────────
$ minigit checkout a1b2c3d4
HEAD is now at a1b2c3d4 Initial commit
You are in 'detached HEAD' state.
Restored 2 file(s)

# ── Undo a staged change ────────────────────────────────────────────────
$ echo "oops" >> README.md
$ minigit add README.md
$ minigit status
# ... shows README.md modified ...
$ minigit restore README.md
Restored 'README.md'
$ minigit status
nothing to commit, working tree clean
```

---

## Build Instructions

### Prerequisites

| Tool | Version | Check |
|---|---|---|
| GCC or Clang | GCC ≥ 9 / Clang ≥ 10 | `g++ --version` |
| CMake | ≥ 3.16 | `cmake --version` |
| make | any | `make --version` |

### Install dependencies

```bash
# Ubuntu / Debian
sudo apt install build-essential cmake

# macOS
xcode-select --install && brew install cmake

# Fedora / RHEL
sudo dnf install gcc-c++ cmake
```

### Build

```bash
git clone https://github.com/Nishitha-1516/minigit-vcs.git
cd minigit-vcs
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Binary: `build/minigit`

### Install system-wide (optional)

```bash
sudo cp build/minigit /usr/local/bin/minigit
minigit help
```

### Debug build (with symbols)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

---

## File Structure

```
minigit-vcs/
├── include/                    ← Public interfaces (the contract)
│   ├── HashUtility.h           ← SHA-256 API
│   ├── FileUtils.h             ← POSIX filesystem helpers
│   ├── Commit.h                ← Commit value object
│   ├── ObjectStore.h           ← Content-addressed blob storage
│   ├── StagingManager.h        ← Staging index management
│   ├── CommitManager.h         ← Commit graph persistence
│   ├── FileTracker.h           ← Working tree status diffing
│   ├── RepositoryManager.h     ← .minigit scaffold + HEAD + refs
│   ├── DiffEngine.h            ← Myers LCS diff + renderer
│   └── CommandParser.h         ← CLI parser + command dispatcher
│
├── src/                        ← Implementations
│   ├── main.cpp                ← Entry point 
│   ├── HashUtility.cpp         ← Pure SHA-256 implementation
│   ├── FileUtils.cpp           ← stat/readdir/mkdir/read/write
│   ├── Commit.cpp              ← Serialise/deserialise/computeId
│   ├── ObjectStore.cpp         ← Store/load/exists/restore blobs
│   ├── StagingManager.cpp      ← Index load/save/mutate
│   ├── CommitManager.cpp       ← Save/load/resolve/history
│   ├── FileTracker.cpp         ← listFiles/computeStatus
│   ├── RepositoryManager.cpp   ← init/findRoot/HEAD/refs/log
│   ├── DiffEngine.cpp          ← LCS table/backtrack/render
│   └── CommandParser.cpp       ← All 9 command handlers
│
├── CMakeLists.txt              ← Build system
└── README.md                   ← This file
```

---

## Complexity Analysis

| Operation | Time | Space | Notes |
|---|---|---|---|
| `init` | O(1) | O(1) | Fixed directory scaffold |
| `add <file>` | O(n) | O(n) | n = file size (hashing) |
| `commit` | O(f) | O(f) | f = files in snapshot |
| `status` | O(w·n) | O(w) | w = working tree files, n = avg file size |
| `log` | O(c) | O(c) | c = number of commits (chain traversal) |
| `checkout` | O(f·n) | O(n) | Restore each file from object store |
| `diff` (two commits) | O(m·n) | O(m·n) | m, n = lines in each file (LCS table) |
| Object lookup | O(1) | — | stat() on derived path |
| Short hash resolve | O(c) | — | Scan commit filenames for prefix |

---

## Future Work

| Feature | Complexity | Notes |
|---|---|---|
| **Merge** | High | Find LCA via BFS on commit DAG; 3-way merge |
| **`.minigitignore`** | Low | Glob pattern matching in `FileTracker::shouldIgnore` |
| **zlib compression** | Medium | Deflate blobs in `ObjectStore`; major storage win |
| **Garbage collection** | Medium | Mark reachable objects, delete unreachable blobs |
| **Stash** | Medium | Save dirty working tree + staging to a stash ref |
| **Tags** | Low | Lightweight: ref file. Annotated: object with message |
| **Packed refs** | Low | Consolidate many `refs/heads/*` into one file |
| **Delta compression** | High | Store diffs between similar blobs, not full content |
| **Remote protocol** | Very High | Custom wire protocol for push/fetch |
| **Binary index** | Medium | Memory-mapped binary staging index for scale |

---


