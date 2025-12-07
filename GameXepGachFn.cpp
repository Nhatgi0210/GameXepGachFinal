// Tetris - Pure Matrix Architecture (No Grid Coordinates)
// Compile: g++ GameXepGach.cpp -o tetris -lGL -lGLU -lglut

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <map>

#ifdef RGB
#undef RGB
#endif

using namespace std;

// ============================================================================
// CONFIG MODULE
// ============================================================================

namespace Config {
    const int BOARD_W = 10;
    const int BOARD_H = 20;
    const int CELL = 24;
    const int WINDOW_W = CELL * (BOARD_W + 6);
    const int WINDOW_H = CELL * BOARD_H;
    const float DEFAULT_DROP_INTERVAL = 500.0f;
    const float PANEL_X_OFFSET = 20;
    const float PANEL_PREVIEW_SCALE = 12.0f;
    const float COLLISION_EPSILON = 0.4f;
}

// ============================================================================
// MATH MODULE (Vector & Matrix)
// ============================================================================

namespace Math {
    struct Vec2 {
        float x, y;
        Vec2(float _x = 0, float _y = 0) : x(_x), y(_y) {}
        
        Vec2 operator+(const Vec2& other) const {
            return Vec2(x + other.x, y + other.y);
        }
        
        Vec2 operator-(const Vec2& other) const {
            return Vec2(x - other.x, y - other.y);
        }
        
        float distance(const Vec2& other) const {
            float dx = x - other.x;
            float dy = y - other.y;
            return sqrt(dx * dx + dy * dy);
        }
    };

    struct Mat3 {
        float m[3][3];
        
        Mat3() {
            for (int i = 0; i < 3; i++)
                for (int j = 0; j < 3; j++)
                    m[i][j] = 0;
        }
    };

    Mat3 matIdentity() {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j)
                r.m[i][j] = (i == j) ? 1.0f : 0.0f;
        return r;
    }

    Mat3 matMul(const Mat3 &a, const Mat3 &b) {
        Mat3 r;
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                r.m[i][j] = 0.0f;
                for (int k = 0; k < 3; ++k)
                    r.m[i][j] += a.m[i][k] * b.m[k][j];
            }
        return r;
    }

    Mat3 matTranslate(float tx, float ty) {
        Mat3 r = matIdentity();
        r.m[2][0] = tx;
        r.m[2][1] = ty;
        return r;
    }

    Mat3 matRotate(float angleDeg) {
        float rad = angleDeg * 3.14159265f / 180.0f;
        float c = cos(rad);
        float s = sin(rad);
        Mat3 r = matIdentity();
        r.m[0][0] = c;
        r.m[0][1] = -s;
        r.m[1][0] = s;
        r.m[1][1] = c;
        return r;
    }
    Mat3 matScale(float sx, float sy) {
	    Mat3 r = matIdentity();
	    r.m[0][0] = sx;
	    r.m[1][1] = sy;
	    return r;
	}


    Vec2 applyMat3(const Mat3 &m, const Vec2 &v) {
        Vec2 result;
        result.x = v.x * m.m[0][0] + v.y * m.m[1][0] + 1.0f * m.m[2][0];
        result.y = v.x * m.m[0][1] + v.y * m.m[1][1] + 1.0f * m.m[2][1];
        return result;
    }
    
    // Extract translation from transform matrix
    Vec2 getTranslation(const Mat3 &m) {
        return Vec2(m.m[2][0], m.m[2][1]);
    }
}

// ============================================================================
// COLOR MODULE
// ============================================================================

namespace Color {
    enum ColorType {
        CYAN = 1,
        YELLOW = 2,
        PURPLE = 3,
        GREEN = 4,
        RED = 5,
        BLUE = 6,
        ORANGE = 7
    };

    struct RGB {
        float r, g, b;
        RGB(float _r = 0, float _g = 0, float _b = 0) : r(_r), g(_g), b(_b) {}
    };

    inline RGB getColorRGB(int colorIdx) {
        switch (colorIdx) {
        case CYAN:   return RGB(0.0f, 1.0f, 1.0f);
        case YELLOW: return RGB(1.0f, 1.0f, 0.0f);
        case PURPLE: return RGB(0.7f, 0.0f, 0.7f);
        case GREEN:  return RGB(0.0f, 1.0f, 0.0f);
        case RED:    return RGB(1.0f, 0.0f, 0.0f);
        case BLUE:   return RGB(0.0f, 0.0f, 1.0f);
        case ORANGE: return RGB(1.0f, 0.5f, 0.0f);
        default:     return RGB(0.5f, 0.5f, 0.5f);
        }
    }

    inline void setGLColor(int colorIdx) {
        RGB color = getColorRGB(colorIdx);
        glColor3f(color.r, color.g, color.b);
    }
}

// ============================================================================
// TETROMINO MODULE
// ============================================================================

namespace Tetromino {
    using namespace Math;

    struct Block {
        Vec2 localPos;
        int color;
        
        Vec2 getWorldPos(const Mat3& transform) const {
            return applyMat3(transform, localPos);
        }
    };

    class Piece {
    public:
        vector<Block> blocks;
        int colorIndex;
        Mat3 transform;

        Piece() : colorIndex(0) {
            transform = matIdentity();
        }

        vector<Vec2> getWorldPositions() const {
            vector<Vec2> positions;
            for (const auto &block : blocks) {
                positions.push_back(block.getWorldPos(transform));
            }
            return positions;
        }

        void translate(float dx, float dy) {
            Mat3 T = matTranslate(dx, dy);
            transform = matMul(transform, T);  // M_new = M_old   T (row-vector convention)
        }

        void rotate(float angleDeg) {
            Vec2 center = applyMat3(transform, Vec2(0, 0));
            Mat3 T1 = matTranslate(-center.x, -center.y);
            Mat3 R = matRotate(angleDeg);
            Mat3 T2 = matTranslate(center.x, center.y);
            Mat3 combined = matMul(matMul(T1, R), T2);  // Combine: T1   R   T2
            transform = matMul(transform, combined);  // M_new = M_old   combined (row-vector convention)
        }
    };

    class PieceFactory {
    private:
        vector<Piece> templates;

    public:
        PieceFactory() {
            initTemplates();
        }

        void initTemplates() {
            templates.clear();

            // I-piece (centered between blocks for proper rotation)
            {
                Piece piece;
                piece.colorIndex = Color::CYAN;
                piece.blocks = {
                    {Vec2(-2, 0), Color::CYAN},
                    {Vec2(-1, 0), Color::CYAN},
                    {Vec2(0, 0), Color::CYAN},
                    {Vec2(1, 0), Color::CYAN}
                };
                templates.push_back(piece);
            }

            // O-piece (centered at origin)
            {
                Piece piece;
                piece.colorIndex = Color::YELLOW;
                piece.blocks = {
                    {Vec2(0, 0), Color::YELLOW},
                    {Vec2(1, 0), Color::YELLOW},
                    {Vec2(0, 1), Color::YELLOW},
                    {Vec2(1, 1), Color::YELLOW}
                };
                templates.push_back(piece);
            }

            // T-piece
            {
                Piece piece;
                piece.colorIndex = Color::PURPLE;
                piece.blocks = {
                    {Vec2(-1, 0), Color::PURPLE},
                    {Vec2(0, 0), Color::PURPLE},
                    {Vec2(1, 0), Color::PURPLE},
                    {Vec2(0, 1), Color::PURPLE}
                };
                templates.push_back(piece);
            }

            // S-piece
            {
                Piece piece;
                piece.colorIndex = Color::GREEN;
                piece.blocks = {
                    {Vec2(0, 0), Color::GREEN},
                    {Vec2(1, 0), Color::GREEN},
                    {Vec2(-1, 1), Color::GREEN},
                    {Vec2(0, 1), Color::GREEN}
                };
                templates.push_back(piece);
            }

            // Z-piece
            {
                Piece piece;
                piece.colorIndex = Color::RED;
                piece.blocks = {
                    {Vec2(-1, 0), Color::RED},
                    {Vec2(0, 0), Color::RED},
                    {Vec2(0, 1), Color::RED},
                    {Vec2(1, 1), Color::RED}
                };
                templates.push_back(piece);
            }

            // J-piece
            {
                Piece piece;
                piece.colorIndex = Color::BLUE;
                piece.blocks = {
                    {Vec2(-1, 0), Color::BLUE},
                    {Vec2(0, 0), Color::BLUE},
                    {Vec2(1, 0), Color::BLUE},
                    {Vec2(1, 1), Color::BLUE}
                };
                templates.push_back(piece);
            }

            // L-piece
            {
                Piece piece;
                piece.colorIndex = Color::ORANGE;
                piece.blocks = {
                    {Vec2(-1, 0), Color::ORANGE},
                    {Vec2(0, 0), Color::ORANGE},
                    {Vec2(1, 0), Color::ORANGE},
                    {Vec2(-1, 1), Color::ORANGE}
                };
                templates.push_back(piece);
            }
        }

        Piece createRandomPiece() const {
            int idx = rand() % templates.size();
            return templates[idx];
        }
    };
}

// ============================================================================
// BOARD MODULE (Pure Continuous Space)
// ============================================================================

namespace Board {
    using namespace Tetromino;
    using namespace Config;
    using namespace Math;

    struct LockedBlock {
        Vec2 position;
        int color;
        
        LockedBlock(const Vec2& pos, int col) : position(pos), color(col) {}
    };

    class GameBoard {
    private:
        vector<LockedBlock> lockedBlocks;
        int score;
        int highScore;
        int linesClearedTotal;
        bool gameOver;

        bool overlaps(const Vec2& pos1, const Vec2& pos2) const {
            return pos1.distance(pos2) < COLLISION_EPSILON;
        }

    public:
        GameBoard() : score(0), highScore(0), linesClearedTotal(0), gameOver(false) {}

        void reset() {
            lockedBlocks.clear();
            score = 0;
            linesClearedTotal = 0;
            gameOver = false;
        }

        bool canPlace(const Piece &piece) const {
            vector<Vec2> positions = piece.getWorldPositions();
            
            for (const auto &pos : positions) {
                // Check boundaries
                if (pos.x < (-0.01f) || pos.x >= BOARD_W) return false;
                if (pos.y >= BOARD_H) return false;
                
                // Check collision with locked blocks
                if (pos.y >= 0) {
                    for (const auto &locked : lockedBlocks) {
                        if (overlaps(pos, locked.position))
                            return false;
                    }
                }
            }
            return true;
        }

        void lockPiece(const Piece &piece) {
            vector<Vec2> positions = piece.getWorldPositions();
            
            for (const auto &pos : positions) {
                if (pos.y >= 0 && pos.y < BOARD_H && pos.x >= (-0.01f) && pos.x < BOARD_W) {
                    lockedBlocks.push_back(LockedBlock(pos, piece.colorIndex));
                }
            }
        }

        int clearLines() {
            vector<float> yPositions;
            
            // Collect all unique Y positions
            for (const auto &block : lockedBlocks) {
                float y = round(block.position.y);
                bool found = false;
                for (float yPos : yPositions) {
                    if (fabs(yPos - y) < 0.1f) {
                        found = true;
                        break;
                    }
                }
                if (!found) yPositions.push_back(y);
            }
            
            // Check which lines are full
            vector<float> fullLines;
            for (float y : yPositions) {
                int count = 0;
                for (const auto &block : lockedBlocks) {
                    if (fabs(block.position.y - y) < 0.5f)
                        count++;
                }
                if (count >= BOARD_W) {
                    fullLines.push_back(y);
                }
            }
            
            // Remove full lines and shift blocks down
            if (!fullLines.empty()) {
                vector<LockedBlock> newBlocks;
                
                for (auto &block : lockedBlocks) {
                    bool inFullLine = false;
                    for (float y : fullLines) {
                        if (fabs(block.position.y - y) < 0.5f) {
                            inFullLine = true;
                            break;
                        }
                    }
                    
                    if (!inFullLine) {
                        // Count how many full lines are below this block
                        float shift = 0;
                        for (float y : fullLines) {
                            if (y > block.position.y)
                                shift += 1.0f;
                        }
                        
                        LockedBlock newBlock = block;
                        newBlock.position.y += shift;
                        newBlocks.push_back(newBlock);
                    }
                }
                
                lockedBlocks = newBlocks;
                
                int lines = fullLines.size();
                int points = (lines == 1) ? 100 : (lines == 2) ? 300 : (lines == 3) ? 500 : 800;
                score += points;
                if (score > highScore)
                    highScore = score;
                linesClearedTotal += lines;
                
                return lines;
            }
            
            return 0;
        }

        const vector<LockedBlock>& getLockedBlocks() const { return lockedBlocks; }
        int getScore() const { return score; }
        int getHighScore() const { return highScore; }
        int getLinesClearedTotal() const { return linesClearedTotal; }
        bool isGameOver() const { return gameOver; }
        void setGameOver(bool value) { gameOver = value; }
    };
}

// ============================================================================
// TEXT MODULE
// ============================================================================

namespace BlockFont {

    using Math::Vec2;

    // M?i ký t? là list các cell (x,y)
    using Glyph = std::vector<Vec2>;

    std::map<char, Glyph> font;

    void init() {
        using Math::Vec2;

        font['G'] = {
		    {1,6},{2,6},{3,6},{4,6},
		    {0,5},{0,4},{0,3},{0,2},{0,1},
		    {1,0},{2,0},{3,0},{4,0},
		    {4,3},{4,2},{4,1},{3,3},{2,3}
		};
		
		// Ch? A
		font['A'] = {
		    {1,6},{2,6},{3,6},{4,6},
		    {0,5},{5,5},
		    {0,4},{5,4},
		    {0,3},{1,3},{2,3},{3,3},{4,3},{5,3},
		    {0,2},{5,2},
		    {0,1},{5,1},
		    {0,0},{5,0}
		};
		
		// Ch? M
		font['M'] = {
		    {0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},
		    {6,0},{6,1},{6,2},{6,3},{6,4},{6,5},{6,6},
		    {1,4},{2,3},{3,2},{4,3},{5,4}
		};
		
		// Ch? E
		font['E'] = {
		    {0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{6,0},
		    {0,1},{0,2},{0,3},{0,4},{0,5},{0,6},
		    {1,3},{2,3},{3,3},{4,3},{5,3},
		    {1,6},{2,6},{3,6},{4,6},{5,6}
		};
		
		// Ch? O
		font['O'] = {
		    {1,6},{2,6},{3,6},{4,6},{5,6},
		    {0,5},{0,4},{0,3},{0,2},{0,1},
		    {6,5},{6,4},{6,3},{6,2},{6,1},
		    {1,0},{2,0},{3,0},{4,0},{5,0}
		};
		
		// Ch? V
		font['V'] = {
		    {0,6},{0,5},{0,4},{0,3},
		    {6,6},{6,5},{6,4},{6,3},
		    {1,2},{2,1},{3,0},{4,1},{5,2}
		};
		
		// Ch? R
		font['R'] = {
		    {0,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,6},
		    {1,6},{2,6},{3,6},{4,5},{4,4},{4,3},
		    {1,3},{2,3},{3,3},
		    {5,2},{6,1},{6,0}
		};
    }
}

// ============================================================================
// RENDERER MODULE
// ============================================================================

namespace Renderer {
    using namespace Config;
    using namespace Color;
    using namespace Tetromino;
    using namespace Board;
    using namespace Math;

    class GameRenderer {
    private:
        GameBoard *board;
        Piece *currentPiece;
        Piece *nextPiece;

    public:
        GameRenderer(GameBoard *b, Piece *curr, Piece *next) 
            : board(b), currentPiece(curr), nextPiece(next) {}

        void drawBlockAt(const Vec2& worldPos, int colorIdx) const {
            const float pad = 1.5f;
            float x = worldPos.x * CELL;
            float y = (BOARD_H - worldPos.y) * CELL;

            setGLColor(colorIdx);
            glBegin(GL_QUADS);
            glVertex2f(x + pad, y - CELL + pad);
            glVertex2f(x + CELL - pad, y - CELL + pad);
            glVertex2f(x + CELL - pad, y - pad);
            glVertex2f(x + pad, y - pad);
            glEnd();

            glColor3f(0.1f, 0.1f, 0.1f);
            glBegin(GL_LINE_LOOP);
            glVertex2f(x + pad, y - CELL + pad);
            glVertex2f(x + CELL - pad, y - CELL + pad);
            glVertex2f(x + CELL - pad, y - pad);
            glVertex2f(x + pad, y - pad);
            glEnd();
        }

        void drawBoard() const {
            // Background
            glColor3f(0.05f, 0.05f, 0.05f);
            glBegin(GL_QUADS);
            glVertex2f(0, 0);
            glVertex2f(BOARD_W * CELL, 0);
            glVertex2f(BOARD_W * CELL, BOARD_H * CELL);
            glVertex2f(0, BOARD_H * CELL);
            glEnd();

            // Grid lines
            glColor3f(0.15f, 0.15f, 0.15f);
            for (int i = 0; i <= BOARD_W; i++) {
                glBegin(GL_LINES);
                glVertex2f(i * CELL, 0);
                glVertex2f(i * CELL, BOARD_H * CELL);
                glEnd();
            }
            for (int i = 0; i <= BOARD_H; i++) {
                glBegin(GL_LINES);
                glVertex2f(0, i * CELL);
                glVertex2f(BOARD_W * CELL, i * CELL);
                glEnd();
            }

            // Locked blocks
            for (const auto &block : board->getLockedBlocks()) {
                if (block.position.y >= 0 && block.position.y < BOARD_H)
                    drawBlockAt(block.position, block.color);
            }

            // Current piece
            vector<Vec2> positions = currentPiece->getWorldPositions();
            for (const auto &pos : positions) {
                if (pos.y >= 0 && pos.y < BOARD_H && pos.x >= (-0.01f)&& pos.x < BOARD_W)
                    drawBlockAt(pos, currentPiece->colorIndex);
            }
        }

        void drawText(float x, float y, const string &s) const {
            glRasterPos2f(x, y);
            for (char ch : s)
                glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, ch);
        }

		static void drawBlockGlyph(char c, const Math::Mat3 &M, float cellSize) {
		    using namespace Math;
		
		    auto it = BlockFont::font.find(c);
		    if (it == BlockFont::font.end()) return;
		
		    for (auto cell : it->second) {
		        // T?a d? g?c cell
		        Vec2 p(cell.x * cellSize, cell.y * cellSize);
		
		        // Áp ma tr?n vào t?ng cell
		        Vec2 t = applyMat3(M, p);
		
		        // V? block (gi?ng block tetromino)
		        glBegin(GL_QUADS);
		        glVertex2f(t.x,           t.y);
		        glVertex2f(t.x+cellSize,  t.y);
		        glVertex2f(t.x+cellSize,  t.y+cellSize);
		        glVertex2f(t.x,           t.y+cellSize);
		        glEnd();
		    }
		}
	    static void drawTextGAMEOVER(float x, float y, float animScale) {
		    using namespace Math;
		
		    string line1 = "GAME";
		    string line2 = "OVER";
		    float cell = 10.0f;
		    float spacing = 70.0f;
		    float lineSpacing = 80.0f; // Kho?ng cách gi?a hai hàng
		
		    // V? dòng 1
		    float cx = x;
		    for (char c : line1) {
		        Mat3 T = matTranslate(cx, y);
		        Mat3 S = matScale(animScale, animScale);
		        Mat3 M = matMul(T, S);
		
		        drawBlockGlyph(c, M, cell);
		        cx += spacing;
		    }
		
		    // V? dòng 2
		    cx = x;
		    float y2 = y - lineSpacing; // H? dòng 2 xu?ng
		    for (char c : line2) {
		        Mat3 T = matTranslate(cx, y2);
		        Mat3 S = matScale(animScale, animScale);
		        Mat3 M = matMul(T, S);
		
		        drawBlockGlyph(c, M, cell);
		        cx += spacing;
		    }
		}


        void drawSidePanel() const {
            float panelX = BOARD_W * CELL + PANEL_X_OFFSET;
            float yPos = WINDOW_H - 20;

            // Next piece section
            glColor3f(1, 1, 1);
            drawText(panelX, yPos, "Next:");
            yPos -= 30;

            for (const auto &block : nextPiece->blocks) {
                Vec2 localPos = block.localPos;
                float x = panelX + 20 + (localPos.x + 1.5f) * PANEL_PREVIEW_SCALE;
                float y = yPos - (localPos.y + 1.5f) * PANEL_PREVIEW_SCALE;

                setGLColor(nextPiece->colorIndex);
                glBegin(GL_QUADS);
                glVertex2f(x, y);
                glVertex2f(x + PANEL_PREVIEW_SCALE - 2, y);
                glVertex2f(x + PANEL_PREVIEW_SCALE - 2, y + PANEL_PREVIEW_SCALE - 2);
                glVertex2f(x, y + PANEL_PREVIEW_SCALE - 2);
                glEnd();
            }

            // Score section
            yPos -= 100;
            glColor3f(1, 1, 1);
            drawText(panelX, yPos, "Score: " + to_string(board->getScore()));
            drawText(panelX, yPos - 20, "High: " + to_string(board->getHighScore()));
            drawText(panelX, yPos - 40, "Lines: " + to_string(board->getLinesClearedTotal()));

            // Controls section
            yPos -= 80;
            glColor3f(0.7f, 0.7f, 0.7f);
            drawText(panelX, yPos, "Controls:");
            drawText(panelX, yPos - 20, "Arrows: Move");
            drawText(panelX, yPos - 40, "Up: Rotate");
            drawText(panelX, yPos - 60, "Space: Drop");
            drawText(panelX, yPos - 80, "R: Restart");

            // Game over
            if (board->isGameOver()) {
               	float t = glutGet(GLUT_ELAPSED_TIME) / 1000.0f;
			    float scale = 1.0f + 0.3f * sinf(t * 4.0f);
			
			    glColor3f(1, 0, 0);
			    drawTextGAMEOVER(40, Config::WINDOW_H / 2, scale);
            }
        }

        void render() const {
            glClear(GL_COLOR_BUFFER_BIT);
            drawBoard();
            drawSidePanel();
            glutSwapBuffers();
        }
    };
}

// ============================================================================
// GAME ENGINE MODULE
// ============================================================================

namespace GameEngine {
    using namespace Config;
    using namespace Tetromino;
    using namespace Board;
    using namespace Renderer;
    using namespace Math;

    class Game {
    private:
        GameBoard board;
        Piece currentPiece;
        Piece nextPiece;
        PieceFactory factory;
        GameRenderer renderer;
        float dropInterval;

    public:
        Game() : renderer(&board, &currentPiece, &nextPiece), 
                 dropInterval(DEFAULT_DROP_INTERVAL) {
            nextPiece = factory.createRandomPiece();
            spawnPiece();
        }

        void spawnPiece() {
            // Use nextPiece if it has blocks, otherwise create new piece
            if (nextPiece.blocks.empty()) {
                currentPiece = factory.createRandomPiece();
            } else {
                currentPiece = nextPiece;
            }
            
            currentPiece.transform = matIdentity();
            currentPiece.translate(BOARD_W / 2.0f, 1.0f);

            nextPiece = factory.createRandomPiece();
            nextPiece.transform = matIdentity();

            if (!board.canPlace(currentPiece)) {
                board.setGameOver(true);
            }
        }

        bool tryMove(float dx, float dy) {
            Piece testPiece = currentPiece;
            testPiece.translate(dx, dy);

            if (board.canPlace(testPiece)) {
                currentPiece = testPiece;
                return true;
            }
            return false;
        }

        bool tryRotate() {
            Piece testPiece = currentPiece;
            testPiece.rotate(90.0f);

            if (board.canPlace(testPiece)) {
                currentPiece = testPiece;
                return true;
            }

            // Wall kick
            const float kicks[] = {-1, 1, -2, 2};
            for (float k : kicks) {
                Piece kickPiece = testPiece;
                kickPiece.translate(k, 0);

                if (board.canPlace(kickPiece)) {
                    currentPiece = kickPiece;
                    return true;
                }
            }
            return false;
        }

        void softDrop() {
            if (board.isGameOver()) return;

            Piece testPiece = currentPiece;
            testPiece.translate(0, 1);

            if (board.canPlace(testPiece)) {
                currentPiece = testPiece;
            } else {
                board.lockPiece(currentPiece);
                board.clearLines();
                spawnPiece();
            }
        }

        void hardDrop() {
            if (board.isGameOver()) return;

            while (true) {
                Piece testPiece = currentPiece;
                testPiece.translate(0, 1);

                if (!board.canPlace(testPiece))
                    break;
                currentPiece = testPiece;
            }
            board.lockPiece(currentPiece);
            board.clearLines();
            spawnPiece();
        }

        void handleKeyLeft() { tryMove(-1, 0); }
        void handleKeyRight() { tryMove(1, 0); }
        void handleKeyUp() { tryRotate(); }
        void handleKeyDown() { softDrop(); }
        void handleKeySpace() { hardDrop(); }
        void handleKeyR() {
            board.reset();
            nextPiece = factory.createRandomPiece();
            spawnPiece();
        }

        void update() { softDrop(); }
        void render() { renderer.render(); }
        float getDropInterval() const { return dropInterval; }
        bool isGameOver() const { return board.isGameOver(); }
    };
}

// ============================================================================
// GLOBAL GAME INSTANCE
// ============================================================================

GameEngine::Game *gameInstance = nullptr;

// ============================================================================
// GLUT CALLBACKS
// ============================================================================

void display() {
    if (gameInstance)
        gameInstance->render();
}

void timerFunc(int value) {
    if (gameInstance) {
        gameInstance->update();
        glutPostRedisplay();
        glutTimerFunc((int)gameInstance->getDropInterval(), timerFunc, 0);
    }
}

void specialKey(int key, int x, int y) {
    if (!gameInstance) return;

    switch (key) {
    case GLUT_KEY_LEFT:
        gameInstance->handleKeyLeft();
        break;
    case GLUT_KEY_RIGHT:
        gameInstance->handleKeyRight();
        break;
    case GLUT_KEY_DOWN:
        gameInstance->handleKeyDown();
        break;
    case GLUT_KEY_UP:
        gameInstance->handleKeyUp();
        break;
    }
    glutPostRedisplay();
}

void keyboard(unsigned char key, int x, int y) {
    if (!gameInstance) return;

    if (key == 27) exit(0);
    if (key == ' ') gameInstance->handleKeySpace();
    if (key == 'r' || key == 'R') gameInstance->handleKeyR();
    glutPostRedisplay();
}

void reshape(int w, int h) {
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void initGL() {
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glShadeModel(GL_FLAT);
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char **argv) {
    srand((unsigned)time(nullptr));

    gameInstance = new GameEngine::Game();

    glutInit(&argc, argv);
    BlockFont::init();
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(Config::WINDOW_W, Config::WINDOW_H);
    glutInitWindowPosition(100, 100);
    glutCreateWindow("Tetris - Pure Matrix Transform (No Grid)");

    initGL();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutTimerFunc((int)gameInstance->getDropInterval(), timerFunc, 0);

    glutMainLoop();

    delete gameInstance;
    return 0;
}

