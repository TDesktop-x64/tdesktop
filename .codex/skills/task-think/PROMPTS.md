# Phase Prompts

Use these templates for `codex exec --json` child runs. Replace `<TASK>`, `<SLUG>`, and `<REPO_ROOT>`.

## Phase 1: Context

```text
You are the context phase for task "<TASK>" in repository <REPO_ROOT>.

Read CLAUDE.md for the basic coding rules and guidelines.

Read AGENTS.md and all relevant source files. Write a focused context doc:
- .ai/<SLUG>/context.md

Include:
1. Relevant files and why they matter
2. Existing patterns to follow
3. Risks and unknowns
4. Verification hooks (what to build/test later)

Do not implement code in this phase.
```

## Phase 2: Plan

```text
You are the planning phase for task "<TASK>" in repository <REPO_ROOT>.

Read CLAUDE.md for the basic coding rules and guidelines.

Read:
- .ai/<SLUG>/inputs.md
- .ai/<SLUG>/context.md

Create:
- .ai/<SLUG>/plan.md

Plan requirements:
1. Concrete file-level edits
2. Ordered phases
3. Verification commands
4. Rollback/risk notes
```

## Phase 3: Implement

```text
You are the implementation phase for task "<TASK>" in repository <REPO_ROOT>.

Read CLAUDE.md for the basic coding rules and guidelines.

Read:
- .ai/<SLUG>/inputs.md
- .ai/<SLUG>/context.md
- .ai/<SLUG>/plan.md

Implement the plan in code. Then write:
- .ai/<SLUG>/implementation.md

Include:
1. Files changed
2. What was implemented
3. Any deviations from plan and why
```

## Phase 4: Verify

```text
You are the verification phase for task "<TASK>" in repository <REPO_ROOT>.

Read CLAUDE.md for the basic coding rules and guidelines.

Read:
- .ai/<SLUG>/plan.md
- .ai/<SLUG>/implementation.md

Run the relevant build/test commands from AGENTS.md and plan.md.
Append results to:
- .ai/<SLUG>/implementation.md

If blocked by locked files or access errors, stop and report exact blocker.
```

## Phase 5: Review

```text
You are the review phase for task "<TASK>" in repository <REPO_ROOT>.

Read AGENTS.md for the basic coding rules and guidelines.
Read REVIEW.md for the style and formatting rules you must enforce.

Read:
- .ai/<SLUG>/context.md
- .ai/<SLUG>/plan.md
- .ai/<SLUG>/implementation.md

Run `git diff` to see all uncommitted changes made by the implementation. Implementation phases do not commit, so `git diff` shows exactly the current feature's changes. Then read the modified source files in full.

Perform a code review using these criteria (in order of importance):

1. Correctness and safety: logic errors, null-check gaps at API boundaries, crashes, use-after-free, dangling references, race conditions.
2. Dead code: code added or left behind that is never called or used. Unused variables, unreachable branches, leftover scaffolding.
3. Redundant changes: changes in the diff with no functional effect — moving declarations or code blocks without reason, reformatting untouched code, reordering includes or fields with no purpose. Every line in the diff should serve the feature. If a file appears in `git diff` but contains only no-op rearrangements, flag it for revert.
4. Code duplication: unnecessary repetition of logic that should be shared.
5. Wrong placement: code added to a module where it doesn't logically belong.
6. Function decomposition: for longer functions (~50+ lines), consider whether a sub-task could be cleanly extracted. Only suggest when there is a genuinely self-contained piece of logic.
7. Module structure: only flag if a large amount of new code (hundreds of lines) is logically distinct from its host module.
8. Style compliance: verify adherence to REVIEW.md rules and AGENTS.md conventions.

Write:
- .ai/<SLUG>/review.md

If issues are found, implement fixes and update implementation.md/review.md with final status.
```

## Example Runner Commands

```powershell
codex exec --json -C <REPO_ROOT> "<PHASE_PROMPT>" | Tee-Object .ai/<SLUG>/logs/phase-1-context.jsonl
codex exec --json -C <REPO_ROOT> "<PHASE_PROMPT>" | Tee-Object .ai/<SLUG>/logs/phase-2-plan.jsonl
codex exec --json -C <REPO_ROOT> "<PHASE_PROMPT>" | Tee-Object .ai/<SLUG>/logs/phase-3-implement.jsonl
codex exec --json -C <REPO_ROOT> "<PHASE_PROMPT>" | Tee-Object .ai/<SLUG>/logs/phase-4-verify.jsonl
codex exec --json -C <REPO_ROOT> "<PHASE_PROMPT>" | Tee-Object .ai/<SLUG>/logs/phase-5-review.jsonl
```
