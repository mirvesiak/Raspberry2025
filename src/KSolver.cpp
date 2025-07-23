#include "KSolver.hpp"
#include <cmath>

KSolver::KSolver(double L1, double L2, double offset) {
    this->L1 = L1;
    this->L1_2 = L1 * L1;
    this->L2 = L2;
    this->L2_2 = L2 * L2;
    this->offset = offset;
    this->d2_2 = this->L2_2 + offset * offset;
    this->d2 = std::sqrt(this->d2_2);
    this->gamma2 = std::atan(L2 / offset);
    this->beta2 = PI / 2 - this->gamma2;
    this->max_reach = L1 + std::sqrt(this->L2_2 + offset * offset);
};

bool KSolver::calculateIK(double target_X, double target_Y, double& out_A, double& out_B) {
    double d1_2 = target_X * target_X + target_Y * target_Y;
    double d1 = std::sqrt(d1_2);

    if (this->max_reach < d1) {
        out_A = std::atan2(target_X, target_Y);    // flip the x, y and the sign for the result to be relative to y axis
        out_B = -this->beta2;
        return false; // Target is out of reach
    }
    else {
        double alpha = std::acos((d1_2 + this->L1_2 - this->d2_2) / (2 * d1 * this->L1));
        double beta1 = std::acos((this->d2_2 + this->L1_2 - d1_2) / (2 * this->d2 * this->L1));

        out_A = std::atan2(target_X, target_Y) - alpha;    // flip the x, y and the sign for the result to be relative to y axis
        out_B = PI - beta1 - this->beta2;
        return true; // Target is reachable
    }
}

void KSolver::calculateFK(double& target_X, double& target_Y, double out_A, double out_B) {
    target_X = this->L1 * std::sin(out_A) + this->d2 * std::sin(out_A + out_B + this->beta2);
    target_Y = this->L1 * std::cos(out_A) + this->d2 * std::cos(out_A + out_B + this->beta2);
}