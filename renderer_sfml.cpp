#include "renderer_sfml.h"
#include <iostream>
#include <cmath>
#include <cstdio>

// Define M_PI for Windows if not already defined
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

RendererSFML::RendererSFML()
    : textureWidth(1600), textureHeight(900), dataUpdated(false),
      maxIterations(1000), overscanFactor(1.4f),
      viewport(0.0, 0.0, 4.0, 1600, 900),
      visualViewport(0.0, 0.0, 4.0, 1600, 900),
      isDragging(false), isInteracting(false), lastMousePos(0, 0),
      isTransforming(false), lastZoomTime(std::chrono::steady_clock::now()),
      onParamsChanged(nullptr), onWindowResize(nullptr),
      fontLoaded(false), showInfo(true), showHelp(true), showPresets(true),
      hoveredPreset(-1), onPresetSelected(nullptr),
      computeMode("CPU"), lastComputeTimeMs(0.0) {
    setupColorPalette();
}

RendererSFML::~RendererSFML() {
    close();
}

bool RendererSFML::init(int texWidth, int texHeight, int winWidth, int winHeight, const std::string& title) {
    textureWidth = texWidth;
    textureHeight = texHeight;
    
    // Derive overscan factor from initial sizes to keep it constant thereafter
    overscanFactor = static_cast<float>(textureWidth) / static_cast<float>(winWidth);
    
    // Create window with specified dimensions
    window.create(sf::VideoMode(winWidth, winHeight), title);
    window.setFramerateLimit(60);
    
    // Create texture with oversized dimensions
    if (!texture.create(textureWidth, textureHeight)) {
        std::cerr << "Failed to create texture" << std::endl;
        return false;
    }
    
    sprite.setTexture(texture);

    // Load font for UI text
    const char* fontPaths[] = {
        "C:/Windows/Fonts/consola.ttf",
        "C:/Windows/Fonts/arial.ttf",
        "C:/Windows/Fonts/segoeui.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
    };
    for (const char* path : fontPaths) {
        if (uiFont.loadFromFile(path)) {
            fontLoaded = true;
            break;
        }
    }
    if (!fontLoaded) {
        std::cerr << "Warning: Could not load any font for UI text" << std::endl;
    }

    // viewport represents the entire texture
    viewport.updateWindowSize(textureWidth, textureHeight);
    // visualViewport represents the visible window area
    visualViewport.updateWindowSize(winWidth, winHeight);
    // Initialize visual viewport to use visible complex height
    visualViewport.setCenter(viewport.getCenterReal(), viewport.getCenterImag());
    visualViewport.setDistance(viewport.getDistance() / static_cast<double>(overscanFactor));
    
    std::cout << "SFML renderer initialized successfully" << std::endl;
    return true;
}

void RendererSFML::close() {
    if (window.isOpen()) {
        window.close();
    }
}

void RendererSFML::setupColorPalette() {
    colorPalette.clear();
    
    // Create a smooth color palette
    for (int i = 0; i < 256; i++) {
        float t = i / 255.0f;
        
        // Create smooth color transitions
        int r = static_cast<int>(255 * (0.5f + 0.5f * sin(t * 2.0f * M_PI)));
        int g = static_cast<int>(255 * (0.5f + 0.5f * sin(t * 2.0f * M_PI + 2.0f * M_PI / 3.0f)));
        int b = static_cast<int>(255 * (0.5f + 0.5f * sin(t * 2.0f * M_PI + 4.0f * M_PI / 3.0f)));
        
        colorPalette.push_back(sf::Color(r, g, b));
    }
}

sf::Color RendererSFML::getColor(int iteration) const {
    if (iteration >= maxIterations) {
        return sf::Color::Black; // Inside the set
    }
    
    // Logarithmic mapping for better visual distribution
    float normalized = std::log(static_cast<float>(iteration) + 1.0f) / std::log(static_cast<float>(maxIterations));
    int index = static_cast<int>(normalized * (colorPalette.size() - 1));
    
    if (index >= 0 && index < static_cast<int>(colorPalette.size())) {
        return colorPalette[index];
    }
    
    return sf::Color::White;
}

void RendererSFML::updateMandelbrotData(const std::vector<int>& data) {
    if (data.size() != static_cast<size_t>(textureWidth * textureHeight)) {
        std::cerr << "Data size mismatch: expected " << (textureWidth * textureHeight) << ", got " << data.size() << std::endl;
        return;
    }
    
    // Prepare a pixel buffer (RGBA)
    std::vector<sf::Uint8> pixels(textureWidth * textureHeight * 4);
    for (size_t i = 0; i < textureWidth * textureHeight; ++i) {
        sf::Color color = getColor(data[i]);
        pixels[i * 4 + 0] = color.r;
        pixels[i * 4 + 1] = color.g;
        pixels[i * 4 + 2] = color.b;
        pixels[i * 4 + 3] = 255;
    }
    
    // Update texture
    texture.update(pixels.data());
    dataUpdated = true;
}

void RendererSFML::updateMandelbrotData(const int* data, int size) {
    if (size != textureWidth * textureHeight) {
        std::cerr << "Data size mismatch: expected " << (textureWidth * textureHeight) << ", got " << size << std::endl;
        return;
    }
    
    // Prepare a pixel buffer (RGBA)
    std::vector<sf::Uint8> pixels(textureWidth * textureHeight * 4);
    for (int i = 0; i < textureWidth * textureHeight; ++i) {
        sf::Color color = getColor(data[i]);
        pixels[i * 4 + 0] = color.r;
        pixels[i * 4 + 1] = color.g;
        pixels[i * 4 + 2] = color.b;
        pixels[i * 4 + 3] = 255;
    }
    
    // Update texture
    texture.update(pixels.data());
    dataUpdated = true;
}

void RendererSFML::setMandelbrotParams(double realCenter, double imagCenter, double distance, int maxIterations) {
    this->maxIterations = maxIterations;
    
    // Dynamically adjust max iterations based on zoom level for better detail at high zoom
    double zoomLevel = 1.0 / distance;
    if (zoomLevel > 1000000) {  // At very high zoom levels
        int suggestedIterations = static_cast<int>(std::min(10000.0, maxIterations * std::log10(zoomLevel / 1000.0)));
        if (suggestedIterations > maxIterations) {
            this->maxIterations = suggestedIterations;
            std::cout << "[INFO] High zoom detected (zoom: " << static_cast<int>(zoomLevel) 
                      << "), increasing max iterations to " << this->maxIterations << std::endl;
        }
    }
    
    // Check for precision limits
    double aspect = static_cast<double>(viewport.getWidth()) / viewport.getHeight();
    double realRange = distance * aspect;
    double pixelWidth = realRange / viewport.getWidth();
    double relativePrecision = pixelWidth / std::abs(realCenter);
    
    if (relativePrecision < 1e-14) {
        std::cout << "[WARNING] Approaching double precision limits!" << std::endl;
        std::cout << "[WARNING] Zoom level: " << static_cast<int>(zoomLevel) << std::endl;
        std::cout << "[WARNING] Relative precision needed: " << relativePrecision << std::endl;
        std::cout << "[WARNING] You may experience pixelation artifacts." << std::endl;
    }
    
    // Update viewport with new parameters
    viewport.setCenter(realCenter, imagCenter);
    viewport.setDistance(distance);
    
    // Reset visual transformation
    isTransforming = false;
    sprite.setScale(1.0f, 1.0f);
    sprite.setPosition(0, 0);
    
    // Update visual viewport to match (visual uses visible complex height)
    visualViewport.setCenter(realCenter, imagCenter);
    visualViewport.setDistance(distance / static_cast<double>(overscanFactor));
    
    // Recreate color palette for new max iterations
    setupColorPalette();
}

void RendererSFML::setParamsChangedCallback(std::function<void(double, double, double, int)> callback) {
    onParamsChanged = callback;
}

void RendererSFML::setWindowResizeCallback(std::function<void(int, int)> callback) {
    onWindowResize = callback;
}

void RendererSFML::update() {
    // Check if we should commit zoom after timeout
    if (isInteracting && !isDragging) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastZoomTime);
        if (elapsed.count() >= ZOOM_COMMIT_TIMEOUT_MS) {
            commitZoom();
        }
    }
}

void RendererSFML::updateTexture() {
    if (dataUpdated) {
        dataUpdated = false;
    }
}

void RendererSFML::render() {
    window.clear(sf::Color::Black);
    
    // Update texture if data changed
    updateTexture();
    
    // Apply visual transformation only during interaction
    if (isInteracting)
        applyVisualTransformation();
    else {
        // When not interacting, the sprite is not scaled (1:1 pixel mapping).
        // It is positioned to center the oversized texture within the window.
        float offsetX = (viewport.getWidth() - visualViewport.getWidth()) / 2.0f;
        float offsetY = (viewport.getHeight() - visualViewport.getHeight()) / 2.0f;

        sprite.setPosition(-offsetX, -offsetY);
        sprite.setScale(1.0f, 1.0f);
    }
    
    // Draw the Mandelbrot image
    window.draw(sprite);
    
    // Draw UI
    drawUI();
    
    window.display();
}

void RendererSFML::setPresetSelectedCallback(std::function<void(int)> callback) {
    onPresetSelected = callback;
}

void RendererSFML::setPresets(const std::vector<PresetInfo>& presetList) {
    presets = presetList;
}

void RendererSFML::setComputeInfo(const std::string& mode, double timeMs) {
    computeMode = mode;
    lastComputeTimeMs = timeMs;
}

void RendererSFML::drawUI() {
    if (!fontLoaded) return;

    window.setTitle("Mandelbrot Set");

    if (showInfo) drawInfoPanel();
    if (showPresets) drawPresetPanel();
    if (showHelp) drawHelpBar();
}

void RendererSFML::drawInfoPanel() {
    const float padding = 10.0f;
    const float lineHeight = 18.0f;
    const unsigned int fontSize = 13;

    // Prepare text lines
    char buf[256];
    std::vector<std::string> lines;

    snprintf(buf, sizeof(buf), "Center: (%.12f, %.12f)", viewport.getCenterReal(), viewport.getCenterImag());
    lines.push_back(buf);

    double zoom = 1.0 / viewport.getDistance();
    if (zoom >= 1e6) snprintf(buf, sizeof(buf), "Zoom: %.3e", zoom);
    else snprintf(buf, sizeof(buf), "Zoom: %.1f", zoom);
    lines.push_back(buf);

    snprintf(buf, sizeof(buf), "Iterations: %d", maxIterations);
    lines.push_back(buf);

    snprintf(buf, sizeof(buf), "Mode: %s", computeMode.c_str());
    lines.push_back(buf);

    snprintf(buf, sizeof(buf), "Compute: %.1f ms", lastComputeTimeMs);
    lines.push_back(buf);

    float panelWidth = 320.0f;
    float panelHeight = padding * 2 + lineHeight * lines.size();

    sf::RectangleShape bg(sf::Vector2f(panelWidth, panelHeight));
    bg.setPosition(10.0f, 10.0f);
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    bg.setOutlineColor(sf::Color(255, 255, 255, 60));
    bg.setOutlineThickness(1.0f);
    window.draw(bg);

    for (size_t i = 0; i < lines.size(); i++) {
        sf::Text text(lines[i], uiFont, fontSize);
        text.setFillColor(sf::Color(220, 220, 220));
        text.setPosition(10.0f + padding, 10.0f + padding + i * lineHeight);
        window.draw(text);
    }
}

void RendererSFML::drawPresetPanel() {
    if (presets.empty()) return;

    const float padding = 10.0f;
    const float buttonHeight = 28.0f;
    const float buttonSpacing = 4.0f;
    const float panelWidth = 180.0f;
    const unsigned int fontSize = 13;

    sf::Vector2u winSize = window.getSize();
    float panelHeight = padding * 2 + 20.0f + (buttonHeight + buttonSpacing) * presets.size();
    float panelX = winSize.x - panelWidth - 10.0f;
    float panelY = 10.0f;

    sf::RectangleShape bg(sf::Vector2f(panelWidth, panelHeight));
    bg.setPosition(panelX, panelY);
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    bg.setOutlineColor(sf::Color(255, 255, 255, 60));
    bg.setOutlineThickness(1.0f);
    window.draw(bg);

    sf::Text title("Presets", uiFont, 14);
    title.setFillColor(sf::Color(255, 255, 255));
    title.setStyle(sf::Text::Bold);
    title.setPosition(panelX + padding, panelY + padding);
    window.draw(title);

    presetButtonBounds.resize(presets.size());

    for (size_t i = 0; i < presets.size(); i++) {
        float btnX = panelX + padding;
        float btnY = panelY + padding + 22.0f + i * (buttonHeight + buttonSpacing);
        float btnW = panelWidth - padding * 2;

        presetButtonBounds[i] = sf::FloatRect(btnX, btnY, btnW, buttonHeight);

        sf::RectangleShape btn(sf::Vector2f(btnW, buttonHeight));
        btn.setPosition(btnX, btnY);

        if (static_cast<int>(i) == hoveredPreset) {
            btn.setFillColor(sf::Color(80, 120, 200, 200));
        } else {
            btn.setFillColor(sf::Color(60, 60, 60, 180));
        }
        btn.setOutlineColor(sf::Color(255, 255, 255, 40));
        btn.setOutlineThickness(1.0f);
        window.draw(btn);

        char label[64];
        snprintf(label, sizeof(label), "%zu. %s", i + 1, presets[i].name.c_str());
        sf::Text btnText(label, uiFont, fontSize);
        btnText.setFillColor(sf::Color(220, 220, 220));
        btnText.setPosition(btnX + 8.0f, btnY + 5.0f);
        window.draw(btnText);
    }
}

void RendererSFML::drawHelpBar() {
    const unsigned int fontSize = 12;
    sf::Vector2u winSize = window.getSize();

    std::string helpStr = "Drag: Pan | Scroll: Zoom | 1-" + std::to_string(presets.size()) +
                          ": Presets | H: Help | I: Info | P: Presets | R: Reset | Esc: Quit";

    sf::Text text(helpStr, uiFont, fontSize);
    sf::FloatRect textBounds = text.getLocalBounds();

    float barHeight = 28.0f;
    float barY = winSize.y - barHeight;

    sf::RectangleShape bg(sf::Vector2f(static_cast<float>(winSize.x), barHeight));
    bg.setPosition(0.0f, barY);
    bg.setFillColor(sf::Color(0, 0, 0, 180));
    window.draw(bg);

    text.setFillColor(sf::Color(180, 180, 180));
    text.setPosition((winSize.x - textBounds.width) / 2.0f, barY + (barHeight - textBounds.height) / 2.0f - 2.0f);
    window.draw(text);
}

void RendererSFML::handleEvents() {
    sf::Event event;
    while (window.pollEvent(event)) {
        switch (event.type) {
            case sf::Event::Closed:
                window.close();
                break;
                
            case sf::Event::Resized:
                handleWindowResize(event.size.width, event.size.height);
                break;
                
            case sf::Event::MouseButtonPressed:
                if (event.mouseButton.button == sf::Mouse::Left) {
                    handleMousePress(event.mouseButton.x, event.mouseButton.y, true);
                }
                break;
                
            case sf::Event::MouseButtonReleased:
                if (event.mouseButton.button == sf::Mouse::Left) {
                    handleMouseRelease(event.mouseButton.x, event.mouseButton.y);
                }
                break;
                
            case sf::Event::MouseMoved:
                handleMouseMove(event.mouseMove.x, event.mouseMove.y);
                break;
                
            case sf::Event::MouseWheelScrolled:
                handleMouseWheel(static_cast<int>(event.mouseWheelScroll.delta));
                break;
                
            case sf::Event::KeyPressed:
                if (event.key.code == sf::Keyboard::Escape) {
                    window.close();
                } else if (event.key.code == sf::Keyboard::H) {
                    showHelp = !showHelp;
                } else if (event.key.code == sf::Keyboard::I) {
                    showInfo = !showInfo;
                } else if (event.key.code == sf::Keyboard::P) {
                    showPresets = !showPresets;
                } else if (event.key.code == sf::Keyboard::R) {
                    if (onPresetSelected && !presets.empty()) onPresetSelected(1);
                } else if (event.key.code >= sf::Keyboard::Num1 && event.key.code <= sf::Keyboard::Num9) {
                    int idx = event.key.code - sf::Keyboard::Num1 + 1;
                    if (onPresetSelected && idx >= 1 && idx <= static_cast<int>(presets.size()))
                        onPresetSelected(idx);
                }
                break;
        }
    }
}

bool RendererSFML::isOpen() const {
    return window.isOpen();
}

void RendererSFML::handleWindowResize(int newWidth, int newHeight) {
    // The window itself has been resized. We need to update our state.
    // newWidth and newHeight are the new WINDOW dimensions.

    // Update the view to match the new window size
    sf::View view = window.getView();
    view.setSize(static_cast<float>(newWidth), static_cast<float>(newHeight));
    view.setCenter(static_cast<float>(newWidth) / 2.f, static_cast<float>(newHeight) / 2.f);
    window.setView(view);

    // The texture size needs to be recalculated based on the new window size
    int newTextureWidth = static_cast<int>(newWidth * overscanFactor);
    int newTextureHeight = static_cast<int>(newHeight * overscanFactor);

    textureWidth = newTextureWidth;
    textureHeight = newTextureHeight;
    
    // Update viewports with new size and sync their centers/distances
    viewport.updateWindowSize(textureWidth, textureHeight);
    visualViewport.updateWindowSize(newWidth, newHeight);
    visualViewport.setCenter(viewport.getCenterReal(), viewport.getCenterImag());
    // visual uses visible complex height (window spans a subset of texture)
    visualViewport.setDistance(viewport.getDistance() / static_cast<double>(overscanFactor));

    // Cancel any ongoing interaction
    isDragging = false;
    isInteracting = false;
    
    // Recreate texture for new size
    if (!texture.create(textureWidth, textureHeight)) {
        std::cerr << "Failed to recreate texture for new window size" << std::endl;
        return;
    }
    sprite.setTexture(texture, true);
    
    // Reset visual transformation to prevent artifacts
    sprite.setPosition(0, 0);
    sprite.setScale(1.0f, 1.0f);
    
    // Mark that data needs to be updated for new size
    dataUpdated = true;
    
    // Notify main application of window resize
    if (onWindowResize) {
        onWindowResize(newWidth, newHeight);
    }
    
    std::cout << "[DEBUG] Renderer: Window resized to " << newWidth << "x" << newHeight << std::endl;
    std::cout << "[DEBUG] Renderer: Texture size is now " << texture.getSize().x << "x" << texture.getSize().y << std::endl;
    std::cout << "[DEBUG] Renderer: viewport size is " << viewport.getWidth() << "x" << viewport.getHeight() << std::endl;
    std::cout << "[DEBUG] Renderer: visualViewport size is " << visualViewport.getWidth() << "x" << visualViewport.getHeight() << std::endl;
}

void RendererSFML::handleMousePress(int x, int y, bool leftButton) {
    if (leftButton) {
        // Check if clicking on a preset button
        if (showPresets) {
            for (size_t i = 0; i < presetButtonBounds.size(); i++) {
                if (presetButtonBounds[i].contains(static_cast<float>(x), static_cast<float>(y))) {
                    if (onPresetSelected) onPresetSelected(static_cast<int>(i) + 1);
                    return;
                }
            }
        }
        isDragging = true;
        isInteracting = true;
        // Store the starting positions for the drag operation
        startMousePos = sf::Vector2i(x, y);
        startSpritePos = sprite.getPosition();
        lastMousePos = startMousePos; // Initialize lastMousePos for delta calcs
        
        // Sync visual viewport to the committed viewport's state
        // Visual distance uses visible complex height
        visualViewport.setCenter(viewport.getCenterReal(), viewport.getCenterImag());
        visualViewport.setDistance(viewport.getDistance() / static_cast<double>(overscanFactor));

        isTransforming = true;
    }
}

void RendererSFML::handleMouseRelease(int x, int y) {
    if (isDragging) {
        isDragging = false;
        isInteracting = false;

        // The visual viewport has been updated by the sequence of pans.
        // Now we commit this state to the main viewport (convert visible -> texture distance).
        viewport.setCenter(visualViewport.getCenterReal(), visualViewport.getCenterImag());
        viewport.setDistance(visualViewport.getDistance() * static_cast<double>(overscanFactor));
        
        if (onParamsChanged) {
            onParamsChanged(viewport.getCenterReal(), viewport.getCenterImag(), viewport.getDistance(), maxIterations);
        }
    }
}

void RendererSFML::handleMouseMove(int x, int y) {
    // Update preset hover state
    hoveredPreset = -1;
    if (showPresets) {
        for (size_t i = 0; i < presetButtonBounds.size(); i++) {
            if (presetButtonBounds[i].contains(static_cast<float>(x), static_cast<float>(y))) {
                hoveredPreset = static_cast<int>(i);
                break;
            }
        }
    }

    if (isDragging) {
        // For panning, we calculate the new sprite position directly
        // relative to the start of the drag for a stable 1:1 mapping.
        float deltaX = static_cast<float>(x - startMousePos.x);
        float deltaY = static_cast<float>(y - startMousePos.y);
        
        sprite.setPosition(startSpritePos.x + deltaX, startSpritePos.y + deltaY);

        // We still need to update the visual viewport for the final commit.
        // This tracks the total pan distance in the complex plane with subpixel precision.
        int frameDeltaX = x - lastMousePos.x;
        int frameDeltaY = y - lastMousePos.y;

        // Visual viewport operates in window units with visible complex height:
        // no overscan scaling needed.
        visualViewport.panPrecise(static_cast<double>(frameDeltaX), static_cast<double>(frameDeltaY));
        lastMousePos = sf::Vector2i(x, y);
    }
}

void RendererSFML::handleMouseWheel(int delta) {
    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
    isInteracting = true;
    lastZoomTime = std::chrono::steady_clock::now();
    
    // Calculate zoom factor
    double zoomFactor = delta > 0 ? 0.9 : 1.1;
    
    // Debug invariants before applying zoom
    auto preComplex = visualViewport.screenToComplex(mousePos.x, mousePos.y);
    std::cout << "[DEBUG][Zoom] Mouse (" << mousePos.x << ", " << mousePos.y << ")"
              << " preComplex=(" << preComplex.first << ", " << preComplex.second << ")"
              << " visualDist=" << visualViewport.getDistance()
              << " textureDist=" << viewport.getDistance()
              << " overscan=" << overscanFactor
              << std::endl;

    // Use viewport's zoom method
    visualViewport.zoomAt(mousePos.x, mousePos.y, zoomFactor);

    // Reproject the same complex back to screen in the updated visual viewport
    auto postPxVisual = visualViewport.complexToScreenPrecise(preComplex.first, preComplex.second);
    double errVisualX = postPxVisual.first - static_cast<double>(mousePos.x);
    double errVisualY = postPxVisual.second - static_cast<double>(mousePos.y);
    std::cout << "[DEBUG][Zoom] Visual reprojection after zoom: screen=("
              << postPxVisual.first << ", " << postPxVisual.second << ")"
              << " error=(" << errVisualX << ", " << errVisualY << ")"
              << " newVisualDist=" << visualViewport.getDistance()
              << std::endl;

    // Predict on-screen position using the sprite mapping we actually apply
    // Use distances in the same unit system (visible complex height)
    double committedVisibleDist = viewport.getDistance() / static_cast<double>(overscanFactor);
    double scale = committedVisibleDist / visualViewport.getDistance();
    auto visualCenter = std::make_pair(visualViewport.getCenterReal(), visualViewport.getCenterImag());
    auto pixelOfVisualCenterPrecise = viewport.complexToScreenPrecise(visualCenter.first, visualCenter.second);
    float targetX = visualViewport.getWidth() / 2.0f;
    float targetY = visualViewport.getHeight() / 2.0f;
    float offsetX = targetX - (static_cast<float>(pixelOfVisualCenterPrecise.first) * static_cast<float>(scale));
    float offsetY = targetY - (static_cast<float>(pixelOfVisualCenterPrecise.second) * static_cast<float>(scale));
    auto anchorTexPx = viewport.complexToScreenPrecise(preComplex.first, preComplex.second);
    float predictedScreenX = offsetX + static_cast<float>(anchorTexPx.first) * static_cast<float>(scale);
    float predictedScreenY = offsetY + static_cast<float>(anchorTexPx.second) * static_cast<float>(scale);
    double errSpriteX = static_cast<double>(predictedScreenX) - static_cast<double>(mousePos.x);
    double errSpriteY = static_cast<double>(predictedScreenY) - static_cast<double>(mousePos.y);
    std::cout << "[DEBUG][Zoom] Sprite mapping predicted screen=(" << predictedScreenX << ", " << predictedScreenY
              << ") error=(" << errSpriteX << ", " << errSpriteY << ") scale=" << scale
              << std::endl;
    applyVisualTransformation();
}

void RendererSFML::applyVisualTransformation() {
    // The goal is to transform the sprite (which represents the `viewport`)
    // so that it visually matches the state of `visualViewport`.

    // 1. Calculate the scale factor using visible complex heights (same unit system)
    double committedVisibleDist = viewport.getDistance() / static_cast<double>(overscanFactor);
    double scale = committedVisibleDist / visualViewport.getDistance();

    // 2. The anchor for the transformation is the center of the target view.
    //    Find where the center of the `visualViewport` is located in the texture's pixel coords.
    std::pair<double, double> visualCenter = {visualViewport.getCenterReal(), visualViewport.getCenterImag()};
    // Use precise (subpixel) mapping to avoid rounding drift during interaction
    std::pair<double, double> pixelOfVisualCenterPrecise = viewport.complexToScreenPrecise(visualCenter.first, visualCenter.second);

    // 3. The target on-screen position for this anchor point is the center of the window.
    float targetX = visualViewport.getWidth() / 2.0f;
    float targetY = visualViewport.getHeight() / 2.0f;

    // 4. We want to position the sprite such that the point `pixelOfVisualCenter` (after scaling) 
    //    ends up at `(targetX, targetY)`.
    //    The screen position of a texture point `p` is `spritePos + p * scale`.
    //    So, we solve for the sprite's position: `spritePos = target - pixelOfVisualCenter * scale`.
    float offsetX = targetX - (static_cast<float>(pixelOfVisualCenterPrecise.first) * static_cast<float>(scale));
    float offsetY = targetY - (static_cast<float>(pixelOfVisualCenterPrecise.second) * static_cast<float>(scale));

    sprite.setPosition(offsetX, offsetY);
    sprite.setScale(static_cast<float>(scale), static_cast<float>(scale));
}

void RendererSFML::commitZoom() {
    if (isInteracting && !isDragging) {
        isInteracting = false;
        
        // Commit visual parameters to actual parameters
        viewport.setCenter(visualViewport.getCenterReal(), visualViewport.getCenterImag());
        // Convert visible complex height back to texture complex height
        viewport.setDistance(visualViewport.getDistance() * static_cast<double>(overscanFactor));
        
        if (onParamsChanged) {
            onParamsChanged(viewport.getCenterReal(), viewport.getCenterImag(), viewport.getDistance(), maxIterations);
        }
    }
}

 