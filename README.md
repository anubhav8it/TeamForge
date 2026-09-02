# TeamForge

## Skill-Based Hackathon & Project Team Matchmaker

TeamForge is a C++ and Qt based PBL project developed for the **Department of Computer Science & Engineering, Graphic Era (Deemed to be University)**.

The project aims to help students form better teams for hackathons, PBL projects, coding competitions, and academic projects by matching the skills students can contribute with the skills required by a project.

---

## Project Information

| Detail | Information |
|---|---|
| **Team Name** | TEAMFORGE |
| **Team ID** | DSCPP-III-2026-T238 |
| **Course** | DS with C (TCS-302) & OOPS with C++ (TCS-307) |
| **Semester** | III |
| **Sections** | ML2, ML1, DS2, ML3 |
| **Domain** | DSA + OOP with C++ |
| **Mentor** | Dr. Hriday Kumar Gupta |
| **Academic Session** | 2026–27 |
| **Department** | Computer Science & Engineering |
| **University** | Graphic Era (Deemed to be University) |

---

## Project Overview

Students often form project teams through friends, existing groups, or informal messaging channels. This can result in teams having overlapping skills while missing important roles.

TeamForge is designed to make this process more structured. Students can provide their skills, experience, and availability, while a project can define the skills and roles it requires. The system then helps retrieve suitable candidates, rank them, and identify the skills covered and the gaps that remain.

The main idea is to encourage **complementary skill matching** instead of forming teams only on the basis of friendship or availability.

---

# PBL Phase Breakdown

The TeamForge project is being developed through three PBL phases.

## Phase 1 – Project Proposal & Planning

**Status: Completed ✅**

Phase 1 establishes the project idea, requirements, architecture, technology choices, course integration, team responsibilities, and development roadmap.

### Phase 1 Includes

- Problem statement and motivation
- Project objectives
- Proposed solution
- Project goals and milestones
- Project approach
- System architecture
- DSA and OOP integration
- Matching score model
- Team task distribution
- Development roadmap
- Project outcomes and deliverables
- Assumptions
- References

### Phase 1 Deliverables

**Report**
- Final report PDF
- LaTeX source
- Team member photographs
- Architecture and figure files
- Report README

**PPT**
- Phase 1 presentation PDF
- LaTeX presentation source
- Presentation assets

---

## Phase 2 – Application Development

**Status: Upcoming ⏳**

Phase 2 will focus on implementing the TeamForge application based on the design prepared in Phase 1.

Planned work includes:

- C++ application development
- Qt desktop interface
- Student profile management
- Project requirement management
- Skill-based candidate retrieval
- Matching and ranking
- Team formation
- Skill coverage and gap checking
- Data-structure implementation
- OOP-based application structure
- Local file persistence
- Initial testing

Phase 2 files will be added to the repository when development begins.

---

## Phase 3 – Testing, Refinement & Final Demonstration

**Status: Upcoming ⏳**

Phase 3 will focus on completing and refining the application.

Planned work includes:

- Final testing
- Bug fixing and refinement
- Performance comparison
- Final documentation
- Final project build
- Demonstration preparation
- Viva preparation
- Final PBL submission

Phase 3 files will be added to the repository when the phase begins.

---

# Technology Stack

- **Programming Language:** C++
- **UI Framework:** Qt
- **IDE:** VS Code / Qt Creator
- **Storage:** Local structured files
- **Version Control:** Git / GitHub

---

# DSA Integration

TeamForge uses Data Structures and Algorithms concepts from TCS-302 as part of the project design.

### Data Structures

- Arrays
- Sets
- Vectors
- Queues
- Priority Queues
- Linked Structures
- Trees
- Graphs
- Hash Maps

### Algorithms

- Searching
- Sorting
- Graph Traversal
- Ranking
- Compatibility checking

These concepts are connected to candidate retrieval, ranking, skill coverage, team formation, and the Algorithm Lab.

---

# OOP Integration

The project uses concepts from TCS-307 including:

- Classes and Objects
- Constructors
- Encapsulation
- Inheritance
- Abstract Classes
- Virtual Functions
- Polymorphism
- Templates
- Operator Overloading
- Exception Handling
- File I/O
- Strategy-based design

---

# Matching Model

The TeamForge matching system uses a weighted scoring approach:

| Matching Factor | Weight |
|---|---:|
| Required-skill coverage | 40% |
| Complementarity | 25% |
| Experience / level | 15% |
| Availability | 10% |
| Team diversity / balance | 10% |

The purpose of the score is to rank suitable candidates while keeping the reason for the ranking understandable.

---

# Team Members

1. **Mukul Fartiyal** – Team / Integration Lead
2. **Anubhav Bisht** – UI / Testing / Documentation Lead
3. **Siddhant Singh** – C++ / OOP Lead
4. **Mehul** – DSA Lead

---

# Complete Repository Structure

The repository is organized phase-wise. The structure will grow as the project moves through the three PBL phases.

```text
TeamForge/
│
├── Phase-1/
│   │
│   ├── Report/
│   │   ├── main.tex
│   │   ├── TeamForge_Report.pdf
│   │   ├── README.md
│   │   │
│   │   ├── images/
│   │   │   ├── member1.jpeg
│   │   │   ├── member2.jpeg
│   │   │   ├── member3.jpeg
│   │   │   └── member4.jpeg
│   │   │
│   │   └── figures/
│   │       └── architecture.tex
│   │
│   └── PPT/
│       ├── TeamForge_Phase1_PPT_Final.tex
│       ├── TeamForge_Phase1_PPT_Final.pdf
│       └── graphicera_logo.png
│
├── Phase-2/
│   └── [Development files will be added here]
│
├── Phase-3/
│   └── [Testing, final build and submission files will be added here]
│
├── README.md
└── .gitignore
