#include <SDL2/SDL.h>
#include <SDL2/SDL2_gfxPrimitives.h> 
#include <SDL2/SDL_ttf.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>


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


SDL_Color WHITE = {255, 255, 255, 255};
SDL_Color BLACK = {0, 0, 0, 255};
SDL_Color RED   = {255, 0, 0, 255};
SDL_Color BLUE  = {0, 0, 255, 255};
SDL_Color GREEN = {0, 255, 0, 255};
SDL_Color GRAY  = {200, 200, 200, 255};
SDL_Color DARK_GRAY = {100, 100, 100, 255};


SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
TTF_Font *fontSmall = NULL;
TTF_Font *fontLarge = NULL;


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
    SDL_SetRenderDrawColor(renderer, btn->current_color.r, btn->current_color.g, btn->current_color.b, btn->current_color.a);
    SDL_RenderFillRect(renderer, &btn->rect);
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

    SDL_SetRenderDrawColor(renderer, GRAY.r, GRAY.g, GRAY.b, GRAY.a);
    SDL_RenderFillRect(renderer, &s->rect);
    float ratio = (s->val - s->min_val) / (s->max_val - s->min_val);
    int handle_x = s->rect.x + (int)(ratio * s->rect.w);
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

typedef struct {
    SDL_Rect rect;
    float speed;
    SDL_Color color;
    int trajectory_type; 
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

typedef struct {
    SDL_Rect rect;
    int size;
    float dx, dy;
    int trajectory_type;  
    float gravity;
    float base_dy;
    float base_speed;
    float time;
    float loc_sinusoidal[2];
    float amplitude;
    float frequency;
    float speed;
    float color_angle;  

    float spinAngle;   
    float spinSpeed;  
} Ball;

void Ball_init(Ball *b, int x, int y, int size) {
    b->size = size;
    b->rect.w = size;
    b->rect.h = size;
    b->rect.x = x - size / 2;
    b->rect.y = y - size / 2;
    b->dx = (rand() % 2 == 0) ? -5 : 5;
    b->dy = (rand() % 2 == 0) ? -5 : 5;
    b->trajectory_type = 0;
    b->gravity = 0.5f;
    b->base_dy = b->dy;
    // b->base_speed = sqrtf(b->dx * b->dx + b->dy * b->dy);
    {
        // SIMD version using SSE instructions
        float dx = b->dx, dy = b->dy, base_speed;
        __asm__ volatile (
            "movss %[dx], %%xmm0\n\t"        // Load dx into xmm0 (only the lowest element)
            "movss %[dy], %%xmm1\n\t"        // Load dy into xmm1 (only the lowest element)
            "unpcklps %%xmm1, %%xmm0\n\t"     // Combine: xmm0 = {dx, dy}
            "mulps %%xmm0, %%xmm0\n\t"        // Multiply elements: xmm0 = {dx*dx, dy*dy}
            "haddps %%xmm0, %%xmm0\n\t"       // Horizontal add: xmm0 = {dx*dx + dy*dy, ...}
            "sqrtss %%xmm0, %%xmm0\n\t"       // Compute square root: xmm0 = sqrt(dx*dx + dy*dy)
            "movss %%xmm0, %[base_speed]\n\t" // Store the result in base_speed
            : [base_speed] "=m" (base_speed)
            : [dx] "m" (dx), [dy] "m" (dy)
            : "xmm0", "xmm1"
        );
        // Optionally, assign the computed base_speed to b->base_speed
        // b->base_speed = base_speed;
    }
    b->time = 0;
    b->loc_sinusoidal[0] = 0;
    b->loc_sinusoidal[1] = 0;
    b->amplitude = 30;
    b->frequency = 0.02f;
    b->speed = 5;
    b->color_angle = 0;

    // Initialize rotation: always rotating at a constant speed.
    b->spinAngle = 0;
    b->spinSpeed = 5.0f;  // Default spin speed (degrees per frame)
}


void Ball_reset(Ball *b) {
    // Set ball position to the center of the screen.
    b->rect.x = SCREEN_WIDTH / 2 - b->size / 2;
    b->rect.y = SCREEN_HEIGHT / 2 - b->size / 2;

    // Initialize ball velocity in x and y directions randomly.
    b->dx = (rand() % 2 == 0) ? -5 : 5;
    b->dy = (rand() % 2 == 0) ? -5 : 5;
    
    b->trajectory_type = 0;
    b->gravity = 0.5f;
    b->base_dy = b->dy;
    
    // Calculate the base speed as the Euclidean norm: sqrt(dx*dx + dy*dy)
    float dx = b->dx, dy = b->dy, base_speed;
    __asm__ volatile (
        // Load dx into the lower part of xmm0.
        "movss %[dx], %%xmm0\n\t"
        // Load dy into the lower part of xmm1.
        "movss %[dy], %%xmm1\n\t"
        // Interleave xmm0 and xmm1 so that xmm0 now contains {dx, dy, ?, ?}.
        "unpcklps %%xmm1, %%xmm0\n\t"
        // Multiply the vector by itself: {dx*dx, dy*dy, ?, ?}.
        "mulps %%xmm0, %%xmm0\n\t"
        // Horizontally add the two components: lower float becomes dx*dx + dy*dy.
        "haddps %%xmm0, %%xmm0\n\t"
        // Compute the square root of the sum (only the lower float is used).
        "sqrtss %%xmm0, %%xmm0\n\t"
        // Store the result into the variable base_speed.
        "movss %%xmm0, %[base_speed]\n\t"
        : [base_speed] "=m" (base_speed)        // output operand
        : [dx] "m" (dx), [dy] "m" (dy)           // input operands
        : "xmm0", "xmm1"                        // clobbered registers
    );
    b->base_speed = base_speed;

    b->time = 0;
    b->loc_sinusoidal[0] = b->rect.x;
    b->loc_sinusoidal[1] = b->rect.y;
    b->color_angle = 0;

    // Reset rotation parameters.
    b->spinAngle = 0;
    b->spinSpeed = 5.0f;
}


void Ball_update(Ball *b) {
   Uint64 start_time = SDL_GetPerformanceCounter();


    // using SSE packed instructions to load both dx and dy in parallel.
    float real_speed;
    __asm__ volatile (
        "movlps %[dxy], %%xmm0\n\t"         // Load b->dx and b->dy into xmm0 (assumes they are consecutive)
        "mulps %%xmm0, %%xmm0\n\t"            // Square both elements: [dx^2, dy^2]
        "haddps %%xmm0, %%xmm0\n\t"           // Horizontally add: xmm0[0] = dx^2 + dy^2
        "sqrtss %%xmm0, %%xmm0\n\t"           // Compute square root of the sum (only lower float)
        "movss %%xmm0, %[real_speed]\n\t"     // Store result into real_speed
        : [real_speed] "=m" (real_speed)
        : [dxy] "m" (b->dx)                  // b->dx and b->dy must be stored consecutively!
        : "xmm0"
    );

    float move_dx = 0, move_dy = 0;

    if (b->trajectory_type == 0) {
        // Trajectory type 0: Straight-line movement.
        // Compute move = ([dx, dy] / real_speed) * b->speed using SSE packed operations.
        float moves[2];
        __asm__ volatile (
            "movlps %[dxy], %%xmm0\n\t"          // Load b->dx and b->dy into xmm0
            "movss %[rs], %%xmm1\n\t"             // Load real_speed into xmm1
            "shufps $0x00, %%xmm1, %%xmm1\n\t"     // Broadcast real_speed to both lanes
            "divps %%xmm1, %%xmm0\n\t"            // Divide [dx, dy] by real_speed
            "movss %[bs], %%xmm1\n\t"             // Load b->speed into xmm1
            "shufps $0x00, %%xmm1, %%xmm1\n\t"     // Broadcast b->speed to both lanes
            "mulps %%xmm1, %%xmm0\n\t"            // Multiply the result by b->speed
            "movlps %%xmm0, %[moves]\n\t"         // Store the two results into the moves array
            : [moves] "=m" (moves)
            : [dxy] "m" (b->dx), [rs] "m" (real_speed), [bs] "m" (b->speed)
            : "xmm0", "xmm1"
        );
        move_dx = moves[0];
        move_dy = moves[1];

        // Update ball position
        b->rect.x += (int)move_dx;
        b->rect.y += (int)move_dy;
    }
    else if (b->trajectory_type == 1) {
        // Trajectory type 1: Convex trajectory with gravity.
        // First, update the vertical velocity by adding gravity.
        __asm__ volatile (
            "movss %[dy], %%xmm0\n\t"           // Load b->dy
            "addss %[grav], %%xmm0\n\t"          // Add b->gravity
            "movss %%xmm0, %[dy]\n\t"            // Store updated dy back to b->dy
            : [dy] "+m" (b->dy)
            : [grav] "m" (b->gravity)
            : "xmm0"
        );

        // Compute move = ([dx, updated dy] / real_speed) * b->speed,
        // then apply a constant factor (1.2) only to the X component.
        float moves[2];
        float factors[2] = { 1.2f, 1.0f }; // Multiply X by 1.2; leave Y unchanged.
        __asm__ volatile (
            "movlps %[dxy], %%xmm0\n\t"          // Load b->dx and updated b->dy into xmm0
            "movss %[rs], %%xmm1\n\t"             // Load real_speed
            "shufps $0x00, %%xmm1, %%xmm1\n\t"     // Broadcast real_speed to both lanes
            "divps %%xmm1, %%xmm0\n\t"            // Compute [dx/real_speed, dy/real_speed]
            "movss %[bs], %%xmm1\n\t"             // Load b->speed
            "shufps $0x00, %%xmm1, %%xmm1\n\t"     // Broadcast b->speed
            "mulps %%xmm1, %%xmm0\n\t"            // Multiply to get move vector
            "movaps %[factors], %%xmm1\n\t"       // Load factors vector [1.2, 1.0]
            "mulps %%xmm1, %%xmm0\n\t"            // Multiply element‐wise to adjust X component
            "movlps %%xmm0, %[moves]\n\t"         // Store the results into the moves array
            : [moves] "=m" (moves)
            : [dxy] "m" (b->dx), [rs] "m" (real_speed),
              [bs] "m" (b->speed), [factors] "m" (factors)
            : "xmm0", "xmm1"
        );
        move_dx = moves[0];
        move_dy = moves[1];

        // Update the ball's position:
        b->rect.x += (int)move_dx;
        {
            // Compute half of move_dy using scalar SSE multiplication.
            float half = 0.5f;
            float temp;
            __asm__ volatile (
                "movss %[mvy], %%xmm0\n\t"      // Load move_dy
                "mulss %[half], %%xmm0\n\t"      // Multiply by 0.5
                "movss %%xmm0, %[temp]\n\t"      // Store result in temp
                : [temp] "=m" (temp)
                : [mvy] "m" (move_dy), [half] "m" (half)
                : "xmm0"
            );
            b->rect.y += (int)temp;
        }
    }
    else if (b->trajectory_type == 2) {
        // Trajectory type 2: Sinusoidal (wavy) movement.
        float moves[2];
        // Compute move = ([dx, dy] / real_speed) * b->speed using SSE packed instructions.
        __asm__ volatile (
            "movlps %[dxy], %%xmm0\n\t"          // Load b->dx and b->dy into xmm0
            "movss %[rs], %%xmm1\n\t"             // Load real_speed
            "shufps $0x00, %%xmm1, %%xmm1\n\t"     // Broadcast real_speed to both lanes
            "divps %%xmm1, %%xmm0\n\t"            // Compute [dx/real_speed, dy/real_speed]
            "movss %[bs], %%xmm1\n\t"             // Load b->speed
            "shufps $0x00, %%xmm1, %%xmm1\n\t"     // Broadcast b->speed
            "mulps %%xmm1, %%xmm0\n\t"            // Multiply to get move vector
            "movlps %%xmm0, %[moves]\n\t"         // Store results into the moves array
            : [moves] "=m" (moves)
            : [dxy] "m" (b->dx), [rs] "m" (real_speed), [bs] "m" (b->speed)
            : "xmm0", "xmm1"
        );
        float move_dx = moves[0];
        float move_dy = moves[1];

        // Compute speed_val = sqrt(move_dx^2 + move_dy^2) using SSE packed instructions.
        float speed_val;
        __asm__ volatile (
            "movlps %[moves], %%xmm0\n\t"         // Load move_dx and move_dy into xmm0
            "mulps %%xmm0, %%xmm0\n\t"             // Square both components
            "haddps %%xmm0, %%xmm0\n\t"            // Horizontal add: move_dx^2 + move_dy^2
            "sqrtss %%xmm0, %%xmm0\n\t"            // Compute square root of the sum
            "movss %%xmm0, %[speed_val]\n\t"       // Store result into speed_val
            : [speed_val] "=m" (speed_val)
            : [moves] "m" (moves)
            : "xmm0"
        );

        // Update the ball's time using the computed speed value.
        b->time += speed_val;

        // Compute frequency * time (freq_time)
        float freq_time;
        __asm__ volatile (
            "movss %[freq], %%xmm0\n\t"           // Load b->frequency
            "mulss %[time], %%xmm0\n\t"           // Multiply by b->time
            "movss %%xmm0, %[freq_time]\n\t"      // Store the result in freq_time
            : [freq_time] "=m" (freq_time)
            : [freq] "m" (b->frequency), [time] "m" (b->time)
            : "xmm0"
        );

        // Compute new_sine = sin(freq_time) * b->amplitude using x87 fsin instruction.
        float new_sine;
        __asm__ volatile (
            "flds %[ft]\n\t"       // Load freq_time onto the x87 FPU stack
            "fsin\n\t"             // Compute sine, result in ST(0)
            "fmuls %[amp]\n\t"     // Multiply by b->amplitude
            "fstps %[new_sine]\n\t" // Store result in new_sine and pop from the FPU stack
            : [new_sine] "=m" (new_sine)
            : [ft] "m" (freq_time), [amp] "m" (b->amplitude)
            : "st"
        );

        // Compute the direction vector = move / speed_val using SSE packed instructions.
        float dirs[2];
        __asm__ volatile (
            "movlps %[moves], %%xmm0\n\t"         // Load move_dx and move_dy into xmm0
            "movss %[sv], %%xmm1\n\t"              // Load speed_val
            "shufps $0x00, %%xmm1, %%xmm1\n\t"      // Broadcast speed_val
            "divps %%xmm1, %%xmm0\n\t"             // Compute [move_dx/speed_val, move_dy/speed_val]
            "movlps %%xmm0, %[dirs]\n\t"           // Store direction components into dirs array
            : [dirs] "=m" (dirs)
            : [moves] "m" (moves), [sv] "m" (speed_val)
            : "xmm0", "xmm1"
        );
        float dir_x = dirs[0];
        float dir_y = dirs[1];

        // Update the ball's sinusoidal location.
        b->loc_sinusoidal[0] += move_dx;
        b->loc_sinusoidal[1] += move_dy;

        {
            // Compute new X position:
            // new X = loc_sinusoidal[0] - (dir_y * new_sine)
            float temp;
            __asm__ volatile (
                "movss %[dir_y], %%xmm0\n\t"      // Load dir_y
                "movss %[sine], %%xmm1\n\t"         // Load new_sine
                "mulss %%xmm1, %%xmm0\n\t"          // Multiply dir_y by new_sine
                "movss %[loc0], %%xmm1\n\t"         // Load loc_sinusoidal[0]
                "subss %%xmm0, %%xmm1\n\t"          // Subtract the product from loc_sinusoidal[0]
                "movss %%xmm1, %[temp]\n\t"         // Store result in temp
                : [temp] "=m" (temp)
                : [dir_y] "m" (dir_y), [sine] "m" (new_sine), [loc0] "m" (b->loc_sinusoidal[0])
                : "xmm0", "xmm1"
            );
            b->rect.x = (int)temp;
        }
        {
            // Compute new Y position:
            // new Y = loc_sinusoidal[1] + (dir_x * new_sine)
            float temp;
            __asm__ volatile (
                "movss %[dir_x], %%xmm0\n\t"      // Load dir_x
                "movss %[sine], %%xmm1\n\t"         // Load new_sine
                "mulss %%xmm1, %%xmm0\n\t"          // Multiply dir_x by new_sine
                "movss %[loc1], %%xmm1\n\t"         // Load loc_sinusoidal[1]
                "addss %%xmm0, %%xmm1\n\t"          // Add the product to loc_sinusoidal[1]
                "movss %%xmm1, %[temp]\n\t"         // Store result in temp
                : [temp] "=m" (temp)
                : [dir_x] "m" (dir_x), [sine] "m" (new_sine), [loc1] "m" (b->loc_sinusoidal[1])
                : "xmm0", "xmm1"
            );
            b->rect.y = (int)temp;
        }
    }

    // Bounce off left/right walls.
    // If the ball has hit the left or right boundaries then invert its horizontal speed.
    if (b->rect.x <= 0 || (b->rect.x + b->rect.w) >= SCREEN_WIDTH) {
        float minus_one = -1.0f;
        __asm__ volatile (
            "movl    %[rect_x], %%eax        \n\t"
            "cmpl    $0, %%eax               \n\t"
            "jle     invert_dx             \n\t"
            "movl    %[rect_x], %%eax        \n\t"
            "addl    %[rect_w], %%eax        \n\t"
            "cmpl    %[screen_width], %%eax  \n\t"
            "jge     invert_dx             \n\t"
            "jmp     end_check             \n\t"
            "invert_dx:                    \n\t"
            "movss   %[dx], %%xmm0         \n\t"  
            "mulss   %[minus_one], %%xmm0  \n\t"  
            "movss   %%xmm0, %[dx]         \n\t"  
            "end_check:                    \n\t"
            : [dx] "+m" (b->dx)
            : [rect_x] "m" (b->rect.x),
              [rect_w] "m" (b->rect.w),
              [screen_width] "i" (SCREEN_WIDTH),
              [minus_one] "m" (minus_one)
            : "eax", "xmm0"
        );
    }

Uint64 end_time = SDL_GetPerformanceCounter();
double elapsed = (double)(end_time - start_time) / SDL_GetPerformanceFrequency();
printf("Execution time (Ball_update): %.9f seconds\n", elapsed);
    // Update the color rotation angle (using the angle of movement).
    // The angle is computed from move_dy and move_dx.
    float angle = atan2f(move_dy, move_dx);
    b->color_angle += angle * b->speed;
    while (b->color_angle < 0)
        b->color_angle += 360;
    while (b->color_angle >= 360)
        b->color_angle -= 360;

    // Update the ball's spin (rotation) regardless of trajectory.
    b->spinAngle += b->spinSpeed;
    while (b->spinAngle < 0)
        b->spinAngle += 360;
    while (b->spinAngle >= 360)
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
        // int x_span = (int)sqrt(radius * radius - y * y);
      int y_val = y;
int radius_sq = radius * radius;
int y_sq = y_val * y_val;
int diff = radius_sq - y_sq;
float diff_float = (float)diff;
float x_span_float;

__asm__ volatile (
    "sqrtss %[diff], %%xmm0\n\t"
    "movss %%xmm0, %[x_span]\n\t"
    : [x_span] "=m" (x_span_float)
    : [diff] "m" (diff_float)
    : "xmm0"
);
int x_span = (int)x_span_float;
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
    Slider_init(&speed_slider, 400, SCREEN_HEIGHT - SLIDER_HEIGHT - 10, SLIDER_WIDTH, SLIDER_HEIGHT, 1, 100, 8);

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
            Paddle_move(&player, -1 * speed_slider.val / 8);
        }
        if (keystate[SDL_SCANCODE_RIGHT]) {
            Paddle_move(&player, 1 * speed_slider.val / 8);
        }

        // ---- Improved Opponent Movement ----
        int opp_center = opponent.rect.x + opponent.rect.w/2;
        int ball_center = ball.rect.x + ball.rect.w/2;
        int diff = ball_center - opp_center;
        if (abs(diff) > 10) {
            if (diff > 0) {
                Paddle_move(&opponent, 1 * speed_slider.val / 8);
            } else {
                Paddle_move(&opponent, -1 * speed_slider.val / 8);
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
// // At the beginning of the Ball_update function, start the timer
// Uint64 start_time = SDL_GetPerformanceCounter();

// // // ... [Insert the entire Ball_update code here] ...

// // // At the end of the function, stop the timer and calculate the elapsed time
// Uint64 end_time = SDL_GetPerformanceCounter();
// double elapsed = (double)(end_time - start_time) / SDL_GetPerformanceFrequency();
// printf("Execution time (Ball_update): %.9f seconds\n", elapsed);
