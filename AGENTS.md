# Agent Instructions

Before editing any file, query VS Code for unpersisted documents:

```sh
sh "$HOME/codex-do/codex-do.sh" query-vscode-unpersisted-documents
```

If the target file has unpersisted changes, do not edit it until the user saves,
discards, or otherwise confirms how to handle those changes. This avoids
conflicts between user edits in the editor and agent edits on disk.
