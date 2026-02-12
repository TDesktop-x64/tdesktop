---
description: Implement a feature or fix using multi-agent workflow with fresh context at each phase
allowed-tools: Read, Write, Edit, Glob, Grep, Bash, Task, AskUserQuestion, TodoWrite
---

# Task - Multi-Agent Implementation Workflow

You orchestrate a multi-phase implementation workflow that uses fresh agent spawns to work within context window limits on a large codebase.

**Arguments:** `$ARGUMENTS` = "$ARGUMENTS"

If `$ARGUMENTS` is provided, it's the task description. If empty, ask the user what they want implemented.

## Overview

The workflow produces `.ai/<feature-name>/` containing:
- `context.md` - Gathered codebase context relevant to the task
- `plan.md` - Detailed implementation plan with phases
- `review1.md`, `review2.md`, `review3.md` - Code review documents (up to 3 iterations)

Then spawns implementation agents to execute each phase, verifies the build, and runs up to 3 review-fix iterations to improve code quality.

## Phase 0: Setup

1. Understand the task from `$ARGUMENTS` or ask the user.
2. **Follow-up detection:** Check if `$ARGUMENTS` starts with a task name (the first word/token before any whitespace or newline). Look for `.ai/<that-name>/` directory:
   - If `.ai/<that-name>/` exists AND contains both `context.md` and `plan.md`, this is a **follow-up task**. Read both files. The rest of `$ARGUMENTS` (after the task name) is the follow-up task description describing what additional changes are needed.
   - If no matching directory exists, this is a **new task** - proceed normally.
3. For new tasks: check existing folders in `.ai/` to pick a unique short name (1-2 lowercase words, hyphen-separated) and create `.ai/<feature-name>/`.
4. For follow-up tasks: the folder already exists, skip creation.

### Follow-up Task Flow

When a follow-up task is detected (existing `.ai/<name>/` with `context.md` and `plan.md`):

1. Skip Phase 1 (Context Gathering) - context already exists.
2. Skip Phase 2 (Planning) - original plan already exists.
3. Go directly to **Phase 2F (Follow-up Planning)** instead of Phase 3.

**Phase 2F: Follow-up Planning**

Spawn an agent (Task tool, subagent_type=`general-purpose`) with this prompt:

```
You are a planning agent for a follow-up task on an existing implementation.

Read these files:
- .ai/<feature-name>/context.md - Previously gathered codebase context
- .ai/<feature-name>/plan.md - Previous implementation plan (already completed)

Then read the source files referenced in context.md and plan.md to understand what was already implemented.

FOLLOW-UP TASK: <paste the follow-up task description here>

The previous plan was already implemented and tested. Now there are follow-up changes needed.

YOUR JOB:
1. Understand what was already done from plan.md (look at the completed phases).
2. Read the actual source files to see the current state of the code.
3. If context.md needs updates for the follow-up task (new files relevant, new patterns needed), update it with additional sections marked "## Follow-up Context (iteration 2)" or similar.
4. Create a NEW follow-up plan. Update plan.md by:
   - Keep the existing content as history (do NOT delete it)
   - Add a new section at the end:

   ---
   ## Follow-up Task
   <description>

   ## Follow-up Approach
   <high-level description>

   ## Follow-up Files to Modify
   <list>

   ## Follow-up Implementation Steps

   ### Phase F1: <name>
   1. <specific step>
   2. ...

   ### Phase F2: <name> (if needed)
   ...

   ## Follow-up Status
   Phases: <N>
   - [ ] Phase F1: <name>
   - [ ] Phase F2: <name> (if applicable)
   - [ ] Build verification
   - [ ] Code review
   Assessed: yes

Use /ultrathink to reason carefully. The follow-up plan should be self-contained enough that an implementation agent can execute it by reading context.md and the updated plan.md.
```

After this agent completes, read `plan.md` to verify the follow-up plan was written. Then proceed to Phase 4 (Implementation), using the follow-up phases (F1, F2, etc.) instead of the original phases.

### New Task Flow

When this is a new task (no existing folder), proceed with Phases 1-5 as described below.

## Phase 1: Context Gathering

Spawn an agent (Task tool, subagent_type=`general-purpose`) with this prompt structure:

```
You are a context-gathering agent for a large C++ codebase (Telegram Desktop).

TASK: <paste the user's task description here>

YOUR JOB: Read CLAUDE.md, inspect the codebase, find ALL files and code relevant to this task, and write a comprehensive context document.

Steps:
1. Read CLAUDE.md for project conventions and build instructions.
2. Search the codebase for files, classes, functions, and patterns related to the task.
3. Read all potentially relevant files. Be thorough - read more rather than less.
4. For each relevant file, note:
   - File path
   - Relevant line ranges
   - What the code does and how it relates to the task
   - Key data structures, function signatures, patterns used
5. Look for similar existing features that could serve as a reference implementation.
6. Check api.tl if the task involves Telegram API.
7. Check .style files if the task involves UI.
8. Check lang.strings if the task involves user-visible text.

Write your findings to: .ai/<feature-name>/context.md

The context.md should contain:
- **Task Description**: The full task restated clearly
- **Relevant Files**: Every file path with line ranges and descriptions of what's there
- **Key Code Patterns**: How similar things are done in the codebase (with code snippets)
- **Data Structures**: Relevant types, structs, classes
- **API Methods**: Any TL schema methods involved (copied from api.tl)
- **UI Styles**: Any relevant style definitions
- **Localization**: Any relevant string keys
- **Build Info**: Build command and any special notes
- **Reference Implementations**: Similar features that can serve as templates

Be extremely thorough. Another agent with NO prior context will read this file and must be able to understand everything needed to implement the task.
```

After this agent completes, read `context.md` to verify it was written properly.

## Phase 2: Planning

Spawn an agent (Task tool, subagent_type=`general-purpose`) with this prompt structure:

```
You are a planning agent. You must create a detailed implementation plan.

Read these files:
- .ai/<feature-name>/context.md - Contains all gathered context
- Then read the specific source files referenced in context.md to understand the code deeply.

Use /ultrathink to reason carefully about the implementation approach.

Create a detailed plan in: .ai/<feature-name>/plan.md

The plan.md should contain:

## Task
<one-line summary>

## Approach
<high-level description of the implementation approach>

## Files to Modify
<list of files that will be created or modified>

## Files to Create
<list of new files, if any>

## Implementation Steps

Each step must be specific enough that an agent can execute it without ambiguity:
- Exact file paths
- Exact function names
- What code to add/modify/remove
- Where exactly in the file (after which function, in which class, etc.)

Number every step. Group steps into phases if there are more than ~8 steps.

### Phase 1: <name>
1. <specific step>
2. <specific step>
...

### Phase 2: <name> (if needed)
...

## Build Verification
- Build command to run
- Expected outcome

## Status
- [ ] Phase 1: <name>
- [ ] Phase 2: <name> (if applicable)
- [ ] Build verification
- [ ] Code review
```

After this agent completes, read `plan.md` to verify it was written properly.

## Phase 3: Plan Assessment

Spawn an agent (Task tool, subagent_type=`general-purpose`) with this prompt structure:

```
You are a plan assessment agent. Review and refine an implementation plan.

Read these files:
- .ai/<feature-name>/context.md
- .ai/<feature-name>/plan.md
- Then read the actual source files referenced to verify the plan makes sense.

Use /ultrathink to assess the plan:

1. **Correctness**: Are the file paths and line references accurate? Does the plan reference real functions and types?
2. **Completeness**: Are there missing steps? Edge cases not handled?
3. **Code quality**: Will the plan minimize code duplication? Does it follow existing codebase patterns from CLAUDE.md?
4. **Design**: Could the approach be improved? Are there better patterns already used in the codebase?
5. **Phase sizing**: Each phase should be implementable by a single agent in one session. If a phase has more than ~8-10 substantive code changes, split it further.

Update plan.md with your refinements. Keep the same structure but:
- Fix any inaccuracies
- Add missing steps
- Improve the approach if you found better patterns
- Ensure phases are properly sized for single-agent execution
- Add a line at the top of the Status section: `Phases: <N>` indicating how many implementation phases there are
- Add `Assessed: yes` at the bottom of the file

If the plan is small enough for a single agent (roughly <=8 steps), mark it as a single phase.
```

After this agent completes, read `plan.md` to verify it was assessed.

## Phase 4: Implementation

Now read `plan.md` yourself to understand the phases.

For each phase in the plan that is not yet marked as done, spawn an implementation agent (Task tool, subagent_type=`general-purpose`):

```
You are an implementation agent working on phase <N> of an implementation plan.

Read these files first:
- .ai/<feature-name>/context.md - Full codebase context
- .ai/<feature-name>/plan.md - Implementation plan

Then read the source files you'll be modifying.

YOUR TASK: Implement ONLY Phase <N> from the plan:
<paste the specific phase steps here>

Rules:
- Follow the plan precisely
- Follow CLAUDE.md coding conventions (no comments except complex algorithms, use auto, empty line before closing brace, etc.)
- Do NOT modify .ai/ files except to update the Status section in plan.md
- When done, update plan.md Status section: change `- [ ] Phase <N>: ...` to `- [x] Phase <N>: ...`
- Do NOT work on other phases

When finished, report what you did and any issues encountered.
```

After each implementation agent returns:
1. Read `plan.md` to check the status was updated.
2. If more phases remain, spawn the next implementation agent.
3. If all phases are done, proceed to build verification.

## Phase 5: Build Verification

Only run this phase if the task involved modifying project source code (not just docs or config).

Spawn a build verification agent (Task tool, subagent_type=`general-purpose`):

```
You are a build verification agent.

Read these files:
- .ai/<feature-name>/context.md
- .ai/<feature-name>/plan.md

The implementation is complete. Your job is to build the project and fix any build errors.

Steps:
1. Run: cmake --build "c:\Telegram\tdesktop\out" --config Debug --target Telegram
2. If the build succeeds, update plan.md: change `- [ ] Build verification` to `- [x] Build verification`
3. If the build fails:
   a. Read the error messages carefully
   b. Read the relevant source files
   c. Fix the errors in accordance with the plan and CLAUDE.md conventions
   d. Rebuild and repeat until the build passes
   e. Update plan.md status when done

Rules:
- Only fix build errors, do not refactor or improve code
- Follow CLAUDE.md conventions
- If build fails with file-locked errors (C1041, LNK1104), STOP and report - do not retry

When finished, report the build result.
```

After the build agent returns, read `plan.md` to confirm the final status. Then proceed to Phase 6.

## Phase 6: Code Review Loop

After build verification passes, run up to 3 review-fix iterations to improve code quality. Set iteration counter `R = 1`.

### Review Loop

```
LOOP:
  1. Spawn review agent (Step 6a) with iteration R
  2. Read review<R>.md verdict:
     - "APPROVED" → go to FINISH
     - Has improvement suggestions → spawn fix agent (Step 6b)
  3. After fix agent completes and build passes:
     R = R + 1
     If R > 3 → go to FINISH (stop iterating, accept current state)
     Otherwise → go to step 1

FINISH:
  - Update plan.md: change `- [ ] Code review` to `- [x] Code review`
  - Proceed to Completion
```

### Step 6a: Code Review Agent

Spawn an agent (Task tool, subagent_type=`general-purpose`):

```
You are a code review agent for Telegram Desktop (C++ / Qt).

Read these files:
- .ai/<feature-name>/context.md - Codebase context
- .ai/<feature-name>/plan.md - Implementation plan
- REVIEW.md - Style and formatting rules to enforce
<if R > 1, also read:>
- .ai/<feature-name>/review<R-1>.md - Previous review (to see what was already addressed)

Then run `git diff` to see all uncommitted changes made by the implementation. Implementation agents do not commit, so `git diff` shows exactly the current feature's changes.

Then read the modified source files in full to understand changes in context.

Use /ultrathink to perform a thorough code review.

REVIEW CRITERIA (in order of importance):

1. **Correctness and safety**: Obvious logic errors, missing null checks at API boundaries, potential crashes, use-after-free, dangling references, race conditions. This is the highest priority — bugs and safety issues must be caught first. Do NOT nitpick internal code that relies on framework guarantees.

2. **Dead code**: Any code added or left behind that is never called or used, within the scope of the changes. Unused variables, unreachable branches, leftover scaffolding.

3. **Redundant changes**: Changes in the diff that have no functional effect — moving declarations or code blocks to a different location without reason, reformatting untouched code, reordering includes or fields with no purpose. Every line in the diff should serve the feature. If a file appears in `git diff` but contains only no-op rearrangements, flag it for revert.

4. **Code duplication**: Unnecessary repetition of logic that should be shared. Look for near-identical blocks that differ only in minor details and could be unified.

5. **Wrong placement**: Code added to a module where it doesn't logically belong. If another existing module is a clearly better fit for the new code, flag it. Consider the existing module boundaries and responsibilities visible in context.md.

6. **Function decomposition**: For longer functions (roughly 50+ lines), consider whether a logical sub-task could be cleanly extracted into a separate function. This is NOT a hard rule — a 100-line function that flows naturally and isn't easily divisible is perfectly fine. But sometimes even a 20-line function contains a clear isolated subtask that reads better as two 10-line functions. The key is to think about it each time: does extracting improve readability and reduce cognitive load, or does it just scatter logic across call sites for no real benefit? Only suggest extraction when there's a genuinely self-contained piece of logic with a clear name and purpose.

7. **Module structure**: Only in exceptional cases — if a large amount of newly added code (hundreds of lines) is logically distinct from the rest of its host module, suggest extracting it into a new module. But do NOT suggest new modules lightly: every module adds significant build overhead due to PCH and heavy template usage. Only suggest this when the new code is both large enough AND logically separated enough to justify it. At the same time, don't let modules grow into multi-thousand-line monoliths either.

8. **Style compliance**: Verify adherence to REVIEW.md rules (empty line before closing brace, operators at start of continuation lines, minimize type checks with direct cast instead of is+as, no if-with-initializer when simpler alternatives exist) and CLAUDE.md conventions (no unnecessary comments, `auto` usage, no hardcoded sizes — must use .style definitions), etc.

IMPORTANT GUIDELINES:
- Review ONLY the changes made, not pre-existing code in the repository.
- Be pragmatic. Don't suggest changes for the sake of it. Each suggestion should have a clear, concrete benefit.
- Don't suggest adding comments, docstrings, or type annotations — the codebase style avoids these.
- Don't suggest error handling for impossible scenarios or over-engineering.

Write your review to: .ai/<feature-name>/review<R>.md

The review document should contain:

## Code Review - Iteration <R>

## Summary
<1-2 sentence overall assessment>

## Verdict: <APPROVED or NEEDS_CHANGES>

<If APPROVED, stop here. Everything looks good.>

<If NEEDS_CHANGES, continue with:>

## Changes Required

### <Issue 1 title>
- **Category**: <dead code | duplication | wrong placement | function decomposition | module structure | style | correctness>
- **File(s)**: <file paths>
- **Problem**: <clear description of what's wrong>
- **Fix**: <specific description of what to change>

### <Issue 2 title>
...

Keep the list focused. Only include issues that genuinely improve the code. If you find yourself listing more than ~5-6 issues, prioritize the most impactful ones.

When finished, report your verdict clearly as: APPROVED or NEEDS_CHANGES.
```

After the review agent returns, read `review<R>.md`. If the verdict is APPROVED, proceed to Completion. If NEEDS_CHANGES, spawn the fix agent.

### Step 6b: Review Fix Agent

Spawn an agent (Task tool, subagent_type=`general-purpose`):

```
You are a review fix agent. You implement improvements identified during code review.

Read these files:
- .ai/<feature-name>/context.md - Codebase context
- .ai/<feature-name>/plan.md - Original implementation plan
- .ai/<feature-name>/review<R>.md - Code review with required changes

Then read the source files mentioned in the review.

YOUR TASK: Implement ALL changes listed in review<R>.md.

For each issue in the review:
1. Read the relevant source file(s).
2. Make the specified change.
3. Verify the change makes sense in context.

After all changes are made:
1. Build: cmake --build "c:\Telegram\tdesktop\out" --config Debug --target Telegram
2. If the build fails, fix build errors and rebuild until it passes.
3. If build fails with file-locked errors (C1041, LNK1104), STOP and report - do not retry.

Rules:
- Implement exactly the changes from the review, nothing more.
- Follow CLAUDE.md coding conventions.
- Do NOT modify .ai/ files.

When finished, report what changes were made.
```

After the fix agent returns, increment R and loop back to Step 6a (unless R > 3, in which case proceed to Completion).

## Completion

When all phases including build verification and code review are done:
1. Read the final `plan.md` and report the summary to the user.
2. Show which files were modified/created.
3. Note any issues encountered during implementation.
4. Summarize code review iterations: how many rounds, what was found and fixed, or if it was approved on first pass.

## Error Handling

- If any agent fails or gets stuck, report the issue to the user and ask how to proceed.
- If context.md or plan.md is not written properly by an agent, re-spawn that agent with more specific instructions.
- If build errors persist after the build agent's attempts, report the remaining errors to the user.
- If a review fix agent introduces new build errors that it cannot resolve, report to the user.
