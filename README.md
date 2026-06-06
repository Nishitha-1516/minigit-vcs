# minigit — A Git-Inspired Mini Version Control System

A from-scratch, systems-programming implementation of core version control
concepts in C++17. No external VCS libraries. No shortcuts.

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                          main.cpp                               │
│                     (entry point, argc/argv)                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                    ┌───────▼────────┐
                    │ CommandParser  │  Parses argv, dispatches handlers
                    └───────┬────────┘
                            │
        ┌───────────────────┼───────────────────┐
        │                   │                   │
┌───────▼───────┐  ┌────────▼───────┐  ┌───────▼────────┐
│RepositoryMgr  │  │ CommitManager  │  │ StagingManager │
│               │  │                │  │                │
│ - init()      │  │ - saveCommit() │  │ - stageFile()  │
│ - findRoot()  │  │ - loadCommit() │  │ - unstage()    │
│ - HEAD mgmt   │  │ - getHistory() │  │ - clearAll()   │
│ - branch refs │  │ - resolveHash()│  │ - load/save    │
└───────────────┘  └────────────────┘  └────────────────┘
        │                   │
┌───────▼───────┐  ┌────────▼───────┐
│  ObjectStore  │  │  FileTracker   │
│               │  │                │
│ Content-addr. │  │ - listFiles()  │
│ blob storage  │  │ - computeStatus│
│ SHA-256 keys  │  │ - hashFile()   │
└───────┬───────┘  └────────────────┘
        │
┌───────▼───────┐  ┌────────────────┐  ┌────────────────┐
│  HashUtility  │  │   DiffEngine   │  │   FileUtils    │
│               │  │                │  │                │
│ Pure SHA-256  │  │ Myers LCS diff │  │ POSIX wrappers │
│ (no openssl)  │  │ Unified output │  │ path / I/O ops │
└───────────────┘  └────────────────┘  └────────────────┘
```

### Class Responsibilities

| Class | Role |
|---|---|
| `CommandParser` | Parse CLI args, dispatch to handlers |
| `RepositoryManager` | Own `.minigit/`, manage HEAD & branch refs |
| `CommitManager` | Persist & traverse the commit graph |
| `StagingManager` | Manage the staging index (add/unstage) |
| `ObjectStore` | Content-addressed blob storage (SHA-256 keyed) |
| `FileTracker` | Diff working tree vs HEAD/staging for `status` |
| `DiffEngine` | Myers LCS line-level diff, unified-diff rendering |
| `HashUtility` | Pure C++ SHA-256 (no external deps) |
| `FileUtils` | POSIX filesystem helpers (stat, readdir, I/O) |
| `Commit` | Plain data object + serialise/deserialise |

---

## Storage Design

```
.minigit/
├── HEAD                        ← "ref: refs/heads/main"  or  "<sha256>"
├── objects/
│   ├── ab/                     ← first 2 hex chars of SHA-256
│   │   └── cdef…               ← remaining 62 chars — raw file content
│   └── …
├── commits/
│   └── <full-sha256>           ← serialised Commit text (key=value lines)
├── refs/
│   └── heads/
│       ├── main                ← SHA-256 of the latest commit on main
│       └── feature-foo         ← SHA-256 of the latest commit on that branch
├── staging/
│   └── index                   ← "<relPath> <contentHash>" per line
└── logs/
    └── activity.log            ← append-only timestamped action log
```

### Commit file format

```
id      <sha256>
parent  <sha256 | "">
branch  <name>
time    <YYYY-MM-DD HH:MM:SS>
message <text>
files   <N>
src/main.cpp   <sha256>
README.md      <sha256>
…
```

### Object deduplication

Every unique file content is stored **exactly once**.  
Two files with identical content share one blob — the hash IS the key.  
Re-adding an unchanged file is a no-op (existence check, no write).

---

## Commands

### `init [dir]`
Initialise a new repository in the current (or given) directory.

```
$ minigit init
Initialized empty minigit repository in /my/project/.minigit
```

### `add <file>…  |  add .`
Stage files for the next commit.

```
$ minigit add README.md src/main.cpp
add 'README.md'
add 'src/main.cpp'

$ minigit add .          # stage all working-tree files
```

### `commit -m "<message>"`
Create a commit from the current staging area.  
Carries forward unchanged files from the parent snapshot.

```
$ minigit commit -m "Initial commit"
[main 4a3f1b2c] Initial commit
 3 file(s) tracked
```

### `status`
Show staged, unstaged, and untracked files.

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
Show the commit history from HEAD backwards.

```
$ minigit log --oneline
b9b6e8c9 Add divide function
580dedc0 Update hello.cpp
546fb016 Initial commit
```

### `checkout <commit-id|branch>`
Switch branch or restore the working tree to a past commit.

```
$ minigit checkout feature-auth       # switch branch
$ minigit checkout 546fb016           # detached HEAD (short hash OK)
```

### `branch [name]`
List branches or create a new one at HEAD.

```
$ minigit branch                  # list
* main
  feature-auth

$ minigit branch feature-auth     # create
Created branch 'feature-auth' at b9b6e8c9
```

### `diff [c1 [c2]]`
Show line-level differences.

```
$ minigit diff                    # HEAD vs working tree
$ minigit diff 546fb016           # commit vs working tree
$ minigit diff 546fb016 b9b6e8c9  # commit vs commit
```

### `restore <file>`
Unstage a file and restore the working-tree copy to its HEAD version.

```
$ minigit restore src/main.cpp
Restored 'src/main.cpp'
```

### `help`
Print command reference.

---

## Build Instructions

### Prerequisites
- C++17 compiler (GCC ≥ 9 or Clang ≥ 10)
- CMake ≥ 3.16

### Build

```bash
git clone <repo>
cd minigit
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Binary: `build/minigit`

### Install (optional)

```bash
sudo make install   # installs to /usr/local/bin/minigit
```

### Run tests / example workflow

```bash
mkdir /tmp/demo && cd /tmp/demo
minigit init
echo "hello" > a.txt
minigit add a.txt
minigit commit -m "first"
echo "world" >> a.txt
minigit add a.txt
minigit commit -m "second"
minigit log
minigit diff 
```

---

## Example Workflow

```
$ mkdir myproject && cd myproject
$ minigit init
Initialized empty minigit repository in /myproject/.minigit

$ echo '#include <stdio.h>' > main.c
$ minigit add main.c
add 'main.c'

$ minigit commit -m "Initial commit"
[main a1b2c3d4] Initial commit
 1 file(s) tracked

$ minigit branch feature
Created branch 'feature' at a1b2c3d4

$ minigit checkout feature
Switched to branch 'feature'

$ echo 'int helper() {}' > helper.c
$ minigit add helper.c
$ minigit commit -m "Add helper on feature branch"
[feature 9f8e7d6c] Add helper on feature branch

$ minigit log --oneline
9f8e7d6c Add helper on feature branch
a1b2c3d4 Initial commit

$ minigit checkout main
Switched to branch 'main'
$ ls
main.c    # helper.c is gone — correct!

$ minigit diff a1b2c3d4 9f8e7d6c
--- helper.c
+++ helper.c
  (new file)
+ int helper() {}
```

---

## Design Decisions & Trade-offs

### SHA-256 from scratch
**Decision**: Implement SHA-256 without OpenSSL or any crypto library.  
**Why**: Keeps the project dependency-free and demonstrates understanding of the algorithm.  
**Trade-off**: Not constant-time (not needed for a VCS); slightly longer code.

### Content-addressed object store
**Decision**: Files are stored by hash, mirroring Git's loose object format.  
**Why**: Automatic deduplication — identical files across commits share one blob.  
**Trade-off**: No delta compression (future work); reading requires a hash lookup.

### Snapshot model (not delta model)
**Decision**: Every commit stores a full file listing (path → hash).  
**Why**: Simpler to reason about, O(1) checkout (no chain of deltas to apply).  
**Trade-off**: More metadata per commit; could grow large for repos with thousands of files.

### Staging area as a flat index file
**Decision**: `staging/index` is a simple `path hash` text file.  
**Why**: Human-readable, easy to debug, trivial to serialise.  
**Trade-off**: Not memory-mapped; for large repos a binary format would be faster.

### Myers LCS diff
**Decision**: O(ND) LCS-based diff for the `diff` command.  
**Why**: Produces minimal diffs (same algorithm as GNU diff / Git).  
**Trade-off**: O(m×n) space for the full DP table. For large files a space-optimal variant would be used.

### Branch model
**Decision**: Branches are simple ref files (`refs/heads/<name>`) containing a commit hash.  
**Why**: Identical to Git's ref model — easy to understand and extend.  
**Trade-off**: No packed-refs, no remote tracking refs (future work).

---

## Advanced Features (Future Work)

| Feature | Notes |
|---|---|
| Merge | Find common ancestor, apply 3-way merge |
| Ignore file | `.minigitignore` patterns in FileTracker |
| Compression | zlib deflate on object blobs |
| Garbage collection | Remove unreachable objects |
| Stash | Save/restore dirty working tree |
| Tags | Lightweight & annotated |
| Remote / push / pull | Network layer + ref exchange protocol |

---

## File Structure

```
minigit/
├── include/
│   ├── HashUtility.h
│   ├── FileUtils.h
│   ├── Commit.h
│   ├── ObjectStore.h
│   ├── StagingManager.h
│   ├── CommitManager.h
│   ├── FileTracker.h
│   ├── RepositoryManager.h
│   ├── DiffEngine.h
│   └── CommandParser.h
├── src/
│   ├── main.cpp
│   ├── HashUtility.cpp
│   ├── FileUtils.cpp
│   ├── Commit.cpp
│   ├── ObjectStore.cpp
│   ├── StagingManager.cpp
│   ├── CommitManager.cpp
│   ├── FileTracker.cpp
│   ├── RepositoryManager.cpp
│   ├── DiffEngine.cpp
│   └── CommandParser.cpp
├── CMakeLists.txt
└── README.md
```
