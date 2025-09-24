#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>  // For audio; compile with -lSDL2_mixer
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <vector>
#include <array>  // Added for std::array
#include <cmath>
#include <thread>
#include <chrono>

// Assume UniversalEquation.hpp is included and compiled with this
// TODO: In universal_equation.hpp, add public getter:
// const std::vector<std::vector<double>>& getNCubeVertices() const { return nCubeVertices_; }
#include "universal_equation.hpp"

class ShowerHearer {
public:
    ShowerHearer(int width, int height, int maxDims = 3)
        : screenWidth_(width), screenHeight_(height), maxDimensions_(maxDims),
          ue_(maxDims, 1, 1.0, 0.1, 0.5, 0.2, 0.3, 0.1, 0.05, 0.02, 0.5, 0.1, true),
          rotation_{0.0f, 0.0f} {  // Fixed: Use aggregate init
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
            std::cerr << "SDL init failed: " << SDL_GetError() << std::endl;
            exit(1);
        }
        if (Mix_Init(MIX_INIT_MOD) == 0) {
            std::cerr << "Mixer init failed: " << Mix_GetError() << std::endl;
            exit(1);
        }
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);  // Doubled buffer size for smoother audio

        // Allocate twice as many channels for fuller sound
        Mix_AllocateChannels(16);

        window_ = SDL_CreateWindow("DUST v2 Shower Hearer: n-Cube Permeation Viz (Twice as Good)",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!window_) {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            exit(1);
        }

        context_ = SDL_GL_CreateContext(window_);
        if (!context_) {
            std::cerr << "GL context failed: " << SDL_GetError() << std::endl;
            exit(1);
        }

        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluPerspective(45.0, (double)width / height, 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_POINT_SMOOTH);
        glPointSize(10.0f);  // Doubled point size for brighter shower

        // Precompute sine wave for audio tones (simple beep generator)
        generateTone(440.0, 1000);  // Base tone; will modulate
    }

    ~ShowerHearer() {
        Mix_FreeChunk(toneChunk_);
        Mix_CloseAudio();
        Mix_Quit();
        SDL_GL_DeleteContext(context_);
        SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    void run() {
        bool running = true;
        int dim = 1;
        ue_.setMode(dim);

        while (running) {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                } else if (event.type == SDL_KEYDOWN) {
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        // Advance dimension on spacebar
                        dim = (dim % maxDimensions_) + 1;
                        ue_.setCurrentDimension(dim);
                        std::cout << "Advanced to dimension: " << dim << std::endl;
                    }
                }
            }

            // Full speed: No FPS throttle or delays

            // Compute energies
            auto result = ue_.compute();
            observable_ = result.observable;
            potential_ = result.potential;
            darkMatter_ = result.darkMatter;
            darkEnergy_ = result.darkEnergy;

            // Sonify: Doubled freq range for wider, richer modulation (40-320 Hz)
            double packedFreq = 40.0 + (observable_ * 280.0);  // High observable -> much higher tone
            generateTone(packedFreq, 100);  // Short burst

            // Play on twice as many channels (16) at full volume for immersive, doubled intensity
            for (int ch = 0; ch < 16; ++ch) {
                Mix_Volume(ch, MIX_MAX_VOLUME);
                Mix_PlayChannel(ch, toneChunk_, 0);
            }

            // Render
            render();
            SDL_GL_SwapWindow(window_);
        }
    }

private:
    void render() {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity();
        glTranslatef(0.0f, 0.0f, -5.0f);
        glRotatef(rotation_[0], 1.0f, 0.0f, 0.0f);
        glRotatef(rotation_[1], 0.0f, 1.0f, 0.0f);
        rotation_[0] += 1.0f;  // Doubled rotation speed for dynamic shower
        rotation_[1] += 0.6f;  // Doubled rotation speed

        // Get n-cube vertices and interactions
        auto interactions = ue_.getInteractions();
        // Fixed: Use getter (add to header: const std::vector<std::vector<double>>& getNCubeVertices() const { return nCubeVertices_; })
        const auto& vertices = ue_.getNCubeVertices();

        // Draw vertices as points, colored by strength/permeation (rainbow shower effect)
        glBegin(GL_POINTS);
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (i == 0) continue;  // Skip reference

            auto it = std::find_if(interactions.begin(), interactions.end(),
                                   [i](const auto& intr) { return intr.vertexIndex == static_cast<int>(i); });
            float strength = it != interactions.end() ? it->strength : 0.0f;
            float normStrength = std::min(1.0f, strength * 5.0f);  // Normalize for color

            // Enhanced Color: Fuller rainbow (add green for mid-strength vibrancy)
            float r = (1.0f - normStrength);
            float g = normStrength * 0.5f;  // Doubled green component for richer hues
            float b = normStrength;
            float alpha = 0.5f + (darkEnergy_ * 0.5f);

            glColor4f(r, g, b, alpha);

            // Fixed: Cast double* to GLfloat* and use min(3, dim) coords for 3D proj
            int dim = std::min(3, static_cast<int>(vertices[i].size()));
            GLfloat coords[3] = {0.0f, 0.0f, 0.0f};
            for (int k = 0; k < dim; ++k) {
                coords[k] = static_cast<GLfloat>(vertices[i][k]);
            }
            glVertex3fv(coords);
        }
        glEnd();

        // Draw edges for wireframe shower (octant connections)
        glColor3f(0.5f, 0.5f, 0.5f);
        glBegin(GL_LINES);
        for (size_t i = 0; i < vertices.size(); ++i) {
            for (size_t j = i + 1; j < vertices.size(); ++j) {
                // Simple hamming distance check for edges (differ by 1 coord)
                int diffCount = 0;
                size_t minSize = std::min(vertices[i].size(), vertices[j].size());
                for (size_t k = 0; k < minSize; ++k) {
                    if (std::abs(vertices[i][k] - vertices[j][k]) > 0.1) ++diffCount;
                }
                if (diffCount == 1) {  // Edge
                    // Fixed: Same cast/projection
                    int dim = std::min(3, static_cast<int>(vertices[i].size()));
                    GLfloat coordsI[3] = {0.0f, 0.0f, 0.0f};
                    GLfloat coordsJ[3] = {0.0f, 0.0f, 0.0f};
                    for (int k = 0; k < dim; ++k) {
                        coordsI[k] = static_cast<GLfloat>(vertices[i][k]);
                        coordsJ[k] = static_cast<GLfloat>(vertices[j][k]);
                    }
                    glVertex3fv(coordsI);
                    glVertex3fv(coordsJ);
                }
            }
        }
        glEnd();

        // HUD: Print energies (use gluOrtho2D for overlay if needed)
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, screenWidth_, screenHeight_, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);

        glColor3f(1.0f, 1.0f, 1.0f);
        // Simple text via GLUT or printf overlay; here, placeholder
        // For real text, integrate FreeType or ImGui

        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glEnable(GL_DEPTH_TEST);
    }

    void generateTone(double freq, int durationMs) {
        // Simple sine wave chunk with doubled harmonics for fuller, richer audio
        int samples = (44100 * durationMs / 1000);
        std::vector<double> buffer(samples);
        for (int i = 0; i < samples; ++i) {
            double time = i / 44100.0;
            double wave = 0.0;
            // Fundamental
            wave += std::sin(2 * M_PI * freq * time);
            // Odd harmonics for richer tone (square-like) - doubled count and amplitude
            wave += 0.6 * std::sin(2 * M_PI * 3 * freq * time);  // Doubled amp
            wave += 0.4 * std::sin(2 * M_PI * 5 * freq * time);  // Doubled amp
            wave += 0.2 * std::sin(2 * M_PI * 7 * freq * time);
            wave += 0.15 * std::sin(2 * M_PI * 9 * freq * time);  // Added extra harmonic
            wave += 0.1 * std::sin(2 * M_PI * 11 * freq * time); // Added extra harmonic
            // Normalize amplitude (adjusted for more harmonics)
            buffer[i] = wave / 2.45;
        }

        // Unfiltered: No low-pass filter, hear all harmonics for twice the auditory detail

        // Convert to int16_t stereo (use raw buffer)
        int16_t* audioBuffer = new int16_t[2 * samples];
        for (int i = 0; i < samples; ++i) {
            int16_t sample = static_cast<int16_t>(32767 * buffer[i]);
            audioBuffer[2 * i] = sample;     // Left
            audioBuffer[2 * i + 1] = sample; // Right
        }

        Mix_FreeChunk(toneChunk_);
        toneChunk_ = Mix_QuickLoad_RAW(reinterpret_cast<Uint8*>(audioBuffer), 2 * samples * sizeof(int16_t));
        delete[] audioBuffer;
    }

    int screenWidth_, screenHeight_, maxDimensions_;
    UniversalEquation ue_;
    SDL_Window* window_;
    SDL_GLContext context_;
    Mix_Chunk* toneChunk_ = nullptr;
    double observable_ = 0.0, potential_ = 0.0, darkMatter_ = 0.0, darkEnergy_ = 0.0;
    std::array<float, 2> rotation_;  // Fixed: Declare here, init in ctor
};

int main(int argc, char* argv[]) {
    if (argc > 1) {
        int maxDims = std::stoi(argv[1]);
        ShowerHearer shower(1024, 768, maxDims);
        shower.run();
    } else {
        ShowerHearer shower(1024, 768);
        shower.run();
    }
    return 0;
}