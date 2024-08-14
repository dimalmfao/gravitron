#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

#define WINDOW_SIZE 1600

#define GRAVITY_CONSTANT 6.67430e-2
#define TIME_STEP 0.1
#define TRAIL_LENGTH 100
#define FPS_CAP 60

struct Planet {
    double x, y;
    double vx, vy;
    double mass;
    int radius;
    SDL_Color color;
    SDL_Point trail[TRAIL_LENGTH];
};

std::vector<Planet> planets;
bool dragging = false;
int dragged_planet_index = -1;
int drag_offset_x = 0;
int drag_offset_y = 0;

void apply_gravity(Planet *a, Planet *b) {
    double dx = b->x - a->x;
    double dy = b->y - a->y;
    double distance = sqrt(dx * dx + dy * dy);

    if (distance < 1.0) distance = 1.0;

    double force = (GRAVITY_CONSTANT * a->mass * b->mass) / (distance * distance);

    double fx = force * dx / distance;
    double fy = force * dy / distance;

    a->vx += fx / a->mass * TIME_STEP;
    a->vy += fy / a->mass * TIME_STEP;
    b->vx -= fx / b->mass * TIME_STEP;
    b->vy -= fy / b->mass * TIME_STEP;
}

void update_planets() {
    for (size_t i = 0; i < planets.size(); i++) {
        for (size_t j = i + 1; j < planets.size(); j++) {
            apply_gravity(&planets[i], &planets[j]);
        }
    }

    for (size_t i = 0; i < planets.size(); i++) {
        if (dragging && i == dragged_planet_index) continue;

        planets[i].x += planets[i].vx * TIME_STEP;
        planets[i].y += planets[i].vy * TIME_STEP;

        if (planets[i].x < 0) planets[i].x += WINDOW_SIZE;
        if (planets[i].x >= WINDOW_SIZE) planets[i].x -= WINDOW_SIZE;
        if (planets[i].y < 0) planets[i].y += WINDOW_SIZE;
        if (planets[i].y >= WINDOW_SIZE) planets[i].y -= WINDOW_SIZE;

        for (int t = TRAIL_LENGTH - 1; t > 0; t--) {
            planets[i].trail[t] = planets[i].trail[t - 1];
        }
        planets[i].trail[0].x = static_cast<int>(planets[i].x);
        planets[i].trail[0].y = static_cast<int>(planets[i].y);
    }
}

void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

void render_planets(SDL_Renderer *renderer) {
    for (size_t i = 0; i < planets.size(); i++) {
        for (int t = TRAIL_LENGTH - 1; t > 0; t--) {
            int alpha = (255 * t) / TRAIL_LENGTH;
            SDL_SetRenderDrawColor(renderer, planets[i].color.r, planets[i].color.g, planets[i].color.b, alpha);

            int dx = abs(planets[i].trail[t].x - planets[i].trail[t - 1].x);
            int dy = abs(planets[i].trail[t].y - planets[i].trail[t - 1].y);

            if (dx < WINDOW_SIZE / 2 && dy < WINDOW_SIZE / 2) {
                SDL_RenderDrawLine(renderer, planets[i].trail[t].x, planets[i].trail[t].y,
                                   planets[i].trail[t - 1].x, planets[i].trail[t - 1].y);
            }
        }

        SDL_SetRenderDrawColor(renderer, planets[i].color.r, planets[i].color.g, planets[i].color.b, 255);
        draw_circle(renderer, static_cast<int>(planets[i].x), static_cast<int>(planets[i].y), planets[i].radius);
    }
}

void render_text(SDL_Renderer *renderer, TTF_Font *font, const std::string &text, int x, int y, SDL_Color color) {
    SDL_Surface *surface = TTF_RenderText_Solid(font, text.c_str(), color);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dstrect = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dstrect);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

void render_gui(SDL_Renderer *renderer, TTF_Font *font) {
    SDL_Color white = {255, 255, 255, 255};
    render_text(renderer, font, "Add Planet", 10, 10, white);
    for (size_t i = 0; i < planets.size(); i++) {
        std::string planet_label = "Planet " + std::to_string(i + 1);
        render_text(renderer, font, planet_label, 10, 50 + i * 60, white);
        render_text(renderer, font, " Mass:", 10, 70 + i * 60, white);
        render_text(renderer, font, std::to_string(planets[i].mass), 100, 70 + i * 60, white); // Adjusted x position
        render_text(renderer, font, " Delete", 10, 90 + i * 60, white); // Add Delete button
    }
}

void add_planet() {
    Planet new_planet = {800, 600, 0, 0, 1e4, 15, {255, 255, 255, 255}};
    for (int t = 0; t < TRAIL_LENGTH; t++) {
        new_planet.trail[t].x = static_cast<int>(new_planet.x);
        new_planet.trail[t].y = static_cast<int>(new_planet.y);
    }
    planets.push_back(new_planet);
}

bool is_mouse_over_button(int mouse_x, int mouse_y, int button_x, int button_y, int button_w, int button_h) {
    return mouse_x >= button_x && mouse_y >= button_y && mouse_x <= button_x + button_w && mouse_y <= button_y + button_h;
}

bool is_mouse_over_planet(int mouse_x, int mouse_y, Planet &planet) {
    int dx = mouse_x - static_cast<int>(planet.x);
    int dy = mouse_y - static_cast<int>(planet.y);
    return (dx * dx + dy * dy) <= (planet.radius * planet.radius);
}

void modify_planet_mass(int planet_index, double new_mass) {
    if (planet_index >= 0 && planet_index < planets.size()) {
        planets[planet_index].mass = new_mass;
        planets[planet_index].radius = static_cast<int>(std::cbrt(new_mass)); // Update radius based on mass
    }
}

void delete_planet(int planet_index) {
    if (planet_index >= 0 && planet_index < planets.size()) {
        planets.erase(planets.begin() + planet_index);
    }
}

int main(int argc, char *argv[]) {
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Event event;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init: " << TTF_GetError() << std::endl;
        return 1;
    }

    window = SDL_CreateWindow("2D Gravity Simulation",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              WINDOW_SIZE, WINDOW_SIZE,
                              SDL_WINDOW_SHOWN);
    if (window == nullptr) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (renderer == nullptr) {
        std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = TTF_OpenFont("code.ttf", 24);
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    planets = {
        {600, 400, 0, 4, 1e4, 15, {255, 0, 0, 255}},
        {1200, 800, -2, -3, 5e4, 20, {0, 255, 0, 255}},
        {800, 300, 2, 1.5, 8e4, 18, {0, 0, 255, 255}},
        {200, 600, 1, -1, 3e4, 12, {255, 255, 0, 255}},
        {1000, 100, -1.5, 2.5, 2e4, 10, {255, 0, 255, 255}}
    };

    for (size_t i = 0; i < planets.size(); i++) {
        for (int t = 0; t < TRAIL_LENGTH; t++) {
            planets[i].trail[t].x = static_cast<int>(planets[i].x);
            planets[i].trail[t].y = static_cast<int>(planets[i].y);
        }
    }

    bool running = true;
    Uint32 frame_start;
    int frame_time;
    int hovered_planet_index = -1;

    while (running) {
        frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&event) != 0) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouse_x, mouse_y;
                SDL_GetMouseState(&mouse_x, &mouse_y);

                if (is_mouse_over_button(mouse_x, mouse_y, 10, 10, 100, 30)) {
                    add_planet();
                } else {
                    for (size_t i = 0; i < planets.size(); i++) {
                        if (is_mouse_over_planet(mouse_x, mouse_y, planets[i])) {
                            dragging = true;
                            dragged_planet_index = i;
                            drag_offset_x = mouse_x - static_cast<int>(planets[i].x);
                            drag_offset_y = mouse_y - static_cast<int>(planets[i].y);
                            break;
                        }
                    }
                    for (size_t i = 0; i < planets.size(); i++) {
                        if (is_mouse_over_button(mouse_x, mouse_y, 10, 90 + i * 60, 100, 30)) {
                            delete_planet(i);
                            break;
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                dragging = false;
                dragged_planet_index = -1;
            } else if (event.type == SDL_MOUSEMOTION) {
                int mouse_x = event.motion.x;
                int mouse_y = event.motion.y;

                if (dragging && dragged_planet_index != -1) {
                    planets[dragged_planet_index].x = mouse_x - drag_offset_x;
                    planets[dragged_planet_index].y = mouse_y - drag_offset_y;
                } else {
                    hovered_planet_index = -1;
                    for (size_t i = 0; i < planets.size(); i++) {
                        if (is_mouse_over_button(mouse_x, mouse_y, 10, 70 + i * 60, 100, 30)) {
                            hovered_planet_index = i;
                            break;
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEWHEEL) {
                if (hovered_planet_index != -1) {
                    double new_mass = planets[hovered_planet_index].mass * (1 + event.wheel.y * 0.1); // Change mass by 10%
                    modify_planet_mass(hovered_planet_index, new_mass);
                }
            }
        }

        update_planets();

        SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xFF);
        SDL_RenderClear(renderer);

        render_planets(renderer);
        render_gui(renderer, font);

        SDL_RenderPresent(renderer);

        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < 1000 / FPS_CAP) {
            SDL_Delay((1000 / FPS_CAP) - frame_time);
        }
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
