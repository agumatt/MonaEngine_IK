#pragma once
#ifndef TRAJECTORYGENERATOR_HPP
#define TRAJECTORYGENERATOR_HPP

#include "TrajectoryGeneratorBase.hpp"

namespace Mona{

    typedef int ChainIndex;
    typedef int AnimationIndex;
    typedef int FrameIndex;

    class IKRig;
    class IKAnimation;
    class TrajectoryGenerator {
        friend class IKNavigationComponent;
        friend class IKRigController;
    private:
        IKRig* m_ikRig;
        EnvironmentData m_environmentData;
        StrideCorrector m_strideCorrector;
        bool m_strideValidationEnabled;
        bool m_strideCorrectionEnabled;
        void generateEETrajectory(ChainIndex ikChain, IKAnimation* ikAnim,
            ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
        void generateFixedTrajectory(glm::vec2 basePos,
            glm::vec2 timeRange, int baseCurveID, float supportHeight,
            EEGlobalTrajectoryData* trData, ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
        void generateHipTrajectory(IKAnimation* ikAnim,
            ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
		float calcHipAdjustedHeight(std::pair<EEGlobalTrajectoryData*, EEGlobalTrajectoryData*> oppositeTrajectories, 
            float targetCurvesTime_rep,
            float originalCurvesTime_extendedAnim, IKAnimation* ikAnim,
            ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
        bool calcStrideStartingPoint(float supportHeightStart, glm::vec2 xyReferencePoint, float targetDistance, 
            glm::vec2 targetDirection, glm::vec3& outStrideStartPoint,
            ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
        bool calcStrideFinalPoint(EEGlobalTrajectoryData* baseTrajectoryData, int baseTrajecoryID,
            IKAnimation* ikAnim,
            glm::vec3 startingPoint, float targetDistance,
            glm::vec2 targetDirection, glm::vec3& outStrideFinalPoint,
            ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
		static void buildHipTrajectory(IKAnimation* ikAnim, std::vector<glm::vec3> const& hipGlobalPositions);
		static void buildEETrajectories(IKAnimation* ikAnim,
			std::vector<std::vector<bool>> supportFramesPerChain,
			std::vector<std::vector<glm::vec3>> globalPositionsPerChain,
            std::vector<ChainIndex> oppositePerChain);
    public:
        TrajectoryGenerator(IKRig* ikRig);
        TrajectoryGenerator() = default;
        void init();
        void generateNewTrajectories(AnimationIndex animIndex,
            ComponentManager<TransformComponent>& transformManager,
            ComponentManager<StaticMeshComponent>& staticMeshManager);
        void enableStrideValidation(bool enableStrideValidation) { m_strideValidationEnabled = enableStrideValidation; }
        void enableStrideCorrection(bool enableStrideCorrection) { m_strideCorrectionEnabled = enableStrideCorrection; }
    };

    
}





#endif