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

## Releasing

Review changes, then release them manually:

```sh
sh codex-do-src/release.sh
```

The release script refuses to run from the Codex project sandbox because
`~/codex-do` is not writable there. It also refuses to release if any source
file has execute bits set.
