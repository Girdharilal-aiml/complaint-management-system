# 🗂️ Public Complaint Management Portal

A console-based complaint management system built as an OOP Lab project in C++.
Uses the Windows Console API to render a full GUI-style interface — color-coded
tables, stat cards, sidebar navigation, and an audit log — entirely in the terminal.

---

## ✨ Features

- **Role-based login** — separate Admin and User interfaces
- **Three user types** — Student, Employee, Citizen (each with unique profile fields)
- **Submit complaints** — choose organization, department, category, and priority
- **Admin panel** — view, search, filter, and update status of all complaints
- **Auto-escalation** — complaints pending for 7+ days are automatically flagged
- **Audit log** — every status change is recorded with timestamp and who made it
- **Analytics screen** — bar charts showing complaints by category and organization
- **Persistent storage** — all data saved to `.dat` files between sessions

---

## 🖥️ Platform

> **Windows only** — this project uses `windows.h` and `conio.h` for the console UI.

---

## ⚙️ How to Compile & Run

Make sure you have **MinGW (g++)** installed and added to your PATH.

Open **Command Prompt (CMD)** — not PowerShell — in the project folder:

```cmd
g++ main.cpp -o cms -std=c++17
cms.exe
```

**In VS Code:**
Open the integrated terminal with `Ctrl+`` → click the dropdown → select **Command Prompt** → then run the two commands above.

---

## 🔐 Default Login

| Role  | Username | Password  |
|-------|----------|-----------|
| Admin | `admin`  | `admin123` |
| User  | Register from inside the app | — |

---

## 📁 Project Structure

```
main.cpp          ← entire source code (single-file project)
users.dat         ← auto-created when first user registers
complaints.dat    ← auto-created when first complaint is submitted
audit.dat         ← auto-created when first status change is made
```

> The `.dat` files are generated at runtime and are excluded from the repo via `.gitignore`.

---

## 🧱 Class Design

| Class | Responsibility |
|---|---|
| `Person` | Abstract base class for all user types |
| `Student`, `Employee`, `Citizen` | Concrete user types — inherit from `Person` |
| `Admin` | Admin account — inherits from `Person` |
| `Complaint` | Stores all data for a single complaint |
| `AuditEntry` | Records one status change (who, when, old → new) |
| `Organization` | Static lookup table of organizations and their departments |
| `ComplaintManager` | Central data layer — owns all objects, handles file I/O and business logic |
| `App` | UI controller — runs the main screen loop and handles navigation |

**OOP concepts used:** Inheritance, Polymorphism, Encapsulation, Abstract Classes, File I/O

---

## ⚠️ Known Limitations

- Windows only (uses `windows.h` — not cross-platform)
- Single-file architecture (no header/source split) — intentional for lab submission
- The `|` character in complaint text can corrupt `.dat` files (pipe is used as delimiter)
- Passwords stored in plain text in `users.dat` (no hashing — demo project)

---

## 🛠️ Built With

- C++17
- Windows Console API (`windows.h`, `conio.h`)
- File-based persistence (no external database or libraries)

---

*OOP Lab Project — FAST NUCES*
