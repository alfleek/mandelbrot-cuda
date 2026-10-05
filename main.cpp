#include <iostream>
#include <chrono>
#include <string>
#include <vector>

#include "gpu_detector.h"
#include "mandelbrot_cpu.h"
#include "renderer_sfml.h"

// CUDA function declarations
extern "C" cudaError_t compute_mandelbrot_cuda(
    int* results,
    double realMin, double realMax,
    double imagMin, double imagMax,
    int width, int height, int maxIterations,
    bool useOptimizedKernel,
    bool useFloatPrecision
);

extern "C" void checkCudaError(cudaError_t error, const char* msg);

// Preset configurations (matching original Java program)
struct Preset {
    std::string name;
    double realCenter;
    double imagCenter;
    double distance;
    int maxIterations;
};

const std::vector<Preset> presets = {
    {"Full Set", 0.0, 0.0, 4.0, 1000},
    {"Detail 1", -0.745428, 0.113009, 0.00003, 1000},
    {"Detail 2", -1.25066, 0.02012, 0.00017, 1000},
    {"Detail 3", -0.748, 0.1, 0.0014, 1000},
    {"Detail 4", -0.7453, 0.1127, 0.00065, 1000},
    {"Detail 5", -0.16, 1.0405, 0.026, 1000}
};

class MandelbrotApp {
private:
    GPUDetector gpuDetector;
    MandelbrotCPU cpuComputer;
    RendererSFML renderer;
    
    // Overscan factor to render more than the visible area
    const float OVERSCAN_FACTOR = 1.4f;

    bool useGPU;
    bool gpuOptimized;
    bool gpuFloatPrecision;
    
    int windowWidth;
    int windowHeight;
    
    // Current parameters
    double realCenter;
    double imagCenter;
    double distance;
    int maxIterations;
    
    std::vector<int> results;

public:
    MandelbrotApp() : useGPU(false), gpuOptimized(false), gpuFloatPrecision(false),
                      windowWidth(1600), windowHeight(900),  // 16:9 aspect ratio
                      realCenter(0.0), imagCenter(0.0), distance(4.0), maxIterations(1000) {
    }
    
    bool initialize(int argc, char** argv) {
        std::cout << "=== Mandelbrot Set - CUDA Implementation ===" << std::endl;
        
        // Check CUDA availability
        if (gpuDetector.isCUDAAvailable()) {
            std::cout << "CUDA is available. Scanning for GPUs..." << std::endl;
            
            if (gpuDetector.scanGPUs()) {
                useGPU = true;
                const GPUInfo& gpu = gpuDetector.getSelectedGPU();
                
                // Determine optimization settings based on GPU capabilities
                gpuOptimized = gpu.computeCapabilityMajor >= 3; // Use optimized kernel for compute capability 3.0+
                gpuFloatPrecision = !gpu.supportsDoublePrecision; // Use float if double precision not supported
                
                std::cout << "Using GPU: " << gpu.name << std::endl;
                std::cout << "Optimized kernel: " << (gpuOptimized ? "Yes" : "No") << std::endl;
                std::cout << "Float precision: " << (gpuFloatPrecision ? "Yes" : "No") << std::endl;
            } else {
                std::cout << "No suitable GPU found. Falling back to CPU." << std::endl;
                useGPU = false;
            }
        } else {
            std::cout << "CUDA not available. Using CPU implementation." << std::endl;
            useGPU = false;
        }
        
        // Initialize renderer with oversized texture
        int textureWidth = static_cast<int>(windowWidth * OVERSCAN_FACTOR);
        int textureHeight = static_cast<int>(windowHeight * OVERSCAN_FACTOR);
        if (!renderer.init(textureWidth, textureHeight, windowWidth, windowHeight)) {
            std::cout << "Failed to initialize renderer. Running in console mode." << std::endl;
            return false;
        }
        
        // Set up callback for parameter changes
        renderer.setParamsChangedCallback([this](double real, double imag, double dist, int maxIter) {
            this->handleParamsChanged(real, imag, dist, maxIter);
        });
        
        // Set up callback for window resize events
        renderer.setWindowResizeCallback([this](int newWidth, int newHeight) {
            this->handleWindowResize(newWidth, newHeight);
        });
        
        // Set initial parameters to the first preset
        const Preset& initialPreset = presets[0];
        setParameters(initialPreset.realCenter, initialPreset.imagCenter, initialPreset.distance, initialPreset.maxIterations);
        
        return true;
    }
    
    void setParameters(double realCenter, double imagCenter, double distance, int maxIterations) {
        // Incoming distance is the VISIBLE window complex height
        this->realCenter = realCenter;
        this->imagCenter = imagCenter;
        this->distance = distance; // keep as visible distance internally
        this->maxIterations = maxIterations;
        int textureWidth = static_cast<int>(windowWidth * OVERSCAN_FACTOR);
        int textureHeight = static_cast<int>(windowHeight * OVERSCAN_FACTOR);
        double textureDistance = distance * static_cast<double>(OVERSCAN_FACTOR);

        // Update CPU computer with oversized texture dimensions
        cpuComputer.setParameters(realCenter, imagCenter, textureDistance, maxIterations, textureWidth, textureHeight);
        
        // Update renderer
        renderer.setMandelbrotParams(realCenter, imagCenter, textureDistance, maxIterations);
        
        // Resize results vector to match oversized texture size
        results.resize(static_cast<size_t>(textureWidth) * textureHeight);
    }
    
    void compute() {
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (useGPU) {
            computeGPU();
        } else {
            computeCPU();
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        std::cout << "Computation time: " << duration.count() / 1000.0 << " ms" << std::endl;
        
        // Update renderer with results
        renderer.updateMandelbrotData(results);
    }
    
    void computeGPU() {
        std::cout << "Computing on GPU..." << std::endl;
        
        // Get current viewport parameters from renderer
        const Viewport& currentViewport = renderer.getViewport();
        double realCenter = currentViewport.getCenterReal();
        double imagCenter = currentViewport.getCenterImag();
        double distance = currentViewport.getDistance();
        
        // Calculate coordinate ranges with proper aspect ratio handling
        // IMPORTANT: Use the viewport's dimensions (the oversized texture), not the window's
        double aspectRatio = static_cast<double>(currentViewport.getWidth()) / currentViewport.getHeight();
        double realRange = distance * aspectRatio;
        double imagRange = distance;
        
        double realMin = realCenter - realRange / 2.0;
        double realMax = realCenter + realRange / 2.0;
        double imagMin = imagCenter - imagRange / 2.0;
        double imagMax = imagCenter + imagRange / 2.0;
        
        // Debug output for aspect ratio verification
        std::cout << "Texture: " << currentViewport.getWidth() << "x" << currentViewport.getHeight() << " (aspect: " << aspectRatio << ")" << std::endl;
        std::cout << "Ranges: real=" << realRange << ", imag=" << imagRange << std::endl;
        std::cout << "Bounds: real[" << realMin << ", " << realMax << "], imag[" << imagMin << ", " << imagMax << "]" << std::endl;
        
        // Call CUDA kernel
        cudaError_t error = compute_mandelbrot_cuda(
            results.data(),
            realMin, realMax,
            imagMin, imagMax,
            currentViewport.getWidth(), currentViewport.getHeight(), maxIterations,
            gpuOptimized, gpuFloatPrecision
        );
        
        if (error != cudaSuccess) {
            std::cout << "GPU computation failed: " << cudaGetErrorString(error) << std::endl;
            std::cout << "Falling back to CPU..." << std::endl;
            computeCPU();
        }
    }
    
    void computeCPU() {
        std::cout << "Computing on CPU..." << std::endl;
        
        // Use optimized CPU computation
        cpuComputer.computeOptimized();
        
        // Copy results
        const std::vector<int>& cpuResults = cpuComputer.getResults();
        results.assign(cpuResults.begin(), cpuResults.end());
    }
    
    void showPresets() {
        std::cout << "\nAvailable presets:" << std::endl;
        for (size_t i = 0; i < presets.size(); i++) {
            std::cout << i + 1 << ". " << presets[i].name << std::endl;
        }
        std::cout << "0. Custom parameters" << std::endl;
    }
    
    void selectPreset(int presetIndex) {
        if (presetIndex >= 1 && presetIndex <= (int)presets.size()) {
            const Preset& preset = presets[presetIndex - 1];
            setParameters(preset.realCenter, preset.imagCenter, preset.distance, preset.maxIterations);
            std::cout << "Selected preset: " << preset.name << std::endl;
        } else if (presetIndex == 0) {
            getCustomParameters();
        } else {
            std::cout << "Invalid preset selection." << std::endl;
        }
    }
    
    void getCustomParameters() {
        std::cout << "Enter custom parameters:" << std::endl;
        
        std::cout << "Real center: ";
        std::cin >> realCenter;
        
        std::cout << "Imaginary center: ";
        std::cin >> imagCenter;
        
        std::cout << "Distance: ";
        std::cin >> distance;
        
        std::cout << "Max iterations: ";
        std::cin >> maxIterations;
        
        setParameters(realCenter, imagCenter, distance, maxIterations);
    }
    
    void run() {
        // Initial computation
        compute();

        while (renderer.isOpen()) {
            renderer.handleEvents();
            renderer.update();
            renderer.render();
        }
    }
    
    // Getters for interactive features
    double getRealCenter() const { return realCenter; }
    double getImagCenter() const { return imagCenter; }
    double getDistance() const { return distance; }
    int getMaxIterations() const { return maxIterations; }
    
    void setRealCenter(double value) { realCenter = value; }
    void setImagCenter(double value) { imagCenter = value; }
    void setDistance(double value) { distance = value; }
    void setMaxIterations(int value) { maxIterations = value; }
    
    void recompute() {
        setParameters(realCenter, imagCenter, distance, maxIterations);
        compute();
    }
    
    void handleParamsChanged(double real, double imag, double dist, int maxIter) {
        // Renderer reports TEXTURE complex height; convert to VISIBLE for internal state
        this->realCenter = real;
        this->imagCenter = imag;
        this->distance = dist / static_cast<double>(OVERSCAN_FACTOR);
        this->maxIterations = maxIter;
        int textureWidth = static_cast<int>(windowWidth * OVERSCAN_FACTOR);
        int textureHeight = static_cast<int>(windowHeight * OVERSCAN_FACTOR);

        // Update CPU computer with current window dimensions
        cpuComputer.setParameters(real, imag, dist, maxIter, textureWidth, textureHeight);
        
        // Resize results vector if needed
        if (results.size() != static_cast<size_t>(textureWidth * textureHeight)) {
            results.resize(static_cast<size_t>(textureWidth) * textureHeight);
        }
        
        // Recompute with new parameters
        compute();
    }
    
    void handleWindowResize(int newWidth, int newHeight) {
        windowWidth = newWidth;
        windowHeight = newHeight;
        
        int textureWidth = static_cast<int>(windowWidth * OVERSCAN_FACTOR);
        int textureHeight = static_cast<int>(windowHeight * OVERSCAN_FACTOR);

        // Get current viewport parameters from renderer
        const Viewport& currentViewport = renderer.getViewport();
        
        // Update CPU computer with new dimensions and current viewport
        cpuComputer.setParameters(
            currentViewport.getCenterReal(), 
            currentViewport.getCenterImag(), 
            currentViewport.getDistance(), 
            maxIterations, 
            textureWidth, 
            textureHeight
        );
        
        // Resize results vector
        results.resize(static_cast<size_t>(textureWidth) * textureHeight);
        
        // Recompute with new window size
        compute();
        
        std::cout << "Application updated for new window size: " << windowWidth << "x" << windowHeight << std::endl;
    }
};

int main(int argc, char** argv) {
    MandelbrotApp app;
    
    if (app.initialize(argc, argv)) {
        app.run();
    } else {
        std::cerr << "Failed to initialize MandelbrotApp" << std::endl;
        return 1;
    }
    
    return 0;
} 