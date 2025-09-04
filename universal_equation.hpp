// universal_equation.hpp
// This file is like a magical toy box that helps us explore a universe where dimensions (like 1D, 2D, 3D, up to 9D) 
// act like glowing spheres that influence each other! Each dimension is like a bubble that lives inside the bubble 
// below it (e.g., 3D is inside 2D, 4D is inside 3D). 1D is super special because it's infinite, like a giant blanket 
// wrapping everything. The 2D-1D-2D part in our pattern (1D-9D-8D-...-2D-1D-2D-...-1D) is like a sandwich with 1D 
// in the middle and 2D bubbles around it. The bubbles talk to each other strongly if they're close, but barely whisper 
// if they're far apart. The answers come in pairs (plus and minus), like a seesaw going up and down, showing the 
// universe's wiggly, wavy energy!

#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>   // This helps us make lists to store our bubble friends
#include <cmath>    // This gives us fun math tricks like cos (wiggly waves) and exp (fading whispers)
#include <utility>  // This lets us give two answers at once, like a plus and minus pair

// The UniversalEquation is our toy box that calculates how dimensions talk to each other and make waves!
class UniversalEquation {
public:
    // Constructor: This is like setting up our toy box with all the knobs and dials we need to play with.
    UniversalEquation(
        int maxDimensions = 9,     // The biggest dimension we can play with (like 9D)
        double influence = 1.0,    // How strong 1D's giant blanket hugs everything
        double weak = 0.5,         // A softer push for big dimensions (above 3D)
        double collapse = 0.5,     // How much the universe pulls or pushes the bubbles' energy
        double twoD = 0.5,         // How special 2D's wiggle is in the dance
        double permeation = 2.0,   // How much a dimension (like 3D) loves to fill the one below it (like 2D)
        double alpha = 5.0,        // How fast the bubbles' voices fade when they're far apart
        double beta = 0.2          // How the pulling force changes as we go to bigger dimensions
    )
        : maxDimensions_(maxDimensions), // Save the biggest dimension
          currentDimension_(1),         // Start with 1D, the infinite blanket
          kInfluence_(influence),      // Save 1D's hugging strength
          kWeak_(weak),                // Save the soft push for big dimensions
          kCollapse_(collapse),        // Save the pulling force strength
          kTwoD_(twoD),                // Save 2D's special wiggle strength
          kPermeation_(permeation),    // Save how much dimensions fill the ones below
          alpha_(alpha),               // Save the fading speed
          beta_(beta) {                // Save the pulling force changer
        initializeDimensions();        // Get our bubble friends ready to play
    }

    // Setters: These are like turning the dials on our toy box to change how it works.
    void setInfluence(double value) { kInfluence_ = value; }   // Change 1D's hugging strength
    void setWeak(double value) { kWeak_ = value; }             // Change the soft push for big dimensions
    void setCollapse(double value) { kCollapse_ = value; }     // Change the pulling force
    void setTwoD(double value) { kTwoD_ = value; }             // Change 2D's wiggle strength
    void setPermeation(double value) { kPermeation_ = value; } // Change how much dimensions fill the ones below
    void setAlpha(double value) { alpha_ = value; }            // Change how fast voices fade
    void setBeta(double value) { beta_ = value; }              // Change the pulling force changer

    // Getters: These tell us what the dials are set to right now.
    double getInfluence() const { return kInfluence_; }   // What's 1D's hugging strength?
    double getWeak() const { return kWeak_; }             // What's the soft push?
    double getCollapse() const { return kCollapse_; }     // What's the pulling force?
    double getTwoD() const { return kTwoD_; }             // What's 2D's wiggle strength?
    double getPermeation() const { return kPermeation_; } // How much do dimensions fill the ones below?
    double getAlpha() const { return alpha_; }            // How fast do voices fade?
    double getBeta() const { return beta_; }              // What's the pulling force changer?

    // DimensionData: This is like a little card that tells us about a bubble friend we're talking to.
    struct DimensionData {
        int dPrime;       // The friend dimension (like 4D if we're in 5D)
        double distance;  // How far apart the bubbles are (like steps between 5D and 4D)
    };

    // calculateInfluenceTerm: This figures out how loudly two bubbles talk to each other.
    // If they're close, they shout; if far, they whisper. 1D is super loud when talking to itself!
    double calculateInfluenceTerm(int d, int dPrime) const {
        if (d == 1 && dPrime == 1) return kInfluence_; // 1D talking to 1D is a big hug!
        double distance = std::abs(d - dPrime); // How many steps apart are the bubbles?
        double modifier = (d > 3 && dPrime > 3) ? kWeak_ : 1.0; // Big bubbles (above 3D) talk softer
        // The talk strength is based on how far apart they are, divided by a big number
        return kInfluence_ * (distance / std::pow(d, dPrime)) * modifier;
    }

    // calculatePermeationFactor: This makes a bubble (like 3D) talk extra loud to the bubble below it (like 2D).
    // It's like 3D is living inside 2D, so they share a special connection!
    double calculatePermeationFactor(int d, int dPrime) const {
        if (d >= 2 && dPrime == d - 1) return kPermeation_; // Extra loud if talking to the bubble below
        return 1.0; // Normal talk for other bubbles
    }

    // calculateCollapseTerm: This is like a magical pull that makes the bubbles' energy go up or down.
    // It wiggles like a wave and gets softer for bigger dimensions.
    double calculateCollapseTerm(int d) const {
        static const double omega = 2.0 * M_PI / 19.0; // The wave's speed, based on our 19-step pattern
        if (d == 1) return 0.0; // No pull for 1D, because it's the giant blanket
        // The pull grows with the dimension, but fades (exp) and wiggles (cos)
        return kCollapse_ * d * std::exp(-beta_ * (d - 1)) * std::abs(std::cos(omega * d));
    }

    // compute: This is the heart of our toy box! It gives two answers: one up, one down, like a seesaw.
    // It adds up all the bubble talks and adds or subtracts the pull to make waves.
    std::pair<double, double> compute() const {
        // Start with 1D's big hug, like a warm blanket over all bubbles
        double sphereInfluence = kInfluence_;
        // If we're at 2D or higher, add 2D's special wiggle, like a fun dance move
        if (currentDimension_ >= 2) {
            static const double omega = 2.0 * M_PI / 19.0; // Same wave speed
            sphereInfluence += kTwoD_ * std::cos(omega * currentDimension_);
        }
        // Talk to nearby bubbles (like D-1, D, D+1)
        for (const auto& data : dimensionPairs_) {
            // Each talk gets quieter if the bubbles are far apart (exp(-alpha * distance))
            // And extra loud if talking to the bubble below (permeation)
            sphereInfluence += calculateInfluenceTerm(currentDimension_, data.dPrime) * 
                              std::exp(-alpha_ * data.distance) * 
                              calculatePermeationFactor(currentDimension_, data.dPrime);
        }
        // Get the pull force (up or down)
        double collapse = calculateCollapseTerm(currentDimension_);
        // Give two answers: one with a push up, one with a push down
        return {sphereInfluence + collapse, sphereInfluence - collapse};
    }

    // setCurrentDimension: Pick which bubble we're playing with (like choosing 3D to explore).
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension; // Set the bubble
            updateDimensionPairs();        // Find its nearby friends
        }
    }

    // getCurrentDimension: Tells us which bubble we're playing with right now.
    int getCurrentDimension() const { return currentDimension_; }

    // getMaxDimensions: Tells us the biggest bubble we can play with (like 9D).
    int getMaxDimensions() const { return maxDimensions_; }

private:
    // These are the settings and parts of our toy box
    int maxDimensions_;       // The biggest bubble (e.g., 9D)
    int currentDimension_;    // The bubble we're playing with now (e.g., 3D)
    double kInfluence_;       // How strong 1D's blanket hug is
    double kWeak_;            // Softer push for big bubbles (above 3D)
    double kCollapse_;        // How strong the pull force is
    double kTwoD_;            // How strong 2D's wiggle is
    double kPermeation_;      // How much a bubble loves the one below it
    double alpha_;            // How fast bubble talks fade when far apart
    double beta_;             // How the pull force changes for bigger bubbles
    std::vector<DimensionData> dimensionPairs_; // List of nearby bubble friends

    // initializeDimensions: Gets our toy box ready by setting up the bubble friends.
    void initializeDimensions() {
        updateDimensionPairs(); // Make the list of friends
    }

    // updateDimensionPairs: Finds the nearby bubbles (D-1, D, D+1) to talk to.
    // It's like making a list of friends who live next door.
    void updateDimensionPairs() {
        dimensionPairs_.clear(); // Clear the old friend list
        int start = std::max(1, currentDimension_ - 1); // Start at the bubble below, but not below 1D
        int end = std::min(maxDimensions_, currentDimension_ + 1); // End at the bubble above, but not above 9D
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            // Add each friend and how far they are
            dimensionPairs_.push_back({dPrime, static_cast<double>(std::abs(currentDimension_ - dPrime))});
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP