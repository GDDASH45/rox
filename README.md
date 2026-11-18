# ROX

ROX is a local-only version control system written in C. Repository metadata and snapshots live in `.rox/`; patches use the human-readable `.rpt` format.

## Build

Requires a C compiler and GNU Make.

```sh
make
```

This builds `./rox`. To install into a staging root:

```sh
make install DESTDIR=/tmp/rox-root
```

## Commands

```sh
rox init
rox add path/to/file.c
rox commit -m "Initial snapshot"
rox status
rox log
rox branch
rox branch feature-name
rox branch -d feature-name
rox diff > changes.rpt
rox apply changes.rpt
```

ROX has no remote, push, pull, or network commands.

## Branches

ROX stores branches as references to commit IDs. The current branch is recorded in `.rox/HEAD`, while branch tips live in `.rox/refs/heads/`.

```sh
rox branch                 # list branches
rox branch feature-name    # create a branch at the current commit
rox branch -d feature-name # delete a branch
```

Branch names currently use letters, numbers, dots, underscores, and hyphens. ROX protects the currently checked-out branch from deletion. Branch creation does not switch the working tree; branch switching and merging are planned separately.

Older repositories whose `.rox/HEAD` contains a commit ID are upgraded to the `main` branch reference the first time `rox branch` is used.

## Repository layout

```text
.rox/
  HEAD
  config
  index
  refs/
    heads/
      main
  stage/
  commits/
    000001/
      message
      parent
      files/
```

Each commit stores a snapshot. The index lists staged paths; `stage/` stores staged file contents.

## RPT patch format

The first line is the commit message. Each file entry has a path and line-change counts, then a content block delimited by `^^^`:

```text
Update the README

README.md       +2 -1 M1
^^^
 context line
+added line
-deleted line
M-old wording
M+replacement wording
^^^
```

`+N` counts added lines, `-N` counts deleted lines, and `MN` counts modified line pairs. A leading space means unchanged context. `M-old` followed immediately by `M+new` replaces an existing line, and the patch applier verifies the old line before applying it.

## Initial implementation limitations

This version stages explicitly named regular files. Directory-wide staging, tracked-file deletion, binary patches, rename detection, branch switching, merging, and advanced diff alignment are not implemented yet.
