// dx_ball_visuals.cpp (bricks centered)
// Compile: g++ dx_ball_visuals_centered_bricks.cpp -o dx_ball_visuals -lGL -lGLU -lglut
#include <GL/glut.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// -------------------------- Game config --------------------------
enum GameState { STATE_MENU, STATE_INSTRUCTIONS, STATE_PLAYING, STATE_PAUSED, STATE_GAMEOVER, STATE_WIN };
GameState state = STATE_MENU;

// Paddle
float paddleX = 0.0f;
float paddleWidth = 0.30f;
const float paddleHeight = 0.05f;
const float PADDLE_MIN_WIDTH = 0.12f;
const float PADDLE_MAX_WIDTH = 0.7f;

// Ball
float ballX = 0.0f, ballY = -0.5f;
float ballDX = 0.008f, ballDY = 0.01f;
float ballSpeedMultiplier = 1.0f;
const float ballRadius = 0.03f;
bool ballMoving = false;

// Ball trail (store last positions for simple motion blur)
#define TRAIL_LEN 8
float trailX[TRAIL_LEN];
float trailY[TRAIL_LEN];

// Bricks
#define ROWS 5
#define COLS 8
int bricks[ROWS][COLS];              // 1 = alive, 0 = removed
float brickFade[ROWS][COLS];         // >0 means fading animation
const float brickWidth = 0.22f;
const float brickHeight = 0.08f;
// spacing & computed start to center the grid
float brickSpacingX = 0.02f;
float brickSpacingY = 0.02f;
float brickStartX = 0.0f; // computed so grid is centered
float brickStartY = 0.0f; // computed so grid is centered

// Score and Lives
int score = 0;
int lives = 3;

// Power-ups
enum PowerType { POWER_EXTRA_LIFE = 0, POWER_FASTER_BALL = 1, POWER_WIDER_PADDLE = 2 };
struct PowerUp
{
    bool visible;
    PowerType type;
    float x, y;
    float vy;
} powerUps[ROWS*COLS];

// Power-up durations
bool paddleWidened = false;
int paddleWidenEndTimeMs = 0;
const int PADDLE_WIDEN_DURATION_MS = 10000; // 10s

// Time tracking
int gameStartTimeMs = 0;
int pauseStartTimeMs = 0;
int totalPausedMs = 0;

// Ball speed increase over time
int lastSpeedIncreaseCheckMs = 0;
const int SPEED_INCREASE_INTERVAL_MS = 5000;
const float SPEED_INCREASE_FACTOR = 1.05f;

// Window
int g_winW = 900, g_winH = 700;

// Pause menu buttons (normalized coords)
typedef struct
{
    float left, right, top, bottom;
    const char* label;
} Button;
Button pauseButtons[3];

// Utility text
void drawText(float x, float y, const char* text)
{
    glRasterPos2f(x, y);
    for (const char* c = text; *c != '\0'; ++c)
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, *c);
}

// -------------------------- Brick layout helper --------------------------
void computeBrickLayout()
{
    float marginX = 0.05f;
    float marginY = 0.10f; // top margin

    float totalW = COLS * brickWidth + (COLS - 1) * brickSpacingX;
    float totalH = ROWS * brickHeight + (ROWS - 1) * brickSpacingY;

    brickStartX = -totalW * 0.5f;

    brickStartY = 1.0f - marginY;
    brickStartY -= (brickHeight * 0.5f);

    // extra downward shift
    brickStartY -= 0.05f;
}

// -------------------------- Game control --------------------------
void resetBall()
{
    ballX = 0.0f;
    ballY = -0.5f;
    ballDX = 0.008f * ((rand() % 2) ? 1.0f : -1.0f);
    ballDY = 0.01f;
    ballSpeedMultiplier = 1.0f;
    ballMoving = false;
    // clear trail
    for (int i = 0; i < TRAIL_LEN; ++i)
    {
        trailX[i] = ballX;
        trailY[i] = ballY;
    }
}

void resetGame()
{
    score = 0;
    lives = 3;
    paddleWidth = 0.30f;
    paddleWidened = false;
    paddleWidenEndTimeMs = 0;
    totalPausedMs = 0;
    gameStartTimeMs = glutGet(GLUT_ELAPSED_TIME);
    lastSpeedIncreaseCheckMs = gameStartTimeMs;
    for (int i=0; i<ROWS; ++i) for (int j=0; j<COLS; ++j)
        {
            bricks[i][j] = 1;
            brickFade[i][j] = 0.0f;
        }
    for (int i=0; i<ROWS*COLS; ++i) powerUps[i].visible = false;
    computeBrickLayout();
    resetBall();
}

// -------------------------- Visual improvements --------------------------

void drawInstructionsOverlay()
{
    // dim background
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.7f);
    glBegin(GL_QUADS);
    glVertex2f(-1,-1);
    glVertex2f(1,-1);
    glVertex2f(1,1);
    glVertex2f(-1,1);
    glEnd();
    glDisable(GL_BLEND);

    // panel
    float panelW = 0.7f, panelH = 0.6f;
    glColor3f(0.1f,0.1f,0.15f);
    glBegin(GL_QUADS);
    glVertex2f(-panelW/2, panelH/2);
    glVertex2f(panelW/2, panelH/2);
    glVertex2f(panelW/2, -panelH/2);
    glVertex2f(-panelW/2, -panelH/2);
    glEnd();

    // title
    glColor3f(1,1,1);
    drawText(-0.12f, 0.22f, "Instructions");

    // instructions text
    drawText(-0.25f, 0.12f, "• Mouse to move paddle");
    drawText(-0.25f, 0.05f, "• A / D or Arrow Keys to move");
    drawText(-0.25f, -0.02f, "• SPACE to launch the ball");
    drawText(-0.25f, -0.09f, "• P to pause/resume");
    drawText(-0.25f, -0.16f, "• Esc to exit game");

    drawText(-0.25f, -0.28f, "Click anywhere to return to menu");
}

// Gradient background
void drawBackground()
{
    glBegin(GL_QUADS);
    // top-left (slightly bluish)
    glColor3f(0.02f, 0.03f, 0.12f);
    glVertex2f(-1.0f,  1.0f);
    // top-right
    glColor3f(0.07f, 0.05f, 0.2f);
    glVertex2f( 1.0f,  1.0f);
    // bottom-right (darker)
    glColor3f(0.01f, 0.01f, 0.05f);
    glVertex2f( 1.0f, -1.0f);
    // bottom-left
    glColor3f(0.01f, 0.01f, 0.05f);
    glVertex2f(-1.0f, -1.0f);
    glEnd();

    // subtle stars (random seed stable by frame)
    int t = glutGet(GLUT_ELAPSED_TIME) / 700;
    srand(t);
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for (int i=0; i<30; i++)
    {
        float sx = (rand()%200 - 100)/100.0f;
        float sy = (rand()%140 - 70)/100.0f;
        float alpha = 0.4f + (rand()%60)/150.0f;
        glColor4f(0.9f, 0.9f, 1.0f, alpha);
        glVertex2f(sx, sy);
    }
    glEnd();
}

// Paddle with gradient/shading
void drawPaddle()
{
    // center colors vary a bit over time for subtle liveliness
    float t = glutGet(GLUT_ELAPSED_TIME)/1000.0f;
    float pulse = 0.05f * sinf(t*2.0f);

    // top gradient
    glBegin(GL_QUADS);
    glColor3f(0.12f + pulse, 0.45f + pulse, 0.95f); // top-left
    glVertex2f(paddleX - paddleWidth/2, -0.95f + paddleHeight);
    glColor3f(0.02f + pulse, 0.25f + pulse, 0.7f);  // top-right
    glVertex2f(paddleX + paddleWidth/2, -0.95f + paddleHeight);
    glColor3f(0.0f, 0.12f, 0.3f);                    // bottom-right
    glVertex2f(paddleX + paddleWidth/2, -0.95f);
    glColor3f(0.05f, 0.2f, 0.6f);                    // bottom-left
    glVertex2f(paddleX - paddleWidth/2, -0.95f);
    glEnd();

    // small bevel lines
    glColor3f(0,0,0);
    glLineWidth(1.0f);
    glBegin(GL_LINE_LOOP);
    glVertex2f(paddleX - paddleWidth/2, -0.95f + paddleHeight);
    glVertex2f(paddleX + paddleWidth/2, -0.95f + paddleHeight);
    glVertex2f(paddleX + paddleWidth/2, -0.95f);
    glVertex2f(paddleX - paddleWidth/2, -0.95f);
    glEnd();
}

// Ball glow (soft layered circles)
void drawBallGlow()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 5; i >= 1; --i)
    {
        float a = 0.06f + 0.02f * i;
        float r = ballRadius + 0.004f*i;
        glColor4f(1.0f, 0.3f, 0.3f, a);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(ballX, ballY);
        for (int a_deg = 0; a_deg <= 360; a_deg += 12)
        {
            float ang = a_deg * (3.1415926f / 180.0f);
            glVertex2f(ballX + r * cosf(ang), ballY + r * sinf(ang));
        }
        glEnd();
    }
    glDisable(GL_BLEND);
}

// Ball core
void drawBallCore()
{
    glColor3f(1.0f, 0.7f, 0.7f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(ballX, ballY);
    for (int a_deg = 0; a_deg <= 360; a_deg += 10)
    {
        float ang = a_deg * (3.1415926f / 180.0f);
        glVertex2f(ballX + ballRadius * cosf(ang), ballY + ballRadius * sinf(ang));
    }
    glEnd();
}

// Ball trail: draw faded circles at last positions
void drawBallTrail()
{
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (int i = 0; i < TRAIL_LEN; ++i)
    {
        float alpha = 0.10f * (1.0f - (float)i / TRAIL_LEN);
        float r = ballRadius * (1.0f - 0.07f * i);
        glColor4f(1.0f, 0.4f, 0.4f, alpha);
        glBegin(GL_TRIANGLE_FAN);
        glVertex2f(trailX[i], trailY[i]);
        for (int a_deg = 0; a_deg <= 360; a_deg += 18)
        {
            float ang = a_deg * (3.1415926f / 180.0f);
            glVertex2f(trailX[i] + r * cosf(ang), trailY[i] + r * sinf(ang));
        }
        glEnd();
    }
    glDisable(GL_BLEND);
}

// Draw bricks - normal and fading-removed with animation
void drawBricks()
{
    for (int i=0; i<ROWS; ++i)
    {
        for (int j=0; j<COLS; ++j)
        {
            float x = brickStartX + j * (brickWidth + brickSpacingX);
            float y = brickStartY - i * (brickHeight + brickSpacingY);

            if (bricks[i][j])
            {
                // main brick body with slight vertical gradient
                glBegin(GL_QUADS);
                glColor3f(0.9f, 0.4f - i*0.06f, 0.2f + j*0.03f);
                glVertex2f(x, y);
                glColor3f(0.7f, 0.25f - i*0.04f, 0.15f + j*0.02f);
                glVertex2f(x + brickWidth, y);
                glColor3f(0.5f, 0.12f - i*0.02f, 0.10f + j*0.01f);
                glVertex2f(x + brickWidth, y - brickHeight);
                glColor3f(0.65f, 0.20f - i*0.03f, 0.12f + j*0.015f);
                glVertex2f(x, y - brickHeight);
                glEnd();
                // border
                glColor3f(0.08f, 0.06f, 0.04f);
                glLineWidth(1.5f);
                glBegin(GL_LINE_LOOP);
                glVertex2f(x, y);
                glVertex2f(x + brickWidth, y);
                glVertex2f(x + brickWidth, y - brickHeight);
                glVertex2f(x, y - brickHeight);
                glEnd();
            }
            // draw fading remnants if any
            if (brickFade[i][j] > 0.001f)
            {
                float f = brickFade[i][j];
                glEnable(GL_BLEND);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                glColor4f(1.0f, 0.6f - i*0.05f, 0.25f + j*0.02f, f);
                // simple expanding square fade
                float inset = (1.0f - f) * 0.06f;
                glBegin(GL_QUADS);
                glVertex2f(x - inset, y + inset);
                glVertex2f(x + brickWidth + inset, y + inset);
                glVertex2f(x + brickWidth + inset, y - brickHeight - inset);
                glVertex2f(x - inset, y - brickHeight - inset);
                glEnd();
                glDisable(GL_BLEND);
            }
        }
    }
}

// Power-ups draw with pulse animation
void drawPowerUps()
{
    int now = glutGet(GLUT_ELAPSED_TIME);
    for (int i = 0; i < ROWS*COLS; ++i)
    {
        if (!powerUps[i].visible) continue;
        float s = 0.02f * (1.0f + 0.15f * sinf(now/250.0f + i));
        switch (powerUps[i].type)
        {
        case POWER_EXTRA_LIFE:
            glColor3f(0.2f, 1.0f, 0.2f);
            break;
        case POWER_FASTER_BALL:
            glColor3f(1.0f, 0.6f, 0.6f);
            break;
        case POWER_WIDER_PADDLE:
            glColor3f(0.6f, 0.8f, 1.0f);
            break;
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_QUADS);
        glVertex2f(powerUps[i].x - 0.03f - s, powerUps[i].y + s);
        glVertex2f(powerUps[i].x + 0.03f + s, powerUps[i].y + s);
        glVertex2f(powerUps[i].x + 0.03f + s, powerUps[i].y - 0.05f - s);
        glVertex2f(powerUps[i].x - 0.03f - s, powerUps[i].y - 0.05f - s);
        glEnd();
        glDisable(GL_BLEND);

        // label
        char label = 'L';
        if (powerUps[i].type == POWER_FASTER_BALL) label = 'F';
        if (powerUps[i].type == POWER_WIDER_PADDLE) label = 'W';
        glColor3f(0,0,0);
        char str[2] = {label, 0};
        glRasterPos2f(powerUps[i].x - 0.01f, powerUps[i].y - 0.03f);
        glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, str[0]);
    }
}

// HUD drawing
void drawHUD()
{
    char buffer[64];
    sprintf(buffer, "Score: %d", score);
    drawText(-0.95f, 0.93f, buffer);

    sprintf(buffer, "Lives: %d", lives);
    drawText(0.75f, 0.93f, buffer);

    int elapsedMs = 0;
    if (state == STATE_PLAYING || state == STATE_PAUSED)
    {
        int now = glutGet(GLUT_ELAPSED_TIME);
        elapsedMs = now - gameStartTimeMs - totalPausedMs;
    }
    int seconds = elapsedMs / 1000;
    sprintf(buffer, "Time: %02d:%02d", seconds / 60, seconds % 60);
    drawText(-0.1f, 0.93f, buffer);

    // -------------- MENU SCREEN --------------
    if (state == STATE_MENU)
    {
        drawText(-0.25f, 0.45f, "🎮 WELCOME TO DX-BALL 🎮");
        drawText(-0.15f, 0.25f, "Click anywhere to start");

        drawText(-0.25f, 0.05f, "Controls:");
        drawText(-0.18f, -0.05f, "• Mouse to move paddle");
        drawText(-0.18f, -0.12f, "• A / D or Arrow Keys to move");
        drawText(-0.18f, -0.19f, "• SPACE to launch the ball");
        drawText(-0.18f, -0.26f, "• P to pause/resume");
        drawText(-0.18f, -0.33f, "• Esc to exit game");
    }

    // -------------- PAUSED SCREEN --------------
    if (state == STATE_PAUSED)
    {
        drawText(-0.15f, 0.1f, "⏸ GAME PAUSED ⏸");
        drawText(-0.25f, -0.05f, "• Press P or click to resume");
        drawText(-0.25f, -0.12f, "• Press Esc to quit to menu");
    }

    // -------------- GAME OVER SCREEN --------------
    if (state == STATE_GAMEOVER)
    {
        drawText(-0.25f, 0.2f, "💀 GAME OVER 💀");
        sprintf(buffer, "Final Score: %d", score);
        drawText(-0.18f, 0.05f, buffer);
        drawText(-0.22f, -0.1f, "• Click to restart");
        drawText(-0.22f, -0.18f, "• Press Esc to exit");
    }

    // -------------- WIN SCREEN --------------
    if (state == STATE_WIN)
    {
        drawText(-0.25f, 0.2f, "🏆 YOU WIN! 🏆");
        sprintf(buffer, "Final Score: %d", score);
        drawText(-0.18f, 0.05f, buffer);
        drawText(-0.22f, -0.1f, "• Click to play again");
        drawText(-0.22f, -0.18f, "• Press Esc to exit");
    }
}
// Main Menu overlay with buttons
void drawMenuScreenOverlay()
{
    // dim entire screen
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(-1, -1);
    glVertex2f(1, -1);
    glVertex2f(1, 1);
    glVertex2f(-1, 1);
    glEnd();
    glDisable(GL_BLEND);

    // center panel
    float panelW = 0.7f, panelH = 0.6f;
    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(-panelW/2,  panelH/2);
    glVertex2f( panelW/2,  panelH/2);
    glVertex2f( panelW/2, -panelH/2);
    glVertex2f(-panelW/2, -panelH/2);
    glEnd();

    // title
    glColor3f(1, 1, 1);
    drawText(-0.20f, 0.22f, "DX-Ball OpenGL");

    // example menu buttons (3 items)
    Button menuButtons[3];
    menuButtons[0] = { -0.25f, 0.25f, 0.10f,  0.00f,  "Start Game" };
    menuButtons[1] = { -0.25f, 0.25f, -0.05f, -0.15f, "Instructions" };
    menuButtons[2] = { -0.25f, 0.25f, -0.20f, -0.30f, "Quit" };

    for (int i=0; i<3; i++)
    {
        Button b = menuButtons[i];
        // button bg
        glColor3f(0.18f, 0.18f, 0.22f);
        glBegin(GL_QUADS);
        glVertex2f(b.left, b.top);
        glVertex2f(b.right, b.top);
        glVertex2f(b.right, b.bottom);
        glVertex2f(b.left, b.bottom);
        glEnd();
        // border
        glColor3f(0.9f, 0.9f, 0.9f);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(b.left, b.top);
        glVertex2f(b.right, b.top);
        glVertex2f(b.right, b.bottom);
        glVertex2f(b.left, b.bottom);
        glEnd();
        // label centered
        float tx = (b.left + b.right) * 0.5f - 0.09f;
        float ty = (b.top + b.bottom) * 0.5f - 0.02f;
        drawText(tx, ty, b.label);
    }
}

// Game Over screen overlay with larger panel and centered text
void drawGameOverScreenOverlay()
{
    // Dim background
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
        glVertex2f(-1, -1);
        glVertex2f( 1, -1);
        glVertex2f( 1,  1);
        glVertex2f(-1,  1);
    glEnd();
    glDisable(GL_BLEND);

    // Larger Panel
    float panelW = 0.75f; // increased width
    float panelH = 0.6f;  // increased height
    float panelX = 0.0f;
    float panelY = 0.0f;

    glColor3f(0.1f, 0.1f, 0.15f); // dark panel
    glBegin(GL_QUADS);
        glVertex2f(panelX - panelW/2, panelY + panelH/2);
        glVertex2f(panelX + panelW/2, panelY + panelH/2);
        glVertex2f(panelX + panelW/2, panelY - panelH/2);
        glVertex2f(panelX - panelW/2, panelY - panelH/2);
    glEnd();

    // Border
    glLineWidth(3.0f);
    glColor3f(1.0f, 0.2f, 0.2f);
    glBegin(GL_LINE_LOOP);
        glVertex2f(panelX - panelW/2, panelY + panelH/2);
        glVertex2f(panelX + panelW/2, panelY + panelH/2);
        glVertex2f(panelX + panelW/2, panelY - panelH/2);
        glVertex2f(panelX - panelW/2, panelY - panelH/2);
    glEnd();

    // Updated Y positions for larger panel
    float titleY = 0.2f;
    float scoreY = 0.08f;
    float instr1Y = -0.08f;
    float instr2Y = -0.18f;

    // Title
    glColor3f(1.0f, 0.2f, 0.2f); // red
    drawText(-0.18f, titleY, "💀 GAME OVER 💀");

    // Score
    char buffer[32];
    sprintf(buffer, "Final Score: %d", score);
    glColor3f(1.0f, 1.0f, 1.0f); // white
    drawText(-0.12f, scoreY, buffer);

    // Instructions
    glColor3f(0.8f, 0.8f, 0.8f); // light gray
    drawText(-0.25f, instr1Y, "Click LEFT MOUSE to RESTART");
    drawText(-0.15f, instr2Y, "Press ESC to QUIT");
}

// Pause menu overlay with buttons
void drawPauseMenuOverlay()
{
    // dim entire screen
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0,0,0,0.6f);
    glBegin(GL_QUADS);
    glVertex2f(-1,-1);
    glVertex2f(1,-1);
    glVertex2f(1,1);
    glVertex2f(-1,1);
    glEnd();
    glDisable(GL_BLEND);

    // center panel
    float panelW = 0.6f, panelH = 0.5f;
    glColor3f(0.08f, 0.08f, 0.12f);
    glBegin(GL_QUADS);
    glVertex2f(-panelW/2,  panelH/2);
    glVertex2f( panelW/2,  panelH/2);
    glVertex2f( panelW/2, -panelH/2);
    glVertex2f(-panelW/2, -panelH/2);
    glEnd();

    // title
    glColor3f(1,1,1);
    drawText(-0.12f, 0.18f, "Game Paused");

    // draw buttons
    for (int i=0; i<3; i++)
    {
        Button b = pauseButtons[i];
        // button bg
        glColor3f(0.18f, 0.18f, 0.22f);
        glBegin(GL_QUADS);
        glVertex2f(b.left, b.top);
        glVertex2f(b.right, b.top);
        glVertex2f(b.right, b.bottom);
        glVertex2f(b.left, b.bottom);
        glEnd();
        // border
        glColor3f(0.9f, 0.9f, 0.9f);
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(b.left, b.top);
        glVertex2f(b.right, b.top);
        glVertex2f(b.right, b.bottom);
        glVertex2f(b.left, b.bottom);
        glEnd();
        // label
        float tx = (b.left + b.right) * 0.5f - 0.10f;
        float ty = (b.top + b.bottom) * 0.5f - 0.02f;
        drawText(tx, ty, b.label);
    }
}

    // Fireworks (simple)
void drawFireworks()
{
    if (state != STATE_WIN) return;
    int now = glutGet(GLUT_ELAPSED_TIME);
    srand(now / 90);
    for (int k=0; k<25; k++)
    {
        float x = (rand()%200 - 100)/100.0f;
        float y = (rand()%140 - 20)/100.0f;
        float r = rand()%256/255.0f, g = rand()%256/255.0f, b = rand()%256/255.0f;
        glColor3f(r,g,b);
        glBegin(GL_LINES);
        glVertex2f(x, y);
        glVertex2f(x + (rand()%40 - 20)/200.0f, y + (rand()%40 - 20)/200.0f);
        glEnd();
    }
}

// Win screen overlay
void drawWinScreenOverlay()

{
    // dim background
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0, 0, 0, 0.6f);
    glBegin(GL_QUADS);
    glVertex2f(-1,-1);
    glVertex2f(1,-1);
    glVertex2f(1,1);
    glVertex2f(-1,1);
    glEnd();
    glDisable(GL_BLEND);
 // panel
    float panelW = 0.6f, panelH = 0.5f;
    glColor3f(0.1f, 0.1f, 0.15f);
    glBegin(GL_QUADS);
    glVertex2f(-panelW/2, panelH/2);
    glVertex2f(panelW/2, panelH/2);
    glVertex2f(panelW/2, -panelH/2);
    glVertex2f(-panelW/2, -panelH/2);
    glEnd();

 // title
    glColor3f(1,1,0.2f);
    drawText(-0.18f, 0.15f, "🏆 YOU WIN! 🏆");













