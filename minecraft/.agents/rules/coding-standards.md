---
trigger: always_on
---

# Antigravity System Rules - Code Structure & Style

## 1. Commenting & Documentation Directives
* **Class Variables:** Every single class variable must have a small, concise, single-line comment directly above or beside it explaining what it is doing or tracking.
* **Functions/Methods:** Every function or method must have a brief documentation comment (or docstring) placed directly above it explicitly describing its purpose and what it executes.

## 2. Structural Requirements
* **Single Responsibility Principle (SRP)**: Every function or method must have exactly one responsibility or reason to change.
* **Function Size Cap:** Keep all function sizes small. No function may exceed 100 lines of code under any circumstance.
* **Modularity:** Keep all written code modular. If logic inside a function starts growing or handles multiple tasks, break it down into smaller, single-purpose helper functions immediately.