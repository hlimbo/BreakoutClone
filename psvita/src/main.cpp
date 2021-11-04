#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Include bool type only required for C
#include <stdbool.h>

// RNG
#include <stdlib.h>
#include <time.h>

#include <unordered_map>
#include <string>

#include "debugScreen.h"

#define printf psvDebugScreenPrintf

//Screen dimension constants
enum {
  WIDTH  = 960,
  HEIGHT = 544
};

SDL_Window    * gWindow   = NULL;
SDL_Renderer  * gRenderer = NULL;

const int TARGET_FRAME_RATE = 60;
const int TARGET_MILLIS_PER_FRAME = ((float)1 / TARGET_FRAME_RATE) * 1000;

const int STARTING_LIVES = 3;
// measured in meters per second
const int BALL_SPEED = 250;

const int BRICKS_PER_LAYER = 13;
const int LAYERS = 8;
const int BRICKS_COUNT = BRICKS_PER_LAYER * LAYERS;
const int BRICK_WIDTH = 70;
const int BRICK_HEIGHT = 15;
const int BRICK_HORIZ_PADDING = 3;
const int BRICK_VERT_PADDING = 5;
const int CEILING_OFFSET = 30;

const SDL_Color CYAN{ 0, 255, 255, 255 };
const SDL_Color PURPLE{ 128, 0, 128, 255 };
const SDL_Color BLUE{ 0, 0, 255, 255 };
const SDL_Color GREEN{ 0, 255, 0, 255 };
const SDL_Color YELLOW{ 255, 255, 0, 255 };
const SDL_Color ORANGE{ 255, 165, 0, 255 };
const SDL_Color RED{ 255, 0, 0, 255 };
const SDL_Color PINK{ 255, 192, 203, 255 };

const SDL_Color BRICK_COLORS[LAYERS]{ PINK, RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, CYAN };

enum class ColorLabel {
    PINK = 0, RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, CYAN
};

void initBrickPositions(SDL_Rect* bricks, int layerCount, int bricksPerLayer, int width, int height, int horizPadding, int vertPadding) {
    for (int i = 0; i < layerCount; ++i) {
        for (int j = 0; j < bricksPerLayer; ++j) {
            bricks[i * bricksPerLayer + j] = {
                (j * (width + horizPadding)) + 6,
                (i * (height + vertPadding)) + vertPadding + CEILING_OFFSET,
                width,
                height
            };
        }
    }
}

void drawBricks(SDL_Renderer* renderer, SDL_Rect* bricks, bool* bricksCollisionMap, const SDL_Color colors[], int layerCount, int bricksPerLayer) {
    for (int i = 0; i < layerCount; ++i) {
        SDL_SetRenderDrawColor(renderer, colors[i].r, colors[i].g, colors[i].b, colors[i].a);
        for (int j = 0; j < bricksPerLayer; ++j) {
            if (!bricksCollisionMap[i * bricksPerLayer + j]) {
                SDL_RenderFillRect(renderer, &bricks[i * bricksPerLayer + j]);
            }
        }
    }
}

bool canRectanglesOverlap(const SDL_Rect& r1, const SDL_Rect& r2) {
    return (
        // Horizontal component
        r1.x < r2.x + r2.w && r1.x + r1.w > r2.x &&
        // Vertical component
        r1.y < r2.y + r2.h && r1.y + r1.h > r2.y
    );
}

// return -1 if no collision found
int checkBrickCollision(SDL_Rect* bricks, bool* bricksCollisionMap, int bricksCount, const SDL_Rect& ball, float xVel, float yVel) {
    for (int i = 0; i < bricksCount; ++i) {
        // skip to the next brick if a collision already happened
        if (bricksCollisionMap[i]) {
            continue;
        }

        // Needs improvement: (Ball moving diagonally will sometimes will fail the collision detection
        // check and cause the ball to go through the next set of bricks above the brick we just hit)
        // Collision detection can be more accurate by calculating
        // the point of intersection between a projected point and edge of a brick
        // What can I do with point of intersection?
        // Can correct the position of the brick (1 frame) so that it doesn't pass over bricks
        // it should have not hit in the upcoming frames by setting the ball's position to
        // the point of intersection
        // See: https://codeincomplete.com/articles/collision-detection-in-breakout/
        if (canRectanglesOverlap(ball, bricks[i])) {
            bricksCollisionMap[i] = true;
            return i;
        }
    }

    return -1;
}

void ResetBallPosition(SDL_Rect* ball) {
    ball->x = (WIDTH - ball->w) / 2;
    ball->y = (HEIGHT - ball->h) / 2;
}

void SetRandomBallDirection(int* xDir, int* yDir) {
    int directions[2] = { -1, 1 };
    *xDir = directions[rand() % 2];
    *yDir = 1;
}

void ResetGame(int* score, int* lives, bool* isGameOver, SDL_Rect* ball, int* xDir, int* yDir) {
    *score = 0;
    *lives = STARTING_LIVES;
    *isGameOver = false;

    ResetBallPosition(ball);
    SetRandomBallDirection(xDir, yDir);
}

void ResetBrickMap(bool* bricks) {
    for (int i = 0; i < BRICKS_COUNT; ++i) {
        bricks[i] = false;
    }
}

void ResetPaddlePosition(SDL_Rect* paddle) {
    paddle->x = (WIDTH - paddle->w) / 2;
}

int main(int argc, char *argv[]) 
{
    // generate random seed based on time
    srand(time(NULL));

    psvDebugScreenInit();

    int currentLives = STARTING_LIVES;
    int currentScore = 0;

    std::unordered_map<ColorLabel, int> scoresTable;
    scoresTable[ColorLabel::PINK] = 8;
    scoresTable[ColorLabel::RED] = 7;
    scoresTable[ColorLabel::ORANGE] = 6;
    scoresTable[ColorLabel::YELLOW] = 5;
    scoresTable[ColorLabel::GREEN] = 4;
    scoresTable[ColorLabel::BLUE] = 3;
    scoresTable[ColorLabel::PURPLE] = 2;
    scoresTable[ColorLabel::CYAN] = 1;

    if( SDL_Init( SDL_INIT_VIDEO ) < 0 )
        return -1;

    if (TTF_Init() == -1) {
        return -1;
    }

    gWindow = SDL_CreateWindow(
        "BreakoutClone", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        WIDTH, 
        HEIGHT, 
        SDL_WINDOW_SHOWN
    );
    if (gWindow == NULL)
        return -1;

    gRenderer = SDL_CreateRenderer(gWindow, -1, 0);
    if (gRenderer == NULL)
        return -1;
    
    TTF_Font* font = TTF_OpenFont("app0:/resources/font.otf", 28);
    if (font == nullptr) {
        printf("Font could not be opened! sample.ttf !\n");
        sceKernelDelayThread(2 * 1000 * 1000);
        return -1;
    }

    TTF_Font* bigFont = TTF_OpenFont("app0:/resources/sample.ttf", 48);
    if (bigFont == nullptr) {
        return -1;
    }

    int paddleWidth = 150;
    int paddleHeight = 20;
    int paddleSpeed = 1000;
    SDL_Rect paddle{ (WIDTH - paddleWidth) / 2, (HEIGHT - paddleHeight) - 20, paddleWidth, paddleHeight };

    int ballWidth = 15;
    int ballHeight = 15;
    // measured in units per second
    int ballSpeed = BALL_SPEED;
    int xDirection;
    int yDirection;

    SetRandomBallDirection(&xDirection, &yDirection);
    SDL_Rect ball{ (WIDTH - ballWidth) / 2, (HEIGHT - ballHeight) / 2, ballWidth, ballHeight };

    SDL_Rect bricks[BRICKS_COUNT];
    initBrickPositions(bricks, LAYERS, BRICKS_PER_LAYER, BRICK_WIDTH, BRICK_HEIGHT, BRICK_HORIZ_PADDING, BRICK_VERT_PADDING);

    bool bricksCollisionMap[BRICKS_COUNT];
    for (int i = 0; i < BRICKS_COUNT; ++i) {
        bricksCollisionMap[i] = false;
    }

    SDL_Color textColor{ 255, 255, 255, 255 };
    SDL_Surface* surf = TTF_RenderText_Solid(font, "Score: ", textColor);
    SDL_Texture* scoreLabelTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect scoreLabelRect{ 0, 0, 0, 0 };
    SDL_QueryTexture(scoreLabelTexture, NULL, NULL, &(scoreLabelRect.w), &(scoreLabelRect.h));
    SDL_FreeSurface(surf);

    surf = TTF_RenderText_Solid(font, "Lives: ", textColor);
    SDL_Texture* livesLabelTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect livesLabelRect{ 0, 0, 0, 0 };
    SDL_QueryTexture(livesLabelTexture, NULL, NULL, &(livesLabelRect.w), &(livesLabelRect.h));
    SDL_FreeSurface(surf);

    surf = TTF_RenderText_Solid(bigFont, "Game Over", textColor);
    SDL_Texture* gameOverTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect gameOverRect{ 0, 0, 0, 0 };
    SDL_QueryTexture(gameOverTexture, NULL, NULL, &(gameOverRect.w), &(gameOverRect.h));
    SDL_FreeSurface(surf);
    gameOverRect.x = (WIDTH - gameOverRect.w) / 2;
    gameOverRect.y = (HEIGHT - gameOverRect.h) / 2;

    surf = TTF_RenderText_Solid(bigFont, "Press X to Play Again", textColor);
    SDL_Texture* playAgainTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect playAgainRect{ 0, 0, 0, 0 };
    SDL_QueryTexture(playAgainTexture, NULL, NULL, &(playAgainRect.w), &(playAgainRect.h));
    SDL_FreeSurface(surf);
    playAgainRect.x = (WIDTH - playAgainRect.w) / 2;
    playAgainRect.y = (HEIGHT + gameOverRect.h + 100 - playAgainRect.h) / 2;

    surf = TTF_RenderText_Solid(bigFont, "Game Paused", textColor);
    SDL_Texture* gamePauseTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect gamePauseRect{ 0, 0, 0, 0 };
    SDL_QueryTexture(gamePauseTexture, NULL, NULL, &(gamePauseRect.w), &(gamePauseRect.h));
    SDL_FreeSurface(surf);
    gamePauseRect.x = (WIDTH - gamePauseRect.w) / 2;
    gamePauseRect.y = (HEIGHT - gamePauseRect.y) / 2;

    surf = TTF_RenderText_Solid(bigFont, "Press Triangle to Unpause", textColor);
    SDL_Texture* pauseLabelTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
    SDL_Rect pauseLabelRect{ 0, 0, 0, 0 };
    SDL_QueryTexture(pauseLabelTexture, NULL, NULL, &(pauseLabelRect.w), &(pauseLabelRect.h));
    SDL_FreeSurface(surf);
    pauseLabelRect.x = (WIDTH - pauseLabelRect.w) / 2;
    pauseLabelRect.y = (HEIGHT + gamePauseRect.h + 100 - pauseLabelRect.h) / 2;


    bool leftPressed = false;
    bool rightPressed = false;

    // measured in 1 / X seconds
    float timeDelta = (TARGET_MILLIS_PER_FRAME / (float)1000);

    bool isGameOver = false;
    bool isGamePaused = false;
    bool isGameRunning = true;

    SceCtrlData ctrl;
    while (isGameRunning)
    {
        Uint32 start = SDL_GetTicks();

        // inputs
        sceCtrlPeekBufferPositive(0, &ctrl, 1);

        if (!isGameOver) {
            if (!isGamePaused) {
                if (ctrl.buttons == SCE_CTRL_LEFT)
                {
                    paddle.x -= paddleSpeed * timeDelta;
                }
                else if (ctrl.buttons == SCE_CTRL_RIGHT)
                {
                    paddle.x += paddleSpeed * timeDelta;
                }
            }

            if (ctrl.buttons == SCE_CTRL_TRIANGLE)
            {
                isGamePaused = !isGamePaused;
            }
        }

        if (isGameOver) {
            if (ctrl.buttons == SCE_CTRL_CROSS) {
                isGameOver = false;
                ResetGame(&currentScore, &currentLives, &isGameOver, &ball, &xDirection, &yDirection);
                ResetPaddlePosition(&paddle);
                ResetBrickMap(bricksCollisionMap);
            }
        }

        float xVel = xDirection * ballSpeed * timeDelta;
        float yVel = yDirection * ballSpeed * timeDelta;

        // Screen Bounds Ball Collision Check
        {
            if (ball.x + xVel < 0)
            {
                xDirection *= -1;
            }

            if (ball.x + ball.w + xVel > WIDTH)
            {
                xDirection *= -1;
            }

            // lose live condition
            if (ball.y + ball.h + yVel > HEIGHT)
            {
                if (currentLives - 1 >= 0) {
                    --currentLives;
                }

                if (currentLives > 0) {
                    SetRandomBallDirection(&xDirection, &yDirection);
                    ResetBallPosition(&ball);
                }
                else {
                    isGameOver = true;
                }
            }

            if (ball.y + yVel < CEILING_OFFSET)
            {
                yDirection *= -1;
            }
        }

        // Ball to Paddle Collision Check
        {
            if (canRectanglesOverlap(ball, paddle)) {
                yDirection *= -1;
            }
        }

        int brickIndex = checkBrickCollision(bricks, bricksCollisionMap, BRICKS_COUNT, ball, xVel, yVel);
        if (brickIndex != -1) {
            yDirection *= -1;
            // update score
            int layerIndex = brickIndex / BRICKS_PER_LAYER;
            currentScore += scoresTable[(ColorLabel)layerIndex];
        }

        // Move Ball
        {
            if (!isGamePaused) {
                ball.x += xDirection * ballSpeed * timeDelta;
                ball.y += yDirection * ballSpeed * timeDelta;
            }
        }


        // RENDER UI
        {
            surf = TTF_RenderText_Solid(font, std::to_string(currentScore).c_str(), textColor);
            SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
            SDL_Rect scoreRect{ 0, 0, 0, 0 };
            SDL_QueryTexture(scoreTexture, NULL, NULL, &(scoreRect.w), &(scoreRect.h));
            SDL_FreeSurface(surf);
            surf = NULL;

            surf = TTF_RenderText_Solid(font, std::to_string(currentLives).c_str(), textColor);
            SDL_Texture* livesTexture = SDL_CreateTextureFromSurface(gRenderer, surf);
            SDL_Rect livesRect{ 0, 0, 0, 0 };
            SDL_QueryTexture(livesTexture, NULL, NULL, &(livesRect.w), &(livesRect.h));
            SDL_FreeSurface(surf);
            surf = NULL;

            scoreRect.x = scoreLabelRect.x + scoreLabelRect.w;
            livesRect.x = WIDTH - livesRect.w;
            livesLabelRect.x = WIDTH - livesLabelRect.w - livesRect.w;

            if (isGameOver) {
                SDL_RenderCopy(gRenderer, gameOverTexture, NULL, &gameOverRect);
                SDL_RenderCopy(gRenderer, playAgainTexture, NULL, &playAgainRect);
            }

            if (isGamePaused) {
                SDL_RenderCopy(gRenderer, gamePauseTexture, NULL, &gamePauseRect);
                SDL_RenderCopy(gRenderer, pauseLabelTexture, NULL, &pauseLabelRect);
            }

            SDL_RenderCopy(gRenderer, scoreLabelTexture, NULL, &scoreLabelRect);
            SDL_RenderCopy(gRenderer, livesLabelTexture, NULL, &livesLabelRect);
            SDL_RenderCopy(gRenderer, scoreTexture, NULL, &scoreRect);
            SDL_RenderCopy(gRenderer, livesTexture, NULL, &livesRect);

            // destroy on every frame the score and lives textures ~~ may be bad for performance
            // but should at least solve memory leak issue
            SDL_DestroyTexture(scoreTexture);
            SDL_DestroyTexture(livesTexture);
        }


        // Render Graphics
        drawBricks(gRenderer, bricks, bricksCollisionMap, BRICK_COLORS, LAYERS, BRICKS_PER_LAYER);

        SDL_SetRenderDrawColor(gRenderer, 255, 255, 255, 255);
        SDL_RenderFillRect(gRenderer, &paddle);
        SDL_RenderFillRect(gRenderer, &ball);

        SDL_RenderPresent(gRenderer);

        // Clear buffer
        SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, 255);
        SDL_RenderClear(gRenderer);

        // sleep app if hardware is running this update iteration too fast
        Uint32 end = SDL_GetTicks();
        Uint32 tickProcessTime = end - start;

        if (tickProcessTime < TARGET_MILLIS_PER_FRAME)
        {
            Uint32 sleepTime = TARGET_MILLIS_PER_FRAME - tickProcessTime;
            timeDelta = (tickProcessTime + sleepTime) / (float)1000;
            SDL_Delay(sleepTime);
        }
        else
        {
            timeDelta = tickProcessTime / (float)1000;
        }
    }

    // Create a template function that will allow us to destroy multiple resources in one line
    // using ellipsis
    // See: https://www.willusher.io/sdl2%20tutorials/2014/08/01/postscript-1-easy-cleanup
    SDL_DestroyTexture(gameOverTexture);
    SDL_DestroyTexture(gamePauseTexture);
    SDL_DestroyTexture(livesLabelTexture);
    SDL_DestroyTexture(pauseLabelTexture);
    SDL_DestroyTexture(playAgainTexture);
    SDL_DestroyTexture(scoreLabelTexture);
  
    TTF_CloseFont(font);
    TTF_CloseFont(bigFont);
    SDL_DestroyRenderer( gRenderer );
    SDL_DestroyWindow( gWindow );
    gWindow = NULL;
    gRenderer = NULL;
  
    TTF_Quit();
    SDL_Quit();
    sceKernelExitProcess(0);
    return 0;
}
