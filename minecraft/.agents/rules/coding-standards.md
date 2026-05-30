---
trigger: always_on
---

# Antigravity System Rules - Code Structure & Style

## 1. Commenting & Documentation Directives

* **File-Level Blocks:** Every file must begin with a documentation block outlining its explicit system responsibility, module path, and build dependencies.
* **Class Variables:** Every single class variable must have a small, concise, single-line comment directly above or beside it explaining what it is doing or tracking.
* **Functions & Methods:** Every function or method must have a brief documentation comment (or docstring) placed directly above it explicitly describing its purpose, execution steps, parameters, and behavioral bounds.
* **Comment Standard**: Try to keep most of top level function docs inside header files and not in cpp files. Put single line comment usint //. For Multiline comment use /**/.

## 2. Structural Requirements

* **Single Responsibility Principle (SRP):** Every function or method must have exactly one responsibility or reason to change. 
* **Function Size Cap:** Keep all function sizes small. No function may exceed **70 lines of executable code** under any circumstance. Aim for 30 to 40 lines.
* **Modularity:** Keep all written code modular. If logic inside a function starts growing or handles multiple tasks, break it down into smaller, single-purpose helper functions immediately.

## 3. Godot specific
* **Getter/Setter**: Try keeping getter setter inside header files and not inside cpp files.