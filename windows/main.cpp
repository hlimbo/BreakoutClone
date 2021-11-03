#include <iostream>
#include <stdio.h>
#include <unordered_map>
#include <string>
#include <stdlib.h>
#include <time.h>

#include <SDL.h>
#include <SDL_ttf.h>

// PS VITA SCREEN DIMENSIONS
const int SCREEN_WIDTH = 960;
const int SCREEN_HEIGHT = 544;

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

const SDL_Color CYAN { 0, 255, 255, 255 };
const SDL_Color PURPLE { 128, 0, 128, 255 };
const SDL_Color BLUE { 0, 0, 255, 255 };
const SDL_Color GREEN { 0, 255, 0, 255 };
const SDL_Color YELLOW { 255, 255, 0, 255 };
const SDL_Color ORANGE{ 255, 165, 0, 255 };
const SDL_Color RED { 255, 0, 0, 255 };
const SDL_Color PINK{ 255, 192, 203, 255 };

const SDL_Color BRICK_COLORS[LAYERS] { PINK, RED, ORANGE, YELLOW, GREEN, BLUE, PURPLE, CYAN };

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

void drawBricks(SDL_Renderer* renderer, SDL_Rect* bricks,bool* bricksCollisionMap, const SDL_Color colors[], int layerCount, int bricksPerLayer) {
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

std::string getResourcePath(const std::string& subDir = "") {
	// We need to choose the path separator properly based on which
	// platform we're running on, since Windows uses a different
	// separator than most systems

#ifdef _WIN32
	const char PATH_SEP = '\\';
#else
	const char PATH_SEP = '/';
#endif

	// static lifetime -- will be created once and destroyed when application closes
	static std::string baseRes;
	if (baseRes.empty()) {
		// SDL_GetBasePath will return NULL if something went wrong in getting the path
		char* basePath = SDL_GetBasePath();
		if (basePath) {
			baseRes = basePath;
			SDL_free(basePath);
		}
		else {
			std::cerr << "Error getting resource path: " << SDL_GetError() << std::endl;
			return "";
		}

		// We replace the mingw_build with res/ to get the resource path
		size_t pos = baseRes.rfind("mingw_build");
		baseRes = baseRes.substr(0, pos) + "res" + PATH_SEP;
	}

	// If we want a specific sub-directory path in the resource directory
	// append it to the base path. This would be something like res/sub-directory
	return subDir.empty() ? baseRes : baseRes + subDir + PATH_SEP;
}

void ResetBallPosition(SDL_Rect* ball) {
	ball->x = (SCREEN_WIDTH - ball->w) / 2;
	ball->y = (SCREEN_HEIGHT - ball->h) / 2;
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
	paddle->x = (SCREEN_WIDTH - paddle->w) / 2;
}

int main(int argc, char** argv) {

	// generate random seed based on time
	srand(time(NULL));

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
	
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		std::cout << "Error Initializing SDL: " << SDL_GetError() << std::endl;
		return 0;
	}

	if (TTF_Init() == -1) {
		std::cout << "Error Initializing SDL_TTF: " << TTF_GetError() << std::endl;
		return 0;
	}

	SDL_version linked;
	SDL_GetVersion(&linked);

	SDL_version compiled;
	SDL_VERSION(&compiled);

	std::cout << "SDL INITIALIZED!" << std::endl;	
	std::cout << "SDL VERSION: " << (unsigned int)compiled.major << "." << (unsigned int)compiled.minor << "." << (unsigned int)compiled.patch << std::endl;
	std::cout << "target milliseconds per frame: " << TARGET_MILLIS_PER_FRAME << std::endl;

	const std::string resourcePath(getResourcePath());
	std::cout << "BASE PATH: " << resourcePath << std::endl;
	std::string fontPath(resourcePath + "font.otf");
	TTF_Font* font = TTF_OpenFont(fontPath.c_str(), 28);
	if (font == nullptr) {
		std::cout << "Unable to load font: " << TTF_GetError() << std::endl;
		int num;
		std::cin >> num;
		return 0;
	}

	TTF_Font* bigFont = TTF_OpenFont(fontPath.c_str(), 48);
	if (bigFont == nullptr) {
		std::cout << "Unable to load font: " << TTF_GetError() << std::endl;
		int num;
		std::cin >> num;
		return 0;
	}

	SDL_Window* window = SDL_CreateWindow(
		"Breakout Clone",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		SDL_WINDOW_SHOWN
	);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	int paddleWidth = 150;
	int paddleHeight = 20;
	int paddleSpeed = 1000;
	SDL_Rect paddle { (SCREEN_WIDTH - paddleWidth) / 2, (SCREEN_HEIGHT - paddleHeight) - 20, paddleWidth, paddleHeight };

	int ballWidth = 15;
	int ballHeight = 15;
	// measured in units per second
	int ballSpeed = BALL_SPEED;
	int xDirection;
	int yDirection;

	SetRandomBallDirection(&xDirection, &yDirection);
	SDL_Rect ball{ (SCREEN_WIDTH - ballWidth) / 2, (SCREEN_HEIGHT - ballHeight) / 2, ballWidth, ballHeight };

	SDL_Rect bricks[BRICKS_COUNT];
	initBrickPositions(bricks, LAYERS, BRICKS_PER_LAYER, BRICK_WIDTH, BRICK_HEIGHT, BRICK_HORIZ_PADDING, BRICK_VERT_PADDING);

	bool bricksCollisionMap[BRICKS_COUNT];
	for (int i = 0; i < BRICKS_COUNT; ++i) {
		bricksCollisionMap[i] = false;
	}

	SDL_Color textColor{ 255, 255, 255, 255 };
	SDL_Surface* surf = TTF_RenderText_Solid(font, "Score: ", textColor);
	SDL_Texture* scoreLabelTexture = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_Rect scoreLabelRect{ 0, 0, 0, 0 };
	SDL_QueryTexture(scoreLabelTexture, NULL, NULL, &(scoreLabelRect.w), &(scoreLabelRect.h));
	SDL_FreeSurface(surf);

	surf = TTF_RenderText_Solid(font, "Lives: ", textColor);
	SDL_Texture* livesLabelTexture = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_Rect livesLabelRect{ 0, 0, 0, 0 };
	SDL_QueryTexture(livesLabelTexture, NULL, NULL, &(livesLabelRect.w), &(livesLabelRect.h));
	SDL_FreeSurface(surf);

	surf = TTF_RenderText_Solid(bigFont, "Game Over", textColor);
	SDL_Texture* gameOverTexture = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_Rect gameOverRect{ 0, 0, 0, 0 };
	SDL_QueryTexture(gameOverTexture, NULL, NULL, &(gameOverRect.w), &(gameOverRect.h));
	SDL_FreeSurface(surf);
	gameOverRect.x = (SCREEN_WIDTH - gameOverRect.w) / 2;
	gameOverRect.y = (SCREEN_HEIGHT - gameOverRect.h) / 2;

	surf = TTF_RenderText_Solid(bigFont, "Press Spacebar to Play Again", textColor);
	SDL_Texture* playAgainTexture = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_Rect playAgainRect{ 0, 0, 0, 0 };
	SDL_QueryTexture(playAgainTexture, NULL, NULL, &(playAgainRect.w), &(playAgainRect.h));
	SDL_FreeSurface(surf);
	playAgainRect.x = (SCREEN_WIDTH - playAgainRect.w) / 2;
	playAgainRect.y = (SCREEN_HEIGHT + gameOverRect.h + 100 - playAgainRect.h) / 2;

	surf = TTF_RenderText_Solid(bigFont, "Game Paused", textColor);
	SDL_Texture* gamePauseTexture = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_Rect gamePauseRect{ 0, 0, 0, 0 };
	SDL_QueryTexture(gamePauseTexture, NULL, NULL, &(gamePauseRect.w), &(gamePauseRect.h));
	SDL_FreeSurface(surf);
	gamePauseRect.x = (SCREEN_WIDTH - gamePauseRect.w) / 2;
	gamePauseRect.y = (SCREEN_HEIGHT - gamePauseRect.y) / 2;

	surf = TTF_RenderText_Solid(bigFont, "Press P to Unpause", textColor);
	SDL_Texture* pauseLabelTexture = SDL_CreateTextureFromSurface(renderer, surf);
	SDL_Rect pauseLabelRect{ 0, 0, 0, 0 };
	SDL_QueryTexture(pauseLabelTexture, NULL, NULL, &(pauseLabelRect.w), &(pauseLabelRect.h));
	SDL_FreeSurface(surf);
	pauseLabelRect.x = (SCREEN_WIDTH - pauseLabelRect.w) / 2;
	pauseLabelRect.y = (SCREEN_HEIGHT + gamePauseRect.h + 100 - pauseLabelRect.h) / 2;

	bool leftPressed = false;
	bool rightPressed = false;

	// measured in 1 / X seconds
	float timeDelta = (TARGET_MILLIS_PER_FRAME / (float)1000);
	std::cout << "time delta: " << timeDelta << std::endl;

	bool isGameOver = false;
	bool isGamePaused = false;
	bool isGameRunning = true;

	while (isGameRunning)
	{
		Uint32 start = SDL_GetTicks();

		SDL_Event e;
		while(SDL_PollEvent(&e) > 0)
		{
			if (e.type == SDL_QUIT)
			{
				SDL_Log("Qutting game at %i timestamp", e.quit.timestamp);
				isGameRunning = false;
			}
			// For 1 frame movement feels like it stalls then it continues moving smoothly afterwards
			else if (e.type == SDL_KEYDOWN)
			{
				if (e.key.keysym.sym == SDL_KeyCode::SDLK_LEFT)
				{
					//std::cout << "left pressed" << std::endl;
					leftPressed = true;
				}
				else if (e.key.keysym.sym == SDL_KeyCode::SDLK_RIGHT)
				{
					//std::cout << "right pressed" << std::endl;
					rightPressed = true;
				}

				if (isGameOver && e.key.keysym.sym == SDL_KeyCode::SDLK_SPACE)
				{
					ResetGame(&currentScore, &currentLives, &isGameOver, &ball, &xDirection, &yDirection);
					ResetPaddlePosition(&paddle);
					ResetBrickMap(bricksCollisionMap);
				}

				// toggle pause
				if (!isGameOver && e.key.keysym.sym == SDL_KeyCode::SDLK_p)
				{
					isGamePaused = !isGamePaused;
				}
			}
			else if (e.type == SDL_KEYUP)
			{
				if (e.key.keysym.sym == SDL_KeyCode::SDLK_LEFT)
				{
					//std::cout << "left released" << std::endl;
					leftPressed = false;
				}
				else if (e.key.keysym.sym == SDL_KeyCode::SDLK_RIGHT)
				{
					//std::cout << "right released" << std::endl;
					rightPressed = false;
				}
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

			if (ball.x + ball.w + xVel > SCREEN_WIDTH)
			{
				xDirection *= -1;
			}

			if (ball.y + ball.h + yVel > SCREEN_HEIGHT)
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

		// Move Objects
		{
			if (!isGameOver && !isGamePaused) {
				if (leftPressed)
				{
					paddle.x -= paddleSpeed * timeDelta;
				}
				else if (rightPressed)
				{
					paddle.x += paddleSpeed * timeDelta;
				}
			}

			if (!isGamePaused) {
				ball.x += xDirection * ballSpeed * timeDelta;
				ball.y += yDirection * ballSpeed * timeDelta;
			}
		}

		// RENDER UI
		{
			surf = TTF_RenderText_Solid(font, std::to_string(currentScore).c_str(), textColor);
			SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(renderer, surf);
			SDL_Rect scoreRect{ 0, 0, 0, 0 };
			SDL_QueryTexture(scoreTexture, NULL, NULL, &(scoreRect.w), &(scoreRect.h));
			SDL_FreeSurface(surf);

			surf = TTF_RenderText_Solid(font, std::to_string(currentLives).c_str(), textColor);
			SDL_Texture* livesTexture = SDL_CreateTextureFromSurface(renderer, surf);
			SDL_Rect livesRect{ 0, 0, 0, 0 };
			SDL_QueryTexture(livesTexture, NULL, NULL, &(livesRect.w), &(livesRect.h));
			SDL_FreeSurface(surf);

			scoreRect.x = scoreLabelRect.x + scoreLabelRect.w;
			livesRect.x = SCREEN_WIDTH - livesRect.w;
			livesLabelRect.x = SCREEN_WIDTH - livesLabelRect.w - livesRect.w;

			if (isGameOver) {
				SDL_RenderCopy(renderer, gameOverTexture, NULL, &gameOverRect);
				SDL_RenderCopy(renderer, playAgainTexture, NULL, &playAgainRect);
			}

			if (isGamePaused) {
				SDL_RenderCopy(renderer, gamePauseTexture, NULL, &gamePauseRect);
				SDL_RenderCopy(renderer, pauseLabelTexture, NULL, &pauseLabelRect);
			}

			SDL_RenderCopy(renderer, scoreLabelTexture, NULL, &scoreLabelRect);
			SDL_RenderCopy(renderer, livesLabelTexture, NULL, &livesLabelRect);
			SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreRect);
			SDL_RenderCopy(renderer, livesTexture, NULL, &livesRect);
		}

		// Render updates

		drawBricks(renderer, bricks, bricksCollisionMap, BRICK_COLORS, LAYERS, BRICKS_PER_LAYER);

		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderFillRect(renderer, &paddle);
		SDL_RenderFillRect(renderer, &ball);

		SDL_RenderPresent(renderer);
		// Clear front buffer so that the back buffer can be drawn on a fresh front buffer
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);

		// Sleep if processing this current iteration of update loop too fast
		Uint32 end = SDL_GetTicks();
		Uint32 tickProcessTime = end - start;

		// if hardware processed this loop iteration faster than target FPS, sleep the remaining time in milliseconds
		if (tickProcessTime < TARGET_MILLIS_PER_FRAME)
		{
			Uint32 sleepTime = TARGET_MILLIS_PER_FRAME - tickProcessTime;
			timeDelta = ((tickProcessTime + sleepTime) / (float)1000);
			SDL_Delay(sleepTime);
		}
		else
		{
			timeDelta = tickProcessTime / (float)1000;
		}
	}

	// Possible improvement: Use templates to create an ellipsis function that 
	// will recursively destroy all resources instantiated in the heap
	SDL_DestroyTexture(gameOverTexture);
	SDL_DestroyTexture(gamePauseTexture);
	SDL_DestroyTexture(livesLabelTexture);
	SDL_DestroyTexture(pauseLabelTexture);
	SDL_DestroyTexture(playAgainTexture);
	SDL_DestroyTexture(scoreLabelTexture);

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	renderer = NULL;
	window = NULL;

	TTF_CloseFont(font);
	TTF_CloseFont(bigFont);
	TTF_Quit();
	SDL_Quit();

	return 0;
}