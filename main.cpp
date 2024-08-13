// clean your eyes a million times,
// yes, this is the cleanest code
// ive ever written, i think.

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

inline void apply_gravity(Planet &a, Planet &b) {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double distance_sq = dx * dx + dy * dy;
    if (distance_sq < 1.0) distance_sq = 1.0;
    double distance = std::sqrt(distance_sq);
    double force = (GRAVITY_CONSTANT * a.mass * b.mass) / distance_sq;
    double fx = force * dx / distance;
    double fy = force * dy / distance;
    a.vx += fx / a.mass * TIME_STEP;
    a.vy += fy / a.mass * TIME_STEP;
    b.vx -= fx / b.mass * TIME_STEP;
    b.vy -= fy / b.mass * TIME_STEP;
}

inline void update_position(Planet &planet) {
    if (!dragging || dragged_planet_index != &planet - &planets[0]) {
        planet.x += planet.vx * TIME_STEP;
        planet.y += planet.vy * TIME_STEP;
        if (planet.x < 0) planet.x += WINDOW_SIZE;
        if (planet.x >= WINDOW_SIZE) planet.x -= WINDOW_SIZE;
        if (planet.y < 0) planet.y += WINDOW_SIZE;
        if (planet.y >= WINDOW_SIZE) planet.y -= WINDOW_SIZE;
    }
}

inline void update_trail(Planet &planet) {
    for (int t = TRAIL_LENGTH - 1; t > 0; --t) {
        planet.trail[t] = planet.trail[t - 1];
    }
    planet.trail[0].x = static_cast<int>(planet.x);
    planet.trail[0].y = static_cast<int>(planet.y);
}

void update_planets() {
    for (size_t i = 0; i < planets.size(); ++i) {
        for (size_t j = i + 1; j < planets.size(); ++j) {
            apply_gravity(planets[i], planets[j]);
        }
    }
    for (auto &planet : planets) {
        update_position(planet);
        update_trail(planet);
    }
}

inline void draw_circle(SDL_Renderer *renderer, int cx, int cy, int radius, SDL_Color color) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, 255);
    for (int w = 0; w < radius * 2; ++w) {
        for (int h = 0; h < radius * 2; ++h) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
            }
        }
    }
}

void render_planets(SDL_Renderer *renderer) {
    for (const auto &planet : planets) {
        for (int t = TRAIL_LENGTH - 1; t > 0; --t) {
            int alpha = (255 * t) / TRAIL_LENGTH;
            SDL_SetRenderDrawColor(renderer, planet.color.r, planet.color.g, planet.color.b, alpha);
            int dx = abs(planet.trail[t].x - planet.trail[t - 1].x);
            int dy = abs(planet.trail[t].y - planet.trail[t - 1].y);
            if (dx < WINDOW_SIZE / 2 && dy < WINDOW_SIZE / 2) {
                SDL_RenderDrawLine(renderer, planet.trail[t].x, planet.trail[t].y, planet.trail[t - 1].x, planet.trail[t - 1].y);
            }
        }
        draw_circle(renderer, static_cast<int>(planet.x), static_cast<int>(planet.y), planet.radius, planet.color);
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
    static SDL_Color white = {255, 255, 255, 255};
    render_text(renderer, font, "Add Planet", 10, 10, white);
    for (size_t i = 0; i < planets.size(); ++i) {
        std::string planet_label = "Planet " + std::to_string(i + 1);
        render_text(renderer, font, planet_label, 10, 50 + i * 60, white);
        render_text(renderer, font, " Mass:", 10, 70 + i * 60, white);
        render_text(renderer, font, std::to_string(planets[i].mass), 100, 70 + i * 60, white);
        render_text(renderer, font, " Delete", 10, 90 + i * 60, white);
    }
}

inline void add_planet() {
    Planet new_planet = {800, 600, 0, 0, 1e4, 15, {255, 255, 255, 255}};
    std::fill(std::begin(new_planet.trail), std::end(new_planet.trail), SDL_Point{static_cast<int>(new_planet.x), static_cast<int>(new_planet.y)});
    planets.push_back(new_planet);
}

inline bool is_mouse_over_button(int mouse_x, int mouse_y, int button_x, int button_y, int button_w, int button_h) {
    return mouse_x >= button_x && mouse_y >= button_y && mouse_x <= button_x + button_w && mouse_y <= button_h;
}

inline bool is_mouse_over_planet(int mouse_x, int mouse_y, Planet &planet) {
    int dx = mouse_x - static_cast<int>(planet.x);
    int dy = mouse_y - static_cast<int>(planet.y);
    return (dx * dx + dy * dy) <= (planet.radius * planet.radius);
}

inline void modify_planet_mass(int planet_index, double new_mass) {
    if (planet_index >= 0 && planet_index < planets.size()) {
        planets[planet_index].mass = new_mass;
        planets[planet_index].radius = static_cast<int>(std::cbrt(new_mass));
    }
}

inline void delete_planet(int planet_index) {
    if (planet_index >= 0 && planet_index < planets.size()) {
        planets.erase(planets.begin() + planet_index);
    }
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return 1;
    }
    if (TTF_Init() == -1) {
        std::cerr << "TTF_Init: " << TTF_GetError() << std::endl;
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("2D Gravity Simulation", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_SIZE, WINDOW_SIZE, SDL_WINDOW_SHOWN);
    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer could not be created! SDL Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = TTF_OpenFont("arial.ttf", 24);
    if (!font) {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    planets = {
        {600, 800, 0, 2, 5e4, 20, {255, 0, 0, 255}},
        {1200, 800, 0, -2, 5e4, 20, {0, 255, 0, 255}},
        {800, 1200, 2, -1, 1e5, 25, {0, 0, 255, 255}}
    };
    for (auto &planet : planets) {
        std::fill(std::begin(planet.trail), std::end(planet.trail), SDL_Point{static_cast<int>(planet.x), static_cast<int>(planet.y)});
    }

    SDL_Event event;
    bool quit = false;
    Uint32 last_time = SDL_GetTicks();

    while (!quit) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = true;
            } else if (event.type == SDL_MOUSEBUTTONDOWN) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                if (is_mouse_over_button(x, y, 10, 10, 120, 30)) {
                    add_planet();
                } else {
                    for (size_t i = 0; i < planets.size(); ++i) {
                        if (is_mouse_over_button(x, y, 10, 90 + i * 60, 60, 30)) {
                            delete_planet(i);
                            break;
                        } else if (is_mouse_over_planet(x, y, planets[i])) {
                            dragging = true;
                            dragged_planet_index = i;
                            drag_offset_x = x - static_cast<int>(planets[i].x);
                            drag_offset_y = y - static_cast<int>(planets[i].y);
                            break;
                        }
                    }
                }
            } else if (event.type == SDL_MOUSEBUTTONUP) {
                dragging = false;
            } else if (event.type == SDL_MOUSEMOTION && dragging) {
                int x, y;
                SDL_GetMouseState(&x, &y);
                if (dragged_planet_index >= 0 && dragged_planet_index < planets.size()) {
                    planets[dragged_planet_index].x = x - drag_offset_x;
                    planets[dragged_planet_index].y = y - drag_offset_y;
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_q) {
                    quit = true;
                }
            }
        }

        Uint32 current_time = SDL_GetTicks();
        if (current_time - last_time >= 1000 / FPS_CAP) {
            last_time = current_time;
            update_planets();
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            render_planets(renderer);
            render_gui(renderer, font);
            SDL_RenderPresent(renderer);
        }
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return 0;
}
