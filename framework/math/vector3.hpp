#pragma once
#pragma warning(disable: 4201 4305 4244)
#define _USE_MATH_DEFINES
#include <math.h>
#include <string.h>

struct Vector3
{
    union
    {
        struct
        {
            float X;
            float Y;
            float Z;
        };
        float data[3];
    };

    /**
     * Constructors.
     */
    inline Vector3();
    inline Vector3(float data[]);
    inline Vector3(float value);
    inline Vector3(float x, float y);
    inline Vector3(float x, float y, float z);

    /**
     * Constants for common vectors.
     */
    static inline Vector3 Zero();
    static inline Vector3 One();
    static inline Vector3 Right();
    static inline Vector3 Left();
    static inline Vector3 Up();
    static inline Vector3 Down();
    static inline Vector3 Forward();
    static inline Vector3 Backward();

    /**
     * Returns the angle between two vectors in radians.
     */
    static inline float Angle(Vector3 a, Vector3 b);

    /**
     * Returns a vector with its magnitude clamped to maxLength.
     */
    static inline Vector3 ClampMagnitude(Vector3 vector, float maxLength);

    /**
     * Retorna o componente de a na direcao de b.
     */
    static inline float Component(Vector3 a, Vector3 b);

    /**
     * Retorna o produto vetorial de dois vetores.
     */
    static inline Vector3 Cross(Vector3 lhs, Vector3 rhs);

    /**
     * Returns the distance between a and b.
     */
    static inline float Distance(Vector3 a, Vector3 b);

    static inline char ToChar(Vector3 a);

    /**
     * Returns the dot product of two vectors.
     */
    static inline float Dot(Vector3 lhs, Vector3 rhs);

    /**
     * Converte uma representacao esferica de um vetor em cartesiano.
     */
    static inline Vector3 FromSpherical(float rad, float theta, float phi);

    /**
     * Returns a vector linearly interpolated between a and b.
     */
    static inline Vector3 Lerp(Vector3 a, Vector3 b, float t);
    static inline Vector3 LerpUnclamped(Vector3 a, Vector3 b, float t);

    /**
     * Returns the magnitude of a vector.
     */
    static inline float Magnitude(Vector3 v);

    static inline Vector3 Max(Vector3 a, Vector3 b);
    static inline Vector3 Min(Vector3 a, Vector3 b);

    static inline Vector3 MoveTowards(Vector3 current, Vector3 target, float maxDistanceDelta);
    static inline Vector3 Normalized(Vector3 v);
    static inline Vector3 Orthogonal(Vector3 v);
    static inline void OrthoNormalize(Vector3& normal, Vector3& tangent, Vector3& binormal);
    static inline Vector3 Project(Vector3 a, Vector3 b);
    static inline Vector3 ProjectOnPlane(Vector3 vector, Vector3 planeNormal);
    static inline Vector3 Reflect(Vector3 vector, Vector3 planeNormal);
    static inline Vector3 Reject(Vector3 a, Vector3 b);
    static inline Vector3 RotateTowards(Vector3 current, Vector3 target, float maxRadiansDelta, float maxMagnitudeDelta);
    static inline Vector3 Scale(Vector3 a, Vector3 b);
    static inline Vector3 Slerp(Vector3 a, Vector3 b, float t);
    static inline Vector3 SlerpUnclamped(Vector3 a, Vector3 b, float t);
    static inline float SqrMagnitude(Vector3 v);
    static inline void ToSpherical(Vector3 vector, float& rad, float& theta, float& phi);

    /**
     * Operator overloading.
     */
    inline struct Vector3& operator+=(const float rhs);
    inline struct Vector3& operator-=(const float rhs);
    inline struct Vector3& operator*=(const float rhs);
    inline struct Vector3& operator/=(const float rhs);
    inline struct Vector3& operator+=(const Vector3 rhs);
    inline struct Vector3& operator-=(const Vector3 rhs);
};

inline Vector3 operator-(Vector3 rhs);
inline Vector3 operator+(Vector3 lhs, const float rhs);
inline Vector3 operator-(Vector3 lhs, const float rhs);
inline Vector3 operator*(Vector3 lhs, const float rhs);
inline Vector3 operator/(Vector3 lhs, const float rhs);
inline Vector3 operator+(const float lhs, Vector3 rhs);
inline Vector3 operator-(const float lhs, Vector3 rhs);
inline Vector3 operator*(const float lhs, Vector3 rhs);
inline Vector3 operator/(const float lhs, Vector3 rhs);
inline Vector3 operator+(Vector3 lhs, const Vector3 rhs);
inline Vector3 operator-(Vector3 lhs, const Vector3 rhs);
inline bool operator==(const Vector3 lhs, const Vector3 rhs);
inline bool operator!=(const Vector3 lhs, const Vector3 rhs);

/*******************************************************************************
 * Implementation
 */

inline Vector3::Vector3() : X(0.0f), Y(0.0f), Z(0.0f) {}
inline Vector3::Vector3(float data[]) : X(data[0]), Y(data[1]), Z(data[2]) {}
inline Vector3::Vector3(float value) : X(value), Y(value), Z(value) {}
inline Vector3::Vector3(float x, float y) : X(x), Y(y), Z(0.0f) {}
inline Vector3::Vector3(float x, float y, float z) : X(x), Y(y), Z(z) {}

inline Vector3 Vector3::Zero() { return Vector3(0.0f, 0.0f, 0.0f); }
inline Vector3 Vector3::One() { return Vector3(1.0f, 1.0f, 1.0f); }
inline Vector3 Vector3::Right() { return Vector3(1.0f, 0.0f, 0.0f); }
inline Vector3 Vector3::Left() { return Vector3(-1.0f, 0.0f, 0.0f); }
inline Vector3 Vector3::Up() { return Vector3(0.0f, 1.0f, 0.0f); }
inline Vector3 Vector3::Down() { return Vector3(0.0f, -1.0f, 0.0f); }
inline Vector3 Vector3::Forward() { return Vector3(0.0f, 0.0f, 1.0f); }
inline Vector3 Vector3::Backward() { return Vector3(0.0f, 0.0f, -1.0f); }

inline float Vector3::Angle(Vector3 a, Vector3 b)
{
    float v = Dot(a, b) / (Magnitude(a) * Magnitude(b));
    v = fmaxf(v, -1.0f);
    v = fminf(v, 1.0f);
    return acosf(v);
}

inline Vector3 Vector3::ClampMagnitude(Vector3 vector, float maxLength)
{
    float length = Magnitude(vector);
    if (length > maxLength)
        vector *= maxLength / length;
    return vector;
}

inline float Vector3::Component(Vector3 a, Vector3 b)
{
    return Dot(a, b) / Magnitude(b);
}

inline Vector3 Vector3::Cross(Vector3 lhs, Vector3 rhs)
{
    float x = lhs.Y * rhs.Z - lhs.Z * rhs.Y;
    float y = lhs.Z * rhs.X - lhs.X * rhs.Z;
    float z = lhs.X * rhs.Y - lhs.Y * rhs.X;
    return Vector3(x, y, z);
}

inline float Vector3::Distance(Vector3 a, Vector3 b)
{
    return Vector3::Magnitude(a - b);
}

inline float Vector3::Dot(Vector3 lhs, Vector3 rhs)
{
    return lhs.X * rhs.X + lhs.Y * rhs.Y + lhs.Z * rhs.Z;
}

inline Vector3 Vector3::FromSpherical(float rad, float theta, float phi)
{
    Vector3 v;
    v.X = rad * sinf(theta) * cosf(phi);
    v.Y = rad * sinf(theta) * sinf(phi);
    v.Z = rad * cosf(theta);
    return v;
}

inline Vector3 Vector3::Lerp(Vector3 a, Vector3 b, float t)
{
    if (t < 0.0f) return a;
    else if (t > 1.0f) return b;
    return LerpUnclamped(a, b, t);
}

inline Vector3 Vector3::LerpUnclamped(Vector3 a, Vector3 b, float t)
{
    return (b - a) * t + a;
}

inline float Vector3::Magnitude(Vector3 v)
{
    return sqrtf(SqrMagnitude(v));
}

inline Vector3 Vector3::Max(Vector3 a, Vector3 b)
{
    float x = a.X > b.X ? a.X : b.X;
    float y = a.Y > b.Y ? a.Y : b.Y;
    float z = a.Z > b.Z ? a.Z : b.Z;
    return Vector3(x, y, z);
}

inline Vector3 Vector3::Min(Vector3 a, Vector3 b)
{
    float x = a.X > b.X ? b.X : a.X;
    float y = a.Y > b.Y ? b.Y : a.Y;
    float z = a.Z > b.Z ? b.Z : a.Z;
    return Vector3(x, y, z);
}

inline Vector3 Vector3::MoveTowards(Vector3 current, Vector3 target, float maxDistanceDelta)
{
    Vector3 d = target - current;
    float m = Magnitude(d);
    if (m < maxDistanceDelta || m == 0.0f)
        return target;
    return current + (d * maxDistanceDelta / m);
}

inline Vector3 Vector3::Normalized(Vector3 v)
{
    float mag = Magnitude(v);
    if (mag == 0.0f)
        return Vector3::Zero();
    return v / mag;
}

inline Vector3 Vector3::Orthogonal(Vector3 v)
{
    return v.Z < v.X ? Vector3(v.Y, -v.X, 0.0f) : Vector3(0.0f, -v.Z, v.Y);
}

inline void Vector3::OrthoNormalize(Vector3& normal, Vector3& tangent, Vector3& binormal)
{
    normal = Normalized(normal);
    tangent = ProjectOnPlane(tangent, normal);
    tangent = Normalized(tangent);
    binormal = ProjectOnPlane(binormal, tangent);
    binormal = ProjectOnPlane(binormal, normal);
    binormal = Normalized(binormal);
}

inline Vector3 Vector3::Project(Vector3 a, Vector3 b)
{
    float m = Magnitude(b);
    return Dot(a, b) / (m * m) * b;
}

inline Vector3 Vector3::ProjectOnPlane(Vector3 vector, Vector3 planeNormal)
{
    return Reject(vector, planeNormal);
}

inline Vector3 Vector3::Reflect(Vector3 vector, Vector3 planeNormal)
{
    return vector - 2.0f * Project(vector, planeNormal);
}

inline Vector3 Vector3::Reject(Vector3 a, Vector3 b)
{
    return a - Project(a, b);
}

inline Vector3 Vector3::RotateTowards(Vector3 current, Vector3 target, float maxRadiansDelta, float maxMagnitudeDelta)
{
    float magCur = Magnitude(current);
    float magTar = Magnitude(target);
    float newMag = magCur + maxMagnitudeDelta * ((magTar > magCur) - (magCur > magTar));
    newMag = fminf(newMag, fmaxf(magCur, magTar));
    newMag = fmaxf(newMag, fminf(magCur, magTar));

    float totalAngle = Angle(current, target) - maxRadiansDelta;
    if (totalAngle <= 0.0f)
        return Normalized(target) * newMag;
    else if (totalAngle >= (float)M_PI)
        return Normalized(-target) * newMag;

    Vector3 axis = Cross(current, target);
    float magAxis = Magnitude(axis);
    if (magAxis == 0.0f)
        axis = Normalized(Cross(current, current + Vector3(3.95f, 5.32f, -4.24f)));
    else
        axis /= magAxis;
    current = Normalized(current);
    Vector3 newVector = current * cosf(maxRadiansDelta) + Cross(axis, current) * sinf(maxRadiansDelta);
    return newVector * newMag;
}

inline Vector3 Vector3::Scale(Vector3 a, Vector3 b)
{
    return Vector3(a.X * b.X, a.Y * b.Y, a.Z * b.Z);
}

inline Vector3 Vector3::Slerp(Vector3 a, Vector3 b, float t)
{
    if (t < 0.0f) return a;
    else if (t > 1.0f) return b;
    return SlerpUnclamped(a, b, t);
}

inline Vector3 Vector3::SlerpUnclamped(Vector3 a, Vector3 b, float t)
{
    float magA = Magnitude(a);
    float magB = Magnitude(b);
    a /= magA;
    b /= magB;
    float dot = Dot(a, b);
    dot = fmaxf(dot, -1.0f);
    dot = fminf(dot, 1.0f);
    float theta = acosf(dot) * t;
    Vector3 relativeVec = Normalized(b - a * dot);
    Vector3 newVec = a * cosf(theta) + relativeVec * sinf(theta);
    return newVec * (magA + (magB - magA) * t);
}

inline float Vector3::SqrMagnitude(Vector3 v)
{
    return v.X * v.X + v.Y * v.Y + v.Z * v.Z;
}

inline void Vector3::ToSpherical(Vector3 vector, float& rad, float& theta, float& phi)
{
    rad = Magnitude(vector);
    float v = vector.Z / rad;
    v = fmaxf(v, -1.0f);
    v = fminf(v, 1.0f);
    theta = acosf(v);
    phi = atan2f(vector.Y, vector.X);
}

inline struct Vector3& Vector3::operator+=(const float rhs)
{
    X += rhs;
    Y += rhs;
    Z += rhs;
    return *this;
}

inline struct Vector3& Vector3::operator-=(const float rhs)
{
    X -= rhs;
    Y -= rhs;
    Z -= rhs;
    return *this;
}

inline struct Vector3& Vector3::operator*=(const float rhs)
{
    X *= rhs;
    Y *= rhs;
    Z *= rhs;
    return *this;
}

inline struct Vector3& Vector3::operator/=(const float rhs)
{
    X /= rhs;
    Y /= rhs;
    Z /= rhs;
    return *this;
}

inline struct Vector3& Vector3::operator+=(const Vector3 rhs)
{
    X += rhs.X;
    Y += rhs.Y;
    Z += rhs.Z;
    return *this;
}

inline struct Vector3& Vector3::operator-=(const Vector3 rhs)
{
    X -= rhs.X;
    Y -= rhs.Y;
    Z -= rhs.Z;
    return *this;
}

inline char Vector3::ToChar(Vector3 a) {
    (void)a;
    return '\0';
}

inline Vector3 operator-(Vector3 rhs) { return rhs * -1.0f; }
inline Vector3 operator+(Vector3 lhs, const float rhs) { return lhs += rhs; }
inline Vector3 operator-(Vector3 lhs, const float rhs) { return lhs -= rhs; }
inline Vector3 operator*(Vector3 lhs, const float rhs) { return lhs *= rhs; }
inline Vector3 operator/(Vector3 lhs, const float rhs) { return lhs /= rhs; }
inline Vector3 operator+(const float lhs, Vector3 rhs) { return rhs += lhs; }
inline Vector3 operator-(const float lhs, Vector3 rhs) { return rhs -= lhs; }
inline Vector3 operator*(const float lhs, Vector3 rhs) { return rhs *= lhs; }
inline Vector3 operator/(const float lhs, Vector3 rhs) { return rhs /= lhs; }
inline Vector3 operator+(Vector3 lhs, const Vector3 rhs) { return lhs += rhs; }
inline Vector3 operator-(Vector3 lhs, const Vector3 rhs) { return lhs -= rhs; }

inline bool operator==(const Vector3 lhs, const Vector3 rhs)
{
    return lhs.X == rhs.X && lhs.Y == rhs.Y && lhs.Z == rhs.Z;
}

inline bool operator!=(const Vector3 lhs, const Vector3 rhs)
{
    return !(lhs == rhs);
}
