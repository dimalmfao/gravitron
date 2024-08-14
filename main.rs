extern crate sdl2;
extern crate sdl2_ttf;

use sdl2::pixels::Color;
use sdl2::rect::Rect;
use sdl2::render::{Canvas, RendererBuilder};
use sdl2::video::Window;
use sdl2::event::Event;
use sdl2::keyboard::Keycode;
use sdl2::ttf::Font;
use std::collections::VecDeque;
use std::f64;
use std::env;

const WINDOW_SIZE: u32 = 1600;
const GRAVITY_CONSTANT: f64 = 6.67430e-2;
const TIME_STEP: f64 = 0.1;
const TRAIL_LENGTH: usize = 100;
const FPS_CAP: u32 = 60;

struct Planet {
    x: f64,
    y: f64,
    vx: f64,
    vy: f64,
    mass: f64,
    radius: i32,
    color: Color,
    trail: VecDeque<(i32, i32)>,
}

impl Planet {
    fn new(x: f64, y: f64, vx: f64, vy: f64, mass: f64, radius: i32, color: Color) -> Self {
        let mut trail = VecDeque::with_capacity(TRAIL_LENGTH);
        for _ in 0..TRAIL_LENGTH {
            trail.push_front((x as i32, y as i32));
        }
        Self {
            x,
            y,
            vx,
            vy,
            mass,
            radius,
            color,
            trail,
        }
    }
}

fn apply_gravity(a: &mut Planet, b: &mut Planet) {
    let dx = b.x - a.x;
    let dy = b.y - a.y;
    let distance = (dx * dx + dy * dy).sqrt().max(1.0);
    let force = (GRAVITY_CONSTANT * a.mass * b.mass) / (distance * distance);

    let fx = force * dx / distance;
    let fy = force * dy / distance;

    a.vx += fx / a.mass * TIME_STEP;
    a.vy += fy / a.mass * TIME_STEP;
    b.vx -= fx / b.mass * TIME_STEP;
    b.vy -= fy / b.mass * TIME_STEP;
}

fn update_planets(planets: &mut Vec<Planet>, dragging: bool, dragged_planet_index: Option<usize>, drag_offset_x: i32, drag_offset_y: i32) {
    for i in 0..planets.len() {
        for j in (i + 1)..planets.len() {
            let (a, b) = (&mut planets[i], &mut planets[j]);
            apply_gravity(a, b);
        }
    }

    for i in 0..planets.len() {
        if dragging && Some(i) == dragged_planet_index {
            continue;
        }

        let planet = &mut planets[i];
        planet.x += planet.vx * TIME_STEP;
        planet.y += planet.vy * TIME_STEP;

        if planet.x < 0.0 {
            planet.x += WINDOW_SIZE as f64;
        }
        if planet.x >= WINDOW_SIZE as f64 {
            planet.x -= WINDOW_SIZE as f64;
        }
        if planet.y < 0.0 {
            planet.y += WINDOW_SIZE as f64;
        }
        if planet.y >= WINDOW_SIZE as f64 {
            planet.y -= WINDOW_SIZE as f64;
        }

        planet.trail.push_front((planet.x as i32, planet.y as i32));
        if planet.trail.len() > TRAIL_LENGTH {
            planet.trail.pop_back();
        }
    }
}

fn draw_circle(canvas: &mut Canvas<Window>, cx: i32, cy: i32, radius: i32, color: Color) {
    canvas.set_draw_color(color);
    for w in 0..radius * 2 {
        for h in 0..radius * 2 {
            let dx = radius - w;
            let dy = radius - h;
            if (dx * dx + dy * dy) <= (radius * radius) {
                canvas.draw_point((cx + dx, cy + dy)).unwrap();
            }
        }
    }
}

fn render_planets(canvas: &mut Canvas<Window>, planets: &[Planet]) {
    for planet in planets {
        for (i, &(x1, y1)) in planet.trail.iter().enumerate().skip(1) {
            let (x0, y0) = planet.trail[i - 1];
            let alpha = (255 * i as u8) / TRAIL_LENGTH as u8;
            canvas.set_draw_color(Color::RGBA(planet.color.r, planet.color.g, planet.color.b, alpha));
            canvas.draw_line((x0, y0), (x1, y1)).unwrap();
        }

        canvas.set_draw_color(planet.color);
        draw_circle(canvas, planet.x as i32, planet.y as i32, planet.radius, planet.color);
    }
}

fn render_text(canvas: &mut Canvas<Window>, font: &Font, text: &str, x: i32, y: i32, color: Color) {
    let surface = font.render(text)
        .blended(color)
        .expect("Failed to render text");
    let texture_creator = canvas.texture_creator();
    let texture = texture_creator.create_texture_from_surface(&surface)
        .expect("Failed to create texture from surface");
    let texture_query = texture.query();
    let dstrect = Rect::new(x, y, texture_query.width, texture_query.height);
    canvas.copy(&texture, None, Some(dstrect)).unwrap();
}

fn render_gui(canvas: &mut Canvas<Window>, font: &Font, planets: &[Planet]) {
    let white = Color::RGB(255, 255, 255);
    render_text(canvas, font, "Add Planet", 10, 10, white);

    for (i, planet) in planets.iter().enumerate() {
        let y = 50 + i as i32 * 60;
        render_text(canvas, font, &format!("Planet {}", i + 1), 10, y, white);
        render_text(canvas, font, " Mass:", 10, y + 20, white);
        render_text(canvas, font, &planet.mass.to_string(), 100, y + 20, white);
        render_text(canvas, font, " Delete", 10, y + 40, white);
    }
}

fn add_planet(planets: &mut Vec<Planet>) {
    let new_planet = Planet::new(800.0, 600.0, 0.0, 0.0, 1e4, 15, Color::RGBA(255, 255, 255, 255));
    planets.push(new_planet);
}

fn is_mouse_over_button(mouse_x: i32, mouse_y: i32, button_x: i32, button_y: i32, button_w: i32, button_h: i32) -> bool {
    mouse_x >= button_x && mouse_y >= button_y && mouse_x <= button_x + button_w && mouse_y <= button_y + button_h
}

fn is_mouse_over_planet(mouse_x: i32, mouse_y: i32, planet: &Planet) -> bool {
    let dx = mouse_x - planet.x as i32;
    let dy = mouse_y - planet.y as i32;
    (dx * dx + dy * dy) <= (planet.radius * planet.radius)
}

fn modify_planet_mass(planets: &mut Vec<Planet>, planet_index: usize, new_mass: f64) {
    if let Some(planet) = planets.get_mut(planet_index) {
        planet.mass = new_mass;
        planet.radius = (new_mass.cbrt() as i32).max(1);
    }
}

fn delete_planet(planets: &mut Vec<Planet>, planet_index: usize) {
    if planet_index < planets.len() {
        planets.remove(planet_index);
    }
}

fn main() -> Result<(), String> {
    let sdl_context = sdl2::init()?;
    let video_subsystem = sdl_context.video()?;
    let ttf_context = sdl2_ttf::init()?;

    let window = video_subsystem.window("2D Gravity Simulation", WINDOW_SIZE, WINDOW_SIZE)
        .position_centered()
        .build()
        .map_err(|e| e.to_string())?;

    let mut canvas = window.into_canvas()
        .accelerated()
        .build()
        .map_err(|e| e.to_string())?;

    let font = ttf_context.load_font("code.ttf", 24)
        .map_err(|e| e.to_string())?;

    let mut planets = vec![
        Planet::new(600.0, 400.0, 0.0, 4.0, 1e4, 15, Color::RGBA(255, 0, 0, 255)),
        Planet::new(1200.0, 800.0, -2.0, -3.0, 5e4, 20, Color::RGBA(0, 255, 0, 255)),
        Planet::new(800.0, 300.0, 2.0, 1.5, 8e4, 18, Color::RGBA(0, 0, 255, 255)),
        Planet::new(200.0, 600.0, 1.0, -1.0, 3e4, 12, Color::RGBA(255, 255, 0, 255)),
        Planet::new(1000.0, 100.0, -1.5, 2.5, 2e4, 10, Color::RGBA(255, 0, 255, 255))
    ];

    let mut event_pump = sdl_context.event_pump()?;
    let mut dragging = false;
    let mut dragged_planet_index: Option<usize> = None;
    let mut drag_offset_x = 0;
    let mut drag_offset_y = 0;

    let mut frame_start;
    let mut frame_time;
    let mut hovered_planet_index = None;

    'running: loop {
        frame_start = sdl2::TimerSubsystem::performance_counter();

        for event in event_pump.poll_iter() {
            match event {
                Event::Quit { .. } => break 'running,
                Event::MouseButtonDown { x, y, .. } => {
                    if is_mouse_over_button(x, y, 10, 10, 100, 30) {
                        add_planet(&mut planets);
                    } else {
                        for (i, planet) in planets.iter_mut().enumerate() {
                            if is_mouse_over_planet(x, y, planet) {
                                dragging = true;
                                dragged_planet_index = Some(i);
                                drag_offset_x = x - planet.x as i32;
                                drag_offset_y = y - planet.y as i32;
                                break;
                            }
                        }
                        for (i, planet) in planets.iter_mut().enumerate() {
                            if is_mouse_over_button(x, y, 10, 90 + i as i32 * 60, 100, 30) {
                                delete_planet(&mut planets, i);
                                break;
                            }
                        }
                    }
                }
                Event::MouseButtonUp { .. } => {
                    dragging = false;
                    dragged_planet_index = None;
                }
                Event::MouseMotion { x, y, .. } => {
                    if dragging {
                        if let Some(i) = dragged_planet_index {
                            let planet = &mut planets[i];
                            planet.x = x as f64 - drag_offset_x as f64;
                            planet.y = y as f64 - drag_offset_y as f64;
                        }
                    } else {
                        hovered_planet_index = planets.iter().position(|planet| is_mouse_over_button(x, y, 10, 70 + planet.trail.len() as i32 * 60, 100, 30));
                    }
                }
                Event::MouseWheel { y, .. } => {
                    if let Some(i) = hovered_planet_index {
                        let new_mass = planets[i].mass * (1.0 + y as f64 * 0.1); // Change mass by 10%
                        modify_planet_mass(&mut planets, i, new_mass);
                    }
                }
                _ => {}
            }
        }

        update_planets(&mut planets, dragging, dragged_planet_index, drag_offset_x, drag_offset_y);

        canvas.set_draw_color(Color::RGB(0, 0, 0));
        canvas.clear();

        render_planets(&mut canvas, &planets);
        render_gui(&mut canvas, &font, &planets);

        canvas.present();

        frame_time = sdl2::TimerSubsystem::performance_counter() - frame_start;
        if frame_time < 1000 / FPS_CAP {
            sdl2::TimerSubsystem::delay((1000 / FPS_CAP) - frame_time as u32);
        }
    }

    Ok(())
}
