#pragma once
#include "../math/vector3.hpp"
#include <string>
#ifndef BOOL3_H
#define BOOL3_H

// IMPORTANT: True must NOT be 0 — zero-init / memset would mark everyone as teammate
// and Silent Aim would skip all enemies (this broke silent after refactors).
enum class Bool3 : int {
    Unknown = 0,
    False = 1,
    True = 2
};
enum class XPose {
    Standing = 0, // Em pé (parado ou andando normalmente)
    Crouching = 1,// Agachado
    Dashing = 2,  // Correndo
    Creeping = 3,    // Deitado (barriga no chão)
    Jumping = 11, // Pulo (no ar)
    Knocked = 8   // Caído (ferido/incapacitado)
};

class Player {
public:
    uint32_t lastSeenFrame = 0;
    bool IsBot = false;
    bool IsKnown = false;
    bool IsDead = false;
    bool IsVisible = false;
    //por que deletuo isso manito

    int Level = 0;  // Nivel del jugador
    int CurrentShield = 0;
    uint64_t ID = 0;
    XPose Pose = XPose::Standing;
    int Distance = 0;
    short Health = 0;
    short Shield = 0;
    short Gun = 0;
    Bool3 IsTeam = Bool3::Unknown;
    bool IsKnocked = false;
    short WeaponID = 0;

    bool IsFemale = false;

    std::string Name;
    uintptr_t Address;
    Vector3 Head;       // Cabeça
    Vector3 Neck;       // Pescoço
    Vector3 RightShoulder; // Ombro direito
    Vector3 LeftShoulder;  // Ombro esquerdo
    Vector3 RightElbow;    // Cotovelo direito
    Vector3 LeftElbow;     // Cotovelo esquerdo
    Vector3 RightWrist;    // Pulso direito
    Vector3 LeftWrist;     // Pulso esquerdo
    Vector3 LeftHand;      // Mão esquerda
    Vector3 RightHand;     // Mão direita

    Vector3 Hip;          // Quadril
    Vector3 Groin;        // Virilha

    Vector3 RightAnkle;   // Tornozelo direito
    Vector3 LeftAnkle;    // Tornozelo esquerdo
    Vector3 LeftFoot;     // Pé esquerdo
    Vector3 RightFoot;    // Pé direito

    Vector3 Root;         // Raiz do corpo
    Vector3 RootBone;     // Raiz adicional do corpo
    float LastSeenTime;   // Time when this entity was last found in a scan
    float LastVisibleTime;// Time when this entity was last seen as VISIBLE
};

#endif
