// universal_equation.hpp
// This file is like a magical playground where we explore a universe made of dimensions (1D, 2D, 3D, up to 9D) 
// that fit inside each other like Russian dolls! 1D is an infinite, giant blanket that wraps everything up. 
// 2D is like the edge or skin of a big bubble that holds all the dimensions, not a bubble itself. 
// Then, 3D lives inside and spreads throughout 2D, 4D lives inside and spreads throughout 3D, and so on, 
// up to 9D! The pattern 1D-9D-8D-...-2D-1D-2D-...-1D is like a dance where 2D-1D-2D (the middle part) 
// shows 2D as the bubble's edge with 1D in the center. The dimensions talk to each other: close ones shout, 
// far ones whisper, and each dimension loves to fill the one below it. We get two answers (plus and minus) 
// like a seesaw, showing the universe's wiggly energy waves!

#ifndef UNIVERSAL_EQUATION_HPP
#define UNIVERSAL_EQUATION_HPP

#include <vector>   // This helps us make lists to keep track of our dimension friends
#include <cmath>    // This gives us cool math tools like cos (wiggly waves) and exp (fading whispers)
#include <utility>  // This lets us give two answers at once, like a plus and minus pair

// The UniversalEquation is our playground toy that figures out how dimensions talk and wave!
class UniversalEquation {
public:
    // Constructor: This sets up our toy with dials to adjust how it works.
    // Think of it like building a playground with sliders to change the game!
    UniversalEquation(
        int maxDimensions = 9,     // The biggest dimension we can play with (like 9D)
        double influence = 1.0,    // How strong 1D's giant blanket hugs everything
        double weak = 0.5,         // A softer voice for big dimensions (above 3D)
        double collapse = 0.5,     // How much the energy pull or push works
        double twoD = 0.5,         // How strong 2D's edge wiggle is
        double permeation = 2.0,   // How much a dimension loves to fill the one below it
        double alpha = 5.0,        // How fast voices fade when dimensions are far apart
        double beta = 0.2          // How the pull changes as dimensions get bigger
    )
        : maxDimensions_(maxDimensions), // Save the biggest dimension
          currentDimension_(1),         // Start with 1D, the infinite blanket
          kInfluence_(influence),      // Save 1D's hugging strength
          kWeak_(weak),                // Save the soft voice for big dimensions
          kCollapse_(collapse),        // Save the pull strength
          kTwoD_(twoD),                // Save 2D's edge wiggle strength
          kPermeation_(permeation),    // Save how much dimensions fill the ones below
          alpha_(alpha),               // Save the fading speed
          beta_(beta) {                // Save the pull changer
        initializeDimensions();        // Get our dimension friends ready to play
    }

    // Setters: These are like sliders to change the playground rules.
    void setInfluence(double value) { kInfluence_ = value; }   // Change 1D's hugging strength
    void setWeak(double value) { kWeak_ = value; }             // Change the soft voice
    void setCollapse(double value) { kCollapse_ = value; }     // Change the pull strength
    void setTwoD(double value) { kTwoD_ = value; }             // Change 2D's edge wiggle
    void setPermeation(double value) { kPermeation_ = value; } // Change how much dimensions fill below
    void setAlpha(double value) { alpha_ = value; }            // Change the fading speed
    void setBeta(double value) { beta_ = value; }              // Change the pull changer

    // Getters: These tell us where the sliders are set right now.
    double getInfluence() const { return kInfluence_; }   // What's 1D's hugging strength?
    double getWeak() const { return kWeak_; }             // What's the soft voice?
    double getCollapse() const { return kCollapse_; }     // What's the pull strength?
    double getTwoD() const { return kTwoD_; }             // What's 2D's edge wiggle?
    double getPermeation() const { return kPermeation_; } // How much do dimensions fill below?
    double getAlpha() const { return alpha_; }            // How fast do voices fade?
    double getBeta() const { return beta_; }              // What's the pull changer?

    // DimensionData: This is like a little note card about a dimension friend we're chatting with.
    struct DimensionData {
        int dPrime;       // The friend dimension (like 4D if we're in 5D)
        double distance;  // How many steps apart the dimensions are
    };

    // calculateInfluenceTerm: This figures out how loudly two dimensions talk.
    // Close friends shout, far friends whisper, and 1D talking to itself is a big hug!
    double calculateInfluenceTerm(int d, int dPrime) const {
        if (d == 1 && dPrime == 1) return kInfluence_; // 1D's big hug to itself!
        double distance = std::abs(d - dPrime); // How many steps between the dimensions?
        double modifier = (d > 3 && dPrime > 3) ? kWeak_ : 1.0; // Big dimensions talk softer
        // The talk strength depends on distance and a big number
        return kInfluence_ * (distance / std::pow(d, dPrime)) * modifier;
    }

    // calculatePermeationFactor: This makes a dimension talk extra loud to the one below it.
    // Like 3D spreading all over 2D or 4D filling 3D, because they live inside each other!
    double calculatePermeationFactor(int d, int dPrime) const {
        if (d >= 3 && dPrime == d - 1) return kPermeation_; // Extra loud to the dimension below
        if (dPrime == 2 && d >= 3) return kTwoD_;           // 2D's edge talks to higher dimensions
        return 1.0; // Normal talk for other friends
    }

    // calculateCollapseTerm: This is like a magical pull that wiggles the energy up or down.
    // It’s like a wave that gets softer for bigger dimensions.
    double calculateCollapseTerm(int d) const {
        static const double omega = 2.0 * M_PI / 19.0; // The wave speed, based on our 19-step dance
        if (d == 1) return 0.0; // No pull for 1D, the infinite blanket
        // The pull grows with the dimension, fades a bit, and wiggles
        return kCollapse_ * d * std::exp(-beta_ * (d - 1)) * std::abs(std::cos(omega * d));
    }

    // compute: This is the fun part! It adds up all the dimension talks and gives two answers.
    // Like a seesaw: one side goes up (plus), one goes down (minus), showing the energy waves!
    std::pair<double, double> compute() const {
        // Start with 1D's big blanket hug, covering all dimensions
        double sphereInfluence = kInfluence_;
        // If we're at 2D or higher, add 2D's edge wiggle, like a dance on the bubble's skin
        if (currentDimension_ >= 2) {
            static const double omega = 2.0 * M_PI / 19.0; // Same wave speed
            sphereInfluence += kTwoD_ * std::cos(omega * currentDimension_);
        }
        // Talk to nearby dimensions (like D-1, D, D+1)
        for (const auto& data : dimensionPairs_) {
            // Each talk gets quieter if far apart (exp(-alpha * distance))
            // And extra loud if filling the dimension below or talking to 2D's edge
            sphereInfluence += calculateInfluenceTerm(currentDimension_, data.dPrime) * 
                              std::exp(-alpha_ * data.distance) * 
                              calculatePermeationFactor(currentDimension_, data.dPrime);
        }
        // Get the pull force (up or down)
        double collapse = calculateCollapseTerm(currentDimension_);
        // Give two answers: one with a push up, one with a push down
        return {sphereInfluence + collapse, sphereInfluence - collapse};
    }

    // setCurrentDimension: Pick which dimension to play with, like choosing a doll in the nest!
    void setCurrentDimension(int dimension) {
        if (dimension >= 1 && dimension <= maxDimensions_) {
            currentDimension_ = dimension; // Set the current dimension
            updateDimensionPairs();        // Find its nearby friends
        }
    }

    // getCurrentDimension: Tells us which doll we're playing with now.
    int getCurrentDimension() const { return currentDimension_; }

    // getMaxDimensions: Tells us the biggest doll we can play with (like 9D).
    int getMaxDimensions() const { return maxDimensions_; }

private:
    // These are the playground's tools and settings
    int maxDimensions_;       // The biggest dimension (e.g., 9D)
    int currentDimension_;    // The dimension we're playing with now (e.g., 3D)
    double kInfluence_;       // How strong 1D's blanket hug is
    double kWeak_;            // Softer voice for big dimensions (above 3D)
    double kCollapse_;        // How strong the pull force is
    double kTwoD_;            // How strong 2D's edge wiggle is
    double kPermeation_;      // How much a dimension fills the one below
    double alpha_;            // How fast voices fade when far apart
    double beta_;             // How the pull changes for bigger dimensions
    std::vector<DimensionData> dimensionPairs_; // List of nearby dimension friends

    // initializeDimensions: Sets up our playground with the dimension friends.
    void initializeDimensions() {
        updateDimensionPairs(); // Make the friend list
    }

    // updateDimensionPairs: Finds the nearby dimensions (D-1, D, D+1) to chat with.
    // Like making a list of friends who live next door in the doll nest.
    void updateDimensionPairs() {
        dimensionPairs_.clear(); // Clear the old friend list
        int start = std::max(1, currentDimension_ - 1); // Start at the dimension below, but not below 1D
        int end = std::min(maxDimensions_, currentDimension_ + 1); // End at the dimension above, but not above 9D
        for (int dPrime = start; dPrime <= end; ++dPrime) {
            // Add each friend and how far they are
            dimensionPairs_.push_back({dPrime, static_cast<double>(std::abs(currentDimension_ - dPrime))});
        }
    }
};

#endif // UNIVERSAL_EQUATION_HPP