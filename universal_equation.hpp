// universal_equation.hpp
// This file defines a special math tool called UniversalEquation that helps us understand how different "dimensions" (like 1D, 2D, 3D, up to 9D) work together as "spheres of influence" in a big system, like a cosmic dance of fields and frequencies!

#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>   // This helps us store lists of numbers
#include <cmath>    // This gives us math functions like cos() and exp()
#include <utility>  // This lets us return two numbers at once (like a pair)

// The UniversalEquation class is like a magic calculator for dimensions, fields, and frequencies.
class UniversalEquation {
public:
    // Constructor: This sets up our calculator with default settings.
    // Imagine we're building a toy with knobs we can adjust to change how it works.
    UniversalEquation(
        int maxDimensions = 9,     // Maximum number of dimensions (like 1D to 9D)
        double influence = 1.0,    // Strength of the 1D universal base (like a big hug from 1D)
        double weak = 0.5,         // A weaker effect for higher dimensions (like a soft push)
        double collapse = 0.5,     // How much the "collapse" force pulls things together
        double twoD = 0.5,         // Special strength for 2D's influence (like 2D's special dance move)
        double alpha = 5.0,        // How fast influence fades with distance (like shouting quieter far away)
        double beta = 0.2          // Controls how the collapse force changes across dimensions
    )
        : maxDimensions_(maxDimensions), // Save the max number of dimensions
          currentDimension_(1),         // Start at 1D (our base dimension)
          kInfluence_(influence),      // Save 1D's strength
          kWeak_(weak),                // Save the weak effect for higher dimensions
          kCollapse_(collapse),        // Save the collapse force strength
          kTwoD_(twoD),                // Save 2D's special strength
          alpha_(alpha),               // Save the fading factor
          beta_(beta) {                // Save the collapse modulator
        initializeDimensions();        // Set up our list of dimensions to work with
    }

    // Setters: These let us change the knobs on our calculator.
    // Think of them as turning dials to adjust how strong things are.
    void setInfluence(double value) { kInfluence_ = value; } // Change 1D's strength
    void setWeak(double value) { kWeak_ = value; }           // Change the weak effect
    void setCollapse(double value) { kCollapse_ = value; }   // Change the collapse force
    void setTwoD(double value) { kTwoD_ = value; }           // Change 2D's special strength
    void setAlpha(double value) { alpha_ = value; }          // Change how fast influence fades
    void setBeta(double value) { beta_ = value; }            // Change the collapse modulator

    // Getters: These tell us the current settings of our knobs.
    double getInfluence() const { return kInfluence_; } // What's 1D's strength?
    double getWeak() const { return kWeak_; }           // What's the weak effect?
    double getCollapse() const { return kCollapse_; }   // What's the collapse force?
    double getTwoD() const { return kTwoD_; }           // What's 2D's special strength?
    double getAlpha() const { return alpha_; }          // How fast does influence fade?
    double getBeta() const { return beta_; }            // What's the collapse modulator?

    // DimensionData: This is like a little note card that holds info about two dimensions talking to each other.
    struct DimensionData {
        int dPrime;       // The other dimension we're interacting with (like 4D talking to 5D)
        double distance;  // How far apart the dimensions are (like how many steps between 4D and 5D)
    };

    // calculateInfluenceTerm: This figures out how strongly two dimensions affect each other.
    // Imagine two friends (dimensions) passing a ball: if they're close, it's a strong pass; if far, it's weak.
    double calculateInfluenceTerm(int d, int dPrime) const {
        if (d == 1 && dPrime == 1) return kInfluence_; // Special case: 1D talking to itself is super strong!
        double distance = std::abs(d - dPrime); // How far apart are the dimensions?
        double modifier = (d > 3 && dPrime > 3) ? kWeak_ : 1.0; // Higher dimensions (above 3D) have a weaker effect
        // The influence is based on distance divided by a big number (d raised to dPrime power)
        return kInfluence_ * (distance / std::pow(d, dPrime)) * modifier;
    }

    // calculateCollapseTerm: This is like a force that pulls or pushes the dimensions' energy.
    // It changes based on the dimension and has a wiggly (cosine) part to make it feel like a wave.
    double calculateCollapseTerm(int d) const {
        static const double omega = 2.0 * M_PI / 19.0; // The "frequency" of our wave, based on 19 steps in the hierarchy
        if (d == 1) return 0.0; // No collapse for 1D, because it's the infinite base
        // Collapse grows with dimension (d), but fades a bit (exp(-beta * (d-1))) and wiggles (cos)
        return kCollapse_ * d * std::exp(-beta_ * (d - 1)) * std::abs(std::cos(omega * d));
    }

    // compute: This is the main calculator! It gives two answers: a positive one and a negative one.
    // Think of it like a seesaw: one side goes up (positive), the other goes down (negative).
    std::pair<double, double> compute() const {
        // Start with 1D's universal influence, like a big blanket over everything
        double sphereInfluence = kInfluence_;
        // If we're at 2D or higher, add 2D's special influence (like a special dance move)
        if (currentDimension_ >= 2) {
            static const double omega = 2.0 * M_PI / 19.0; // Same frequency as above
            sphereInfluence += kTwoD_ * std::cos(omega * currentDimension_); // 2D's wiggly contribution
        }
        // Add up influences from nearby dimensions (like D-1, D, D+1)
        for (const auto& data : dimensionPairs_) {
            // Each influence gets weaker if the dimensions are farther apart (exp(-alpha * distance))
            sphereInfluence += calculateInfluenceTerm(currentDimension_, data.dPrime) * 
                              std::exp(-alpha_ * data.distance);
        }
        // Get the collapse force (the seesaw's push or pull)
        double collapse = calculateCollapseTerm(currentDimension_);
        // Return two results: one with a push up, one with a push down
        return {sphereInfluence + collapse, sphereInfluence - collapse};
    }

    // setCurrentDimension: Choose which dimension we're working with (like picking a toy to play with).
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension; // Set the current dimension
            updateDimensionPairs();        // Update the list of nearby dimensions to interact with
        }
    }

    // getCurrentDimension: Tells us which dimension we're using right now.
    int getCurrentDimension() const { return currentDimension_; }

    // getMaxDimensions: Tells us the highest dimension we can use (like 9D).
    int getMaxDimensions() const { return maxDimensions_; }

private:
    // These are our calculator's settings, like the knobs and dials we mentioned
    int maxDimensions_;       // The highest dimension (e.g., 9D)
    int currentDimension_;    // The dimension we're working with right now (e.g., 3D)
    double kInfluence_;       // Strength of 1D's universal influence
    double kWeak_;            // Weaker effect for dimensions above 3D
    double kCollapse_;        // Strength of the collapse force
    double kTwoD_;            // Special strength for 2D's influence
    double alpha_;            // How fast influence fades with distance
    double beta_;             // Controls how collapse changes across dimensions
    std::vector<DimensionData> dimensionPairs_; // List of nearby dimensions we interact with

    // initializeDimensions: Sets up our list of dimensions to start the calculator.
    void initializeDimensions() {
        updateDimensionPairs(); // Get ready to calculate interactions
    }

    // updateDimensionPairs: Makes a list of dimensions close to the current one (D-1, D, D+1).
    // Think of it like picking which friends are close enough to talk to.
    void updateDimensionPairs() {
        dimensionPairs_.clear(); // Clear the old list
        int start = std::max(1, currentDimension_ - 1); // Start at D-1, but not below 1
        int end = std::min(maxDimensions_, currentDimension_ + 1); // End at D+1, but not above max
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            // Add each nearby dimension and its distance to our list
            dimensionPairs_.push_back({dPrime, static_cast<double>(std::abs(currentDimension_ - dPrime))});
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP