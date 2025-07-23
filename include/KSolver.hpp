#ifndef KSOLVER_HPP
#define KSOLVER_HPP

#define PI 3.14159265358979323846

class KSolver {
    double L1;
    double L1_2;
    double L2;
    double L2_2;
    double offset;
    double d2;
    double d2_2;
    double gamma2;
    double beta2;
    double max_reach;
public:
    KSolver(double L1, double L2, double offset);
    bool calculateIK(double targetX, double targetY, double& outA, double& outB);
    void calculateFK(double& targetX, double& targetY, double outA, double outB);
};

#endif // IKSOLVER_HPP