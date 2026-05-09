// ================================================================
//  PUBLIC COMPLAINT MANAGEMENT PORTAL
//  OOP Lab Project — Console GUI using Windows API
//
//  PLATFORM: Windows only (uses windows.h and conio.h)
//
//  HOW TO COMPILE:
//    Open CMD (NOT PowerShell) in this folder, then:
//    g++ main.cpp -o cms -std=c++17
//
//  HOW TO RUN:
//    cms.exe
//
//    In VS Code: open integrated terminal (Ctrl+`) → switch to CMD
//    → run the two commands above separately.
//
//  DEFAULT LOGIN:
//    Admin  →  username: admin   password: admin123
//    Users  →  Register from the app
//
//  DATA FILES (auto-created on first run):
//    users.dat       — registered user accounts
//    complaints.dat  — all submitted complaints
//    audit.dat       — status-change history log
// ================================================================

#include <windows.h>   // Console colors, cursor, screen buffer
#include <conio.h>     // _getch(), _kbhit()
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <map>

using namespace std;

// ================================================================
//  WINDOWS CONSOLE HELPERS
//  These functions wrap the Windows API so the rest of the code
//  can position the cursor, set colors, and draw shapes without
//  dealing with raw WinAPI calls every time.
// ================================================================
HANDLE hCon;   // global handle to the console output window

// Windows console supports 16 foreground and 16 background colors.
// These named constants make the code readable (e.g. RED instead of 12).
enum ConsoleColor {
    BLACK=0, DARK_BLUE=1, DARK_GREEN=2, DARK_CYAN=3,
    DARK_RED=4, DARK_MAGENTA=5, DARK_YELLOW=6, LIGHT_GRAY=7,
    DARK_GRAY=8, BLUE=9, GREEN=10, CYAN=11,
    RED=12, MAGENTA=13, YELLOW=14, WHITE=15
};

// Set the console text color (fg = foreground, bg = background).
// Windows encodes both as: (bg * 16) + fg
void setColor(int fg, int bg = BLACK) {
    SetConsoleTextAttribute(hCon, (WORD)(bg * 16 + fg));
}

// Move cursor to (x, y)
void gotoxy(int x, int y) {
    COORD c = { (SHORT)x, (SHORT)y };
    SetConsoleCursorPosition(hCon, c);
}

// Hide/show blinking cursor
void showCursor(bool show) {
    CONSOLE_CURSOR_INFO ci;
    GetConsoleCursorInfo(hCon, &ci);
    ci.bVisible = show;
    SetConsoleCursorInfo(hCon, &ci);
}

// Clear the entire console screen
void clearScreen() {
    COORD topLeft = {0, 0};
    DWORD written;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hCon, &csbi);
    DWORD cells = csbi.dwSize.X * csbi.dwSize.Y;
    FillConsoleOutputCharacter(hCon, ' ', cells, topLeft, &written);
    FillConsoleOutputAttribute(hCon, csbi.wAttributes, cells, topLeft, &written);
    SetConsoleCursorPosition(hCon, topLeft);
}

// Draw a horizontal line of a character
void drawHLine(int x, int y, int len, char ch, int fg, int bg = BLACK) {
    gotoxy(x, y);
    setColor(fg, bg);
    for (int i = 0; i < len; i++) cout << ch;
}

// Draw a vertical line of a character
void drawVLine(int x, int y, int len, char ch, int fg, int bg = BLACK) {
    setColor(fg, bg);
    for (int i = 0; i < len; i++) {
        gotoxy(x, y + i);
        cout << ch;
    }
}

// Draw a filled rectangle with spaces (background color fill)
void fillRect(int x, int y, int w, int h, int fg, int bg) {
    setColor(fg, bg);
    for (int row = 0; row < h; row++) {
        gotoxy(x, y + row);
        for (int col = 0; col < w; col++) cout << ' ';
    }
}

// Draw a box with border characters
void drawBox(int x, int y, int w, int h, int fg, int bg = BLACK) {
    setColor(fg, bg);
    gotoxy(x, y);
    cout << (char)218;                         // top-left corner ┌
    for (int i = 0; i < w - 2; i++) cout << (char)196;  // top ─
    cout << (char)191;                         // top-right ┐

    for (int row = 1; row < h - 1; row++) {
        gotoxy(x, y + row);       cout << (char)179;  // left │
        gotoxy(x + w - 1, y + row); cout << (char)179;  // right │
    }

    gotoxy(x, y + h - 1);
    cout << (char)192;                         // bottom-left └
    for (int i = 0; i < w - 2; i++) cout << (char)196;  // bottom ─
    cout << (char)217;                         // bottom-right ┘
}

// Print text at position with color
void printAt(int x, int y, const string& text, int fg, int bg = BLACK) {
    gotoxy(x, y);
    setColor(fg, bg);
    cout << text;
}

// Print centered text within a width
void printCentered(int x, int y, int width, const string& text, int fg, int bg = BLACK) {
    int pad = (width - (int)text.size()) / 2;
    if (pad < 0) pad = 0;
    gotoxy(x + pad, y);
    setColor(fg, bg);
    cout << text.substr(0, width);
}

// Draw a filled bar (header/button style)
void drawBar(int x, int y, int w, int h, int fg, int bg) {
    fillRect(x, y, w, h, fg, bg);
}

// ================================================================
//  UTILITY FUNCTIONS
//  Small helper functions used throughout the program for
//  time formatting, string splitting, date math, etc.
// ================================================================
// Returns the current date and time as a formatted string, e.g. "2024-05-09 14:30"
string getCurrentTime() {
    time_t now = time(nullptr);
    tm* t = localtime(&now);
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", t);
    return string(buf);
}

// Splits a pipe-delimited string ("a|b|c") into a vector ["a","b","c"].
// Used when loading data from .dat files.
vector<string> splitLine(const string& line) {
    vector<string> parts;
    stringstream ss(line);
    string token;
    while (getline(ss, token, '|'))
        parts.push_back(token);
    return parts;
}

// Returns how many days have passed since the given date string ("YYYY-MM-DD ...").
// Used to auto-escalate complaints that have been pending for 7+ days.
int daysSince(const string& dateStr) {
    if (dateStr.size() < 10) return 0;
    tm t = {};
    sscanf(dateStr.c_str(), "%d-%d-%d", &t.tm_year, &t.tm_mon, &t.tm_mday);
    t.tm_year -= 1900;
    t.tm_mon  -= 1;
    time_t then = mktime(&t);
    time_t now  = time(nullptr);
    return (int)difftime(now, then) / 86400;
}

// Truncate string if longer than maxLen, add "..." at end
string truncate(const string& s, int maxLen) {
    if ((int)s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 3) + "...";
}

// Pad string to exactly width characters
string padRight(const string& s, int width) {
    if ((int)s.size() >= width) return s.substr(0, width);
    return s + string(width - s.size(), ' ');
}

// Reads a line of text typed by the user at position (x, y).
// Shows '*' instead of actual characters when password=true.
// Supports Backspace to delete and Escape to cancel (returns empty string).
string readInput(int x, int y, int maxLen, bool password = false) {
    string result;
    showCursor(true);
    gotoxy(x, y);
    setColor(WHITE, DARK_BLUE);
    // Draw input field
    for (int i = 0; i < maxLen; i++) cout << ' ';
    gotoxy(x, y);

    while (true) {
        int ch = _getch();
        if (ch == 13) break;                    // Enter
        if (ch == 27) { result = ""; break; }   // Escape — cancel
        if (ch == 8) {                           // Backspace
            if (!result.empty()) {
                result.pop_back();
                gotoxy(x + (int)result.size(), y);
                setColor(WHITE, DARK_BLUE);
                cout << ' ';
                gotoxy(x + (int)result.size(), y);
            }
        } else if (ch >= 32 && (int)result.size() < maxLen) {
            result += (char)ch;
            gotoxy(x + (int)result.size() - 1, y);
            setColor(WHITE, DARK_BLUE);
            cout << (password ? '*' : (char)ch);
        }
    }
    showCursor(false);
    return result;
}

// Wait for any key press
void waitKey(const string& msg = "Press any key to continue...") {
    setColor(DARK_GRAY);
    cout << "\n " << msg;
    _getch();
}

// Blocks until the user presses one of the allowed keys (case-insensitive).
// Example: readChoice("123q") only accepts '1', '2', '3', or 'q'.
char readChoice(const string& options) {
    while (true) {
        int ch = _getch();
        char c = tolower((char)ch);
        if (options.find(c) != string::npos) return c;
        if (options.find((char)ch) != string::npos) return (char)ch;
    }
}

// ================================================================
//  SCREENS ENUM
//  Each value represents one "page" of the application.
//  The App class switches between these in its main loop.
// ================================================================
enum Screen {
    LOGIN, REGISTER, USER_DASHBOARD, SUBMIT_COMPLAINT,
    MY_COMPLAINTS, ADMIN_DASHBOARD, ADMIN_ALL,
    ADMIN_DETAIL, ADMIN_ANALYTICS, ADMIN_AUDIT
};

// ================================================================
//  ORGANIZATION CLASS
//  Provides static lists of organizations and their departments.
//  No objects are created — these are just lookup tables.
//  To add a new org, add it to getAllOrgs() and add a getDepts() case.
// ================================================================
class Organization {
public:
    static vector<string> getAllOrgs() {
        return { "NADRA", "University", "Bank", "Hospital",
                 "Police Station", "WAPDA / Electricity",
                 "PTCL / Telecom", "Municipal Committee" };
    }
    static vector<string> getDepts(const string& orgName) {
        if (orgName == "NADRA")
            return { "CNIC Office", "Passport Office", "Birth Certificate" };
        if (orgName == "University")
            return { "Exam Office", "Finance Dept", "IT Dept", "Registrar", "Dean Office" };
        if (orgName == "Bank")
            return { "Account Services", "Loans", "Card Services" };
        if (orgName == "Hospital")
            return { "Emergency", "OPD", "Billing" };
        if (orgName == "Police Station")
            return { "FIR Registration", "Traffic", "Investigations" };
        if (orgName == "WAPDA / Electricity")
            return { "Billing", "Outages", "New Connection" };
        if (orgName == "PTCL / Telecom")
            return { "Internet Issues", "Billing", "New Connection" };
        if (orgName == "Municipal Committee")
            return { "Sanitation", "Roads", "Water Supply" };
        return { "General" };
    }
};

// ================================================================
//  COMPLAINT CLASS
//  Holds all data for a single complaint.
//  Also handles converting itself to/from a pipe-delimited string
//  for saving and loading from complaints.dat.
// ================================================================
class Complaint {
public:
    int    id;            // unique auto-incremented ID
    string submittedBy;   // username of who filed the complaint
    string title;         // short subject line
    string category;      // IT / HR / Finance / etc.
    string description;   // full details typed by the user
    string priority;      // Low / Medium / High
    string status;        // Pending / In Progress / Resolved
    string date;          // timestamp when complaint was submitted
    string orgName;       // target organization (e.g. "NADRA")
    string orgDept;       // target department within that org
    bool   escalated;     // true if pending for 7+ days without update

    Complaint() : id(0), escalated(false) {}
    Complaint(int id, string by, string title, string cat,
              string desc, string pri, string date, string org, string dept)
        : id(id), submittedBy(by), title(title), category(cat),
          description(desc), priority(pri), status("Pending"),
          date(date), orgName(org), orgDept(dept), escalated(false) {}

    // Returns the color code to use when displaying this complaint's status
    int getStatusColor() const {
        if (status == "Resolved")    return GREEN;
        if (status == "In Progress") return YELLOW;
        return RED;
    }
    // Returns the color code to use when displaying this complaint's priority
    int getPriorityColor() const {
        if (priority == "High")   return RED;
        if (priority == "Medium") return YELLOW;
        return GREEN;
    }

    // Serializes this complaint into one pipe-delimited line for file storage
    string saveToString() const {
        return to_string(id) + "|" + submittedBy + "|" + title + "|" +
               category + "|" + description + "|" + priority + "|" +
               status + "|" + date + "|" + orgName + "|" + orgDept + "|" +
               (escalated ? "1" : "0");
    }
    // Deserializes a pipe-delimited line back into a Complaint object
    static Complaint loadFromString(const string& line) {
        vector<string> p = splitLine(line);
        Complaint c;
        // A valid complaint line must have exactly 11 fields.
        // If the file is corrupted or incomplete, we skip the line silently.
        if (p.size() >= 11) {
            c.id          = stoi(p[0]);
            c.submittedBy = p[1]; c.title    = p[2]; c.category = p[3];
            c.description = p[4]; c.priority = p[5]; c.status   = p[6];
            c.date        = p[7]; c.orgName  = p[8]; c.orgDept  = p[9];
            c.escalated   = (p[10] == "1");
        }
        return c;
    }
};

// ================================================================
//  AUDIT ENTRY CLASS
//  Records a single status change on a complaint.
//  Every time an admin changes a complaint's status, one AuditEntry
//  is created and saved to audit.dat — forming a full change history.
// ================================================================
class AuditEntry {
public:
    int    complaintId;
    string changedBy, oldStatus, newStatus, timestamp;

    AuditEntry() : complaintId(0) {}
    AuditEntry(int id, string by, string oldS, string newS, string time)
        : complaintId(id), changedBy(by), oldStatus(oldS), newStatus(newS), timestamp(time) {}

    string saveToString() const {
        return to_string(complaintId) + "|" + changedBy + "|" +
               oldStatus + "|" + newStatus + "|" + timestamp;
    }
    static AuditEntry loadFromString(const string& line) {
        vector<string> p = splitLine(line);
        AuditEntry e;
        if (p.size() >= 5) {
            e.complaintId = stoi(p[0]);
            e.changedBy   = p[1]; e.oldStatus = p[2];
            e.newStatus   = p[3]; e.timestamp = p[4];
        }
        return e;
    }
};

// ================================================================
//  PERSON CLASS  (Abstract Base Class)
//  Defines the common interface for all user types.
//  Student, Employee, Citizen, and Admin all inherit from this.
//
//  Pure virtual methods force every subclass to implement:
//    getRole()      — "User" or "Admin"
//    getType()      — "Student", "Employee", "Citizen", "Admin"
//    getExtraInfo() — type-specific detail shown on the dashboard
//    saveToString() — serialized form for saving to users.dat
// ================================================================
class Person {
protected:
    string username, password, fullName, contact;
public:
    Person() {}
    Person(string u, string p, string n, string c)
        : username(u), password(p), fullName(n), contact(c) {}
    virtual ~Person() {}

    virtual string getRole()      const = 0;
    virtual string getType()      const = 0;
    virtual string getExtraInfo() const = 0;
    virtual string saveToString() const = 0;

    string getUsername() const { return username; }
    string getFullName() const { return fullName; }
    string getContact()  const { return contact; }
    bool   checkPassword(const string& p) const { return password == p; }
};

// ================================================================
//  CONCRETE USER TYPES
//  Each type stores extra fields specific to that kind of user.
//  They all inherit from Person and implement its virtual methods.
// ================================================================
class Student : public Person {
    string universityName, rollNumber;
public:
    Student() {}
    Student(string u, string p, string n, string c, string uni, string roll)
        : Person(u,p,n,c), universityName(uni), rollNumber(roll) {}
    string getRole()      const override { return "User"; }
    string getType()      const override { return "Student"; }
    string getExtraInfo() const override {
        return "University: " + universityName + "  Roll: " + rollNumber;
    }
    string saveToString() const override {
        return "Student|" + username + "|" + password + "|" +
               fullName + "|" + contact + "|" + universityName + "|" + rollNumber;
    }
};

class Employee : public Person {
    string companyName, designation;
public:
    Employee() {}
    Employee(string u, string p, string n, string c, string company, string desig)
        : Person(u,p,n,c), companyName(company), designation(desig) {}
    string getRole()      const override { return "User"; }
    string getType()      const override { return "Employee"; }
    string getExtraInfo() const override {
        return "Company: " + companyName + "  Role: " + designation;
    }
    string saveToString() const override {
        return "Employee|" + username + "|" + password + "|" +
               fullName + "|" + contact + "|" + companyName + "|" + designation;
    }
};

class Citizen : public Person {
    string cnic, city;
public:
    Citizen() {}
    Citizen(string u, string p, string n, string c, string cnic, string city)
        : Person(u,p,n,c), cnic(cnic), city(city) {}
    string getRole()      const override { return "User"; }
    string getType()      const override { return "Citizen"; }
    string getExtraInfo() const override {
        return "CNIC: " + cnic + "  City: " + city;
    }
    string saveToString() const override {
        return "Citizen|" + username + "|" + password + "|" +
               fullName + "|" + contact + "|" + cnic + "|" + city;
    }
};

class Admin : public Person {
public:
    Admin() {}
    Admin(string u, string p, string n, string c) : Person(u,p,n,c) {}
    string getRole()      const override { return "Admin"; }
    string getType()      const override { return "Admin"; }
    string getExtraInfo() const override { return "System Administrator"; }
    string saveToString() const override { return ""; }
};

// ================================================================
//  COMPLAINT MANAGER CLASS
//  The central data manager for the entire application.
//  Owns all users, complaints, and audit entries.
//  Handles all business logic: login, registration, searching,
//  status updates, escalation checks, and file I/O.
//
//  NOTE: users is public so the App class can display user info,
//  but all mutations go through the manager's methods.
// ================================================================
class ComplaintManager {
    vector<Complaint>  complaints;
    vector<AuditEntry> auditLog;
    int                nextId;

public:
    vector<Person*> users;
    Admin           adminAccount;

    ComplaintManager() : nextId(1) {
        adminAccount = Admin("admin", "admin123", "System Admin", "N/A");
        loadUsers();        // load registered users from users.dat
        loadComplaints();   // load complaints from complaints.dat
        loadAudit();        // load status history from audit.dat
        checkEscalations(); // flag any complaints pending 7+ days
    }
    ~ComplaintManager() {
        for (Person* p : users) delete p;
    }

    // Returns true if the username is already in use (including "admin" which is reserved)
    bool usernameExists(const string& u) {
        if (u == "admin") return true;
        for (Person* p : users)
            if (p->getUsername() == u) return true;
        return false;
    }

    // Adds a new user to memory and immediately saves to disk
    void registerUser(Person* p) {
        users.push_back(p);
        saveUsers();
    }

    // Validates credentials and returns the matching Person pointer,
    // or nullptr if username/password don't match any account.
    Person* login(const string& username, const string& password) {
        if (adminAccount.getUsername() == username && adminAccount.checkPassword(password))
            return &adminAccount;
        for (Person* p : users)
            if (p->getUsername() == username && p->checkPassword(password))
                return p;
        return nullptr;
    }

    // Creates a new complaint with an auto-incremented ID and saves to disk
    void addComplaint(const string& by, const string& title, const string& cat,
                      const string& desc, const string& pri,
                      const string& org, const string& dept) {
        complaints.push_back(Complaint(nextId++, by, title, cat, desc, pri, getCurrentTime(), org, dept));
        saveComplaints();
    }

    // Changes a complaint's status and records the change in the audit log.
    // Also clears the escalation flag when a complaint is resolved.
    void updateStatus(int id, const string& newStatus, const string& changedBy) {
        for (Complaint& c : complaints) {
            if (c.id == id) {
                auditLog.push_back(AuditEntry(id, changedBy, c.status, newStatus, getCurrentTime()));
                c.status = newStatus;
                if (newStatus == "Resolved") c.escalated = false;
                break;
            }
        }
        saveComplaints();
        saveAudit();
    }

    // Scans all complaints and marks any that have been "Pending" for 7+ days
    // as escalated. Called once on startup so the flag is always up to date.
    void checkEscalations() {
        for (Complaint& c : complaints)
            if (c.status == "Pending" && !c.escalated && daysSince(c.date) >= 7)
                c.escalated = true;
    }

    vector<Complaint> getAllComplaints()              { return complaints; }
    vector<Complaint> getUserComplaints(const string& u) {
        vector<Complaint> r;
        for (Complaint& c : complaints)
            if (c.submittedBy == u) r.push_back(c);
        return r;
    }

    vector<Complaint> searchComplaints(const string& keyword, const string& statusF,
                                        const string& catF, const string& priF) {
        vector<Complaint> result;
        string kw = keyword;
        transform(kw.begin(), kw.end(), kw.begin(), ::tolower);
        for (Complaint& c : complaints) {
            string title = c.title;
            transform(title.begin(), title.end(), title.begin(), ::tolower);
            if (!kw.empty() && title.find(kw) == string::npos) continue;
            if (statusF != "All" && !statusF.empty() && c.status   != statusF) continue;
            if (catF    != "All" && !catF.empty()    && c.category != catF)    continue;
            if (priF    != "All" && !priF.empty()    && c.priority != priF)    continue;
            result.push_back(c);
        }
        return result;
    }

    Complaint* findById(int id) {
        for (Complaint& c : complaints) if (c.id == id) return &c;
        return nullptr;
    }

    vector<AuditEntry> getAuditFor(int id) {
        vector<AuditEntry> r;
        for (AuditEntry& e : auditLog) if (e.complaintId == id) r.push_back(e);
        return r;
    }
    vector<AuditEntry> getAllAudit() { return auditLog; }

    int countByStatus(const string& s) {
        int n = 0; for (Complaint& c : complaints) if (c.status == s) n++; return n;
    }
    int countEscalated() {
        int n = 0; for (Complaint& c : complaints) if (c.escalated) n++; return n;
    }
    map<string,int> countByCategory() {
        map<string,int> r; for (Complaint& c : complaints) r[c.category]++; return r;
    }
    map<string,int> countByOrg() {
        map<string,int> r; for (Complaint& c : complaints) r[c.orgName]++; return r;
    }
    int totalComplaints() { return (int)complaints.size(); }

    // ── File I/O ──────────────────────────────────────────────
    // Each save function overwrites the entire file from scratch.
    // Each load function reads line by line and deserializes objects.
    void saveComplaints() {
        ofstream f("complaints.dat");
        f << nextId << "\n";
        for (Complaint& c : complaints) f << c.saveToString() << "\n";
    }
    void loadComplaints() {
        ifstream f("complaints.dat");
        if (!f.is_open()) return;
        f >> nextId; f.ignore();
        string line;
        while (getline(f, line))
            if (!line.empty()) complaints.push_back(Complaint::loadFromString(line));
    }
    void saveAudit() {
        ofstream f("audit.dat");
        for (AuditEntry& e : auditLog) f << e.saveToString() << "\n";
    }
    void loadAudit() {
        ifstream f("audit.dat");
        if (!f.is_open()) return;
        string line;
        while (getline(f, line))
            if (!line.empty()) auditLog.push_back(AuditEntry::loadFromString(line));
    }
    void saveUsers() {
        ofstream f("users.dat");
        for (Person* p : users) f << p->saveToString() << "\n";
    }
    void loadUsers() {
        ifstream f("users.dat");
        if (!f.is_open()) return;
        string line;
        while (getline(f, line)) {
            if (line.empty()) continue;
            vector<string> p = splitLine(line);
            // Each user line needs at least 7 fields: type + 4 base + 2 extra.
            // Skip silently if the line is malformed (e.g. file was hand-edited).
            if (p.size() < 7) continue;
            if      (p[0] == "Student")  users.push_back(new Student (p[1],p[2],p[3],p[4],p[5],p[6]));
            else if (p[0] == "Employee") users.push_back(new Employee(p[1],p[2],p[3],p[4],p[5],p[6]));
            else if (p[0] == "Citizen")  users.push_back(new Citizen (p[1],p[2],p[3],p[4],p[5],p[6]));
            // Unknown types are ignored (forward-compatibility)
        }
    }
};

// ================================================================
//  CONSOLE UI HELPERS
//  Higher-level drawing functions built on top of the low-level
//  console helpers above. These draw common UI elements like the
//  top header bar, sidebar menu, stat cards, and badges.
// ================================================================

// Draws the two-row title bar at the very top of every screen
void drawHeader(const string& title, const string& subtitle = "") {
    drawBar(0, 0, 120, 1, WHITE, DARK_BLUE);
    printCentered(0, 0, 120, "  PUBLIC COMPLAINT MANAGEMENT PORTAL  ", WHITE, DARK_BLUE);
    drawBar(0, 1, 120, 1, LIGHT_GRAY, DARK_BLUE);
    if (!subtitle.empty()) {
        printAt(2, 1, title, YELLOW, DARK_BLUE);
        printAt(2 + (int)title.size() + 2, 1, "| " + subtitle, LIGHT_GRAY, DARK_BLUE);
    } else {
        printCentered(0, 1, 120, title, YELLOW, DARK_BLUE);
    }
    drawBar(0, 2, 120, 1, DARK_BLUE, DARK_BLUE);
}

// Draws the navigation menu on the left side (columns 0-19).
// Shows different options depending on whether the user is an admin.
void drawSidebar(bool isAdmin) {
    drawBar(0, 3, 20, 40, LIGHT_GRAY, DARK_GRAY);
    printAt(1, 4, "MENU", CYAN, DARK_GRAY);
    drawHLine(0, 5, 20, '-', LIGHT_GRAY, DARK_GRAY);
    if (isAdmin) {
        printAt(1,  6, "[1] All Complaints", WHITE, DARK_GRAY);
        printAt(1,  7, "[2] Analytics",      WHITE, DARK_GRAY);
        printAt(1,  8, "[3] Audit Log",      WHITE, DARK_GRAY);
    } else {
        printAt(1,  6, "[1] Dashboard",      WHITE, DARK_GRAY);
        printAt(1,  7, "[2] New Complaint",  WHITE, DARK_GRAY);
        printAt(1,  8, "[3] My Complaints",  WHITE, DARK_GRAY);
    }
    drawHLine(0, 9, 20, '-', LIGHT_GRAY, DARK_GRAY);
    printAt(1, 10, "[L] Logout",             YELLOW, DARK_GRAY);
    printAt(1, 11, "[Q] Quit",               RED,    DARK_GRAY);
}

// Draws a small bordered box showing a number and a label below it.
// Used on dashboards to show counts (Total, Pending, Resolved, etc.)
void drawStatCard(int x, int y, const string& num, const string& label, int color) {
    drawBox(x, y, 22, 4, color);
    printCentered(x + 1, y + 1, 20, num,   color);
    printCentered(x + 1, y + 2, 20, label, LIGHT_GRAY);
}

// Draw a status badge inline
void drawBadge(int x, int y, const string& text, int color) {
    setColor(BLACK, color);
    gotoxy(x, y);
    cout << " " << text << " ";
    setColor(WHITE);
}

// ================================================================
//  COMPLAINT TABLE
//  Draws a paginated table of complaints starting at row startY.
//  startRow controls the scroll offset (which row to start from).
//  maxRows controls how many rows fit on screen at once.
// ================================================================
void drawComplaintTable(const vector<Complaint>& list, int startY, bool isAdmin,
                         int startRow = 0, int maxRows = 12) {
    // Header row
    drawBar(21, startY, 99, 1, WHITE, DARK_BLUE);
    printAt(22, startY, padRight("#", 4), WHITE, DARK_BLUE);
    printAt(26, startY, padRight("Title", 28), WHITE, DARK_BLUE);
    printAt(55, startY, padRight("Organization", 14), WHITE, DARK_BLUE);
    printAt(70, startY, padRight("Category", 10), WHITE, DARK_BLUE);
    printAt(81, startY, padRight("Priority", 9), WHITE, DARK_BLUE);
    printAt(91, startY, padRight("Status", 12), WHITE, DARK_BLUE);
    if (isAdmin) printAt(104, startY, "User", WHITE, DARK_BLUE);

    int shown = 0;
    for (int i = startRow; i < (int)list.size() && shown < maxRows; i++, shown++) {
        const Complaint& c = list[i];
        int row = startY + 1 + shown;
        int bg  = (shown % 2 == 0) ? BLACK : DARK_GRAY;
        if (c.escalated) bg = DARK_RED;

        drawBar(21, row, 99, 1, WHITE, bg);

        string idStr = "#" + to_string(c.id) + (c.escalated ? "!" : " ");
        printAt(22, row, padRight(idStr, 4), c.escalated ? RED : WHITE, bg);
        printAt(26, row, padRight(truncate(c.title, 27), 28), WHITE, bg);
        printAt(55, row, padRight(truncate(c.orgName, 13), 14), LIGHT_GRAY, bg);
        printAt(70, row, padRight(truncate(c.category, 9), 10), LIGHT_GRAY, bg);

        // Priority badge color inline
        int priColor = (c.priority=="High") ? RED : (c.priority=="Medium") ? YELLOW : GREEN;
        int staColor = (c.status=="Resolved") ? GREEN : (c.status=="In Progress") ? YELLOW : RED;
        setColor(priColor, bg); gotoxy(81, row); cout << padRight(c.priority, 9);
        setColor(staColor, bg); gotoxy(91, row); cout << padRight(c.status,   12);

        if (isAdmin) {
            printAt(104, row, truncate(c.submittedBy, 14), CYAN, bg);
        }
    }
    // Footer
    if ((int)list.size() > maxRows || startRow > 0) {
        printAt(22, startY + 1 + shown,
                "  Total: " + to_string(list.size()) +
                "  |  Showing rows " + to_string(startRow+1) +
                "-" + to_string(min(startRow+maxRows,(int)list.size())),
                DARK_GRAY);
    }
}

// Displays a numbered list of options and asks the user to pick one by number.
// Returns the 0-based index of their choice, or -1 if input was invalid/empty.
int pickFromList(const vector<string>& items, const string& prompt, int x, int y) {
    for (int i = 0; i < (int)items.size(); i++) {
        printAt(x, y + i, "  [" + to_string(i+1) + "] " + items[i], CYAN);
    }
    printAt(x, y + (int)items.size() + 1, prompt + " (1-" + to_string(items.size()) + "): ", YELLOW);
    string inp = readInput(x + (int)prompt.size() + 9, y + (int)items.size() + 1, 3);
    if (inp.empty()) return -1;
    try {
        int idx = stoi(inp) - 1;
        if (idx < 0 || idx >= (int)items.size()) return -1;
        return idx;
    } catch (...) {
        // User typed something that isn't a number — treat as cancel
        return -1;
    }
}

// ================================================================
//  APP CLASS
//  The main application controller.
//  Owns the ComplaintManager (data layer) and drives the UI loop.
//
//  How it works:
//    run() loops forever, calling the draw function for whichever
//    screen is currently active. Each draw function handles its own
//    input and sets currentScreen to navigate to the next screen.
// ================================================================
class App {
    Screen           currentScreen;  // which screen to draw next iteration
    ComplaintManager manager;        // owns all data and business logic
    Person*          loggedInUser;   // nullptr when no one is logged in
    int              selectedId;     // ID of complaint being viewed in detail
    int              tableScrollRow; // scroll offset for complaint tables

    // ── SCREEN DRAWING ─────────────────────────────────────────

    void drawLogin() {
        clearScreen();
        drawBar(0, 0, 120, 1, WHITE, DARK_BLUE);
        printCentered(0, 0, 120, "  PUBLIC COMPLAINT MANAGEMENT PORTAL  ", WHITE, DARK_BLUE);

        drawBox(35, 3, 50, 18, CYAN);
        printCentered(35, 4, 50, "LOGIN", CYAN);
        drawHLine(36, 5, 48, '-', DARK_GRAY);

        printAt(37,  7, "Username :", LIGHT_GRAY);
        printAt(37, 10, "Password :", LIGHT_GRAY);

        drawBar(48,  7, 35, 1, WHITE, DARK_BLUE);
        drawBar(48, 10, 35, 1, WHITE, DARK_BLUE);

        printAt(37, 13, "[Enter] Login",    GREEN);
        printAt(37, 14, "[R]     Register", YELLOW);
        printAt(37, 15, "[Q]     Quit",     RED);
        printAt(37, 17, "Hint: admin / admin123", DARK_GRAY);

        // Read username (Escape returns empty string — do nothing, redraw)
        string uname = readInput(48, 7, 30);
        if (uname.empty()) return;

        // Allow pressing R (or r) as the username to jump to registration
        if (uname == "R" || uname == "r") {
            currentScreen = REGISTER;
            return;
        }

        string pass = readInput(48, 10, 30, /*password=*/true);

        Person* user = manager.login(uname, pass);
        if (user) {
            loggedInUser   = user;
            tableScrollRow = 0;
            currentScreen  = (user->getRole() == "Admin") ? ADMIN_DASHBOARD : USER_DASHBOARD;
        } else {
            printAt(37, 19, "Wrong username or password!", RED);
            Sleep(1500);  // pause so the user can read the message
        }
    }

    void drawRegister() {
        clearScreen();
        drawHeader("REGISTER NEW ACCOUNT");

        drawBox(21, 3, 98, 30, CYAN);
        printAt(23, 4, "Account Type: [1] Student   [2] Employee   [3] Citizen", YELLOW);

        int typeIdx = -1;
        while (typeIdx < 0 || typeIdx > 2) {
            printAt(23, 5, "Select type (1/2/3): ", LIGHT_GRAY);
            char tc = readChoice("123");
            typeIdx = tc - '1';
        }
        string types[] = {"Student","Employee","Citizen"};
        string ex1lbl[] = {"University Name","Company Name","CNIC Number"};
        string ex2lbl[] = {"Roll Number",    "Designation", "City"};
        string selType  = types[typeIdx];

        printAt(23, 6, "Type selected: " + selType, GREEN);
        drawHLine(22, 7, 96, '-', DARK_GRAY);

        auto field = [&](int row, const string& label, bool pw = false) -> string {
            printAt(23, row, padRight(label + ":", 20), LIGHT_GRAY);
            drawBar(44, row, 50, 1, WHITE, DARK_BLUE);
            return readInput(44, row, 48, pw);
        };

        string uname = field(8,  "Username");
        string pass  = field(9,  "Password", true);
        string name  = field(10, "Full Name");
        string cont  = field(11, "Contact");
        string ex1   = field(12, ex1lbl[typeIdx]);
        string ex2   = field(13, ex2lbl[typeIdx]);

        // Validate — all three core fields must be filled in
        if (uname.empty() || pass.empty() || name.empty()) {
            printAt(23, 15, "Username, Password, and Full Name are required.", RED);
            Sleep(1500);
            currentScreen = LOGIN;
            return;
        }
        // Enforce a minimum password length so accounts aren't trivially weak
        if ((int)pass.size() < 4) {
            printAt(23, 15, "Password must be at least 4 characters.", RED);
            Sleep(1500);
            currentScreen = REGISTER;
            return;
        }
        if (manager.usernameExists(uname)) {
            printAt(23, 15, "That username is already taken. Please choose another.", RED);
            Sleep(1500);
            currentScreen = REGISTER;
            return;
        }

        Person* newUser = nullptr;
        if      (selType == "Student")  newUser = new Student (uname,pass,name,cont,ex1,ex2);
        else if (selType == "Employee") newUser = new Employee(uname,pass,name,cont,ex1,ex2);
        else if (selType == "Citizen")  newUser = new Citizen (uname,pass,name,cont,ex1,ex2);

        if (newUser) {
            manager.registerUser(newUser);
            printAt(23, 15, "  Account created successfully! Returning to login...", GREEN);
            Sleep(1500);
        }
        currentScreen = LOGIN;
    }

    // ── USER DASHBOARD ─────────────────────────────────────────
    void drawUserDashboard() {
        clearScreen();
        drawHeader("USER DASHBOARD", loggedInUser->getFullName() + " [" + loggedInUser->getType() + "]");
        drawSidebar(false);

        auto myList = manager.getUserComplaints(loggedInUser->getUsername());
        int pending=0, inProg=0, resolved=0, escalated=0;
        for (const Complaint& c : myList) {
            if (c.status == "Pending")     pending++;
            if (c.status == "In Progress") inProg++;
            if (c.status == "Resolved")    resolved++;
            if (c.escalated)               escalated++;
        }

        // Stat cards
        drawStatCard(21,  3, to_string((int)myList.size()), "My Complaints", CYAN);
        drawStatCard(45,  3, to_string(pending),            "Pending",       RED);
        drawStatCard(69,  3, to_string(inProg),             "In Progress",   YELLOW);
        drawStatCard(93,  3, to_string(resolved),           "Resolved",      GREEN);

        // User info
        printAt(22, 8, loggedInUser->getExtraInfo(), CYAN);

        if (escalated > 0) {
            printAt(22, 9, "  WARNING: " + to_string(escalated) +
                           " complaint(s) ESCALATED (pending > 7 days)!", RED);
        }

        printAt(22, 11, "Recent Complaints:", YELLOW);
        int maxRows = 10;
        drawComplaintTable(myList, 12, false, tableScrollRow, maxRows);

        // Nav
        printAt(22, 25, "[1] Dashboard  [2] New Complaint  [3] My Complaints  "
                        "[V] View Detail  [L] Logout  [Q] Quit", DARK_GRAY);

        handleUserNav(myList, 12, maxRows);
    }

    // ── MY COMPLAINTS ──────────────────────────────────────────
    void drawMyComplaints() {
        clearScreen();
        drawHeader("MY COMPLAINTS", loggedInUser->getFullName());
        drawSidebar(false);

        auto myList = manager.getUserComplaints(loggedInUser->getUsername());
        int maxRows = 14;
        drawComplaintTable(myList, 4, false, tableScrollRow, maxRows);

        printAt(22, 20, "[V] View Detail  [U] Scroll Up  [D] Scroll Down  "
                        "[1] Dashboard  [2] New Complaint  [L] Logout", DARK_GRAY);

        handleUserNav(myList, 4, maxRows);
    }

    // ── SUBMIT COMPLAINT ───────────────────────────────────────
    void drawSubmitComplaint() {
        clearScreen();
        drawHeader("SUBMIT NEW COMPLAINT", loggedInUser->getFullName());
        drawSidebar(false);

        drawBox(21, 3, 98, 30, CYAN);
        printAt(23, 4, "Fill in complaint details (press Enter after each field)", YELLOW);
        drawHLine(22, 5, 96, '-', DARK_GRAY);

        auto orgs = Organization::getAllOrgs();
        printAt(23, 6, "Organization:", LIGHT_GRAY);
        int orgIdx = pickFromList(orgs, "Select org", 23, 7);
        if (orgIdx < 0) { currentScreen = USER_DASHBOARD; return; }
        string orgName = orgs[orgIdx];

        auto depts = Organization::getDepts(orgName);
        printAt(23, 7 + (int)orgs.size() + 2, "Department:", LIGHT_GRAY);
        int deptIdx = pickFromList(depts, "Select dept", 23, 8 + (int)orgs.size() + 2);
        if (deptIdx < 0) { currentScreen = USER_DASHBOARD; return; }
        string deptName = depts[deptIdx];

        // Clear area and continue collecting input
        clearScreen();
        drawHeader("SUBMIT NEW COMPLAINT", loggedInUser->getFullName());
        drawSidebar(false);
        drawBox(21, 3, 98, 26, CYAN);

        printAt(23, 4, "Organization : " + orgName + " / " + deptName, GREEN);
        drawHLine(22, 5, 96, '-', DARK_GRAY);

        auto fieldIn = [&](int row, const string& label) -> string {
            printAt(23, row, padRight(label + ":", 18), LIGHT_GRAY);
            drawBar(42, row, 73, 1, WHITE, DARK_BLUE);
            return readInput(42, row, 71);
        };

        string title = fieldIn(6, "Complaint Title");
        if (title.empty()) { currentScreen = USER_DASHBOARD; return; }

        printAt(23, 7, "Description (one line):", LIGHT_GRAY);
        drawBar(23, 8, 92, 1, WHITE, DARK_BLUE);
        string desc = readInput(23, 8, 90);
        if (desc.empty()) { currentScreen = USER_DASHBOARD; return; }

        vector<string> cats = {"IT","HR","Finance","Facilities","Academic","Legal","Other"};
        printAt(23, 10, "Category:", LIGHT_GRAY);
        int catIdx = pickFromList(cats, "Select category", 23, 11);
        if (catIdx < 0) { currentScreen = USER_DASHBOARD; return; }

        vector<string> pris = {"Low","Medium","High"};
        printAt(23, 11 + (int)cats.size() + 2, "Priority:", LIGHT_GRAY);
        int priIdx = pickFromList(pris, "Select priority", 23, 12 + (int)cats.size() + 2);
        if (priIdx < 0) { currentScreen = USER_DASHBOARD; return; }

        manager.addComplaint(loggedInUser->getUsername(), title, cats[catIdx],
                             desc, pris[priIdx], orgName, deptName);

        clearScreen();
        drawHeader("SUBMIT NEW COMPLAINT");
        printAt(22, 5, "  Complaint submitted successfully!", GREEN);
        waitKey();
        currentScreen = USER_DASHBOARD;
    }

    // ── ADMIN DASHBOARD ────────────────────────────────────────
    void drawAdminDashboard() {
        clearScreen();
        drawHeader("ADMIN DASHBOARD", "System Administrator");
        drawSidebar(true);

        int total    = manager.totalComplaints();
        int pending  = manager.countByStatus("Pending");
        int inProg   = manager.countByStatus("In Progress");
        int resolved = manager.countByStatus("Resolved");
        int escalated= manager.countEscalated();

        drawStatCard(21,  3, to_string(total),    "Total",       CYAN);
        drawStatCard(45,  3, to_string(pending),  "Pending",     RED);
        drawStatCard(69,  3, to_string(inProg),   "In Progress", YELLOW);
        drawStatCard(93,  3, to_string(resolved), "Resolved",    GREEN);

        if (escalated > 0)
            printAt(22, 8, "  WARNING: " + to_string(escalated) +
                           " complaint(s) ESCALATED (pending > 7 days)!", RED);

        printAt(22, 9, "Recent Complaints:", YELLOW);
        auto all = manager.getAllComplaints();
        int maxRows = 10;
        drawComplaintTable(all, 10, true, tableScrollRow, maxRows);

        printAt(22, 22, "[1] All  [2] Analytics  [3] Audit  "
                        "[V] View/Update  [U/D] Scroll  [L] Logout  [Q] Quit", DARK_GRAY);

        handleAdminNav(all, 10, maxRows);
    }

    // ── ADMIN ALL COMPLAINTS ───────────────────────────────────
    void drawAdminAll() {
        clearScreen();
        drawHeader("ALL COMPLAINTS", "Admin Panel");
        drawSidebar(true);

        // Search/filter inline
        printAt(22, 3, "Search keyword (Enter to skip): ", LIGHT_GRAY);
        drawBar(55, 3, 50, 1, WHITE, DARK_BLUE);
        string keyword = readInput(55, 3, 48);

        vector<string> statOpts = {"All","Pending","In Progress","Resolved"};
        vector<string> catOpts  = {"All","IT","HR","Finance","Facilities","Academic","Legal","Other"};
        vector<string> priOpts  = {"All","Low","Medium","High"};

        printAt(22, 4, "Status filter (1=All 2=Pending 3=InProgress 4=Resolved): ", LIGHT_GRAY);
        char sc = readChoice("1234");
        string statusF = statOpts[sc - '1'];

        printAt(22, 5, "Priority filter (1=All 2=Low 3=Med 4=High): ", LIGHT_GRAY);
        char pc = readChoice("1234");
        string priF = priOpts[pc - '1'];

        auto list = manager.searchComplaints(keyword, statusF, "All", priF);
        printAt(22, 6, "Results: " + to_string(list.size()), CYAN);

        int maxRows = 12;
        drawComplaintTable(list, 7, true, tableScrollRow, maxRows);

        printAt(22, 21, "[V] View/Update  [U] Scroll Up  [D] Scroll Down  "
                        "[1-3] Nav  [L] Logout  [Q] Quit", DARK_GRAY);

        handleAdminNav(list, 7, maxRows);
    }

    // ── COMPLAINT DETAIL ───────────────────────────────────────
    void drawComplaintDetail(bool isAdmin) {
        clearScreen();
        drawHeader("COMPLAINT DETAIL");
        if (!isAdmin) drawSidebar(false);

        Complaint* c = manager.findById(selectedId);
        if (!c) {
            printAt(22, 5, "Complaint not found!", RED);
            waitKey();
            currentScreen = isAdmin ? ADMIN_ALL : MY_COMPLAINTS;
            return;
        }

        int lx = isAdmin ? 2 : 22;
        drawBox(lx, 3, 116, 32, CYAN);

        printAt(lx+2,  4, "Complaint #" + to_string(c->id) + (c->escalated ? "  [ESCALATED]" : ""), YELLOW);
        drawHLine(lx+1, 5, 114, '-', DARK_GRAY);

        auto row = [&](int y, const string& label, const string& val, int vc=WHITE) {
            printAt(lx+2, y, padRight(label+":", 18), LIGHT_GRAY);
            printAt(lx+20, y, val, vc);
        };

        row(6,  "Title",        c->title);
        row(7,  "Organization", c->orgName + " / " + c->orgDept);
        row(8,  "Category",     c->category);
        row(9,  "Submitted by", c->submittedBy, CYAN);
        row(10, "Date",         c->date);
        row(11, "Priority",     c->priority,
            c->priority=="High" ? RED : c->priority=="Medium" ? YELLOW : GREEN);
        row(12, "Status",       c->status,
            c->status=="Resolved" ? GREEN : c->status=="In Progress" ? YELLOW : RED);

        printAt(lx+2, 13, padRight("Description:", 18), LIGHT_GRAY);
        // Word-wrap description
        string desc = c->description;
        int drow = 14;
        while (!desc.empty() && drow < 22) {
            int len = min((int)desc.size(), 90);
            printAt(lx+2, drow++, desc.substr(0, len), WHITE);
            desc = (int)desc.size() > 90 ? desc.substr(90) : "";
        }

        drawHLine(lx+1, 22, 114, '-', DARK_GRAY);
        printAt(lx+2, 23, "Status History:", LIGHT_GRAY);
        auto history = manager.getAuditFor(c->id);
        if (history.empty()) {
            printAt(lx+2, 24, "No changes yet.", DARK_GRAY);
        } else {
            int hr = 24;
            for (AuditEntry& e : history) {
                if (hr > 30) break;
                printAt(lx+2, hr++, e.timestamp + "  " + e.changedBy +
                        ":  " + e.oldStatus + " -> " + e.newStatus, LIGHT_GRAY);
            }
        }

        if (isAdmin) {
            drawHLine(lx+1, 31, 114, '-', DARK_GRAY);
            printAt(lx+2, 32, "[1] Set PENDING  [2] Set IN PROGRESS  "
                               "[3] Set RESOLVED  [B] Back  [L] Logout  [Q] Quit", YELLOW);
            char ch = readChoice("123blq");
            if      (ch=='1') manager.updateStatus(c->id, "Pending",     loggedInUser->getUsername());
            else if (ch=='2') manager.updateStatus(c->id, "In Progress", loggedInUser->getUsername());
            else if (ch=='3') manager.updateStatus(c->id, "Resolved",    loggedInUser->getUsername());
            else if (ch=='b') { currentScreen = ADMIN_ALL; return; }
            else if (ch=='l') { logout(); return; }
            else if (ch=='q') { exit(0); }
            // Redraw after update
            drawComplaintDetail(isAdmin);
        } else {
            printAt(lx+2, 32, "[B] Back  [L] Logout  [Q] Quit", YELLOW);
            char ch = readChoice("blq");
            if      (ch=='b') currentScreen = MY_COMPLAINTS;
            else if (ch=='l') logout();
            else if (ch=='q') exit(0);
        }
    }

    // ── ANALYTICS ─────────────────────────────────────────────
    void drawAnalytics() {
        clearScreen();
        drawHeader("ANALYTICS", "Admin Panel");
        drawSidebar(true);

        drawStatCard(21,  3, to_string(manager.totalComplaints()),           "Total",       CYAN);
        drawStatCard(45,  3, to_string(manager.countByStatus("Pending")),    "Pending",     RED);
        drawStatCard(69,  3, to_string(manager.countByStatus("In Progress")), "In Progress", YELLOW);
        drawStatCard(93,  3, to_string(manager.countByStatus("Resolved")),   "Resolved",    GREEN);

        // Bar chart — by category
        printAt(22, 8, "Complaints by Category:", YELLOW);
        auto byCat = manager.countByCategory();
        drawBarChart(byCat, 22, 9, 46);

        // Bar chart — by org
        printAt(70, 8, "Complaints by Organization:", YELLOW);
        auto byOrg = manager.countByOrg();
        drawBarChart(byOrg, 70, 9, 46);

        printAt(22, 28, "[1] All  [2] Analytics  [3] Audit  [L] Logout  [Q] Quit", DARK_GRAY);
        char ch = readChoice("123lq");
        if      (ch=='1') { tableScrollRow=0; currentScreen=ADMIN_ALL; }
        else if (ch=='2') currentScreen = ADMIN_ANALYTICS;
        else if (ch=='3') currentScreen = ADMIN_AUDIT;
        else if (ch=='l') logout();
        else if (ch=='q') exit(0);
    }

    // Console bar chart
    void drawBarChart(const map<string,int>& data, int x, int y, int width) {
        if (data.empty()) {
            printAt(x, y, "No data yet.", DARK_GRAY);
            return;
        }
        int maxVal = 1;
        for (const auto& kv : data) maxVal = max(maxVal, kv.second);

        int barMaxH = 10;   // max bar height in rows
        int col = x;
        int barW = max(2, (width / (int)data.size()) - 1);

        // Draw bars from top down
        int colors[] = { CYAN, GREEN, YELLOW, RED, MAGENTA, BLUE };
        int ci = 0;
        for (const auto& kv : data) {
            int barH = (int)((kv.second / (float)maxVal) * barMaxH);
            if (barH == 0 && kv.second > 0) barH = 1;

            // Value label above bar
            printAt(col, y + barMaxH - barH - 1, to_string(kv.second), colors[ci%6]);

            // The bar itself
            for (int r = 0; r < barH; r++) {
                setColor(BLACK, colors[ci%6]);
                gotoxy(col, y + barMaxH - barH + r);
                for (int b = 0; b < barW; b++) cout << ' ';
            }
            // Label below
            string lbl = truncate(kv.first, barW);
            printAt(col, y + barMaxH + 1, lbl, colors[ci%6]);
            col += barW + 1;
            ci++;
        }
        setColor(WHITE);
    }

    // ── AUDIT LOG ──────────────────────────────────────────────
    void drawAuditLog() {
        clearScreen();
        drawHeader("AUDIT LOG", "Complete Status Change History");
        drawSidebar(true);

        auto allAudit = manager.getAllAudit();
        printAt(22, 3, "Total entries: " + to_string(allAudit.size()), CYAN);

        // Table header
        drawBar(21, 4, 99, 1, WHITE, DARK_BLUE);
        printAt(22, 4, padRight("Comp#", 8),      WHITE, DARK_BLUE);
        printAt(31, 4, padRight("Changed By", 16), WHITE, DARK_BLUE);
        printAt(48, 4, padRight("Old Status", 14), WHITE, DARK_BLUE);
        printAt(63, 4, padRight("New Status", 14), WHITE, DARK_BLUE);
        printAt(78, 4, "Timestamp",                WHITE, DARK_BLUE);

        int maxRows = 14;

        // Guard against empty list — computing start on an empty vector gives -1
        if (allAudit.empty()) {
            printAt(22, 5, "No audit entries yet.", DARK_GRAY);
        } else {
            int start = max(0, (int)allAudit.size() - maxRows + tableScrollRow);
            start = min(start, (int)allAudit.size() - 1);

            for (int i = 0; i < maxRows && (start+i) < (int)allAudit.size(); i++) {
                const AuditEntry& e = allAudit[start+i];
                int row = 5 + i;
                int bg  = (i%2==0) ? BLACK : DARK_GRAY;
                drawBar(21, row, 99, 1, WHITE, bg);
                printAt(22, row, padRight("#"+to_string(e.complaintId), 8), CYAN,       bg);
                printAt(31, row, padRight(e.changedBy, 16),                 WHITE,      bg);
                printAt(48, row, padRight(e.oldStatus, 14),                 DARK_GRAY,  bg);
                printAt(63, row, padRight(e.newStatus, 14),
                        e.newStatus=="Resolved" ? GREEN : e.newStatus=="In Progress" ? YELLOW : RED, bg);
                printAt(78, row, e.timestamp,                               LIGHT_GRAY, bg);
            }
        }

        printAt(22, 21, "[1] All  [2] Analytics  [3] Audit  "
                        "[U] Up  [D] Down  [L] Logout  [Q] Quit", DARK_GRAY);
        char ch = readChoice("123udlq");
        if      (ch=='1') { tableScrollRow=0; currentScreen=ADMIN_ALL; }
        else if (ch=='2') currentScreen = ADMIN_ANALYTICS;
        else if (ch=='3') currentScreen = ADMIN_AUDIT;
        else if (ch=='u') tableScrollRow = max(0, tableScrollRow-1);
        else if (ch=='d') tableScrollRow = min((int)allAudit.size()-1, tableScrollRow+1);
        else if (ch=='l') logout();
        else if (ch=='q') exit(0);
    }

    // ── NAV HANDLERS ───────────────────────────────────────────

    // User nav: reads a key and routes
    void handleUserNav(vector<Complaint>& list, int tableY, int maxRows) {
        char ch = readChoice("123vudlq");
        if      (ch=='1') { tableScrollRow=0; currentScreen=USER_DASHBOARD; }
        else if (ch=='2') currentScreen = SUBMIT_COMPLAINT;
        else if (ch=='3') { tableScrollRow=0; currentScreen=MY_COMPLAINTS; }
        else if (ch=='v') { promptSelectComplaint(list); }
        else if (ch=='u') tableScrollRow = max(0, tableScrollRow-1);
        else if (ch=='d') tableScrollRow = min(max(0,(int)list.size()-maxRows), tableScrollRow+1);
        else if (ch=='l') logout();
        else if (ch=='q') exit(0);
    }

    // Admin nav: reads a key and routes
    void handleAdminNav(vector<Complaint>& list, int tableY, int maxRows) {
        char ch = readChoice("123vudlq");
        if      (ch=='1') { tableScrollRow=0; currentScreen=ADMIN_ALL; }
        else if (ch=='2') currentScreen = ADMIN_ANALYTICS;
        else if (ch=='3') currentScreen = ADMIN_AUDIT;
        else if (ch=='v') { promptSelectComplaint(list); }
        else if (ch=='u') tableScrollRow = max(0, tableScrollRow-1);
        else if (ch=='d') tableScrollRow = min(max(0,(int)list.size()-maxRows), tableScrollRow+1);
        else if (ch=='l') logout();
        else if (ch=='q') exit(0);
    }

    // Ask user to type a complaint ID to view
    void promptSelectComplaint(const vector<Complaint>& list) {
        if (list.empty()) { printAt(22, 25, "No complaints to view.", RED); Sleep(800); return; }
        printAt(22, 25, "Enter complaint # to view: ", YELLOW);
        string inp = readInput(50, 25, 6);
        if (inp.empty()) return;
        try {
            int id = stoi(inp);
            // Verify it exists in this list
            for (const Complaint& c : list) {
                if (c.id == id) {
                    selectedId    = id;
                    currentScreen = ADMIN_DETAIL;
                    return;
                }
            }
            printAt(22, 26, "ID not found in current list.", RED);
            Sleep(800);
        } catch (...) {}
    }

    // Clears the logged-in user and returns to the login screen
    void logout() {
        loggedInUser   = nullptr;
        currentScreen  = LOGIN;
        tableScrollRow = 0;
    }

public:
    // Sets up the console window: gets the handle, resizes to 120x50,
    // hides the blinking cursor, and switches to the CP437 character set
    // (needed for box-drawing characters like ─ │ ┌ ┐ └ ┘).
    App() : currentScreen(LOGIN), loggedInUser(nullptr),
            selectedId(-1), tableScrollRow(0) {
        hCon = GetStdHandle(STD_OUTPUT_HANDLE);
        // Set console window size to 120x50
        system("mode con: cols=120 lines=50");
        showCursor(false);
        // Enable UTF-8 for box-drawing chars
        SetConsoleOutputCP(437);
    }

    // Main application loop — runs until the user presses Q or closes the window.
    // Each screen draw function is responsible for reading input and updating
    // currentScreen before returning so the loop picks up the next screen.
    void run() {
        while (true) {
            switch (currentScreen) {
                case LOGIN:            drawLogin();           break;
                case REGISTER:         drawRegister();        break;
                case USER_DASHBOARD:   drawUserDashboard();   break;
                case SUBMIT_COMPLAINT: drawSubmitComplaint(); break;
                case MY_COMPLAINTS:    drawMyComplaints();    break;
                case ADMIN_DASHBOARD:  drawAdminDashboard();  break;
                case ADMIN_ALL:        drawAdminAll();        break;
                case ADMIN_DETAIL:     drawComplaintDetail(loggedInUser && loggedInUser->getRole()=="Admin"); break;
                case ADMIN_ANALYTICS:  drawAnalytics();       break;
                case ADMIN_AUDIT:      drawAuditLog();        break;
            }
        }
    }
};

// ================================================================
//  MAIN
// ================================================================
int main() {
    App app;
    app.run();
    return 0;
}