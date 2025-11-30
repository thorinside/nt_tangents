# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a BMad Method (BMM) project - an AI-driven agile development framework. The `.bmad/` folder contains:
- **agents/** - Specialized AI personas (PM, Architect, SM, DEV, TEA, UX Designer, etc.)
- **workflows/** - Guided processes across 4 phases (Analysis → Planning → Solutioning → Implementation)
- **teams/** - Pre-configured agent groups
- **testarch/** - Testing infrastructure and knowledge base

## Working with BMad Workflows

### Slash Commands

BMad workflows are available as slash commands. Key patterns:

**Agent commands** - Load specialized personas:
- `/bmad:bmm:agents:pm` - Product Manager
- `/bmad:bmm:agents:architect` - System Architect
- `/bmad:bmm:agents:sm` - Scrum Master
- `/bmad:bmm:agents:dev` - Developer
- `/bmad:bmm:agents:analyst` - Business Analyst

**Workflow commands** - Run development workflows:
- `/bmad:bmm:workflows:workflow-init` - Initialize project tracking
- `/bmad:bmm:workflows:workflow-status` - Check current phase and next steps
- `/bmad:bmm:workflows:prd` - Create Product Requirements Document
- `/bmad:bmm:workflows:architecture` - Design system architecture
- `/bmad:bmm:workflows:create-story` - Draft user stories
- `/bmad:bmm:workflows:dev-story` - Implement a story
- `/bmad:bmm:workflows:sprint-planning` - Initialize sprint tracking

### Project Tracking Files

- `docs/bmm-workflow-status.yaml` - Tracks phase progress (created by workflow-init)
- `docs/sprint-status.yaml` - Tracks epics/stories during implementation

## Development Phases

1. **Phase 1: Analysis** (Optional) - Brainstorming, research, product brief
2. **Phase 2: Planning** (Required) - PRD or tech-spec creation
3. **Phase 3: Solutioning** - Architecture design (for BMad Method/Enterprise tracks)
4. **Phase 4: Implementation** - Story-by-story development

### Planning Tracks

- **Quick Flow** - Bug fixes, simple features (uses tech-spec only)
- **BMad Method** - Full products/platforms (PRD + Architecture + UX)
- **Enterprise Method** - Extended planning with security/DevOps/compliance

## Context Management

- Use fresh conversations for each workflow to avoid context limitations
- Story lifecycle: `backlog → drafted → ready → in-progress → review → done`
- Run `workflow-status` if unsure what to do next
