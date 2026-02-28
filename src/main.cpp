#include <ncurses.h>
#include <vector>
#include <string>
#include <fstream>
#include <algorithm>
#include <deque>

// ---------------- EDITOR STATE ----------------
std::vector<std::string> buffer = {""};

int cursorX   = 0;
int cursorY   = 0;
int scrollX   = 0;
int scrollY   = 0;

std::string filename      = "untitled.txt";
std::string statusMessage = "CTRL+S Save | F2 Search | F3 Next | ESC Exit";

std::string searchQuery  = "";
int  searchStartLine     = -1;   // -1 = no active search
bool searchWrapped       = false;

bool isDirty = false;            // unsaved changes flag

// ---------------- UNDO ----------------
struct Snapshot {
    std::vector<std::string> buf;
    int cx, cy;
};
std::deque<Snapshot> undoStack;
std::deque<Snapshot> redoStack;
const int MAX_UNDO = 200;

void pushUndo()
{
    undoStack.push_back({ buffer, cursorX, cursorY });
    if ((int)undoStack.size() > MAX_UNDO)
        undoStack.pop_front();
    redoStack.clear();
}

void doUndo()
{
    if (undoStack.empty()) { statusMessage = "Nothing to undo"; return; }
    redoStack.push_back({ buffer, cursorX, cursorY });
    auto& snap = undoStack.back();
    buffer  = snap.buf;
    cursorX = snap.cx;
    cursorY = snap.cy;
    undoStack.pop_back();
    isDirty = true;
    statusMessage = "Undo";
}

void doRedo()
{
    if (redoStack.empty()) { statusMessage = "Nothing to redo"; return; }
    undoStack.push_back({ buffer, cursorX, cursorY });
    auto& snap = redoStack.back();
    buffer  = snap.buf;
    cursorX = snap.cx;
    cursorY = snap.cy;
    redoStack.pop_back();
    isDirty = true;
    statusMessage = "Redo";
}

// ---------------- SAFE HELPERS ----------------
std::string safeSubstr(const std::string& s, size_t pos, size_t len)
{
    if (pos >= s.size()) return "";
    return s.substr(pos, len);
}

void clampCursor()
{
    if (cursorY >= (int)buffer.size()) cursorY = (int)buffer.size() - 1;
    if (cursorY < 0) cursorY = 0;
    if (cursorX > (int)buffer[cursorY].size()) cursorX = (int)buffer[cursorY].size();
    if (cursorX < 0) cursorX = 0;
}

void adjustScroll(int rows, int cols)
{
    const int lineNumW = 5;
    const int textCols = cols - lineNumW;

    if (cursorY < scrollY) scrollY = cursorY;
    if (cursorY >= scrollY + rows - 1) scrollY = cursorY - rows + 2;

    if (cursorX < scrollX) scrollX = cursorX;
    if (cursorX >= scrollX + textCols) scrollX = cursorX - textCols + 1;

    if (scrollY < 0) scrollY = 0;
    if (scrollX < 0) scrollX = 0;
}

// ---------------- FILE ----------------
void saveFile()
{
    std::ofstream out(filename);
    if (!out) { statusMessage = "ERROR: Cannot write " + filename; return; }

    for (const std::string& line : buffer)
        out << line << '\n';

    isDirty       = false;
    statusMessage = "Saved: " + filename;
}

void loadFile(const std::string& file)
{
    std::ifstream in(file);
    if (!in) return;

    buffer.clear();
    std::string line;

    while (std::getline(in, line))
        buffer.push_back(line);

    if (buffer.empty()) buffer.push_back("");

    filename = file;
    isDirty  = false;
}

// ---------------- SEARCH ----------------
// Returns true and moves cursor if a match is found after (startLine, startCol).
// Wraps around once; sets searchWrapped accordingly.
bool searchFrom(int fromLine, int fromCol, bool& wrapped)
{
    int n = (int)buffer.size();
    wrapped = false;

    // Search from current position to end
    for (int i = fromLine; i < n; i++)
    {
        size_t startPos = (i == fromLine) ? (size_t)fromCol : 0;
        size_t pos = buffer[i].find(searchQuery, startPos);
        if (pos != std::string::npos)
        {
            cursorY = i;
            cursorX = (int)pos;
            return true;
        }
    }

    // Wrap around from beginning
    for (int i = 0; i < fromLine; i++)
    {
        size_t pos = buffer[i].find(searchQuery);
        if (pos != std::string::npos)
        {
            cursorY = i;
            cursorX = (int)pos;
            wrapped = true;
            return true;
        }
    }

    return false;
}

void startSearch()
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    echo();
    curs_set(1);

    move(rows - 1, 0);
    clrtoeol();
    mvprintw(rows - 1, 0, "Search: ");
    refresh();

    char query[256] = {};
    getnstr(query, 255);

    noecho();

    searchQuery     = query;
    searchStartLine = -1;

    if (searchQuery.empty()) { statusMessage = "Search cleared"; return; }

    bool wrapped = false;
    if (searchFrom(cursorY, cursorX + 1, wrapped))
        statusMessage = wrapped ? "Search wrapped" : "Match found (F3 = next)";
    else
        statusMessage = "Not found: " + searchQuery;
}

void findNext()
{
    if (searchQuery.empty()) { statusMessage = "No active search (F2)"; return; }

    bool wrapped = false;
    if (searchFrom(cursorY, cursorX + 1, wrapped))
        statusMessage = wrapped ? "Search wrapped" : "Match found";
    else
        statusMessage = "No more matches";
}

// ---------------- DRAW ----------------
void draw()
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    const int lineNumW = 5;
    clear();

    for (int i = 0; i < rows - 1; i++)
    {
        int fileRow = scrollY + i;
        if (fileRow >= (int)buffer.size()) break;

        // Line number
        attron(A_DIM);
        mvprintw(i, 0, "%4d ", fileRow + 1);
        attroff(A_DIM);

        std::string visible = safeSubstr(buffer[fileRow], scrollX, cols - lineNumW);

        // Highlight search matches in this line
        if (!searchQuery.empty())
        {
            std::string& line = buffer[fileRow];
            int drawCol = lineNumW;
            int visStart = scrollX;
            size_t pos = 0;

            while ((pos = line.find(searchQuery, pos)) != std::string::npos)
            {
                int matchStart = (int)pos - visStart;
                int matchEnd   = matchStart + (int)searchQuery.size();

                // Only highlight if visible
                if (matchEnd > 0 && matchStart < cols - lineNumW)
                {
                    int hlStart = std::max(0, matchStart);
                    int hlLen   = std::min(matchEnd, cols - lineNumW) - hlStart;

                    if (hlLen > 0)
                    {
                        attron(A_STANDOUT);
                        mvprintw(i, drawCol + hlStart,
                                 "%s", visible.substr(hlStart, hlLen).c_str());
                        attroff(A_STANDOUT);
                    }
                }
                pos += searchQuery.size();
            }

            // Print the rest without highlight (non-match parts)
            // We re-print full line first then overlay highlights
            mvprintw(i, lineNumW, "%s", visible.c_str());
            pos = 0;
            while ((pos = line.find(searchQuery, pos)) != std::string::npos)
            {
                int matchStart = (int)pos - visStart;
                int matchEnd   = matchStart + (int)searchQuery.size();
                if (matchEnd > 0 && matchStart < cols - lineNumW)
                {
                    int hlStart = std::max(0, matchStart);
                    int hlLen   = std::min(matchEnd, cols - lineNumW) - hlStart;
                    if (hlLen > 0)
                    {
                        attron(A_STANDOUT);
                        mvprintw(i, lineNumW + hlStart,
                                 "%s", visible.substr(hlStart, hlLen).c_str());
                        attroff(A_STANDOUT);
                    }
                }
                pos += searchQuery.size();
            }
        }
        else
        {
            mvprintw(i, lineNumW, "%s", visible.c_str());
        }
    }

    // Status bar
    attron(A_REVERSE);
    std::string dirtyFlag = isDirty ? "[+] " : "    ";
    std::string undoInfo  = "U:" + std::to_string(undoStack.size())
                          + " R:" + std::to_string(redoStack.size());
    std::string left  = " " + dirtyFlag + filename + " | " + statusMessage;
    std::string right = undoInfo + " | Ln " + std::to_string(cursorY + 1)
                      + ", Col " + std::to_string(cursorX + 1) + " ";

    // Pad middle
    int pad = cols - (int)left.size() - (int)right.size();
    std::string bar = left + std::string(std::max(0, pad), ' ') + right;
    bar = safeSubstr(bar, 0, cols);

    mvprintw(rows - 1, 0, "%s", bar.c_str());
    clrtoeol();
    attroff(A_REVERSE);

    move(cursorY - scrollY, cursorX - scrollX + lineNumW);
    refresh();
}

// ---------------- EDIT ----------------
void insertChar(int ch)
{
    pushUndo();
    buffer[cursorY].insert(buffer[cursorY].begin() + cursorX, (char)ch);
    cursorX++;
    isDirty = true;
}

void insertNewline()
{
    pushUndo();
    std::string remainder = buffer[cursorY].substr(cursorX);
    buffer[cursorY].erase(cursorX);
    buffer.insert(buffer.begin() + cursorY + 1, remainder);
    cursorY++;
    cursorX = 0;
    isDirty = true;
}

void backspace()
{
    if (cursorX > 0)
    {
        pushUndo();
        buffer[cursorY].erase(cursorX - 1, 1);
        cursorX--;
    }
    else if (cursorY > 0)
    {
        pushUndo();
        cursorX = (int)buffer[cursorY - 1].size();
        buffer[cursorY - 1] += buffer[cursorY];
        buffer.erase(buffer.begin() + cursorY);
        cursorY--;
    }
    isDirty = true;
}

void deleteChar()
{
    if (cursorX < (int)buffer[cursorY].size())
    {
        pushUndo();
        buffer[cursorY].erase(cursorX, 1);
    }
    else if (cursorY + 1 < (int)buffer.size())
    {
        pushUndo();
        buffer[cursorY] += buffer[cursorY + 1];
        buffer.erase(buffer.begin() + cursorY + 1);
    }
    isDirty = true;
}

// ---------------- CONFIRM DIALOG ----------------
bool confirmDiscard()
{
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    attron(A_REVERSE);
    mvprintw(rows - 1, 0, " Unsaved changes. Exit anyway? (y/n) ");
    clrtoeol();
    attroff(A_REVERSE);
    refresh();

    int ch = getch();
    return (ch == 'y' || ch == 'Y');
}

// ---------------- MAIN ----------------
int main(int argc, char* argv[])
{
    if (argc > 1)
        loadFile(argv[1]);

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();
    curs_set(1);

    while (true)
    {
        clampCursor();

        int rows, cols;
        getmaxyx(stdscr, rows, cols);

        adjustScroll(rows, cols);
        draw();

        int ch = getch();

        if (ch == 27) // ESC — exit with dirty check
        {
            if (isDirty && !confirmDiscard()) continue;
            break;
        }

        // --- Navigation ---
        else if (ch == KEY_LEFT)  cursorX--;
        else if (ch == KEY_RIGHT) cursorX++;
        else if (ch == KEY_UP)    cursorY--;
        else if (ch == KEY_DOWN)  cursorY++;

        else if (ch == KEY_HOME)  cursorX = 0;
        else if (ch == KEY_END)   cursorX = (int)buffer[cursorY].size();

        else if (ch == KEY_PPAGE) // Page Up
        {
            cursorY -= rows - 2;
            if (cursorY < 0) cursorY = 0;
        }
        else if (ch == KEY_NPAGE) // Page Down
        {
            cursorY += rows - 2;
            if (cursorY >= (int)buffer.size()) cursorY = (int)buffer.size() - 1;
        }

        // CTRL+Home / CTRL+End
        else if (ch == 553) { cursorY = 0; cursorX = 0; }
        else if (ch == 548) { cursorY = (int)buffer.size() - 1;
                              cursorX = (int)buffer[cursorY].size(); }

        // --- Editing ---
        else if (ch == 10 || ch == KEY_ENTER) insertNewline();
        else if (ch == KEY_BACKSPACE || ch == 127) backspace();
        else if (ch == KEY_DC) deleteChar();   // Delete key

        // --- File ---
        else if (ch == 19) saveFile();         // CTRL+S

        // --- Undo / Redo  CTRL+Z / CTRL+Y ---
        else if (ch == 26) doUndo();
        else if (ch == 25) doRedo();

        // --- Search ---
        else if (ch == KEY_F(2))  startSearch();
        else if (ch == KEY_F(3))  findNext();

        // --- Printable chars ---
        else if (ch >= 32 && ch <= 126)
            insertChar(ch);
    }

    endwin();
    return 0;
}
