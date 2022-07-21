#pragma once
#ifndef IKRIGBASE_HPP
#define IKRIGBASE_HPP

#include <memory>
#include "EnvironmentData.hpp"
#include "../PhysicsCollision/RigidBodyLifetimePolicy.hpp"
#include "../Animation/SkeletalMeshComponent.hpp"
#include "ParametricCurves.hpp"
#include "TrajectoryGenerator.hpp"

namespace Mona {
    typedef int AnimationIndex;
    typedef int JointIndex;
    typedef int FrameIndex;
    typedef int ChainIndex;
    class ForwardKinematics;

    class JointRotation {
    private:
        // Rotacion en formato cuaternion
        glm::fquat m_quatRotation;
        // Eje de rotacion
        glm::vec3 m_rotationAxis;
        // Angulo de rotacion
        float m_rotationAngle;
    public:
        JointRotation();
        JointRotation(float rotationAngle, glm::vec3 rotationAxis);
        JointRotation(glm::fquat quatRotation);
        void setRotation(glm::fquat rotation);
        void setRotation(float rotationAngle, glm::vec3 rotationAxis);
        void setRotationAngle(float rotationAngle);
        void setRotationAxis(glm::vec3 rotationAxis);
        glm::fquat getQuatRotation() const { return m_quatRotation; }
        float getRotationAngle() const { return m_rotationAngle; }
        glm::vec3 getRotationAxis() const { return m_rotationAxis; }
    };
    


    enum class  AnimationType {
        IDLE,
        MOVING
    };

    
    class IKRigConfig {
        friend class IKRig;
        friend class IKRigController;
    private:
        // Clip de animacion asociado a esta configuracion
        std::shared_ptr<AnimationClip> m_animationClip;
        // Rotaciones para cada joint de la animacion base para cada frame 
        std::vector<std::vector<JointRotation>> m_baseJointRotations;
        // Rotaciones modificables para cada joint
        std::vector<std::vector<JointRotation>> m_dynamicJointRotations;
        // Escalas para cada joint de la animacion base (fijas)
        std::vector<glm::vec3> m_jointScales;
        // Posiciones para cada joint de la animacion base (fijas)
        std::vector<glm::vec3> m_jointPositions;
        // Timestamps para las rotaciones indexadas por frame
        std::vector<float> m_timeStamps;
        // Indice de la animacion que le corresponde a esta configuracion
        AnimationIndex m_animIndex = -1;
        ForwardKinematics* m_forwardKinematics;
        // Data de trayectoria para cada ikChain (mantiene orden del arreglo original de cadenas)
        std::vector<EEGlobalTrajectoryData> m_ikChainTrajectoryData;
        // Data de trayectoria para la cadera
        HipGlobalTrajectoryData m_hipTrajectoryData;
        // Tiempo actual de la animacion
        float m_currentReproductionTime = -1;
        // Indica el frame mas reciente de la animacion
        FrameIndex m_currentFrameIndex = -1;
        // Indice del siguiente frame de la animacion
        FrameIndex m_nextFrameIndex = -1;
        // Indica si es necesario actualizar las rotaciones de las joints
        bool m_onFrame = true;
        // Numero de veces que la animacion se ha reproducido
        int m_reproductionCount = 0;
        //
        AnimationType m_animationType;
    public:
        IKRigConfig(std::shared_ptr<AnimationClip> animation, AnimationIndex animIndex, ForwardKinematics* fk);
        const std::vector<JointRotation>& getBaseJointRotations(FrameIndex frame) const { return m_baseJointRotations[frame]; }
        std::vector<JointRotation>* getDynamicJointRotations(FrameIndex frame) { return &(m_dynamicJointRotations[frame]); }
        const std::vector<glm::vec3>& getJointScales() const { return m_jointScales; }
        const std::vector<glm::vec3>& getJointPositions() const { return m_jointPositions; }
        float getReproductionTime(float animationTime, int repCountOffset = 0);
        float getReproductionTime(FrameIndex frame, int repCountOffset = 0);
        float getAnimationTime(FrameIndex frame);
        float getAnimationTime(float reproductionTime);
        FrameIndex getFrame(float animationTime);
        float getAnimationDuration() { return m_animationClip->GetDuration(); }
        int getFrameNum() { return m_timeStamps.size(); }
        int getReproductionCount() const { return m_reproductionCount; }
        std::vector<JointRotation>* getDynamicJointRotationsPtr() { return &(m_dynamicJointRotations[m_nextFrameIndex]);  }
        float getCurrentReproductionTime() const { return m_currentReproductionTime; }
        FrameIndex getNextFrameIndex() const { return m_nextFrameIndex; }
        FrameIndex getCurrentFrameIndex() const { return m_currentFrameIndex; }
        std::vector<glm::mat4> getModelSpaceTransforms(bool useDynamicRotations);
        glm::mat4 getModelSpaceTransform(JointIndex jointIndex, FrameIndex frame, bool useDynamicRotations);
        std::vector<glm::vec3> getModelSpacePositions(bool useDynamicRotations);
        std::vector<glm::vec3> getModelSpacePositions(FrameIndex frame, bool useDynamicRotations);
        std::vector<glm::mat4> getCustomSpaceTransforms(glm::mat4 baseTransform, FrameIndex frame, bool useDynamicRotations);
        std::vector<glm::mat4> getJointSpaceTransforms(bool useDynamicRotations);
        EEGlobalTrajectoryData* getEETrajectoryData(ChainIndex chainIndex) { return &(m_ikChainTrajectoryData[chainIndex]); }
        HipGlobalTrajectoryData* getHipTrajectoryData() { return &m_hipTrajectoryData; }
        AnimationType getAnimationType() { return m_animationType; }
        // ajustar animationTime input al rango correspondiente (del arreglo de timeStamps)
        float adjustAnimationTime(float animationTime);
    };

    struct JointData {
        // Minimo angulo de rotacion de la articulacion (grados)
        float minAngle = -90;
        // Maximo angulo de rotacion de la articulacion (grados)
        float maxAngle = 90;
        float weight = 1;
        bool enableIKRotation = false;
    };
    struct ChainEnds {
        // Nombre de la articulacion base de la cadena (al ser la base no se le aplica IK)
        std::string baseJointName;
        // Nombre de la articulacion final de la cadena
        std::string endEffectorName;
    };
    class IKChain {
        friend class IKRig;
        // Nombre de la cadena
        std::string m_name;
        // Articulaciones que conforman la cadena, desde el origen hasta el ee
        std::vector<JointIndex> m_joints;
        // Objetivo actual para el end effector (donde debe posicionarse) (model space)
        glm::vec3 m_currentEETarget;
    public:
        IKChain() = default;
        const std::string& getName() const { return m_name; };
        const std::vector<JointIndex>& getJoints() const { return m_joints; };
        const glm::vec3& getCurrentEETarget() const { return m_currentEETarget; };
    };
    struct RigData {
        friend class IKRig;
    private:
        std::unordered_map<std::string, JointData> jointData;
    public:
        ChainEnds leftLeg;
        ChainEnds rightLeg;
        ChainEnds leftFoot;
        ChainEnds rightFoot;
        float scale;
        std::string hipJointName;
        void setJointData(std::string jointName, float minAngle, float maxAngle, float weight = 1, bool enableData = true);
        void enableJointData(std::string jointName, bool enableData);
        JointData getJointData(std::string jointName);
        bool isValid();
    };

    class IKNode {
        friend class IKRig;
        float m_weight = 1;
        // Minimo angulo de rotacion de la articulacion (radianes)
        float m_minAngle = -90;
        // Maximo angulo de rotacion de la articulacion (radianes)
        float m_maxAngle = 90;
        // Nombre de la articulacion
        std::string m_jointName;
        // Indice de la articulacion
        int m_jointIndex = -1;
        // Puntero al nodo padre
        IKNode* m_parent = nullptr;
    public:
        IKNode() = default;
        IKNode(std::string jointName, JointIndex jointIndex, IKNode * parent = nullptr, float weight = 1);
        IKNode* getParent() const { return m_parent; }
        glm::vec2 getMotionRange() const { return glm::vec2(m_minAngle, m_maxAngle); }
        JointIndex getIndex() const { return m_jointIndex; }
    };

}










#endif