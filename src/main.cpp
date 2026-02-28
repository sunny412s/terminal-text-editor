#include <ncurses.h>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>

std::vector<std::string> buffer(1, "");
int cursorX = 0;
int cursorY = 0;
int rowOffset = 0;

std::string currentFile = "";
std::string statusMessage = "Ctrl+S Save | Ctrl+Q Quit | Ctrl+F Search";

std::string searchQuery = "";
int searchY = -1;
int searchX = -1;

/* -------------------------------------------------- */
/* ------------------ SAFETY ------------------------ */
/* -------------------------------------------------- */

void fixCursor()
{
    if (cursorY < 0) cursorY = 0;
    if (cursorY >= (int)buffer.size()) cursorY = buffer.size() - 1;
    if (cursorY < 0) return;

    if (cursorX < 0) cursorX = 0;
    if (cursorX > (int)buffer[cursorY].size())
        cursorX = buffer[cursorY].size();
}

/* -------------------------------------------------- */
/* ------------------ FILE I/O ---------------------- */
/* -------------------------------------------------- */

void loadFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file) return;

    buffer.clear();
    std::string line;
    while (std::getline(file, line))
        buffer.push_back(line);

    if (buffer.empty())
        buffer.push_back("");

    currentFile = filename;
    cursorX = cursorY = rowOffset = 0;
}

void saveFile()
{
    if (currentFile.empty())
    {
        statusMessage = "No filename";
        return;
    }

    std::ofstream file(currentFile);
    for (size_t i = 0; i < buffer.size(); i++)
    {
        file << buffer[i];
        if (i != buffer.size() - 1)
            file << "\n";
    }

    statusMessage = "Saved: " + currentFile;
}

/* -------------------------------------------------- */
/* ------------------ SEARCH ------------------------ */
/* -------------------------------------------------- */

void findNext()
{
    if (searchQuery.empty()) return;

    int startLine = cursorY;
    int startCol = cursorX + 1;

    for (size_t i = startLine; i < buffer.size(); i++)
    {
        size_t pos = buffer[i].find(searchQuery,
                     (i == (size_t)startLine ? startCol : 0));

        if (pos != std::string::npos)
        {
            cursorY = i;
            cursorX = pos;
            searchY = i;
            searchX = pos;
            return;
        }
    }

    statusMessage = "No more matches";
}

void startSearch()
{
    echo();
    curs_set(1);

    int h, w;
    getmaxyx(stdscr, h, w);

    move(h - 1, 0);
    clrtoeol();
    mvprintw(h - 1, 0, "Search: ");

    char query[256];
    getnstr(query, 255);

    noecho();

    searchQuery = query;
    findNext();
}

/* -------------------------------------------------- */
/* ------------------ DRAW -------------------------- */
/* -------------------------------------------------- */

void draw()
{
    fixCursor();

    int h, w;
    getmaxyx(stdscr, h, w);

    clear();

    if (cursorY < rowOffset)
        rowOffset = cursorY;
    if (cursorY >= rowOffset + h - 1)
        rowOffset = cursorY - h + 2;

    for (int i = 0; i < h - 1; i++)
    {
        int lineIndex = rowOffset + i;
        if (lineIndex >= (int)buffer.size())
            break;

        mvprintw(i, 0, "%4d | ", lineIndex + 1);

        std::string line = buffer[lineIndex];

        if (lineIndex == searchY && searchX >= 0)
        {
            std::string before = line.substr(0, searchX);
            std::string match = line.substr(searchX, searchQuery.size());
            std::string after = line.substr(searchX + searchQuery.size());

            printw("%s", before.c_str());
            attron(A_REVERSE);
            printw("%s", match.c_str());
            attroff(A_REVERSE);
            printw("%s", after.c_str());
        }
        else
        {
            printw("%s", line.c_str());
        }
    }

    attron(A_REVERSE);
    mvprintw(h - 1, 0, "%s", statusMessage.c_str());
    clrtoeol();
    attroff(A_REVERSE);

    move(cursorY - rowOffset, cursorX + 6);
    refresh();
}

/* -------------------------------------------------- */
/* ------------------ EDITING ----------------------- */
/* -------------------------------------------------- */

void insertChar(int ch)
{
    fixCursor();
    buffer[cursorY].insert(cursorX, 1, (char)ch);
    cursorX++;
}

void backspace()
{
    if (cursorX > 0)
    {
        buffer[cursorY].erase(cursorX - 1, 1);
        cursorX--;
    }
    else if (cursorY > 0)
    {
        cursorX = buffer[cursorY - 1].size();
        buffer[cursorY - 1] += buffer[cursorY];
        buffer.erase(buffer.begin() + cursorY);
        cursorY--;
    }
}

void newLine()
{
    std::string rest = buffer[cursorY].substr(cursorX);
    buffer[cursorY].erase(cursorX);
    buffer.insert(buffer.begin() + cursorY + 1, rest);
    cursorY++;
    cursorX = 0;
}

/* -------------------------------------------------- */
/* ------------------ MAIN -------------------------- */
/* -------------------------------------------------- */

int main(int argc, char* argv[])
{
    if (argc > 1)
        loadFile(argv[1]);

    initscr();
    raw();
    keypad(stdscr, TRUE);
    noecho();

    int ch;

    while (true)
    {
        draw();
        ch = getch();

        if (ch == 17) break;              // Ctrl+Q
        else if (ch == 19) saveFile();    // Ctrl+S
        else if (ch == 6) startSearch();  // Ctrl+F
        else if (ch == KEY_F(3) || ch == 14) findNext(); // F3 or Ctrl+N

        else if (ch == KEY_LEFT) cursorX--;
        else if (ch == KEY_RIGHT) cursorX++;
        else if (ch == KEY_UP) cursorY--;
        else if (ch == KEY_DOWN) cursorY++;

        else if (ch == KEY_BACKSPACE || ch == 127) backspace();
        else if (ch == '\n') newLine();

        else if (ch >= 32 && ch <= 126)
            insertChar(ch);
    }

    endwin();
    return 0;
}
