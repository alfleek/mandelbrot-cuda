#pragma once

#include <SFML/Graphics.hpp>
#include <functional>
#include <vector>
#include <string>
#include <chrono>
#include "Viewport.h"

struct PresetInfo {
    std::string name;
    double realCenter;
    double imagCenter;
    double distance;
    int maxIterations;
};

class RendererSFML {
private:
    sf::RenderWindow window;
    sf::Texture texture;
    sf::Sprite sprite;

    // Note: windowWidth/Height now refer to the texture size
    int textureWidth;
    int textureHeight;
    bool dataUpdated;

    // Overscan factor to maintain constant texture/window ratio
    float overscanFactor;

    // Mandelbrot parameters
    int maxIterations;

    // Color palette
    std::vector<sf::Color> colorPalette;

    // Viewport for coordinate mapping
    Viewport viewport;

    // Mouse interaction
    bool isDragging;
    bool isInteracting;
    sf::Vector2i lastMousePos;
    sf::Vector2i startMousePos; // For stable panning
    sf::Vector2f startSpritePos; // For stable panning

    // Visual transformation for immediate feedback
    Viewport visualViewport; // For immediate feedback during interaction
    bool isTransforming;

    // Timer for zoom commit
    std::chrono::steady_clock::time_point lastZoomTime;
    static constexpr int ZOOM_COMMIT_TIMEOUT_MS = 200;

    // Callback for parameter changes
    std::function<void(double, double, double, int)> onParamsChanged;

    // Callback for window resize events
    std::function<void(int, int)> onWindowResize;

    // UI state
    sf::Font uiFont;
    bool fontLoaded;
    bool showInfo;
    bool showHelp;
    bool showPresets;
    std::vector<PresetInfo> presets;
    std::vector<sf::FloatRect> presetButtonBounds;
    int hoveredPreset;
    std::function<void(int)> onPresetSelected;
    std::string computeMode;
    double lastComputeTimeMs;

    void setupColorPalette();
    sf::Color getColor(int iteration) const;
    void applyVisualTransformation();
    void drawInfoPanel();
    void drawPresetPanel();
    void drawHelpBar();

public:
    RendererSFML();
    ~RendererSFML();

    bool init(int textureWidth, int textureHeight, int windowWidth, int windowHeight, const std::string& title = "Mandelbrot Set");
    void close();

    void updateMandelbrotData(const std::vector<int>& data);
    void updateMandelbrotData(const int* data, int size);

    void setMandelbrotParams(double realCenter, double imagCenter, double distance, int maxIterations);
    void setParamsChangedCallback(std::function<void(double, double, double, int)> callback);
    void setWindowResizeCallback(std::function<void(int, int)> callback);
    void setPresetSelectedCallback(std::function<void(int)> callback);
    void setPresets(const std::vector<PresetInfo>& presetList);
    void setComputeInfo(const std::string& mode, double timeMs);

    void update();
    void updateTexture();
    void render();
    void drawUI();
    void handleEvents();

    bool isOpen() const;

    // Window resize handling
    void handleWindowResize(int newWidth, int newHeight);

    // Mouse event handlers
    void handleMousePress(int x, int y, bool leftButton);
    void handleMouseRelease(int x, int y);
    void handleMouseMove(int x, int y);
    void handleMouseWheel(int delta);

    // Zoom commit method
    void commitZoom();

    // Get current viewport for external use
    const Viewport& getViewport() const { return viewport; }

    float getOverscanFactor() const { return overscanFactor; }
}; 