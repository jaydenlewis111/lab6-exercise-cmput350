#include <cmath>
#include <iostream>
#include <limits>
#include <optional>
#include <vector>

#include <SFML/Graphics.hpp>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 800;
const int FPS_LIMIT = 30;

using Point2D = sf::Vector2f;

// These functions sample one cubic segment, using its four control points.
Point2D getPoint(const std::vector<sf::Vector2f>& pts, float t) {
    const float u = 1.f - t;
    return u * u * u * pts.at(0) + 3.f * u * u * t * pts.at(1) +
           3.f * u * t * t * pts.at(2) + t * t * t * pts.at(3);
}

// Return the derivative vector.
Point2D getSlope(const std::vector<sf::Vector2f>& pts, float t) {
    const float u = 1.f - t;
    return 3.f * u * u * (pts.at(1) - pts.at(0)) +
           6.f * u * t * (pts.at(2) - pts.at(1)) +
           3.f * t * t * (pts.at(3) - pts.at(2));
}

// Connected segments share every third point: [0,1,2,3], [3,4,5,6], ...
std::vector<Point2D> points{{100.f, 600.f}, {200.f, 150.f}, {500.f, 150.f}, {600.f, 500.f}};
float animationTime = 0.f;
int draggedPoint = -1;

float lengthSquared(Point2D vector) { return vector.x * vector.x + vector.y * vector.y; }

// Rotate the opposite handle onto the same tangent, preserving its length.
void alignHandle(std::size_t anchor, std::size_t moved, std::size_t opposite) {
    const Point2D direction = points[anchor] - points[moved];
    const float magnitude = std::sqrt(lengthSquared(direction));
    if (magnitude > 0.f) {
        const float distance = std::sqrt(lengthSquared(points[opposite] - points[anchor]));
        points[opposite] = points[anchor] + direction * (distance / magnitude);
    }
}

void movePoint(std::size_t index, Point2D position) {
    const Point2D offset = position - points[index];
    points[index] = position;
    if (index % 3 == 0) {
        // Moving an endpoint translates its handles, preserving the join's tangent.
        if (index > 0) points[index - 1] += offset;
        if (index + 1 < points.size()) points[index + 1] += offset;
    } else if (index % 3 == 2 && index + 2 < points.size()) {
        alignHandle(index + 1, index, index + 2);
    } else if (index % 3 == 1 && index > 1) {
        alignHandle(index - 1, index, index - 2);
    }
}

void addSegment() {
    const Point2D endpoint = points.back();
    Point2D direction = endpoint - points[points.size() - 2];
    const float magnitude = std::sqrt(lengthSquared(direction));
    direction = magnitude > 0.f ? direction / magnitude : Point2D{1.f, 0.f};
    // The first new handle continues the previous segment's end tangent.
    for (int i = 1; i <= 3; ++i) points.push_back(endpoint + direction * (60.f * i));
}

void removeSegment() {
    if (points.size() > 4) {
        points.resize(points.size() - 3);
        if (draggedPoint >= static_cast<int>(points.size())) draggedPoint = -1;
    }
}

void handleInput(sf::RenderWindow& window, bool& shouldQuit) {
    while (const std::optional<sf::Event> event = window.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window.close();
            shouldQuit = true;
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonPressed>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                const Point2D position = window.mapPixelToCoords(mouse->position);
                float closestDistance = std::numeric_limits<float>::max();
                for (std::size_t i = 0; i < points.size(); ++i) {
                    const float distance = lengthSquared(points[i] - position);
                    if (distance < closestDistance) {
                        closestDistance = distance;
                        draggedPoint = static_cast<int>(i);
                    }
                }
                movePoint(static_cast<std::size_t>(draggedPoint), position);
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseButtonReleased>()) {
            if (mouse->button == sf::Mouse::Button::Left) {
                if (draggedPoint >= 0) {
                    movePoint(static_cast<std::size_t>(draggedPoint),
                              window.mapPixelToCoords(mouse->position));
                }
                draggedPoint = -1;
            }
        } else if (const auto* mouse = event->getIf<sf::Event::MouseMoved>()) {
            if (draggedPoint >= 0) {
                movePoint(static_cast<std::size_t>(draggedPoint),
                          window.mapPixelToCoords(mouse->position));
            }
        } else if (event->is<sf::Event::FocusLost>()) {
            draggedPoint = -1;
        } else if (const auto* key = event->getIf<sf::Event::KeyPressed>()) {
            if (key->code == sf::Keyboard::Key::Add ||
                (key->code == sf::Keyboard::Key::Equal && key->shift)) {
                addSegment();
            } else if (key->code == sf::Keyboard::Key::Subtract ||
                       key->code == sf::Keyboard::Key::Hyphen) {
                removeSegment();
            }
        }
    }
}

void drawLine(sf::RenderWindow& window, Point2D start, Point2D end, sf::Color color) {
    const sf::Vertex line[]{{start, color}, {end, color}};
    window.draw(line, 2, sf::PrimitiveType::Lines);
}

void render(sf::RenderWindow& window) {
    window.clear(sf::Color(20, 24, 32));
    const sf::Color handleColor(110, 120, 140);
    constexpr int samples = 100;
    for (std::size_t start = 0; start + 3 < points.size(); start += 3) {
        const std::vector<Point2D> segment(points.begin() + start, points.begin() + start + 4);
        drawLine(window, segment[0], segment[1], handleColor);
        drawLine(window, segment[2], segment[3], handleColor);
        Point2D previous = getPoint(segment, 0.f);
        for (int i = 1; i <= samples; ++i) {
            const Point2D current = getPoint(segment, static_cast<float>(i) / samples);
            drawLine(window, previous, current, sf::Color::Cyan);
            previous = current;
        }
    }

    const std::size_t segmentCount = (points.size() - 1) / 3;
    // Each segment takes three seconds; wrap after traversing the entire curve.
    const float progress = std::fmod(animationTime / 3.f, static_cast<float>(segmentCount));
    const std::size_t start = static_cast<std::size_t>(progress) * 3;
    const float t = progress - std::floor(progress);
    const std::vector<Point2D> segment(points.begin() + start, points.begin() + start + 4);
    const Point2D slope = getSlope(segment, t);
    sf::RectangleShape square({18.f, 18.f});
    square.setOrigin({9.f, 9.f});
    square.setPosition(getPoint(segment, t));
    square.setRotation(sf::radians(std::atan2(slope.y, slope.x)));
    square.setFillColor(sf::Color::Yellow);
    window.draw(square);

    // Draw the control points last so they remain visible above the curve and handles.
    sf::CircleShape circle(7.f);
    circle.setOrigin({7.f, 7.f});
    for (std::size_t i = 0; i < points.size(); ++i) {
        circle.setPosition(points[i]);
        const sf::Color color = i % 3 == 0 ? sf::Color::White : sf::Color(255, 140, 80);
        circle.setFillColor(static_cast<int>(i) == draggedPoint ? sf::Color::Yellow : color);
        window.draw(circle);
    }

    window.display();
}

int main() {
    sf::RenderWindow window;

    try {
        // Initialize window
        window.create(sf::VideoMode({WINDOW_WIDTH, WINDOW_HEIGHT}),
                      "Bezier Curve Editor | Left drag: edit | + / -: add / remove segment");
        window.setFramerateLimit(FPS_LIMIT);
        // Prevent key repeats.
        window.setKeyRepeatEnabled(false);

        bool shouldQuit = false;
        sf::Clock frameClock;
        // Main game loop
        while (window.isOpen()) {
            handleInput(window, shouldQuit);
            if (shouldQuit) {
                break;
            }
            animationTime += frameClock.restart().asSeconds();
            render(window);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return -1;
    }
    return 0;
}
