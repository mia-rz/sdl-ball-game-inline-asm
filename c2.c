#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h>  // For circle drawing and drawing primitives
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ----- Constants ----- */
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define PADDLE_WIDTH 100
#define PADDLE_HEIGHT 20
#define BALL_SIZE 40
#define BUTTON_WIDTH 75
#define BUTTON_HEIGHT 24
#define SLIDER_WIDTH 200
#define SLIDER_HEIGHT 20
#define FPS 60

/* Colors (RGBA) */
SDL_Color WHITE = {255, 255, 255, 255};
SDL_Color BLACK = {0, 0, 0, 255};
SDL_Color RED   = {255, 0, 0, 255};
SDL_Color BLUE  = {0, 0, 255, 255};
SDL_Color GREEN = {0, 255, 0, 255};
SDL_Color GRAY  = {200, 200, 200, 255};
SDL_Color DARK_GRAY = {100, 100, 100, 255};

/* Global SDL objects */
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
TTF_Font *fontSmall = NULL;
TTF_Font *fontLarge = NULL;

/* ----- Utility function to draw text ----- */
void drawText(const char *text, int x, int y, SDL_Color color, TTF_Font *font, SDL_Renderer *renderer) {
    SDL_Surface *surf = TTF_RenderText_Solid(font, text, color);
    if (!surf) {
        fprintf(stderr, "TTF_RenderText_Solid Error: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surf);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
        SDL_FreeSurface(surf);
        return;
    }
    SDL_Rect dst;
    dst.x = x;
    dst.y = y;
    dst.w = surf->w;
    dst.h = surf->h;
    SDL_FreeSurface(surf);
    SDL_RenderCopy(renderer, texture, NULL, &dst);
    SDL_DestroyTexture(texture);
}

/* ----- Button Struct and Functions ----- */
typedef struct {
    SDL_Rect rect;
    char text[32];
    SDL_Color color;
    SDL_Color hover_color;
    SDL_Color current_color;
} Button;

void Button_init(Button *btn, int x, int y, int w, int h,
                 const char *text,
                 SDL_Color color,
                 SDL_Color hover_color) {
    btn->rect.x = x;
    btn->rect.y = y;
    btn->rect.w = w;
    btn->rect.h = h;
    strncpy(btn->text, text, sizeof(btn->text)-1);
    btn->color = color;
    btn->hover_color = hover_color;
    btn->current_color = color;
}

void Button_draw(Button *btn, SDL_Renderer *renderer, TTF_Font *font) {
    // Draw rectangle
    SDL_SetRenderDrawColor(renderer, btn->current_color.r, btn->current_color.g, btn->current_color.b, btn->current_color.a);
    SDL_RenderFillRect(renderer, &btn->rect);
    // Draw text centered in button
    int textW, textH;
    TTF_SizeText(font, btn->text, &textW, &textH);
    int tx = btn->rect.x + (btn->rect.w - textW) / 2;
    int ty = btn->rect.y + (btn->rect.h - textH) / 2;
    drawText(btn->text, tx, ty, BLACK, font, renderer);
}

int Button_isHovered(Button *btn, int mouseX, int mouseY) {
    return (mouseX >= btn->rect.x && mouseX <= (btn->rect.x + btn->rect.w) &&
            mouseY >= btn->rect.y && mouseY <= (btn->rect.y + btn->rect.h));
}

/* ----- Slider Struct and Functions ----- */
typedef struct {
    SDL_Rect rect;
    float min_val;
    float max_val;
    float val;
    int dragging;
} Slider;

void Slider_init(Slider *s, int x, int y, int w, int h, float min_val, float max_val, float initial_val) {
    s->rect.x = x;
    s->rect.y = y;
    s->rect.w = w;
    s->rect.h = h;
    s->min_val = min_val;
    s->max_val = max_val;
    s->val = initial_val;
    s->dragging = 0;
}

void Slider_draw(Slider *s, SDL_Renderer *renderer) {
    // Draw slider track
    SDL_SetRenderDrawColor(renderer, GRAY.r, GRAY.g, GRAY.b, GRAY.a);
    SDL_RenderFillRect(renderer, &s->rect);
    // Calculate handle position
    float ratio = (s->val - s->min_val) / (s->max_val - s->min_val);
    int handle_x = s->rect.x + (int)(ratio * s->rect.w);
    // Draw handle as a circle
    filledCircleRGBA(renderer, handle_x, s->rect.y + s->rect.h/2, s->rect.h/2, BLUE.r, BLUE.g, BLUE.b, BLUE.a);
}

void Slider_update(Slider *s, int mouseX) {
    if (s->dragging) {
        float ratio = (float)(mouseX - s->rect.x) / s->rect.w;
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        s->val = s->min_val + ratio * (s->max_val - s->min_val);
    }
}

/* ----- Paddle Struct and Functions ----- */
typedef struct {
    SDL_Rect rect;
    float speed;
    SDL_Color color;
    int trajectory_type;  // 0: straight, 1: convex, 2: sinusoidal
} Paddle;

void Paddle_init(Paddle *p, int x, int y, int w, int h, float speed, SDL_Color color, int trajectory_type) {
    p->rect.x = x;
    p->rect.y = y;
    p->rect.w = w;
    p->rect.h = h;
    p->speed = speed;
    p->color = color;
    p->trajectory_type = trajectory_type;
}

void Paddle_move(Paddle *p, int dx) {
    p->rect.x += (int)(dx * p->speed);
    if (p->rect.x < 0)
        p->rect.x = 0;
    if (p->rect.x + p->rect.w > SCREEN_WIDTH)
        p->rect.x = SCREEN_WIDTH - p->rect.w;
}

void Paddle_draw(Paddle *p, SDL_Renderer *renderer) {
    SDL_SetRenderDrawColor(renderer, p->color.r, p->color.g, p->color.b, p->color.a);
    SDL_RenderFillRect(renderer, &p->rect);
}

/* ----- Ball Struct and Functions ----- */
typedef struct {
    SDL_Rect rect;
    int size;
    float dx, dy;
    int trajectory_type;  // 0: straight, 1: convex, 2: sinusoidal
    float gravity;
    float base_dy;
    float base_speed;
    float time;
    float loc_sinusoidal[2];
    float amplitude;
    float frequency;
    float speed;
    float color_angle;  // used for computing the ball's dynamic color

    // ---- New fields for rotation ----
    float spinAngle;   // current rotation angle (in degrees)
    float spinSpeed;   // constant rotation speed (degrees per frame)
} Ball;

void Ball_init(Ball *b, int x, int y, int size) {
    b->size = size;
    b->rect.w = size;
    b->rect.h = size;
    b->rect.x = x - size/2;
    b->rect.y = y - size/2;
    b->dx = (rand() % 2 == 0) ? -5 : 5;
    b->dy = (rand() % 2 == 0) ? -5 : 5;
    b->trajectory_type = 0;
    b->gravity = 0.5f;
    b->base_dy = b->dy;
        // Uint64 start_time = SDL_GetPerformanceCounter();
    b->base_speed = sqrtf(b->dx * b->dx + b->dy * b->dy);
    //         Uint64 end_time = SDL_GetPerformanceCounter();
    // double elapsed = (double)(end_time - start_time) / SDL_GetPerformanceFrequency();
    // printf("Execution time (Ball_init): %.9f seconds\n", elapsed);
    b->time = 0;
    b->loc_sinusoidal[0] = 0;
    b->loc_sinusoidal[1] = 0;
    b->amplitude = 30;
    b->frequency = 0.02f;
    b->speed = 5;
    b->color_angle = 0;

    // Initialize rotation: always rotating at a constant speed.

{
    b->spinAngle = 0;
    b->spinSpeed = 5.0f;

    }
}

void Ball_reset(Ball *b) {
    b->rect.x = SCREEN_WIDTH/2 - b->size/2;
    b->rect.y = SCREEN_HEIGHT/2 - b->size/2;
    b->dx = (rand() % 2 == 0) ? -5 : 5;
    b->dy = (rand() % 2 == 0) ? -5 : 5;
    b->trajectory_type = 0;
    b->gravity = 0.5f;
    b->base_dy = b->dy;
    float sum_of_squares = b->dx * b->dx + b->dy * b->dy;
    b->base_speed = sqrtf(sum_of_squares);
    b->time = 0;
    b->loc_sinusoidal[0] = b->rect.x;
    b->loc_sinusoidal[1] = b->rect.y;
    b->color_angle = 0;

    // Reset rotation parameters as well.
    b->spinAngle = 0;
    b->spinSpeed = 5.0f;
}

void Ball_update(Ball *b) {
     Uint64 start_time = SDL_GetPerformanceCounter();
    float real_speed = sqrtf(b->dx * b->dx + b->dy * b->dy);
    float move_dx = 0, move_dy = 0;
    if (b->trajectory_type == 0) {
        // Straight trajectory
        move_dx = b->dx / real_speed * b->speed;
        move_dy = b->dy / real_speed * b->speed;
        b->rect.x += (int)move_dx;
        b->rect.y += (int)move_dy;

    }
    else if (b->trajectory_type == 1) {
        // Convex trajectory with gravity
        b->dy += b->gravity;
        move_dx = b->dx / real_speed * b->speed * 1.2f;
        move_dy = b->dy / real_speed * b->speed;
        b->rect.x += (int)move_dx;
        b->rect.y += (int)(0.5f * move_dy);
    }
    else if (b->trajectory_type == 2) {
        // Sinusoidal trajectory
        move_dx = b->dx / real_speed * b->speed;
        move_dy = b->dy / real_speed * b->speed;
        float speed_val = sqrtf(move_dx * move_dx + move_dy * move_dy);
        b->time += speed_val;
        float new_sine = sinf(b->frequency * b->time) * b->amplitude;
        float dir_x = move_dx / speed_val;
        float dir_y = move_dy / speed_val;
        b->loc_sinusoidal[0] += move_dx;
        b->loc_sinusoidal[1] += move_dy;
        b->rect.x = (int)(b->loc_sinusoidal[0] - dir_y * new_sine);
        b->rect.y = (int)(b->loc_sinusoidal[1] + dir_x * new_sine);
    }

    // Bounce off left/right walls
    if (b->rect.x <= 0 || (b->rect.x + b->rect.w) >= SCREEN_WIDTH) {
        b->dx = -b->dx;
    }
    Uint64 end_time = SDL_GetPerformanceCounter();
    double elapsed = (double)(end_time - start_time) / SDL_GetPerformanceFrequency();
    printf("Execution time (Ball_update): %.9f seconds\n", elapsed);
    
    // Update color rotation angle (using the angle of movement)
    float angle = atan2f(move_dy, move_dx);
    b->color_angle += angle * b->speed;
    while(b->color_angle < 0)
        b->color_angle += 360;
    while(b->color_angle >= 360)
        b->color_angle -= 360;

    // ---- Update the spin (rotation) regardless of trajectory ----
    b->spinAngle += b->spinSpeed;
    while(b->spinAngle < 0)
        b->spinAngle += 360;
    while(b->spinAngle >= 360)
        b->spinAngle -= 360;
}

void Ball_draw(Ball *b, SDL_Renderer *renderer) {
    // --- Render the ball onto an offscreen texture so that we can rotate it ---
    // Create a texture with target access to draw the ball with stripes.
    SDL_Texture *ballTexture = SDL_CreateTexture(renderer,
                              SDL_PIXELFORMAT_RGBA8888,
                              SDL_TEXTUREACCESS_TARGET,
                              b->size, b->size);
    if (!ballTexture) {
        fprintf(stderr, "SDL_CreateTexture Error: %s\n", SDL_GetError());
        return;
    }
    SDL_SetTextureBlendMode(ballTexture, SDL_BLENDMODE_BLEND);

    // Save the current render target.
    SDL_Texture *oldTarget = SDL_GetRenderTarget(renderer);
    // Set the new texture as the render target.
    SDL_SetRenderTarget(renderer, ballTexture);
    // Clear the texture (make it fully transparent).
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);

    // Compute the dynamic color components (for the stripes) based on the ball's color_angle.
    int radius = b->size / 2;
    float angle_rad = b->color_angle * M_PI / 180.0f;
    Uint8 main_r   = (Uint8)(127 + 128 * sinf(angle_rad));
    Uint8 main_g   = (Uint8)(127 + 128 * sinf(angle_rad + 2 * M_PI / 3));
    Uint8 main_b   = (Uint8)(127 + 128 * sinf(angle_rad + 4 * M_PI / 3));
    // A second, darker color for stripes.
    Uint8 stripe_r = main_r / 2;
    Uint8 stripe_g = main_g / 2;
    Uint8 stripe_b = main_b / 2;
    int stripeHeight = 4; // Height in pixels for each stripe

    // Draw the striped ball onto the texture.
    // The texture coordinates have (0,0) at the top-left; we treat the center as (radius, radius).
    for (int y = -radius; y <= radius; y++) {
        int x_span = (int)sqrt(radius * radius - y * y);
        int stripe_index = ((y + radius) / stripeHeight) % 2;
        Uint8 r = (stripe_index == 0) ? main_r : stripe_r;
        Uint8 g = (stripe_index == 0) ? main_g : stripe_g;
        Uint8 b_color = (stripe_index == 0) ? main_b : stripe_b;
        // Draw a horizontal line at y (offset by radius to convert to texture coordinates).
        boxRGBA(renderer, radius - x_span, radius + y, radius + x_span, radius + y, r, g, b_color, 255);
    }

    // Reset the render target back to the default.
    SDL_SetRenderTarget(renderer, oldTarget);

    // Now draw the ball texture with rotation applied.
    SDL_Rect dstRect = b->rect; // Destination rectangle on the screen
    SDL_Point center = { radius, radius };  // Rotate about the texture center
    SDL_RenderCopyEx(renderer, ballTexture, NULL, &dstRect, b->spinAngle, &center, SDL_FLIP_NONE);

    // Destroy the temporary texture.
    SDL_DestroyTexture(ballTexture);
}

/* ----- Collision helper ----- */
int rects_collide(SDL_Rect *a, SDL_Rect *b) {
    return SDL_HasIntersection(a, b);
}

/* ----- show_menu function ----- */
int show_menu() {
    // There are four buttons: three difficulty choices and one PLAY button.
    Button menu_buttons[4];
    int mid_x = SCREEN_WIDTH/2;
    Button_init(&menu_buttons[0], mid_x - BUTTON_WIDTH/2, 200, BUTTON_WIDTH, BUTTON_HEIGHT, "EASY", GRAY, GREEN);
    Button_init(&menu_buttons[1], mid_x - BUTTON_WIDTH/2, 280, BUTTON_WIDTH, BUTTON_HEIGHT, "MEDIUM", GRAY, GREEN);
    Button_init(&menu_buttons[2], mid_x - BUTTON_WIDTH/2, 360, BUTTON_WIDTH, BUTTON_HEIGHT, "HARD", GRAY, GREEN);
    Button_init(&menu_buttons[3], mid_x - BUTTON_WIDTH/2, 450, BUTTON_WIDTH, BUTTON_HEIGHT, "PLAY", BLUE, DARK_GRAY);

    int difficulty = -1;
    int running = 1;
    SDL_Event event;

    while (running) {
        // Clear screen
        SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
        SDL_RenderClear(renderer);

        // Draw title
        {
            const char *title = "BREAKOUT GAME";
            int textW, textH;
            TTF_SizeText(fontLarge, title, &textW, &textH);
            drawText(title, SCREEN_WIDTH/2 - textW/2, 100 - textH/2, WHITE, fontLarge, renderer);
        }
        // Draw "Select Difficulty:"
        {
            const char *diffStr = "Select Difficulty:";
            int textW, textH;
            TTF_SizeText(fontSmall, diffStr, &textW, &textH);
            drawText(diffStr, SCREEN_WIDTH/2 - textW/2, 160 - textH/2, WHITE, fontSmall, renderer);
        }

        // Get mouse position
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        // Draw buttons and update hover colors
        for (int i = 0; i < 4; i++) {
            if (Button_isHovered(&menu_buttons[i], mouseX, mouseY))
                menu_buttons[i].current_color = menu_buttons[i].hover_color;
            else
                menu_buttons[i].current_color = menu_buttons[i].color;
            Button_draw(&menu_buttons[i], renderer, fontSmall);
        }

        SDL_RenderPresent(renderer);

        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                return -1;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    for (int i = 0; i < 4; i++) {
                        if (Button_isHovered(&menu_buttons[i], event.button.x, event.button.y)) {
                            if (i < 3) { // Difficulty buttons
                                difficulty = i;
                                // Highlight selected difficulty: set all to GRAY and selected one to GREEN
                                for (int j = 0; j < 3; j++)
                                    menu_buttons[j].color = GRAY;
                                menu_buttons[i].color = GREEN;
                            } else if (i == 3 && difficulty != -1) { // Play button and difficulty selected
                                return difficulty;
                            }
                        }
                    }
                }
            }
        }
        SDL_Delay(1000 / FPS);
    }
    return -1;
}

/* ----- main_game function ----- */
void main_game(int difficulty) {
    /* 
     * Set opponent parameters based on difficulty.
     * The opponent_trajectories array holds possible trajectory types.
     */
    float opponent_speed;
    int opponent_trajectories[3];
    int num_trajectories = 0;
    if (difficulty == 0) { // Easy
        opponent_speed = 3.0f;
        opponent_trajectories[0] = 0;
        num_trajectories = 1;
    }
    else if (difficulty == 1) { // Medium
        opponent_speed = 4.0f;
        opponent_trajectories[0] = 0;
        opponent_trajectories[1] = 1;
        num_trajectories = 2;
    }
    else { // Hard
        opponent_speed = 6.0f;
        opponent_trajectories[0] = 0;
        opponent_trajectories[1] = 1;
        opponent_trajectories[2] = 2;
        num_trajectories = 3;
    }

    // Create buttons for changing ball trajectory.
    Button buttons[3];
    Button_init(&buttons[0], 20, SCREEN_HEIGHT - BUTTON_HEIGHT - 10, BUTTON_WIDTH, BUTTON_HEIGHT, "Straight", GRAY, GREEN);
    Button_init(&buttons[1], 100, SCREEN_HEIGHT - BUTTON_HEIGHT - 10, BUTTON_WIDTH, BUTTON_HEIGHT, "Convex", GRAY, GREEN);
    Button_init(&buttons[2], 180, SCREEN_HEIGHT - BUTTON_HEIGHT - 10, BUTTON_WIDTH, BUTTON_HEIGHT, "Sine", GRAY, GREEN);
    // Back button
    Button back_button;
    Button_init(&back_button, SCREEN_WIDTH - BUTTON_WIDTH - 20, 20, BUTTON_WIDTH, BUTTON_HEIGHT, "Back", RED, DARK_GRAY);
    // Speed slider
    Slider speed_slider;
    Slider_init(&speed_slider, 400, SCREEN_HEIGHT - SLIDER_HEIGHT - 10, SLIDER_WIDTH, SLIDER_HEIGHT, 1, 30, 8);

    // Create paddles and ball
    Paddle player, opponent;
    Paddle_init(&player, SCREEN_WIDTH/2 - PADDLE_WIDTH/2, SCREEN_HEIGHT - 50, PADDLE_WIDTH, PADDLE_HEIGHT, 10, BLUE, 0);
    Paddle_init(&opponent, SCREEN_WIDTH/2 - PADDLE_WIDTH/2, 30, PADDLE_WIDTH, PADDLE_HEIGHT, 10, RED, 0);
    Ball ball;
    Ball_init(&ball, SCREEN_WIDTH/2, SCREEN_HEIGHT/2, BALL_SIZE);

    // Score variables
    int player_score = 0;
    int opponent_score = 0;

    int running = 1;
    SDL_Event event;
    const Uint8 *keystate = SDL_GetKeyboardState(NULL);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
                return;
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    // Check trajectory buttons
                    for (int i = 0; i < 3; i++) {
                        if (Button_isHovered(&buttons[i], event.button.x, event.button.y)) {
                            player.trajectory_type = i;
                            break;
                        }
                    }
                    // Check back button
                    if (Button_isHovered(&back_button, event.button.x, event.button.y)) {
                        return; // Back to main menu
                    }
                    // Check slider (if mouse inside slider rect)
                    if (event.button.x >= speed_slider.rect.x && event.button.x <= speed_slider.rect.x + speed_slider.rect.w &&
                        event.button.y >= speed_slider.rect.y && event.button.y <= speed_slider.rect.y + speed_slider.rect.h) {
                        speed_slider.dragging = 1;
                    }
                }
            }
            else if (event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    speed_slider.dragging = 0;
                }
            }
            else if (event.type == SDL_MOUSEMOTION) {
                if (speed_slider.dragging) {
                    Slider_update(&speed_slider, event.motion.x);
                }
            }
        }

        // Update button hover states
        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);
        for (int i = 0; i < 3; i++) {
            if (Button_isHovered(&buttons[i], mouseX, mouseY))
                buttons[i].current_color = buttons[i].hover_color;
            else
                buttons[i].current_color = buttons[i].color;
        }
        if (Button_isHovered(&back_button, mouseX, mouseY))
            back_button.current_color = back_button.hover_color;
        else
            back_button.current_color = back_button.color;

        // Update ball speed from slider
        ball.speed = speed_slider.val;

        // Player movement using keyboard (Left/Right arrows)
        if (keystate[SDL_SCANCODE_LEFT]) {
            Paddle_move(&player, -1);
        }
        if (keystate[SDL_SCANCODE_RIGHT]) {
            Paddle_move(&player, 1);
        }

        // ---- Improved Opponent Movement ----
        int opp_center = opponent.rect.x + opponent.rect.w/2;
        int ball_center = ball.rect.x + ball.rect.w/2;
        int diff = ball_center - opp_center;
        if (abs(diff) > 10) {
            if (diff > 0) {
                Paddle_move(&opponent, 1);
            } else {
                Paddle_move(&opponent, -1);
            }
        }
        // ---- End Improved Opponent Movement ----

        // Update ball position
        Ball_update(&ball);

        // ---- Ball and Paddle Collision Handling with Direction Change ----
        // When the ball collides with a paddle, compute a hit factor based on where on the paddle it struck.
        // This factor is then mapped to an angle (max 75°) that sets the new dx and dy.
        // In addition, we update the ball's spin direction based on the hit.
        if (rects_collide(&ball.rect, &player.rect)) {
            // Compute hit factor (-1 to 1)
            float hitPos = ((ball.rect.x + ball.rect.w/2) - (player.rect.x + player.rect.w/2)) / ((float)player.rect.w/2);
            if(hitPos < -1) hitPos = -1;
            if(hitPos > 1) hitPos = 1;
            float maxAngle = 75 * M_PI / 180;
            float angle = hitPos * maxAngle;
            ball.dx = ball.speed * sinf(angle);
            ball.dy = -ball.speed * cosf(angle);  // upward movement
            ball.trajectory_type = player.trajectory_type;
            if (ball.trajectory_type == 1) {  // Convex: reset gravity for convex
                ball.gravity = -0.5f;
            }
            if (ball.trajectory_type == 2) {
                ball.loc_sinusoidal[0] = ball.rect.x;
                ball.loc_sinusoidal[1] = ball.rect.y;
                ball.time = 0;
            }
            ball.base_dy = ball.dy;
            // ---- Update spin based on hit direction ----
            // If hit from the right (hitPos positive), spin clockwise; if left, spin counter-clockwise.
            if(hitPos != 0)
                ball.spinSpeed = 5.0f * (hitPos > 0 ? 1 : -1);
        }
        else if (rects_collide(&ball.rect, &opponent.rect)) {
            float hitPos = ((ball.rect.x + ball.rect.w/2) - (opponent.rect.x + opponent.rect.w/2)) / ((float)opponent.rect.w/2);
            if(hitPos < -1) hitPos = -1;
            if(hitPos > 1) hitPos = 1;
            float maxAngle = 75 * M_PI / 180;
            float angle = hitPos * maxAngle;
            ball.dx = ball.speed * sinf(angle);
            ball.dy = ball.speed * cosf(angle);  // downward movement
            int randIndex = rand() % num_trajectories;
            ball.trajectory_type = opponent_trajectories[randIndex];
            if (ball.trajectory_type == 1) {
                ball.gravity = 0.5f;
            }
            if (ball.trajectory_type == 2) {
                ball.loc_sinusoidal[0] = ball.rect.x;
                ball.loc_sinusoidal[1] = ball.rect.y;
                ball.time = 0;
            }
            ball.base_dy = ball.dy;
            // ---- Update spin based on hit direction ----
            if(hitPos != 0)
                ball.spinSpeed = 5.0f * (hitPos > 0 ? 1 : -1);
        }
        // ---- End Collision Handling ----

        // Check scoring
        if (ball.rect.y >= SCREEN_HEIGHT) {
            opponent_score++;
            Ball_reset(&ball);
        }
        else if (ball.rect.y + ball.rect.h <= 0) {
            player_score++;
            Ball_reset(&ball);
        }

        // Drawing
        SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
        SDL_RenderClear(renderer);

        Paddle_draw(&player, renderer);
        Paddle_draw(&opponent, renderer);
        Ball_draw(&ball, renderer);

        // Draw buttons
        for (int i = 0; i < 3; i++) {
            buttons[i].current_color = GRAY;
            if (i == player.trajectory_type)
                buttons[i].current_color = GREEN;
            Button_draw(&buttons[i], renderer, fontSmall);
        }
        Button_draw(&back_button, renderer, fontSmall);
        Slider_draw(&speed_slider, renderer);

        // Display scores (centered)
        char scoreText[32];
        sprintf(scoreText, "%d", player_score);
        int textW, textH;
        TTF_SizeText(fontLarge, scoreText, &textW, &textH);
        drawText(scoreText, SCREEN_WIDTH/2 - 100 - textW/2, SCREEN_HEIGHT/2 - textH/2, WHITE, fontLarge, renderer);
        sprintf(scoreText, "%d", opponent_score);
        TTF_SizeText(fontLarge, scoreText, &textW, &textH);
        drawText(scoreText, SCREEN_WIDTH/2 + 50 - textW/2, SCREEN_HEIGHT/2 - textH/2, WHITE, fontLarge, renderer);

        SDL_RenderPresent(renderer);
        SDL_Delay(1000 / FPS);
    }
}

/* ----- Main Function ----- */
int main(int argc, char *argv[]) {

    (void)argc; (void)argv;
    srand((unsigned int)time(NULL));

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF_Init Error: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }

    window = SDL_CreateWindow("Breakout-like Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED /*| SDL_RENDERER_PRESENTVSYNC*/);
    if (!renderer) {
        SDL_DestroyWindow(window);
        fprintf(stderr, "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load fonts (adjust path and size as needed)
    fontSmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12);
    fontLarge = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 64);
    if (!fontSmall || !fontLarge) {
        fprintf(stderr, "TTF_OpenFont Error: %s\n", TTF_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Main game loop: show menu then start game
    while (1) {
        int difficulty = show_menu();
        if (difficulty == -1)
            break;
        main_game(difficulty);
    }


    // Cleanup
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontLarge);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();



    return 0;



}
