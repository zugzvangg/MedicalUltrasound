#ifndef SOLVER_VECTOR2_H
#define SOLVER_VECTOR2_H

#include <vector>

class Vector2 {
 private:
	double x, y;

 public:
	Vector2(double x = 0, double y = 0);

	void setX(double x);
	void setY(double y);

	double getX() const;
	double getY() const;
};

double scalar(const Vector2 &A, const Vector2 &B);
double length(const Vector2 &A, const Vector2 &B);
double area(const Vector2 &A, const Vector2 &B, const Vector2 &C);
bool doIntersect(const Vector2 &A, const Vector2 &B, const Vector2 &C, const Vector2 &D, Vector2 *Result);
bool doIntersect(const Vector2 &A,
				 const Vector2 &B,
				 const Vector2 &C,
				 const Vector2 &V,
				 double *distanceToIntersection);
bool isPointInRect(const Vector2 &P, const Vector2 &A, const Vector2 &B, const Vector2 &C, const Vector2 &D);
double distanceToSegment(const Vector2 &A, const Vector2 &B, const Vector2 &C);
Vector2 getReflected(const Vector2 &A, const Vector2 &B, const Vector2 &velocity, double cRel, double *intensity);
Vector2 getRefracted(const Vector2 &A, const Vector2 &B, const Vector2 &Velocity, double cRel, double *intensity);

#endif //SOLVER_VECTOR2_H
