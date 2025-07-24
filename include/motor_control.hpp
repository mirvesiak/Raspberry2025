#pragma once
#include <atomic>

extern std::atomic<bool> go_shutdown;

bool start_ev3_script();
int connect_to_ev3(const char* ip, int port);

void resolvePointAABBCollision(double oldX, double oldY, double& newX, double& newY, double left, double top, double right, double bottom);
float clampAngle(float angle, float limit, bool& reachable);
void joystick_to_coordinates(int angle, int distance, double& x, double& y);
void motorLoop(int sockfd);