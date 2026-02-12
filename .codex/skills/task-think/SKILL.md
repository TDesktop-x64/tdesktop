---
name: task-think
description: Orchestrate a multi-phase implementation workflow for this repository with artifact files under .ai/task-slug and optional fresh codex exec child runs per phase. Use when the user wants one prompt to drive context gathering, planning, implementation, verification, and review iterations while keeping the main session context clean.
---

# Task Pipeline

Run a full implementation workflow with repository artifacts and clear phase boundaries.

## Inputs

Collect:
- task description
- optional task slug (if missing, derive a short kebab-case name)
- optional constraints (files, architecture, deadlines, risk tolerance)
- optional screenshot paths

If screenshots are attached in UI but not present as files, write a brief textual summary in `.ai/<task>/inputs.md` so child runs can consume the requirements.

## Artifacts

Create and maintain:
- `.ai/<task>/inputs.md`
- `.ai/<task>/context.md`
- `.ai/<task>/plan.md`
- `.ai/<task>/implementation.md`
- `.ai/<task>/review.md`
- `.ai/<task>/logs/phase-*.jsonl` (when running child `codex exec`)

## Execution Mode

Run `codex exec --json` child sessions for each phase.

## Fresh-Run Mode Procedure

1. Confirm repository root and task slug.
2. Create `.ai/<task>/` and `logs/`.
3. Run child phase sessions sequentially, waiting for each to finish.
4. After each phase, validate artifact file exists and has substantive content.
5. Summarize status in the parent session after each phase.
6. Stop immediately on blocking errors and report exact blocker.

Use the phase prompt templates in `PROMPTS.md`.

## Verification Rules

- If build or test commands fail due to file locks or access-denied outputs, stop and ask the user to close locking processes before retrying.
- Never claim completion without:
  - implemented code changes present
  - build/test attempt results recorded
  - review pass documented with any follow-up fixes

## Completion Criteria

Mark complete only when:
- plan phases are done
- verification results are recorded
- review issues are addressed or explicitly deferred with rationale

## User Invocation

Use plain language with the skill name in the request, for example:

`Use local task-think skill: make sure FileLoadTask::process does not create or read QPixmap on background threads; use QImage with ARGB32_Premultiplied instead.`

If screenshots are relevant, include file paths in the same prompt.
