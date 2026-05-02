# codex-do source

This directory holds editable source copies for scripts released to:

```text
~/codex-do
```

The released `~/codex-do` directory is intentionally outside this project
workspace, so Codex can read and run the dispatcher without being able to edit
it from this repo sandbox.

## Codex rules

Persistent Codex command approvals live in:

```text
~/.codex/rules/default.rules
```

Codex loads `*.rules` files from `~/.codex/rules/` globally. The files are not
project-scoped.

The intended allow rule is:

```text
prefix_rule(pattern=["sh", "/home/<user>/codex-do/codex-do.sh"], decision="allow")
```

Use the real absolute path for `<user>`. Do not write `~` or `$HOME` in this
rule; Codex matches command arguments literally, so shell-style home expansion
is not applied to the rule pattern.

Keep this rule pointed at the released dispatcher outside the project, not at
the editable source copy in this directory.

## Bootstrapping a fresh clone

From the project root:

```sh
sh codex-do-src/bootstrap.sh
```

The bootstrap script copies this directory to `~/codex-do`, checks the local
tools used by the dispatcher, checks the workspace VS Code REST API settings,
and prints the Codex approval rule for the released dispatcher.

To install or repair the VS Code workspace files directly:

```sh
sh codex-do-src/install-vscode-workspace.sh
```

The released dispatcher can do the same for any workspace:

```sh
sh ~/codex-do/codex-do.sh install-vscode-workspace --workspace /path/to/project
```

If VS Code is not answering yet, install/enable the recommended extension
`mkloubert.vs-rest-api`, open this workspace, reload VS Code, then run:

```sh
sh codex-do-src/bootstrap.sh --check
```

For a clone-style smoke test without touching the real home directory:

```sh
sh codex-do-src/bootstrap.sh --self-test
```

## Releasing

Review changes, then release them manually:

```sh
sh codex-do-src/release.sh
```

The release script refuses to run from the Codex project sandbox because
`~/codex-do` is not writable there. It also refuses to release if any source
file has execute bits set.
