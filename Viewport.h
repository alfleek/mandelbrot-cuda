#pragma once
#include <utility>

class Viewport {
public:
    double centerReal;
    double centerImag;
    double distance; // Height of the view in complex plane
    int windowWidth;
    int windowHeight;

    Viewport(double real = 0.0, double imag = 0.0, double dist = 4.0, int width = 800, int height = 600)
        : centerReal(real), centerImag(imag), distance(dist), windowWidth(width), windowHeight(height) {}

    // Getters
    double getCenterReal() const { return centerReal; }
    double getCenterImag() const { return centerImag; }
    double getDistance() const { return distance; }
    int getWidth() const { return windowWidth; }
    int getHeight() const { return windowHeight; }

    // Setters
    void setCenter(double real, double imag) { centerReal = real; centerImag = imag; }
    void setCenterReal(double real) { centerReal = real; }
    void setCenterImag(double imag) { centerImag = imag; }
    void setDistance(double dist) { distance = dist; }

    void updateWindowSize(int width, int height) {
        windowWidth = width;
        windowHeight = height;
    }

    // Map screen (pixel) coordinates to complex plane
    std::pair<double, double> screenToComplex(int x, int y) const {
        // Use more precise calculation to minimize rounding errors
        double aspect = static_cast<double>(windowWidth) / windowHeight;
        double halfWidth = windowWidth * 0.5;
        double halfHeight = windowHeight * 0.5;
        
        // Calculate offsets from center in normalized coordinates [-0.5, 0.5]
        double normalizedX = (x - halfWidth) / windowWidth;
        double normalizedY = (halfHeight - y) / windowHeight;
        
        // Scale by distance and aspect ratio
        double real = centerReal + normalizedX * distance * aspect;
        double imag = centerImag + normalizedY * distance;
        
        return std::make_pair(real, imag);
    }

    // Map complex plane coordinates to screen (pixel)
    std::pair<int, int> complexToScreen(double real, double imag) const {
        // Use more precise calculation to minimize rounding errors
        double aspect = static_cast<double>(windowWidth) / windowHeight;
        double halfWidth = windowWidth * 0.5;
        double halfHeight = windowHeight * 0.5;
        
        // Calculate normalized coordinates from complex plane
        double normalizedX = (real - centerReal) / (distance * aspect);
        double normalizedY = (imag - centerImag) / distance;
        
        // Convert to screen coordinates with proper rounding
        int x = static_cast<int>(std::round(normalizedX * windowWidth + halfWidth));
        int y = static_cast<int>(std::round(halfHeight - normalizedY * windowHeight));
        
        return std::make_pair(x, y);
    }

    // Map complex plane coordinates to screen (pixel) with subpixel precision
    std::pair<double, double> complexToScreenPrecise(double real, double imag) const {
        double aspect = static_cast<double>(windowWidth) / windowHeight;
        double halfWidth = windowWidth * 0.5;
        double halfHeight = windowHeight * 0.5;
        
        double normalizedX = (real - centerReal) / (distance * aspect);
        double normalizedY = (imag - centerImag) / distance;
        
        double x = normalizedX * static_cast<double>(windowWidth) + halfWidth;
        double y = halfHeight - normalizedY * static_cast<double>(windowHeight);
        
        return std::make_pair(x, y);
    }

    // Zoom around a screen point (x, y)
    void zoomAt(int x, int y, double zoomFactor) {
        // Get the complex coordinate at mouse position before zoom
        std::pair<double, double> coords = screenToComplex(x, y);
        double mouseReal = coords.first;
        double mouseImag = coords.second;
        
        // Apply zoom
        distance *= zoomFactor;
        
        // Recalculate center so the mouse point stays fixed
        // Use the same precise calculation as screenToComplex
        double aspect = static_cast<double>(windowWidth) / windowHeight;
        double halfWidth = windowWidth * 0.5;
        double halfHeight = windowHeight * 0.5;
        
        double normalizedX = (x - halfWidth) / windowWidth;
        double normalizedY = (halfHeight - y) / windowHeight;
        
        centerReal = mouseReal - normalizedX * distance * aspect;
        centerImag = mouseImag - normalizedY * distance;
    }

    // Pan by screen deltas (in pixels)
    void pan(int deltaX, int deltaY) {
        panPrecise(static_cast<double>(deltaX), static_cast<double>(deltaY));
    }

    // Pan by screen deltas (in pixels) with subpixel precision
    void panPrecise(double deltaX, double deltaY) {
        double aspect = static_cast<double>(windowWidth) / windowHeight;
        
        // Convert pixel deltas to normalized coordinates
        double normalizedDeltaX = deltaX / static_cast<double>(windowWidth);
        double normalizedDeltaY = deltaY / static_cast<double>(windowHeight);
        
        // Apply to center coordinates
        centerReal -= normalizedDeltaX * distance * aspect;
        centerImag += normalizedDeltaY * distance;
    }
}; 