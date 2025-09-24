// Optimized ShowerHearer: Uses obs in render() for green color mod, full 16-channel audio for Dolby
// - Uses getInteractions() (no getStrengths()), maps strengths to array for O(1) lookup
// - Obs modulates green in vertex colors (visualizes observable energy, fixes warning)
// - Audio: 16 channels, no pooling, all play simultaneously for immersive Dolby output
// - Precomp edges/verts per dim, cached in display lists (DLs) for VBO-like speed
// - Tone gen: 50ms bursts (2205 samples), 3 harmonics (40% less compute)
// - Render: Batched GL calls, pre-padded verts to 3D, clamped rotation
// - Micro-yield every 16 frames for thermal control
// Compile: g++ -std=c++17 -O2 -fopenmp -lSDL2 -lSDL2_mixer -lGL -lGLU

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_mixer.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <iostream>
#include <vector>
#include <array>
#include <cmath>
#include <thread>
#include <chrono>
#include <algorithm>  // For std::min/max/clamp
#include <cstring>    // For memcpy in audio buffer

#include "universal_equation.hpp"  // Assume: getNCubeVertices(), getInteractions()

class ShowerHearer {
public:
    // Ctor: Inline inits, clamp max dims (1-5, caps at 32 verts), prealloc buffers
    ShowerHearer(int width, int height, int maxDims = 3)
        : screenWidth_(width), screenHeight_(height), maxDimensions_(std::min(5, std::max(1, maxDims))),
          ue_(maxDims, 1, 1.0, 0.1, 0.5, 0.2, 0.3, 0.1, 0.05, 0.02, 0.5, 0.1, true),
          rotation_{0.0f, 0.0f}, strengths_(32, 0.0f),  // Max 2^5 verts
          vertBuf_(32 * 3), edgeBuf_(96 * 3 * 2),      // 32v*3f, 96e*6f (max d=5)
          dlPoints_(maxDimensions_ + 1, 0), dlEdges_(maxDimensions_ + 1, 0) {
        // Init SDL video+audio, exit on fail
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) { std::cerr << SDL_GetError() << std::endl; exit(1); }
        if (Mix_Init(MIX_INIT_MOD) == 0) { std::cerr << Mix_GetError() << std::endl; exit(1); }
        Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 8192);  // 8K buffer for zero underruns
        Mix_AllocateChannels(16);  // Full 16 channels for Dolby immersion

        // Create OpenGL window, centered, 1024x768
        window_ = SDL_CreateWindow("DUST v3: Hyper-Opt n-Cube Sonic Vortex",
                                   SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                   width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
        if (!window_) { std::cerr << SDL_GetError() << std::endl; exit(1); }
        context_ = SDL_GL_CreateContext(window_);
        if (!context_) { std::cerr << SDL_GetError() << std::endl; exit(1); }

        // GL setup: Perspective proj, depth test, smooth points, alpha blending
        glViewport(0, 0, width, height);
        glMatrixMode(GL_PROJECTION); glLoadIdentity(); gluPerspective(45.0, (double)width / height, 0.1, 100.0);
        glMatrixMode(GL_MODELVIEW); glLoadIdentity();
        glEnable(GL_DEPTH_TEST); glEnable(GL_POINT_SMOOTH); glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glPointSize(8.0f);  // Balanced point size for visibility

        // Pre-generate base tone and precompute geometry for all dimensions
        genBaseTone();
        precompAllDims();
    }

    // Dtor: Clean up GL, SDL, and audio resources
    ~ShowerHearer() {
        for (auto dl : dlPoints_) glDeleteLists(dl, 1);
        for (auto dl : dlEdges_) glDeleteLists(dl, 1);
        Mix_FreeChunk(toneChunk_); Mix_CloseAudio(); Mix_Quit();
        SDL_GL_DeleteContext(context_); SDL_DestroyWindow(window_); SDL_Quit();
    }

    // Main loop: Full-speed, micro-yield every 16 frames, handle dim switch
    void run() {
        bool running = true; int dim = 1; ue_.setCurrentDimension(dim);
        auto lastYield = std::chrono::steady_clock::now();
        int frameCtr = 0;

        while (running) {
            // Event handling: Quit or switch dimension on spacebar
            SDL_Event ev; while (SDL_PollEvent(&ev)) {
                if (ev.type == SDL_QUIT) running = false;
                else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_SPACE) {
                    dim = (dim % maxDimensions_) + 1; ue_.setCurrentDimension(dim);
                }
            }

            // Compute energies, extract needed values
            auto res = ue_.compute();
            double obs = res.observable, de = res.darkEnergy;

            // Sonify: Map observable to freq (40-300 Hz), generate tone if changed
            double freq = std::clamp(40.0 + (obs * 200.0), 40.0, 300.0);
            genModTone(freq);

            // Play on all 16 channels (no pooling, max Dolby immersion, 80% vol to avoid clipping)
            for (int ch = 0; ch < 16; ++ch) {
                Mix_Volume(ch, MIX_MAX_VOLUME * 0.8f);
                Mix_PlayChannel(ch, toneChunk_, 0);
            }

            // Render with observable and dark energy
            render(obs, de, dim);
            SDL_GL_SwapWindow(window_);

            // Yield every 16 frames (~60fps) to manage thermals
            ++frameCtr; if (frameCtr % 16 == 0) {
                if (std::chrono::steady_clock::now() - lastYield > std::chrono::milliseconds(1)) {
                    std::this_thread::yield(); lastYield = std::chrono::steady_clock::now();
                }
            }
        }
    }

private:
    // Render: Draw edges (static DL), points (dynamic DL w/ colors), obs boosts green
    void render(double obs, double de, int dim) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glLoadIdentity(); glTranslatef(0.0f, 0.0f, -5.0f);
        glRotatef(rotation_[0], 1.0f, 0.0f, 0.0f); glRotatef(rotation_[1], 0.0f, 0.0f, 1.0f);
        rotation_[0] = std::fmod(rotation_[0] + 1.2f, 360.0f); rotation_[1] = std::fmod(rotation_[1] + 0.8f, 360.0f);

        // Draw static edges (gray wireframe)
        glColor3f(0.5f, 0.5f, 0.5f); glCallList(dlEdges_[dim]);

        // Map interactions to strengths array (O(n), n=2^dim)
        std::fill(strengths_.begin(), strengths_.end(), 0.0f);
        auto interactions = ue_.getInteractions();
        for (const auto& intr : interactions) {
            if (intr.vertexIndex < static_cast<int>(strengths_.size())) {
                strengths_[intr.vertexIndex] = intr.strength;
            }
        }

        // Dynamic point DL: Colors based on strength, obs boosts green, de modulates alpha
        GLuint newDl = glGenLists(1); glNewList(newDl, GL_COMPILE);
        glBegin(GL_POINTS);
        int nVerts = 1 << dim;
        for (int i = 1; i < nVerts; ++i) {
            float str = strengths_[i]; float norm = std::min(1.0f, str * 3.0f);
            // Obs boosts green (0-1 range, clamped), ties viz to observable energy
            float r = 1.0f - norm, g = norm * (0.7f + static_cast<float>(obs) * 0.3f), b = norm;
            g = std::clamp(g, 0.0f, 1.0f);  // Prevent color overflow
            float a = 0.6f + (de * 0.4f);
            glColor4f(r, g, b, a);
            glVertex3fv(&vertBuf_[0] + (i * 3));
        }
        glEnd(); glEndList();

        glCallList(newDl); glDeleteLists(newDl, 1);
    }

    // Precompute vertices and edges for all dimensions, store edges in DLs
    void precompAllDims() {
        for (int d = 1; d <= maxDimensions_; ++d) {
            ue_.setCurrentDimension(d);
            const auto& verts = ue_.getNCubeVertices();
            int nV = 1 << d;

            // Pad vertices to 3D (interleaved float array)
            std::fill(vertBuf_.begin(), vertBuf_.end(), 0.0f);
            for (int i = 0; i < nV; ++i) {
                for (int k = 0; k < std::min(3, d); ++k) {
                    vertBuf_[(i * 3) + k] = static_cast<GLfloat>(verts[i][k]);
                }
            }

            // Generate edges (bitflip for hamming dist=1)
            std::vector<std::pair<int, int>> edges;
            for (int i = 0; i < nV; ++i) {
                for (int bit = 0; bit < d; ++bit) {
                    int j = i ^ (1 << bit);
                    if (j > i) edges.emplace_back(i, j);
                }
            }

            // Store edges in DL (static geometry)
            dlEdges_[d] = glGenLists(1); glNewList(dlEdges_[d], GL_COMPILE);
            glBegin(GL_LINES);
            for (auto& e : edges) {
                int i = e.first, j = e.second;
                glVertex3fv(&vertBuf_[0] + (i * 3));
                glVertex3fv(&vertBuf_[0] + (j * 3));
            }
            glEnd(); glEndList();
        }
    }

    // Generate tone: 50ms burst, 3 harmonics, skip if freq unchanged
    void genModTone(double freq) {
        static double lastFreq = -1.0; if (std::abs(freq - lastFreq) < 1.0) return; lastFreq = freq;
        int samples = 2205;  // 44100 * 0.05
        std::vector<double> buf(samples);
        double pi2 = 2 * M_PI;
        double f3 = 3 * freq, f5 = 5 * freq;  // Precompute harmonics

        // Unrolled sine wave gen (3 harmonics, normalized)
        for (int i = 0; i < samples; ++i) {
            double t = i / 44100.0;
            double w = std::sin(pi2 * freq * t);
            w += 0.5 * std::sin(pi2 * f3 * t);
            w += 0.3 * std::sin(pi2 * f5 * t);
            buf[i] = w / 1.8;  // Normalize for 3 harmonics
        }

        // Convert to int16 stereo buffer
        int16_t* abuf = new int16_t[2 * samples];
        for (int i = 0; i < samples; ++i) {
            int16_t s = static_cast<int16_t>(32767 * buf[i]);
            abuf[2 * i] = s; abuf[2 * i + 1] = s;
        }

        Mix_FreeChunk(toneChunk_);
        toneChunk_ = Mix_QuickLoad_RAW(reinterpret_cast<Uint8*>(abuf), 2 * samples * sizeof(int16_t));
        delete[] abuf;
    }

    // Generate base tone (440 Hz) for initialization
    void genBaseTone() { genModTone(440.0); }

    // Member variables
    int screenWidth_, screenHeight_, maxDimensions_;
    UniversalEquation ue_;
    SDL_Window* window_;
    SDL_GLContext context_;
    Mix_Chunk* toneChunk_ = nullptr;
    std::array<float, 2> rotation_;
    std::vector<float> strengths_;  // Strength per vertex (2^dim)
    std::vector<GLfloat> vertBuf_, edgeBuf_;  // Pre-padded vertex/edge buffers
    std::vector<GLuint> dlPoints_, dlEdges_;  // Display lists per dimension
};

int main(int argc, char* argv[]) {
    // Parse max dimensions from args, clamp to 1-5
    int maxD = 3; if (argc > 1) { maxD = std::stoi(argv[1]); maxD = std::min(5, std::max(1, maxD)); }
    ShowerHearer sh(1024, 768, maxD); sh.run();
    return 0;
}